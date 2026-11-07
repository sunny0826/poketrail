#pragma once

// Included after the clock-sync store and RTC setup helpers.
namespace {
class LostClockPlatform : public TestPlatform {
 public:
  device_clock::Reading readClock() override {
    auto reading = TestPlatform::readClock();
    reading.verificationFailed = !rtcValid;
    return reading;
  }
};

void lostClockDraft() {
  // A previously calibrated device must not present the new-device January 1
  // draft as its recovered clock, nor silently trust or write the saved anchor.
  ScheduledStore store; LostClockPlatform p; setRtc(p, kNowUtc);
  {
    Runtime r(store, p); start(r); skipDateAndStarter(r, p);
    check(resultIs(r.syncClockFromHost(600, kNowUtc, 480), "ok"), "seed a trusted saved clock");
  }
  const unsigned writes = p.clockWrites;
  p.rtcValid = false;
  Runtime reboot(store, p); start(reboot);
  openMenu(reboot, p); select(reboot, p, MenuItem::Settings); select(reboot, p, 0);
  const auto& screen = reboot.screen();
  check(screen.page == Page::Clock && screen.clockNeedsSync, "invalid RTC draft is explicitly uncalibrated");
  check(screen.civil.year == 2026 && screen.civil.month == 10 && screen.civil.day == 22 &&
        screen.civil.hour == 20, "invalid RTC draft uses the persisted trusted local date");
  const auto status = reboot.clockSyncStatus();
  check(!status.rtcValid && !status.trusted && status.rtcVerificationFailed && !status.rtcIoError,
        "query distinguishes RTC validity failure from I2C failure");
  check(p.clockWrites == writes && !reboot.engine().view().clock.pending.active,
        "opening the fallback draft never writes or starts a clock transaction");
  p.rtcValid = true;
  select(reboot, p, 8); select(reboot, p, 0);
  check(!reboot.screen().clockNeedsSync, "fresh valid RTC replaces the fallback draft warning");
}
}
