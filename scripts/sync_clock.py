#!/usr/bin/env python3
"""烧录后通过 USB CDC 把设备 RTC 校到主机当前时间。

协议（每行一条，设备回复以 ``PTC `` 开头）::

    PTC?                            只读状态
    PTC <id> <utcSeconds> <offset>  发起一次正式时钟事务

设备只有在完成 RTC 回读与第二次存档提交后才回复 ``result=ok``。主机脚本
会校验回执中的 UTC、时区、校准/可信/待提交状态，失败时非零退出。
"""

from __future__ import annotations

import argparse
import sys
import time
from datetime import datetime

MAX_LINE = 512
DEFAULT_TIMEOUT = 30.0
READBACK_WINDOW_SECONDS = 5
CLOCK_MATCH_SECONDS = 15


class ClockSyncError(Exception):
    """Device, protocol or validation failure."""


class TransportError(ClockSyncError):
    """USB CDC open/read/write failure or timeout."""


def host_offset_minutes() -> int:
    offset = datetime.now().astimezone().utcoffset()
    if offset is None:
        raise ValueError("无法确定主机时区偏移")
    minutes = int(offset.total_seconds() // 60)
    if not -720 <= minutes <= 840:
        raise ValueError(f"主机时区偏移超出范围: {minutes}")
    return minutes


def parse_reply(line: str):
    """Parse one PTC line; return None for unrelated log lines."""
    text = line.strip()
    if not text.startswith("PTC "):
        return None
    parts = text.split()
    if len(parts) < 2 or parts[0] != "PTC" or parts[1] not in {"q", "s", "e"}:
        return None
    fields: dict[str, str] = {}
    for token in parts[2:]:
        if "=" not in token:
            raise ValueError(f"无法解析字段: {token}")
        key, value = token.split("=", 1)
        if not key or not value or key in fields:
            raise ValueError(f"非法或重复字段: {token}")
        fields[key] = value
    fields["_kind"] = parts[1]
    fields["_raw"] = text
    return fields


def _flag(status: dict, key: str) -> bool:
    value = status.get(key)
    if value not in ("0", "1"):
        raise ClockSyncError(f"回执缺少有效的 {key}")
    return value == "1"


def _number(status: dict, key: str, low: int, high: int) -> int:
    value = status.get(key)
    try:
        number = int(value)
    except (TypeError, ValueError) as exc:
        raise ClockSyncError(f"回执缺少有效的 {key}") from exc
    if not low <= number <= high:
        raise ClockSyncError(f"回执 {key} 超出范围: {number}")
    return number


def resolve_offset(status: dict, host_offset: int) -> int:
    """已校准设备保留自己的时区；未校准时采用主机时区。"""
    if _flag(status, "cal"):
        return _number(status, "off", -720, 840)
    if not -720 <= host_offset <= 840:
        raise ValueError(f"主机时区偏移超出范围: {host_offset}")
    return host_offset


def choose_transaction_id(last_tx: int, now_epoch: float) -> int:
    """Pick a request id greater than the device's last transaction id."""
    if not 0 <= last_tx <= 0xFFFFFFFF:
        raise ValueError(f"设备事务号超出范围: {last_tx}")
    value = int(now_epoch) & 0xFFFFFFFF
    if value == 0:
        value = 1
    if value <= last_tx:
        if last_tx == 0xFFFFFFFF:
            raise ValueError("设备事务号无法继续递增")
        value = last_tx + 1
    return value


def _require_official(status: dict) -> None:
    if status.get("mode") != "official":
        raise ClockSyncError("设备处于测试模式，拒绝正式校时")
    if not _flag(status, "ready"):
        raise ClockSyncError("设备尚未就绪")


def _utc_text(epoch: int) -> str:
    return time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime(epoch))


def validate_query(status: dict, *, now_epoch: int) -> None:
    if status.get("_kind") != "q":
        raise ClockSyncError("不是状态回执")
    _require_official(status)
    if not _flag(status, "rtc"):
        raise ClockSyncError("RTC 回读无效")
    if not _flag(status, "cal"):
        raise ClockSyncError("设备尚未校准")
    if not _flag(status, "tr"):
        raise ClockSyncError("设备时钟不可信")
    if _flag(status, "pend"):
        raise ClockSyncError("设备仍有待确认的校时事务")
    if _flag(status, "anom"):
        raise ClockSyncError("设备时钟处于异常状态")
    if _flag(status, "save"):
        raise ClockSyncError("设备仍有待重试的存档")
    _number(status, "off", -720, 840)
    utc = _number(status, "utc", 946684800, 4102444799)
    _number(status, "day", 20000101, 20991231)
    skew = utc - now_epoch
    if abs(skew) > CLOCK_MATCH_SECONDS:
        raise ClockSyncError(
            f"设备 UTC {utc}（{_utc_text(utc)}）与主机当前 UTC {now_epoch}"
            f"（{_utc_text(now_epoch)}）相差 {abs(skew)} 秒，"
            f"超过 {CLOCK_MATCH_SECONDS} 秒容差")


def validate_success(status: dict, *, transaction_id: int, target_utc: int,
                     offset: int, now_epoch: int) -> None:
    if status.get("_kind") != "s" or status.get("result") != "ok":
        raise ClockSyncError("设备未报告校时成功")
    if status.get("id") != str(transaction_id):
        raise ClockSyncError("回执事务号与请求不一致")
    if _number(status, "tx", 0, 0xFFFFFFFF) != transaction_id:
        raise ClockSyncError("设备事务号与请求不一致，无法证明本次校时")
    _require_official(status)
    if not _flag(status, "rtc"):
        raise ClockSyncError("校时后 RTC 回读无效")
    if not _flag(status, "cal") or not _flag(status, "tr"):
        raise ClockSyncError("校时后设备未进入可信状态")
    if _flag(status, "pend") or _flag(status, "anom") or _flag(status, "save"):
        raise ClockSyncError("校时后仍有待提交/异常/待重试状态")
    if _number(status, "off", -720, 840) != offset:
        raise ClockSyncError("设备时区与请求不一致")
    utc = _number(status, "utc", 946684800, 4102444799)
    if not target_utc <= utc <= target_utc + READBACK_WINDOW_SECONDS:
        raise ClockSyncError(f"RTC 回读不在目标窗口: target={target_utc} rtc={utc}")
    if abs(utc - now_epoch) > CLOCK_MATCH_SECONDS:
        raise ClockSyncError("RTC 回读已过期，不能证明本次校时")


def _open_serial(port: str):
    import serial  # pyserial is installed with the pinned esptool environment.

    return serial.Serial(port, 115200, timeout=0.05, write_timeout=1.0,
                         dsrdtr=False, rtscts=False)


class _Channel:
    def __init__(self, io, sleep, monotonic):
        self.io = io
        self.sleep = sleep
        self.monotonic = monotonic
        self.buffer = bytearray()

    def discard_input(self) -> None:
        """Drop local and OS-level backlog before sending a fresh query."""
        self.buffer.clear()
        reset = getattr(self.io, "reset_input_buffer", None)
        if reset is None:
            return
        try:
            reset()
        except Exception as exc:  # serial.SerialException and OSError
            raise TransportError(f"串口积压清理失败: {exc}") from exc

    def send(self, command: str) -> None:
        payload = (command + "\n").encode("ascii")
        try:
            written = self.io.write(payload)
        except Exception as exc:  # serial.SerialException and OSError
            raise TransportError(f"串口写入失败: {exc}") from exc
        if written != len(payload):
            raise TransportError(
                f"串口写入不完整: {written}/{len(payload)} 字节，命令未完整发出")

    def read_reply(self, deadline: float, kind: str, transaction_id=None) -> dict:
        while self.monotonic() < deadline:
            newline = self.buffer.find(b"\n")
            if newline >= 0:
                raw = bytes(self.buffer[:newline])
                del self.buffer[:newline + 1]
                if len(raw) > MAX_LINE:
                    continue
                text = raw.decode("ascii", "replace")
                if not text.startswith("PTC "):
                    continue
                try:
                    reply = parse_reply(text)
                except ValueError as exc:
                    raise ClockSyncError(str(exc)) from exc
                if reply is None:
                    continue
                if reply["_kind"] == "e":
                    raise ClockSyncError(f"设备拒绝请求: {reply.get('result', 'unknown')}")
                if reply["_kind"] != kind:
                    continue
                if kind == "s" and transaction_id is not None and reply.get("id") != str(transaction_id):
                    continue
                return reply
            try:
                waiting = int(getattr(self.io, "in_waiting", 0) or 0)
                chunk = self.io.read(max(1, waiting))
            except Exception as exc:
                raise TransportError(f"串口读取失败: {exc}") from exc
            if chunk:
                self.buffer.extend(chunk)
                if len(self.buffer) > MAX_LINE and b"\n" not in self.buffer:
                    # A single overlong line cannot be a protocol reply.
                    self.buffer.clear()
            else:
                self.sleep(0.01)
        raise TransportError("等待设备回执超时")


def _confirm_from_query(status: dict, *, transaction_id: int, target_utc: int,
                        offset: int, now_epoch: int) -> dict:
    """Confirm a lost set acknowledgement from a fresh, bound status query.

    The returned dict keeps the device's original ``_kind`` and ``_raw``; only
    ``_confirmed_by`` records that the set reply itself was not received.
    """
    validate_query(status, now_epoch=now_epoch)
    if _number(status, "tx", 0, 0xFFFFFFFF) != transaction_id:
        raise ClockSyncError("状态回执事务号与本次请求不一致，不能证明本次校时")
    if _number(status, "off", -720, 840) != offset:
        raise ClockSyncError("状态回执时区与本次请求不一致")
    utc = _number(status, "utc", 946684800, 4102444799)
    if not target_utc <= utc <= target_utc + READBACK_WINDOW_SECONDS:
        raise ClockSyncError(f"状态回读不在本次目标窗口: target={target_utc} rtc={utc}")
    reply = dict(status)
    reply["_confirmed_by"] = "query"
    return reply


def sync(open_port, *, port: str, query_only: bool = False,
         timeout: float = DEFAULT_TIMEOUT, now=time.time,
         host_offset=host_offset_minutes, sleep=time.sleep,
         monotonic=time.monotonic) -> dict:
    """Run one bounded host calibration session. Raises ClockSyncError."""
    deadline = monotonic() + timeout
    io = None
    channel = None
    pending = None  # (transaction_id, target_utc, offset)
    attempts = 0
    last_error = None

    def connect():
        nonlocal io, channel
        if io is None:
            try:
                io = open_port(port)
            except Exception as exc:
                raise TransportError(f"无法打开串口 {port}: {exc}") from exc
            channel = _Channel(io, sleep, monotonic)
        return channel

    def close():
        nonlocal io, channel
        if io is not None:
            try:
                io.close()
            except Exception:
                pass
        io = None
        channel = None

    def query(wait_until: float) -> dict:
        ch = connect()
        # A recovery query must not consume a reply left over from an earlier
        # transaction; drop local and OS-level backlog before asking again.
        ch.discard_input()
        ch.send("PTC?")
        return ch.read_reply(wait_until, "q")

    try:
        while monotonic() < deadline and attempts < 4:
            try:
                ch = connect()
                status = query(min(deadline, monotonic() + 2.0))
                if query_only:
                    try:
                        validate_query(status, now_epoch=int(now()))
                        return status
                    except ClockSyncError as exc:
                        last_error = exc
                        attempts += 1
                        if attempts >= 4 or monotonic() >= deadline:
                            raise
                        close()
                        sleep(0.2)
                        continue
                _require_official(status)
                if pending is not None:
                    transaction_id, target, offset = pending
                    try:
                        return _confirm_from_query(
                            status, transaction_id=transaction_id,
                            target_utc=target, offset=offset,
                            now_epoch=int(now()))
                    except ClockSyncError:
                        pending = None
                if _flag(status, "save"):
                    raise ClockSyncError("设备仍有待重试的存档，暂不能校时")
                offset = resolve_offset(status, host_offset())
                target = int(now())
                last_tx = _number(status, "tx", 0, 0xFFFFFFFF) if "tx" in status else 0
                transaction_id = choose_transaction_id(last_tx, target)
                pending = (transaction_id, target, offset)
                command = f"PTC {transaction_id} {target} {offset}"
                ch.send(command)
                # Keep a small slice of the overall budget for a readback after a
                # lost acknowledgement, instead of consuming it all on the reply.
                reserve = min(1.0, max(0.2, timeout * 0.2))
                set_deadline = min(deadline - reserve, monotonic() + 10.0)
                if set_deadline <= monotonic():
                    set_deadline = deadline
                reply = ch.read_reply(set_deadline, "s", transaction_id)
                if reply.get("result") == "save":
                    for _ in range(2):
                        if monotonic() >= deadline:
                            break
                        sleep(0.2)
                        ch.send(command)
                        reply = ch.read_reply(min(deadline, monotonic() + 10.0), "s", transaction_id)
                        if reply.get("result") != "save":
                            break
                if reply.get("result") == "ok":
                    validate_success(reply, transaction_id=transaction_id, target_utc=target,
                                     offset=offset, now_epoch=int(now()))
                    return reply
                code = reply.get("result", "unknown")
                pending = None
                if code not in ("rtc", "verify", "stale"):
                    raise ClockSyncError(f"设备拒绝校时: {code}")
                attempts += 1
                close()
                if attempts >= 3:
                    raise ClockSyncError(f"校时重试仍失败: {code}")
            except TransportError as exc:
                last_error = exc
                close()
                if pending is not None:
                    transaction_id, target, offset = pending
                    try:
                        status = query(min(deadline, monotonic() + 2.0))
                        try:
                            return _confirm_from_query(
                                status, transaction_id=transaction_id,
                                target_utc=target, offset=offset,
                                now_epoch=int(now()))
                        except ClockSyncError:
                            pass
                        if _flag(status, "save"):
                            # The commit is pending; resending the same id retries the
                            # same candidate instead of starting a different result.
                            ch = connect()
                            ch.send(f"PTC {transaction_id} {target} {offset}")
                            reply = ch.read_reply(min(deadline, monotonic() + 10.0),
                                                  "s", transaction_id)
                            if reply.get("result") == "ok":
                                validate_success(reply, transaction_id=transaction_id,
                                                 target_utc=target, offset=offset,
                                                 now_epoch=int(now()))
                                return reply
                    except (TransportError, ClockSyncError) as recovery_error:
                        last_error = recovery_error
                        close()
                    pending = None
                    attempts += 1
                    if attempts >= 3:
                        raise ClockSyncError("CDC 重连后仍无法确认校时结果")
                if monotonic() >= deadline:
                    break
                sleep(0.1)
    finally:
        close()
    if query_only and last_error is not None:
        raise last_error
    raise ClockSyncError("校时超时，未收到可信的设备回执" +
                         (f"（最后一次串口错误：{last_error}）" if last_error else ""))


def _select_port(ports) -> str:
    filtered = [port for port in ports
                if "Bluetooth" not in port and
                ("usbmodem" in port.lower() or "modem" not in port.lower())]
    if len(filtered) > 1:
        raise ClockSyncError(
            "检测到多个 CDC 串口：" + "、".join(filtered) +
            "；请用 --port 指定目标设备")
    return filtered[0] if filtered else ""


def _detect_port() -> str:
    try:
        import detect_port
    except ImportError:
        return ""
    return _select_port(detect_port.candidates())


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description="校准 PokeTrail 设备 RTC")
    parser.add_argument("--port", help="CDC 串口路径；省略时自动探测")
    parser.add_argument("--timeout", type=float, default=DEFAULT_TIMEOUT,
                        help=f"等待设备就绪和校时的总超时（默认 {DEFAULT_TIMEOUT:.0f} 秒）")
    parser.add_argument("--query", action="store_true",
                        help="只读查询，不校时；用于重启后核对时间")
    args = parser.parse_args(argv)
    try:
        port = args.port or _detect_port()
    except ClockSyncError as exc:
        print(f"无法确定 CDC 串口: {exc}", file=sys.stderr)
        return 2
    if not port:
        print("未找到 CDC 串口", file=sys.stderr)
        return 2
    try:
        import serial  # noqa: F401
    except ImportError:
        print("缺少 pyserial；请通过 mise 管理的 Python 环境运行", file=sys.stderr)
        return 2
    try:
        reply = sync(_open_serial, port=port, query_only=args.query, timeout=args.timeout)
    except (ClockSyncError, ValueError) as exc:
        prefix = "时钟查询失败" if args.query else "校时失败"
        print(f"{prefix}: {exc}", file=sys.stderr)
        return 1
    print(reply["_raw"])
    if args.query:
        print("设备时钟状态正常", file=sys.stderr)
    else:
        local = datetime.now().astimezone()
        confirmed = "（状态回读确认）" if reply.get("_confirmed_by") == "query" else ""
        print(f"校时完成{confirmed}：设备 UTC {reply.get('utc')}，"
              f"本地 {local.isoformat(timespec='seconds')}", file=sys.stderr)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
