#include <Arduino.h>
#include <M5Unified.h>
#include <esp_system.h>
#include <algorithm>
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
#include "large_object.hpp"

namespace {
class DevicePlatform : public pokedex::ui::Platform {
 public:
  void beginClock() {
    logClockRegisters("boot");
    clock_.begin(M5.In_I2C);
  }
  uint32_t now() override { return millis(); }
  uint32_t randomSeed() override { return esp_random(); }
  pokedex::ui::Sample sample() override {
    pokedex::ui::Sample s;
    const bool ok = c152::readAccel(s.x, s.y, s.z, s.fresh);
    s.at = millis();
    s.ioError = !ok;
    return s;
  }
  pokedex::ui::Input pollInput() override {
    pokedex::ui::Input input;
    if (displayAsleep_) {
      c152::updateVibration();
      input.at = millis();
      samplePowerKey(input, 20u);
      lastInput_ = input;
      return input;
    }
    c152::update();
    input.at = millis();
    samplePowerKey(input, 25u);
    buttons_.sample(input, M5.BtnA, M5.BtnB);
    input.touch = c152::touchWasPressed();
    const auto detail = M5.Touch.getDetail();
    input.x = detail.x; input.y = detail.y;
    const int dx = detail.x - detail.base_x, dy = detail.y - detail.base_y;
    input.click = detail.wasReleased() && detail.wasClicked() &&
                  std::abs(dx) <= pokedex::viewer::kTouchClickTolerancePx &&
                  std::abs(dy) <= pokedex::viewer::kTouchClickTolerancePx;
    input.back = input.click && detail.base_y < 80 && detail.y < 80;
    if (detail.wasReleased() && std::abs(dx) >= 45 && std::abs(dx) > std::abs(dy))
      input.swipe = dx < 0 ? 1 : -1;
    input.touchDown = detail.isPressed();
    input.held = input.held || input.touchDown;
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
  c152::DisplayTransition displaySleep(bool sleeping) override {
    const auto result = c152::setDisplaySleep(sleeping);
    if (result.accepted) {
      displayAsleep_ = sleeping;
      if (!sleeping) c152::resetTouchEdge();
    }
    return result;
  }
  c152::BatteryStatus readBattery() override { return c152::readBattery(); }
 private:
  void samplePowerKey(pokedex::ui::Input& input, uint32_t intervalMs) {
    if (hasPowerPoll_ && input.at - lastPowerPoll_ < intervalMs) return;
    lastPowerPoll_ = input.at;
    hasPowerPoll_ = true;
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
  }
  void logClockRegisters(const char* stage) {
    const uint8_t registers[] = {0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16,
                                 0x1d, 0x1e, 0x1f, 0x20, 0x31};
    Serial.printf("[rtc] %s", stage);
    for (uint8_t reg : registers) {
      uint8_t value = 0;
      if (M5.In_I2C.readRegister(0x32, reg, &value, 1, 100000))
        Serial.printf(" %02x=%02x", unsigned(reg), unsigned(value));
      else Serial.printf(" %02x=io", unsigned(reg));
    }
    Serial.println();
  }
  pokedex::device_clock::Clock clock_;
  pokedex::ui::Input lastInput_;
  pokedex::ui::ButtonInput buttons_;
  bool powerWasDown_ = false;
  bool displayAsleep_ = false;
  bool hasPowerPoll_ = false;
  uint32_t lastPowerPoll_ = 0;
};
pokedex::adventure::DeviceStore store;
DevicePlatform platform;
pokedex::ui::TestModePlatform testPlatform(platform, store);
pokedex::LargeObject<pokedex::ui::Runtime> runtime;
bool ready = false;

bool rebuildRuntime(bool persistBoot = true) {
  runtime.reset();
  runtime = pokedex::makeLargeObject<pokedex::ui::Runtime>(
    store, static_cast<pokedex::ui::Platform&>(testPlatform), &store);
  ready = runtime && runtime->begin(persistBoot);
  if (!runtime) pokedex::ui::drawGraphicsError(platform.readBattery(), pokedex::i18n::Language::Chinese);
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
  for (unsigned budget = 0; budget < 64; ++budget) {
    const int value = Serial.read();
    if (value < 0) break;
    const auto request = clockParser.feed(uint8_t(value));
    if (request.kind == pokedex::clock_sync::CommandKind::None) continue;
    char line[256];
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
  if (clockReplyPending() || !Serial.isPlugged()) return;
  char line[256];
  va_list args; va_start(args, format);
  const int length = vsnprintf(line, sizeof(line), format, args);
  va_end(args);
  if (length > 0 && length < int(sizeof(line)) && Serial.availableForWrite() >= length)
    Serial.write(reinterpret_cast<const uint8_t*>(line), size_t(length));
}
}

namespace pokedex { namespace app {
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
      if (runtime) runtime->openTestPage(entered ? pokedex::ui::Page::TestHub : pokedex::ui::Page::TestEntry, !entered);
      break;
    }
    case Request::ResetSnapshot:
    case Request::ResetBlank: {
      const bool reset = reseedTestMode(request == Request::ResetSnapshot ?
        adventure::TestSeed::Snapshot : adventure::TestSeed::Blank);
      if (runtime) runtime->openTestPage(reset ? pokedex::ui::Page::TestHub : pokedex::ui::Page::TestEntry, !reset);
      break;
    }
    case Request::ScenarioAccount: {
      const bool seeded = seedTestScenario(adventure::TestScenario::AccountBalls);
      if (runtime) runtime->openTestPage(pokedex::ui::Page::TestHub, !seeded);
      break;
    }
    case Request::ScenarioNearEncounter: {
      const bool seeded = seedTestScenario(adventure::TestScenario::NearEncounter);
      if (runtime) runtime->openTestPage(pokedex::ui::Page::TestHub, !seeded);
      break;
    }
    case Request::ScenarioCurrentThrow: {
      const bool seeded = seedTestScenario(adventure::TestScenario::CurrentThrow);
      if (runtime) runtime->openTestPage(pokedex::ui::Page::TestHub, !seeded);
      break;
    }
    case Request::ScenarioLegacy: {
      const bool seeded = seedTestScenario(adventure::TestScenario::LegacyEncounter);
      if (runtime) runtime->openTestPage(pokedex::ui::Page::TestHub, !seeded);
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
  c152::begin("PokeTrail");
  runtime = pokedex::makeLargeObject<pokedex::ui::Runtime>(
    store, static_cast<pokedex::ui::Platform&>(testPlatform), &store);
  if (runtime) runtime->startStartup();
  else pokedex::ui::drawGraphicsError({}, pokedex::i18n::Language::Chinese);
  platform.beginClock();
  store.begin();
  ready = runtime && runtime->begin();
  if (!ready && runtime) {
    pokedex::ui::drawGraphicsError(runtime->screen().battery, runtime->language());
  }
}
void loop() {
  pumpClockSerial();
  if (!runtime) { pumpClockTransmit(); delay(20); return; }
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
  if (Serial.isPlugged() && millis() - report >= 10000) {
    report = millis(); const auto& m = runtime->metrics();
    diagnostic("sample=%lu maxgap=%lu input=%lu gaps=%lu noaccel=%lu imuio=%lu rtc=%lu render=%lu commit=%lu\n",
      (unsigned long)m.samples, (unsigned long)m.maxSampleGapMs, (unsigned long)m.maxInputGapMs,
      (unsigned long)m.qualityGaps, (unsigned long)m.noSample, (unsigned long)m.imuIoErrors,
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
  if (runtime->pocketIdle() && wait > 0 && c152::restUntilInput(wait)) return;
  uint32_t pauseMs = 1;
  if (!clockReplyPending() && Serial.available() <= 0 && !platform.vibrationActive()) {
    if (runtime->dimmed()) {
      pauseMs = wait > 0 ? std::min<uint32_t>(wait, 20u) : 1u;
    } else if (runtime->idlePaceAllowed()) {
      pauseMs = wait > 0 ? std::min<uint32_t>(wait, 10u) : 1u;
    }
  }
  delay(pauseMs);
}
