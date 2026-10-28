// Runtime 主机校时：复用正式时钟事务，覆盖初校、跨日、回拨、失败重试与测试模式隔离。
#include <cstring>
#include <vector>

#include "support/adventure_ui/harness.hpp"

using namespace pokedex;
using namespace pokedex::adventure;
using namespace pokedex::ui;
using namespace pokedex::clock_sync;

namespace {
const int64_t kOldUtc = 1767259321;      // 2026-01-01T09:22:01Z，设备原错误日期
const int64_t kNowUtc = 1792670400;      // 2026-10-22T12:00:00Z

class ScheduledStore : public Store {
 public:
  std::vector<uint8_t> bytes;
  LoadStatus status = LoadStatus::Empty;
  unsigned commits = 0;
  unsigned failAt = 0;  // 1-based commit index; 0 disables.
  bool writeThenFail = true;
  LoadResult load(Save& s) override {
    if (status != LoadStatus::Loaded) return LoadResult(status);
    return LoadResult(codec::decode(bytes.data(), bytes.size(), s) == codec::DecodeStatus::Ok
                          ? LoadStatus::Loaded : LoadStatus::Corrupt);
  }
  CommitStatus commit(const Save& s) override {
    ++commits;
    const bool fail = failAt && commits == failAt;
    if (!fail || writeThenFail) {
      std::vector<uint8_t> encoded(codec::maxEncodedSize);
      size_t length = 0;
      check(codec::encode(s, encoded.data(), encoded.size(), length), "fixture save encodes");
      encoded.resize(length); bytes = encoded; status = LoadStatus::Loaded;
    }
    return fail ? CommitStatus::IoError : CommitStatus::Committed;
  }
};

class FakeTestHost : public TestModeHost {
 public:
  bool active = false;
  bool testMode() const override { return active; }
  bool ramOnly() const override { return false; }
  bool enterTest(TestSeed) override { return false; }
  bool leaveTest() override { return false; }
  TestMode& mode() override { return mode_; }
  void setFault(TestFault) override {}
  TestFault fault() const override { return TestFault::None; }
  TestDiagnostics diagnostics() const override { return {}; }
 private:
  TestMode mode_;
};

class FailWritePlatform : public TestPlatform {
 public:
  bool failWrites = true;
  device_clock::Reading writeClock(int64_t value) override {
    if (failWrites) { ++clockWrites; return {}; }
    return TestPlatform::writeClock(value);
  }
};

class LandedReadbackPlatform : public TestPlatform {
 public:
  device_clock::Reading writeClock(int64_t value) override {
    auto reading = TestPlatform::writeClock(value);
    if (failOnce) { failOnce = false; reading.valid = false; }
    return reading;
  }
  bool failOnce = true;
};

class SkewWritePlatform : public TestPlatform {
 public:
  device_clock::Reading writeClock(int64_t value) override {
    return TestPlatform::writeClock(value + 100);
  }
};

Save fixture(int64_t utc, int16_t offset, uint32_t dayKey, uint64_t daySteps) {
  Save s;
  s.schema = kSchemaVersion;
  s.bootGeneration = 5;
  s.starter = s.partner = 1;
  s.rosterRevision = 1;
  setRegistered(s, 1); setOwned(s, 1);
  s.nextEncounterId = 1; s.encounterTarget = 300;
  s.totalSteps = 8000;
  s.undatedSteps = s.totalSteps - daySteps;
  s.bestDaySteps = daySteps;
  s.explorationCredit = s.totalSteps;
  s.today.dayKey = dayKey; s.today.steps = daySteps;
  s.clock.calibrated = true; s.clock.trusted = false;
  s.clock.offsetMinutes = offset; s.clock.dayHighWater = dayKey;
  (void)utc;
  return s;
}

void setRtc(TestPlatform& p, int64_t utc) { p.utc = utc; p.rtcSetAt = host_time_ms; }

bool resultIs(const char* result, const char* expected) { return std::strcmp(result, expected) == 0; }

void initialSyncAndQuery() {
  ScheduledStore store; TestPlatform p; setRtc(p, kOldUtc);
  Runtime r(store, p); start(r);
  const auto before = r.clockSyncStatus();
  check(before.ready && !before.calibrated && !before.testMode, "uncalibrated device reports ready and official");
  const unsigned commits = store.commits;
  (void)r.clockSyncStatus(); (void)r.clockSyncStatus();
  check(store.commits == commits, "read-only query never commits a save");

  check(resultIs(r.syncClockFromHost(10, kNowUtc, 480), "ok"), "first host sync calibrates the device");
  const auto& s = r.engine().view();
  check(p.clockWrites == 1 && s.clock.calibrated && s.clock.trusted && !s.clock.pending.active &&
        s.clock.offsetMinutes == 480, "first sync writes once and persists a trusted offset");
  check(s.today.dayKey == 20261022 && s.today.steps == 0, "first sync starts the target day without steps");
  const auto after = r.clockSyncStatus();
  check(after.rtcValid && after.trusted && !after.pending && after.dayKey == 20261022 &&
        after.utcSeconds >= kNowUtc && after.utcSeconds - kNowUtc <= 5 &&
        after.transactionId == 10, "readback proves target time, trust and cleared pending");

  check(resultIs(r.syncClockFromHost(10, kNowUtc, 480), "ok"), "lost acknowledgement retry is idempotent");
  check(p.clockWrites == 1, "idempotent retry does not rewrite the RTC");
  idle(r, 10000);
  check(resultIs(r.syncClockFromHost(10, kNowUtc, 480), "stale"), "stale duplicate cannot claim a current sync");
  check(resultIs(r.syncClockFromHost(10, kNowUtc + 60, 480), "id"), "same id cannot change its target");
}

void firstSyncUsesHostOffset() {
  ScheduledStore store; TestPlatform p; setRtc(p, kOldUtc);
  Runtime r(store, p); start(r);
  check(resultIs(r.syncClockFromHost(15, kNowUtc, -300), "ok"),
        "uncalibrated device accepts the host timezone offset");
  const auto& s = r.engine().view();
  check(s.clock.offsetMinutes == -300 && s.today.dayKey == 20261022 &&
        r.clockSyncStatus().offsetMinutes == -300,
        "first calibration persists the host offset and derives the local day");
}

void wrongDateUpgrade() {
  ScheduledStore store; store.commit(fixture(kOldUtc, 480, 20260101, 500));
  TestPlatform p; setRtc(p, kOldUtc);
  Runtime r(store, p); start(r);
  check(r.engine().view().today.dayKey == 20260101 && r.engine().view().today.steps == 500 &&
        r.engine().view().clock.trusted, "device boots with the known wrong but trusted date");
  check(resultIs(r.syncClockFromHost(20, kNowUtc, 480), "ok"), "host sync repairs the trusted wrong date");
  const auto& s = r.engine().view();
  check(s.today.dayKey == 20261022 && s.today.steps == 0, "old date steps are not backfilled into today");
  check(s.historySize == 1 && s.history[0].dayKey == 20260101 && s.history[0].steps == 500,
        "old daily bucket is archived under its original date");
  check(s.totalSteps == 8000 && s.undatedSteps == 7500 && s.clock.dayHighWater == 20261022,
        "accounting totals and day high-water survive the repair");
}

void sameDayAndBackward() {
  ScheduledStore store; store.commit(fixture(kNowUtc, 480, 20261022, 500));
  TestPlatform p; setRtc(p, kNowUtc);
  Runtime r(store, p); start(r);
  check(resultIs(r.syncClockFromHost(30, kNowUtc + 3600, 480), "ok"), "same-day host correction applies");
  check(r.engine().view().today.dayKey == 20261022 && r.engine().view().today.steps == 500,
        "same-day correction preserves today's steps");
  check(resultIs(r.syncClockFromHost(40, kNowUtc - 2 * 86400, 480), "ok"), "backward host correction applies");
  const auto& s = r.engine().view();
  check(s.today.dayKey == 20261020 && s.today.steps == 0 && s.historySize == 1 &&
        s.history[0].dayKey == 20261022 && s.history[0].steps == 500,
        "backward correction archives instead of merging days");
  check(!s.clock.anomaly && s.clock.trusted, "backward correction ends trusted, not anomalous");
  const unsigned writes = p.clockWrites;
  check(resultIs(r.syncClockFromHost(50, kNowUtc, 0), "offset"),
        "calibrated device rejects a different host timezone");
  check(p.clockWrites == writes && r.engine().view().clock.offsetMinutes == 480,
        "offset rejection does not touch the RTC");
}

void testModeAndBusy() {
  ScheduledStore store; TestPlatform p; FakeTestHost host; host.active = true;
  Runtime r(store, p, &host); start(r);
  const unsigned commits = store.commits;
  const auto status = r.clockSyncStatus();
  check(status.testMode && status.ready, "test mode is visible to the host query");
  check(resultIs(r.syncClockFromHost(60, kNowUtc, 480), "test"), "test mode refuses real host calibration");
  check(p.clockWrites == 0 && store.commits == commits && !r.engine().view().clock.calibrated,
        "test-mode refusal writes neither RTC nor official save");

  ScheduledStore busyStore; busyStore.commit(fixture(kNowUtc, 480, 20261022, 500));
  TestPlatform busyPlatform; setRtc(busyPlatform, kNowUtc);
  Runtime busyRuntime(busyStore, busyPlatform); start(busyRuntime);
  openMenu(busyRuntime, busyPlatform); select(busyRuntime, busyPlatform, 5);
  check(busyRuntime.screen().page == Page::Practice, "practice battle is reachable");
  const unsigned writes = busyPlatform.clockWrites;
  check(resultIs(busyRuntime.syncClockFromHost(70, kNowUtc + 3600, 480), "busy"),
        "active battle safely refuses host calibration");
  check(busyPlatform.clockWrites == writes, "busy refusal does not write the RTC");
  for (unsigned i = 0; i < 15 && busyRuntime.practiceView().active; ++i) {
    const auto battle = busyRuntime.practiceView();
    while (int32_t(battle.targetMs - host_time_ms) > 20) frame(busyRuntime);
    busyPlatform.next.left = battle.expected == Action::Dodge;
    busyPlatform.next.right = battle.expected == Action::Attack;
    busyPlatform.next.held = true;
    frame(busyRuntime, battle.targetMs - host_time_ms);
    frame(busyRuntime);
    if (busyRuntime.practiceView().active) idle(busyRuntime, 200);
  }
  for (unsigned i = 0; i < 100 && busyRuntime.screen().page != Page::PracticeThrow; ++i)
    frame(busyRuntime);
  check(!busyRuntime.practiceView().active && busyRuntime.screen().page == Page::PracticeThrow,
        "practice battle reaches explicit throw preparation");
  frame(busyRuntime);
  busyPlatform.next.right = true; busyPlatform.next.held = true; frame(busyRuntime);
  check(busyRuntime.screen().practiceThrowing, "active practice throw is observable");
  check(resultIs(busyRuntime.syncClockFromHost(71, kNowUtc + 3600, 480), "busy"),
        "active throw safely refuses host calibration");
  check(busyPlatform.clockWrites == writes, "throw refusal does not write the RTC");
}

void saveFailureRetries() {
  {
    ScheduledStore store; TestPlatform p; setRtc(p, kOldUtc);
    Runtime r(store, p); start(r);
    store.failAt = store.commits + 1;
    check(resultIs(r.syncClockFromHost(80, kNowUtc, 480), "save"), "failed pending commit is reported as save");
    check(p.clockWrites == 0 && r.engine().savePending(), "failed pending commit blocks the RTC write");
    check(resultIs(r.syncClockFromHost(81, kNowUtc, 480), "save"), "another id cannot bypass the pending commit");
    store.failAt = 0;
    check(resultIs(r.syncClockFromHost(80, kNowUtc, 480), "ok"), "same-id retry resumes the same pending transaction");
    check(p.clockWrites == 1 && !r.engine().savePending() && r.engine().view().clock.trusted,
          "resumed transaction writes once and clears pending");
  }
  {
    ScheduledStore store; TestPlatform p; setRtc(p, kOldUtc);
    Runtime r(store, p); start(r);
    store.failAt = store.commits + 2;
    check(resultIs(r.syncClockFromHost(90, kNowUtc, 480), "save"), "failed final commit is reported as save");
    check(p.clockWrites == 1 && r.engine().savePending(), "final commit failure leaves the verified RTC and pending save");
    store.failAt = 0;
    check(resultIs(r.syncClockFromHost(90, kNowUtc, 480), "ok"), "final commit retry commits the same candidate");
    check(p.clockWrites == 1 && !r.engine().savePending() && r.engine().view().clock.trusted,
          "final commit retry does not rewrite the RTC");
  }
}

void delayedSaveAndUiRetries() {
  {
    ScheduledStore store; store.commit(fixture(kNowUtc, 480, 20261022, 500));
    TestPlatform p; setRtc(p, kNowUtc); Runtime r(store, p); start(r);
    store.failAt = store.commits + 1;
    check(resultIs(r.syncClockFromHost(301, kNowUtc, 480), "save"), "inject delayed begin save");
    frame(r, 20000); store.failAt = 0;
    check(resultIs(r.syncClockFromHost(301, kNowUtc, 480), "stale"), "expired request is rejected before RTC write");
    check(p.clockWrites == 0 && r.clockSyncStatus().utcSeconds >= kNowUtc + 20,
          "delayed begin retry cannot turn the RTC backwards");
    check(resultIs(r.syncClockFromHost(302, kNowUtc + 20, 480), "ok"), "fresh transaction recovers expired request");
  }
  {
    ScheduledStore store; store.commit(fixture(kOldUtc, 480, 20260101, 500));
    TestPlatform p; setRtc(p, kOldUtc); Runtime r(store, p); start(r);
    store.failAt = store.commits + 2;
    check(resultIs(r.syncClockFromHost(303, kNowUtc, 480), "save"), "inject final commit failure for UI retry");
    frame(r); frame(r);
    check(r.screen().page == Page::Error, "failed host save reaches actual error page");
    store.failAt = 0; select(r, p, 0); frame(r, 1500);
    auto status = r.clockSyncStatus();
    check(!status.savePending && !status.pending && status.rtcValid && status.trusted,
          "device retry restores trusted clock without a connected host");
    frame(r, 1500);
    check(r.clockSyncStatus().trusted && p.clockWrites == 1,
          "device retry keeps the RTC ticking and never rewrites it");
    check(r.screen().page == Page::Home, "device retry returns to home");
  }
  {
    ScheduledStore store; TestPlatform p; setRtc(p, kOldUtc); Runtime r(store, p); start(r);
    store.failAt = store.commits + 1;
    check(resultIs(r.syncClockFromHost(304, kNowUtc, 480), "save"), "inject begin failure for UI retry");
    frame(r, 6000); frame(r); store.failAt = 0; select(r, p, 0);
    check(p.clockWrites == 0, "late device retry cannot write expired host time");
    check(r.screen().page == Page::Clock, "expired device retry offers manual recovery");
  }
}

void manualAfterHostFailure() {
  ScheduledStore store; store.commit(fixture(kOldUtc, 480, 20260101, 500));
  FailWritePlatform p; setRtc(p, kOldUtc); Runtime r(store, p); start(r);
  check(resultIs(r.syncClockFromHost(400, kNowUtc, 480), "rtc"), "inject host RTC failure before manual recovery");
  frame(r, 6000);
  openMenu(r, p); select(r, p, 4); select(r, p, 1);
  check(r.screen().page == Page::Clock, "manual recovery uses the real clock page");
  p.failWrites = false;
  select(r, p, 7); frame(r, 1500);
  const auto status = r.clockSyncStatus();
  check(status.trusted && !status.pending && !status.savePending && p.clockWrites == 2,
        "manual clock save supersedes failed host transaction and restores trust");
  check(status.transactionId == 401 && r.screen().page == Page::Home,
        "manual recovery completes its own new transaction");
}

void simultaneousHostAndManualSave() {
  ScheduledStore store; store.commit(fixture(kOldUtc, 480, 20260101, 500));
  TestPlatform p; setRtc(p, kOldUtc); Runtime r(store, p); start(r);
  openMenu(r, p); select(r, p, 4); select(r, p, 1);
  for (unsigned n = 0; n < 20 && r.screen().selected != 7; ++n) edge(r, p, false, true);
  check(r.screen().page == Page::Clock && r.screen().selected == 7, "manual save selected before incoming host request");
  frame(r);
  store.failAt = store.commits + 1;
  p.next.left = p.next.right = p.next.held = true;
  check(resultIs(r.syncClockFromHost(500, kNowUtc, 480), "save"), "host save fails before same-frame manual input");
  frame(r); frame(r);
  check(r.screen().page == Page::Error, "simultaneous save failure reaches retry page");
  store.failAt = 0; select(r, p, 0); frame(r, 1500);
  const auto status = r.clockSyncStatus();
  check(status.trusted && !status.pending && !status.savePending && p.clockWrites == 1,
        "UI retry finishes retained host transaction despite simultaneous manual save");
  check(resultIs(r.syncClockFromHost(501, kNowUtc + 2, 480), "ok"),
        "new host transaction remains usable after simultaneous input recovery");
}

void rtcFailures() {
  {
    ScheduledStore store; FailWritePlatform p; setRtc(p, kOldUtc);
    Runtime r(store, p); start(r);
    check(resultIs(r.syncClockFromHost(100, kNowUtc, 480), "rtc"), "RTC write failure is not reported as success");
    check(p.clockWrites == 1 && r.engine().view().clock.pending.active && !r.engine().view().clock.trusted,
          "RTC failure leaves an explicit pending transaction");
    check(resultIs(r.syncClockFromHost(101, kNowUtc, 480), "rtc"),
          "a fresh id may supersede a failed host RTC write");
    check(p.clockWrites == 2, "superseding transaction attempts its own write");
    check(resultIs(r.syncClockFromHost(101, kNowUtc, 480), "stale"),
          "same-id retry cannot rewrite a stale RTC target");
    p.failWrites = false;
    check(resultIs(r.syncClockFromHost(102, kNowUtc, 480), "ok"),
          "fresh id with a new request completes after the failed writes");
    check(p.clockWrites == 3 && !r.engine().view().clock.pending.active, "new transaction completes once");
  }
  {
    ScheduledStore store; LandedReadbackPlatform p; setRtc(p, kOldUtc);
    Runtime r(store, p); start(r);
    check(resultIs(r.syncClockFromHost(110, kNowUtc, 480), "rtc"), "lost readback is reported as uncertain");
    check(resultIs(r.syncClockFromHost(110, kNowUtc, 480), "ok"),
          "same-id retry confirms a write that already landed");
    check(p.clockWrites == 1, "landed write is confirmed without a second RTC write");
  }
  {
    ScheduledStore store; SkewWritePlatform p; setRtc(p, kOldUtc);
    Runtime r(store, p); start(r);
    check(resultIs(r.syncClockFromHost(120, kNowUtc, 480), "rtc"),
          "readback outside the transaction window is rejected");
    check(!r.engine().view().clock.trusted && r.engine().view().clock.pending.active,
          "skewed readback never becomes a successful calibration");
  }
}
}

int main() {
  initialSyncAndQuery();
  firstSyncUsesHostOffset();
  wrongDateUpgrade();
  sameDayAndBackward();
  testModeAndBusy();
  saveFailureRetries();
  delayedSaveAndUiRetries();
  manualAfterHostFailure();
  simultaneousHostAndManualSave();
  rtcFailures();
  std::printf("clock sync runtime: %u checks\n", checks);
  return 0;
}
