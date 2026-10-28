#include <Arduino.h>
#include <M5Unified.h>
#include <esp_system.h>
#include <cstdlib>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <memory>
#include "board.hpp"
#include "runtime.hpp"
#include "clock_sync.hpp"
#include "button_input.hpp"
#include "device_store.hpp"
#include "device_clock_hw.hpp"
#include "test_mode_platform.hpp"
#include "test_console.hpp"

namespace {
class DevicePlatform : public pokedex::ui::Platform {
 public:
  void beginClock() { clock_.begin(M5.In_I2C); }
  uint32_t now() override { return millis(); }
  uint32_t randomSeed() override { return esp_random(); }
  pokedex::ui::Sample sample() override {
    pokedex::ui::Sample s;
    const auto mask = M5.Imu.update();
    s.at = millis();
    s.fresh = (mask & m5::IMU_Class::sensor_mask_accel) != 0;
    // The installed update API does not distinguish I/O failure from no new data.
    if (s.fresh) {
      const auto& data = M5.Imu.getImuData();
      s.x = data.accel.x; s.y = data.accel.y; s.z = data.accel.z;
    }
    return s;
  }
  pokedex::ui::Input pollInput() override {
    c152::update();
    pokedex::ui::Input input;
    input.at = millis();
    // M5Unified maps only PM1 IRQ bits 0/2 to BtnPWR, but this board reports the
    // power key as bit 1 (wakeup), so installed versions never produce a click.
    // Read the button status register instead: bit 0 is the live level and bit 7
    // is a read-to-clear pressed flag that also catches presses shorter than one poll.
    uint8_t button = 0;
    if (M5.Power.M5pm1.readRegister(0x48, &button, 1)) {
      const bool down = (button & 0x01) != 0;
      input.power = (down && !powerWasDown_) || ((button & 0x80) != 0 && !down);
      powerWasDown_ = down;
    }
    buttons_.sample(input, M5.BtnA, M5.BtnB);
    input.touch = c152::touchWasPressed();
    const auto detail = M5.Touch.getDetail();
    input.x = detail.x; input.y = detail.y;
    const int dx = detail.x - detail.base_x, dy = detail.y - detail.base_y;
    input.click = detail.wasReleased() && detail.wasClicked() && std::abs(dx) <= 12 && std::abs(dy) <= 12;
    input.back = input.click && detail.base_y < 80 && detail.y < 80;
    if (detail.wasReleased() && std::abs(dx) >= 45 && std::abs(dx) > std::abs(dy))
      input.swipe = dx < 0 ? 1 : -1;
    input.held = input.held || detail.isPressed();
    lastInput_ = input;
    return input;
  }
  const pokedex::ui::Input& lastInput() const { return lastInput_; }
  pokedex::device_clock::Reading readClock() override { return clock_.read(); }
  pokedex::device_clock::Reading writeClock(int64_t utc) override { return clock_.setAndVerify(utc); }
  void feedback(uint16_t duration) override { c152::feedback(0, duration, 70); }
  bool vibrationActive() override { return c152::vibrationActive(); }
  bool usbHostPresent() override { return Serial.isPlugged(); }
  void brightness(uint8_t value) override { c152::setDisplayBrightness(value); }
  c152::DisplayTransition displaySleep(bool sleeping) override { return c152::setDisplaySleep(sleeping); }
  c152::BatteryStatus readBattery() override { return c152::readBattery(); }
 private:
  pokedex::device_clock::Clock clock_;
  pokedex::ui::Input lastInput_;
  pokedex::ui::ButtonInput buttons_;
  bool powerWasDown_ = false;
};
pokedex::adventure::DeviceStore store;
DevicePlatform platform;
pokedex::ui::TestModePlatform testPlatform(platform, store);
std::unique_ptr<pokedex::ui::Runtime> runtime;
bool ready = false;

bool rebuildRuntime(bool persistBoot = true) {
  runtime.reset();
  // Always pass the delegating platform. It reads the final store identity
  // after any mount/marker retry, so a late transition to test mode cannot
  // bind the test save to real IMU sampling or hardware RTC writes.
  runtime.reset(new pokedex::ui::Runtime(
    store, static_cast<pokedex::ui::Platform&>(testPlatform), &store));
  ready = runtime->begin(persistBoot);
  return ready;
}

char clockTx[256];
size_t clockTxLength = 0, clockTxSent = 0;
pokedex::clock_sync::Parser clockParser;

bool clockReplyPending() { return clockTxSent < clockTxLength; }

void queueClockReply(const char* line, size_t length) {
  if (clockReplyPending() || !length || length + 2 > sizeof clockTx) return;
  // A leading newline terminates any partially written diagnostic line so the
  // host can always discard it as non-protocol output.
  clockTx[0] = '\n';
  std::memcpy(clockTx + 1, line, length);
  clockTx[1 + length] = '\n';
  clockTxLength = length + 2;
  clockTxSent = 0;
}

void pumpClockSerial() {
  // isPlugged() can be false before the host asserts the CDC control line;
  // still drain bytes so a late attach cannot leave a stale partial command.
  static bool wasPlugged = false;
  const bool plugged = Serial.isPlugged();
  if (!plugged && wasPlugged) clockParser.reset();
  wasPlugged = plugged;
  if (!plugged) clockTxLength = clockTxSent = 0;
  if (clockReplyPending()) return;
  // Bound the work per main-loop pass so serial traffic cannot starve sampling.
  for (unsigned budget = 0; budget < 64; ++budget) {
    const int value = Serial.read();
    if (value < 0) break;
    const auto request = clockParser.feed(uint8_t(value));
    if (request.kind == pokedex::clock_sync::CommandKind::None) continue;
    char line[224];
    size_t length = 0;
    if (request.kind == pokedex::clock_sync::CommandKind::Query && runtime) {
      length = pokedex::clock_sync::formatQuery(line, sizeof line, runtime->clockSyncStatus());
    } else if (request.kind == pokedex::clock_sync::CommandKind::Set && runtime) {
      const char* result = runtime->syncClockFromHost(
        request.id, request.utcSeconds, request.offsetMinutes);
      length = pokedex::clock_sync::formatSet(line, sizeof line, request.id, result,
                                              runtime->clockSyncStatus());
    } else {
      const char* code = request.kind == pokedex::clock_sync::CommandKind::Error
                           ? pokedex::clock_sync::errorName(request.error) : "unavailable";
      length = pokedex::clock_sync::formatError(line, sizeof line, code);
    }
    queueClockReply(line, length);
    if (clockReplyPending()) break;
  }
}

void pumpClockTransmit() {
  while (clockTxSent < clockTxLength) {
    const int room = Serial.availableForWrite();
    if (room <= 0) break;
    size_t chunk = clockTxLength - clockTxSent;
    if (chunk > size_t(room)) chunk = size_t(room);
    const size_t written = Serial.write(reinterpret_cast<const uint8_t*>(clockTx + clockTxSent), chunk);
    clockTxSent += written;
    if (written < chunk) break;
  }
}

void diagnostic(const char* format, ...) {
  if (clockReplyPending()) return;
  char line[256];
  va_list args; va_start(args, format);
  const int length = vsnprintf(line, sizeof(line), format, args);
  va_end(args);
  if (length > 0 && length < int(sizeof(line)) && Serial.availableForWrite() >= length)
    Serial.write(reinterpret_cast<const uint8_t*>(line), size_t(length));
}
}

namespace pokedex { namespace app {
// UI tasks call these entry points. The old Runtime is destroyed before the
// storage root changes so a rebuild never keeps two Screen buffers or Engines.
bool enterTestMode(adventure::TestSeed seed) {
  if (!runtime || store.testMode()) return false;
  if (runtime->engine().savePending()) {
    const adventure::CommandResult retried = runtime->engine().retrySave();
    if (retried != adventure::CommandResult::Applied &&
        retried != adventure::CommandResult::Duplicate) return false;
  }
  if (runtime->engine().ready()) {
    if (!runtime->canSwitchMode()) return false;
    const adventure::CommandResult saved = runtime->checkpoint();
    if (saved != adventure::CommandResult::Applied &&
        saved != adventure::CommandResult::Duplicate) return false;
  }
  runtime.reset();
  if (!store.enterTest(seed)) { rebuildRuntime(false); return false; }
  rebuildRuntime();
  return store.testMode();
}
bool leaveTestMode() {
  if (!runtime || !store.testMode()) return false;
  runtime.reset();
  if (!store.leaveTest()) { rebuildRuntime(); return false; }
  rebuildRuntime(false);
  return !store.testMode();
}
bool reseedTestMode(adventure::TestSeed seed) {
  if (!runtime || !store.testMode()) return false;
  runtime.reset();
  const bool accepted = store.enterTest(seed);
  const bool seeded = accepted && !store.mode().seedFailed;
  rebuildRuntime();
  return seeded;
}
bool seedTestScenario(adventure::TestScenario scenario) {
  if (!runtime || !store.testMode()) return false;
  runtime.reset();
  const bool seeded = adventure::TestConsole::seed(store, store, scenario);
  rebuildRuntime();
  return seeded;
}
void handleTestRequest(pokedex::ui::Runtime::TestRequest request) {
  using Request = pokedex::ui::Runtime::TestRequest;
  if (!runtime) return;
  switch (request) {
    case Request::EnterSnapshot:
    case Request::EnterBlank: {
      const bool entered = enterTestMode(request == Request::EnterSnapshot ?
        adventure::TestSeed::Snapshot : adventure::TestSeed::Blank);
      runtime->openTestPage(entered ? pokedex::ui::Page::TestHub : pokedex::ui::Page::TestEntry, !entered);
      break;
    }
    case Request::ResetSnapshot:
    case Request::ResetBlank: {
      const bool reset = reseedTestMode(request == Request::ResetSnapshot ?
        adventure::TestSeed::Snapshot : adventure::TestSeed::Blank);
      runtime->openTestPage(reset ? pokedex::ui::Page::TestHub : pokedex::ui::Page::TestEntry, !reset);
      break;
    }
    case Request::ScenarioAccount: {
      const bool seeded = seedTestScenario(adventure::TestScenario::AccountBalls);
      runtime->openTestPage(pokedex::ui::Page::TestHub, !seeded);
      break;
    }
    case Request::ScenarioNearEncounter: {
      const bool seeded = seedTestScenario(adventure::TestScenario::NearEncounter);
      runtime->openTestPage(pokedex::ui::Page::TestHub, !seeded);
      break;
    }
    case Request::ScenarioCurrentThrow: {
      const bool seeded = seedTestScenario(adventure::TestScenario::CurrentThrow);
      runtime->openTestPage(pokedex::ui::Page::TestHub, !seeded);
      break;
    }
    case Request::ScenarioLegacy: {
      const bool seeded = seedTestScenario(adventure::TestScenario::LegacyEncounter);
      runtime->openTestPage(pokedex::ui::Page::TestHub, !seeded);
      break;
    }
    case Request::Exit: {
      const bool left = leaveTestMode();
      if (!left && runtime && store.testMode())
        runtime->openTestPage(pokedex::ui::Page::TestExit, true);
      break;
    }
    case Request::None: break;
  }
}
}}
void setup() {
  c152::begin("ADVENTURE");
  platform.beginClock();
  store.begin();
  rebuildRuntime();
  if (!ready) {
    pokedex::ui::drawGraphicsError(runtime->screen().battery);
  }
}
void loop() {
  pumpClockSerial();
  const auto beforePage = runtime->screen().page;
  const auto beforeSelected = runtime->screen().selected;
  runtime->loop();
  pumpClockTransmit();
  const auto request = runtime->takeTestRequest();
  if (request != pokedex::ui::Runtime::TestRequest::None) {
    pokedex::app::handleTestRequest(request);
    pumpClockTransmit();
    return;
  }
  static bool leftDown = false, rightDown = false;
  const auto& input = platform.lastInput();
  if (input.any() || input.leftDown != leftDown || input.rightDown != rightDown) {
    diagnostic("ui t=%lu touch=%u xy=%d,%d keys=%u,%u down=%u,%u held=%u page=%u>%u row=%u>%u dim=%u ready=%u pending=%u\n",
      (unsigned long)input.at, unsigned(input.touch), int(input.x), int(input.y),
      unsigned(input.left), unsigned(input.right), unsigned(input.leftDown), unsigned(input.rightDown), unsigned(input.held),
      unsigned(beforePage), unsigned(runtime->screen().page), unsigned(beforeSelected),
      unsigned(runtime->screen().selected), unsigned(runtime->dimmed()),
      unsigned(runtime->engine().ready()), unsigned(runtime->engine().savePending()));
  }
  leftDown = input.leftDown; rightDown = input.rightDown;
  static uint32_t report = 0;
  if (millis() - report >= 10000) {
    report = millis(); const auto& m = runtime->metrics();
    diagnostic("sample=%lu maxgap=%lu input=%lu gaps=%lu noaccel=%lu imuio=not-exposed rtc=%lu render=%lu commit=%lu\n",
      (unsigned long)m.samples, (unsigned long)m.maxSampleGapMs, (unsigned long)m.maxInputGapMs,
      (unsigned long)m.qualityGaps, (unsigned long)m.noSample,
      (unsigned long)m.rtcErrors, (unsigned long)m.maxRenderMs, (unsigned long)m.maxCommitMs);
    const auto& save = runtime->engine().view();
    diagnostic("state page=%u row=%u ready=%u pending=%u load=%u starter=%u partner=%u phase=%u seq=%llu total=%llu target=%u\n",
      unsigned(runtime->screen().page), unsigned(runtime->screen().selected), unsigned(runtime->engine().ready()),
      unsigned(runtime->engine().savePending()), unsigned(runtime->engine().loadResult().status),
      unsigned(save.starter), unsigned(save.partner), unsigned(save.phase),
      (unsigned long long)save.sequence, (unsigned long long)save.totalSteps, unsigned(save.encounterTarget));
    const auto& battery = runtime->screen().battery;
    diagnostic("power dim=%u battery=%d charge=%u displayerr=%lu\n",
      unsigned(runtime->dimmed()), int(battery.percent), unsigned(battery.charge), (unsigned long)m.displayErrors);
  }
  const uint32_t wait = runtime->nextSampleMs(millis());
  if (!(runtime->pocketIdle() && wait > 0 && c152::restUntilInput(wait))) delay(1);
}
