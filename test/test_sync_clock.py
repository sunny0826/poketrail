import contextlib
import io
import sys
import time
import unittest
from pathlib import Path
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "scripts"))

import sync_clock as clock  # noqa: E402

TARGET = 1792670400
STALE_UTC = 1767259321  # 2026-01-01 07:22:01 UTC, the field baseline date.


def reply(kind, **overrides):
    fields = {
        "mode": "official", "rtc": "1", "low": "0", "io": "0",
        "utc": str(TARGET), "off": "480", "day": "20261022",
        "cal": "1", "tr": "1", "pend": "0", "anom": "0",
        "ready": "1", "busy": "0", "save": "0", "tx": "0", "seq": "1",
    }
    fields.update(overrides)
    return "PTC " + kind + " " + " ".join(f"{key}={value}" for key, value in fields.items())


class FakePort:
    def __init__(self, handler):
        self.handler = handler
        self.rx = bytearray()
        self.writes = []
        self.closed = False
        self.drop_replies = 0
        self.drop_set_replies = 0
        self.short_write_prefix = None
        self.short_write_bytes = 0
        self.reset_count = 0

    @property
    def in_waiting(self):
        return len(self.rx)

    def write(self, data):
        self.writes.append(bytes(data))
        if self.short_write_prefix and bytes(data).startswith(self.short_write_prefix):
            return self.short_write_bytes
        response = self.handler(bytes(data))
        if self.drop_replies:
            self.drop_replies -= 1
            return len(data)
        if self.drop_set_replies and not bytes(data).startswith(b"PTC?"):
            self.drop_set_replies -= 1
            return len(data)
        self.rx.extend(response)
        return len(data)

    def read(self, size=1):
        size = max(1, min(size, len(self.rx)))
        chunk = bytes(self.rx[:size])
        del self.rx[:size]
        return chunk

    def reset_input_buffer(self):
        self.reset_count += 1
        self.rx.clear()

    def close(self):
        self.closed = True


class FakeDevice:
    def __init__(self, calibrated=False, offset=480, utc=TARGET, tx="0"):
        self.state = {
            "tx": tx, "cal": "1" if calibrated else "0",
            "off": str(offset) if calibrated else "0",
            "tr": "1" if calibrated else "0",
            "utc": str(utc),
        }
        self.set_count = 0
        self.save_failures = 0
        self.pending_save = False

    def __call__(self, command):
        parts = command.decode("ascii").strip().split()
        if parts == ["PTC?"]:
            return ("noise line\n" + reply("q", save="1" if self.pending_save else "0",
                                           **self.state) + "\n").encode()
        if parts[0] == "PTC" and len(parts) == 4:
            tx, utc, offset = parts[1], parts[2], parts[3]
            self.set_count += 1
            if self.save_failures:
                self.save_failures -= 1
                self.pending_save = True
                return (reply("s", id=tx, result="save", tx=self.state["tx"],
                              save="1", tr="0", cal=self.state["cal"],
                              off=self.state["off"]) + "\n").encode()
            self.pending_save = False
            self.state.update(tx=tx, utc=utc, off=offset, cal="1", tr="1")
            return ("ui log\n" + reply("s", id=tx, result="ok", **self.state) + "\n").encode()
        return b""


def run_sync(port, **kwargs):
    options = {"port": "fake", "timeout": 1.0,
               "now": lambda: TARGET, "host_offset": lambda: 480}
    options.update(kwargs)
    return clock.sync(lambda _: port, **options)


class ClockSyncTests(unittest.TestCase):
    def test_parse_reply(self):
        parsed = clock.parse_reply(reply("q"))
        self.assertEqual(parsed["_kind"], "q")
        self.assertEqual(parsed["cal"], "1")
        self.assertIsNone(clock.parse_reply("boot log"))
        self.assertEqual(clock.parse_reply("PTC e result=overflow")["_kind"], "e")
        with self.assertRaises(ValueError):
            clock.parse_reply("PTC q cal=1 cal=0")
        with self.assertRaises(ValueError):
            clock.parse_reply("PTC q broken")

    def test_offset_policy(self):
        self.assertEqual(clock.resolve_offset(clock.parse_reply(reply("q")), 0), 480)
        uncalibrated = clock.parse_reply(reply("q", cal="0", off="0"))
        self.assertEqual(clock.resolve_offset(uncalibrated, -300), -300)
        with self.assertRaises(ValueError):
            clock.resolve_offset(uncalibrated, 900)
        with self.assertRaises(clock.ClockSyncError):
            clock.resolve_offset(clock.parse_reply(reply("q", off="900")), 0)

    def test_transaction_id(self):
        self.assertEqual(clock.choose_transaction_id(5, TARGET), TARGET)
        self.assertEqual(clock.choose_transaction_id(TARGET, TARGET), TARGET + 1)
        with self.assertRaises(ValueError):
            clock.choose_transaction_id(0xFFFFFFFF, 1)

    def test_validate_success(self):
        status = clock.parse_reply(reply("s", id="7", result="ok", tx="7"))
        clock.validate_success(status, transaction_id=7, target_utc=TARGET,
                               offset=480, now_epoch=TARGET + 2)
        for overrides in ({"rtc": "0"}, {"tr": "0"}, {"pend": "1"},
                          {"mode": "test"}, {"off": "0"}, {"cal": "0"},
                          {"utc": str(TARGET + 6)}, {"tx": "5"}, {"tx": "8"}):
            with self.assertRaises(clock.ClockSyncError):
                clock.validate_success(
                    clock.parse_reply(reply("s", id="7", result="ok", **overrides)),
                    transaction_id=7, target_utc=TARGET, offset=480,
                    now_epoch=TARGET + 2)
        with self.assertRaises(clock.ClockSyncError):
            clock.validate_success(
                clock.parse_reply(reply("s", id="8", result="ok", tx="8")),
                transaction_id=7, target_utc=TARGET, offset=480,
                now_epoch=TARGET + 2)
        with self.assertRaises(clock.ClockSyncError):
            clock.validate_success(status, transaction_id=7, target_utc=TARGET,
                                   offset=480, now_epoch=TARGET + 60)

    def test_validate_query(self):
        status = clock.parse_reply(reply("q"))
        clock.validate_query(status, now_epoch=TARGET)
        for overrides in ({"cal": "0"}, {"tr": "0"}, {"pend": "1"},
                          {"rtc": "0"}, {"mode": "test"}, {"ready": "0"}):
            with self.assertRaises(clock.ClockSyncError):
                clock.validate_query(clock.parse_reply(reply("q", **overrides)),
                                     now_epoch=TARGET)

    def test_validate_query_rejects_stale_utc(self):
        for delta in (-16, 16):
            with self.assertRaises(clock.ClockSyncError) as caught:
                clock.validate_query(
                    clock.parse_reply(reply("q", utc=str(TARGET + delta))),
                    now_epoch=TARGET)
            self.assertIn("容差", str(caught.exception))
            self.assertIn(str(abs(delta)), str(caught.exception))
        clock.validate_query(clock.parse_reply(reply("q", utc=str(TARGET + 15))),
                             now_epoch=TARGET)

    def test_full_sync_uses_host_offset_when_uncalibrated(self):
        device = FakeDevice(calibrated=False)
        port = FakePort(device)
        result = run_sync(port)
        self.assertEqual(result["result"], "ok")
        self.assertEqual(device.state["off"], "480")
        self.assertEqual(device.state["utc"], str(TARGET))
        self.assertEqual(len(port.writes), 2)
        self.assertTrue(port.closed)

    def test_calibrated_offset_wins(self):
        device = FakeDevice(calibrated=True, offset=480)
        port = FakePort(device)
        result = run_sync(port, host_offset=lambda: 0)
        self.assertEqual(result["result"], "ok")
        self.assertEqual(device.state["off"], "480")

    def test_save_retry_reuses_identity(self):
        device = FakeDevice(calibrated=False)
        device.save_failures = 1
        port = FakePort(device)
        result = run_sync(port)
        self.assertEqual(result["result"], "ok")
        sets = [line for line in port.writes if line.startswith(b"PTC ")]
        self.assertEqual(len(sets), 2)
        self.assertEqual(sets[0], sets[1])

    def test_lost_reply_is_recovered_by_bound_readback(self):
        device = FakeDevice(calibrated=False)
        port = FakePort(device)
        port.drop_set_replies = 1
        result = run_sync(port)
        self.assertEqual(result["_kind"], "q")
        self.assertEqual(result["_confirmed_by"], "query")
        self.assertEqual(result["tx"], str(TARGET))
        self.assertEqual(result["_raw"], reply("q", tx=str(TARGET), utc=str(TARGET), off="480"))
        self.assertEqual(device.set_count, 1)
        self.assertTrue(port.closed)

    def test_foreign_transaction_readback_is_not_success(self):
        def handler(command):
            parts = command.decode("ascii").strip().split()
            if parts == ["PTC?"]:
                # The clock happens to show the target time, but this belongs to
                # transaction 5, not the request id the host sent.
                return (reply("q", tx="5", utc=str(TARGET)) + "\n").encode()
            return b""  # Lost before the device processed the set request.

        port = FakePort(handler)
        with self.assertRaises(clock.ClockSyncError):
            run_sync(port, timeout=1.5)
        self.assertTrue(port.closed)

    def test_lost_save_reply_retries_same_identity(self):
        device = FakeDevice(calibrated=False)
        device.save_failures = 1
        port = FakePort(device)
        port.drop_set_replies = 1
        result = run_sync(port, timeout=1.5)
        self.assertEqual(result["result"], "ok")
        sets = [line for line in port.writes if line.startswith(b"PTC ")]
        self.assertEqual(len(sets), 2)
        self.assertEqual(sets[0], sets[1])

    def test_test_mode_rejected_before_set(self):
        commands = []

        def handler(command):
            commands.append(command)
            return (reply("q", mode="test", cal="1", tr="1") + "\n").encode()

        port = FakePort(handler)
        with self.assertRaises(clock.ClockSyncError):
            run_sync(port, timeout=0.5)
        self.assertEqual(len(commands), 1)
        self.assertTrue(commands[0].startswith(b"PTC?"))
        self.assertTrue(port.closed)

    def test_query_only(self):
        device = FakeDevice(calibrated=True)
        port = FakePort(device)
        result = run_sync(port, timeout=0.5, query_only=True)
        self.assertEqual(result["_kind"], "q")
        self.assertEqual(len(port.writes), 1)
        self.assertGreaterEqual(port.reset_count, 1)
        self.assertTrue(port.closed)

    def test_query_only_rejects_stale_device_time(self):
        device = FakeDevice(calibrated=True, utc=STALE_UTC)
        port = FakePort(device)
        with self.assertRaises(clock.ClockSyncError) as caught:
            run_sync(port, timeout=0.4, query_only=True)
        self.assertIn("容差", str(caught.exception))
        self.assertTrue(port.closed)

    def test_query_discards_stale_backlog(self):
        device = FakeDevice(calibrated=True, utc=TARGET)
        port = FakePort(device)
        port.rx.extend((reply("q", tx="5", utc=str(STALE_UTC)) + "\n").encode())
        result = run_sync(port, timeout=0.5, query_only=True)
        self.assertEqual(result["utc"], str(TARGET))
        self.assertEqual(result["tx"], "0")

    def test_short_write_is_not_ignored(self):
        port = FakePort(lambda _: b"")
        port.short_write_prefix = b"PTC "
        port.short_write_bytes = 4
        channel = clock._Channel(port, lambda _: None, time.monotonic)
        with self.assertRaises(clock.TransportError) as caught:
            channel.send(f"PTC 7 {TARGET} 480")
        self.assertIn("写入不完整", str(caught.exception))

    def test_short_set_write_never_reaches_device(self):
        device = FakeDevice(calibrated=True, utc=TARGET)
        port = FakePort(device)
        port.short_write_prefix = b"PTC "
        port.short_write_bytes = 4
        with self.assertRaises(clock.ClockSyncError):
            run_sync(port, timeout=0.8)
        self.assertEqual(device.set_count, 0)
        self.assertTrue(port.closed)

    def test_select_port_rejects_ambiguous_candidates(self):
        with self.assertRaises(clock.ClockSyncError) as caught:
            clock._select_port(["/dev/cu.usbmodem1101", "/dev/cu.usbserial-210"])
        self.assertIn("--port", str(caught.exception))
        self.assertEqual(clock._select_port(["/dev/cu.usbmodem1101"]),
                         "/dev/cu.usbmodem1101")
        self.assertEqual(clock._select_port([]), "")

    def test_cli_reports_ambiguous_ports_without_traceback(self):
        stderr = io.StringIO()
        error = clock.ClockSyncError(
            "检测到多个 CDC 串口：a、b；请用 --port 指定目标设备")
        with mock.patch.object(clock, "_detect_port", side_effect=error):
            with contextlib.redirect_stderr(stderr):
                code = clock.main(["--query"])
        self.assertEqual(code, 2)
        self.assertIn("--port", stderr.getvalue())
        self.assertNotIn("Traceback", stderr.getvalue())

    def test_cli_query_stale_exit_is_nonzero(self):
        device = FakeDevice(calibrated=True, utc=STALE_UTC)
        port = FakePort(device)
        stderr = io.StringIO()
        with mock.patch.object(clock, "_open_serial", lambda _port: port):
            with contextlib.redirect_stderr(stderr):
                code = clock.main(["--port", "fake", "--query", "--timeout", "0.4"])
        self.assertEqual(code, 1)
        self.assertIn("容差", stderr.getvalue())
        self.assertNotIn("设备时钟状态正常", stderr.getvalue())
        self.assertNotIn("Traceback", stderr.getvalue())
        self.assertTrue(port.closed)

    def test_timeout_is_bounded(self):
        port = FakePort(lambda _: b"")
        started = time.monotonic()
        with self.assertRaises(clock.ClockSyncError):
            run_sync(port, timeout=0.15)
        self.assertLess(time.monotonic() - started, 2.0)
        self.assertTrue(port.closed)


if __name__ == "__main__":
    unittest.main()
