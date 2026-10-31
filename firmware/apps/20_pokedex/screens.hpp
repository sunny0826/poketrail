#pragma once
#include "adventure.hpp"
#include "calendar.hpp"
#include "capture_anim.hpp"
#include "evolution_anim.hpp"
#include "game.hpp"
#include "menu_layout.hpp"
#include "board_power.hpp"
#include "screen_theme.hpp"
#include "test_mode.hpp"
namespace pokedex { namespace ui {
enum class Page : uint8_t {
  Home, Menu, Resources, DateWelcome, Starter, Routes, Partners, Encounter, Battle, Practice,
  PracticeThrow, PracticeResult,
  Pause, Throw, Result, Records, FirstCaptures, Recent, Days, Achievements,
  Clock, Error, Viewer, PartnerDetail, EvolutionConfirm, ReleaseConfirm, Evolution,
  EvolutionPreviewTargets, EvolutionPreview,
  TestEntry, TestHub, TestSteps, TestScenarios, TestFaults, TestDiagnostics, TestExit
};
struct ScreenState {
  Page page = Page::Home;
  c152::BatteryStatus battery;
  uint16_t selected = 0;
  bool editing = false;
  bool clockError = false;
  bool practice = false;
  bool testMode = false;
  bool testFailure = false;
  adventure::TestFault testFault = adventure::TestFault::None;
  adventure::TestDiagnostics testDiagnostics;
  uint16_t species = 0;
  adventure::EvolveRequest evolution;
  EvolutionAnim evolutionAnim;
  adventure::CommandResult command = adventure::CommandResult::Applied;
  bool evolved = false;
  calendar::CivilTime civil = {2026, 1, 1, 12, 0, 0};
  int offset = 480;
  adventure::ClockChangeMode clockMode = adventure::ClockChangeMode::Normal;
  CaptureAnim capture;
  uint32_t chargeStartMs = 0;
  bool charging = false;
  uint16_t practiceSpecies = 0;
  uint32_t practiceTargetMs = 0;
  uint8_t practiceUsedBalls = 0;
  bool practiceThrowing = false;
  adventure::Result practiceResult = adventure::Result::None;
};
const char* routeName(adventure::Route);
const char* speciesName(uint16_t id);
const char* progressionMessage(adventure::CommandResult);
unsigned menuCount(const ScreenState&, const adventure::Engine&);
// Home shows the evolve button only while the carried partner can evolve now.
constexpr menu_layout::Rect kHomeEvolveButton = {173, 67, 120, 30};
bool homeEvolutionReady(const adventure::Engine&);
bool touchBackEnabled(Page);
bool isTestPage(Page);
void drawGraphicsError(const c152::BatteryStatus&);
class Screens {
 public:
  ~Screens();
  bool begin();
  void invalidate();
  bool advanceHome(uint16_t partner, uint32_t now);
  bool advanceResult(uint16_t species, uint32_t now);
  bool advanceScene(adventure::Route, uint32_t now, bool active);
  bool draw(const ScreenState&, const adventure::Engine&,
            const adventure::BattleView& practice, uint32_t now);
 private:
  struct Playback { viewer::State state; uint16_t species = 0; };
  bool exploration(const adventure::Engine&, uint32_t now);
  void home(const ScreenState&, const adventure::Engine&, uint32_t now);
  void progression(const ScreenState&, const adventure::Engine&);
  bool evolution(const ScreenState&, uint32_t now);
  void evolutionPreviewTargets(const ScreenState&, const adventure::Engine&);
  void battle(const ScreenState&, const adventure::Engine&,
              const adventure::BattleView&, uint32_t);
  void practiceThrow(const ScreenState&, uint32_t now);
  void practiceResult(const ScreenState&, uint32_t now);
  void capture(const ScreenState&, uint16_t species, uint32_t now);
  void ballAt(int x, int y, float angle = 0);
  void records(const ScreenState&, const adventure::Engine&);
  void clock(const ScreenState&);
  void testEntry(const ScreenState&);
  void testHub(const ScreenState&, const adventure::Engine&);
  void testSteps(const ScreenState&);
  void testScenarios(const ScreenState&);
  void testFaults(const ScreenState&);
  void testDiagnostics(const ScreenState&, const adventure::Engine&);
  void testExit(const ScreenState&);
  void testMenu(const ScreenState&, const char* const* labels,
                unsigned count, int top = menu_layout::kTop);
  void menu(const ScreenState&, const adventure::Engine&, int top = menu_layout::kTop);
  void item(const ScreenState&, unsigned index, const char*, int top = menu_layout::kTop,
            uint16_t species = 0, bool carried = false, const char* note = nullptr);
  void thumbnail(uint16_t species, int x, int y, unsigned scale = 1);
  void captureBadge(const adventure::Save&, int x, int y);
  void line(const char*, int y, uint16_t color = theme::kText, bool large = false);
  void fitLine(const char*, int y, int width, uint16_t color, bool large = true);
  void title(const char*, const char* subtitle, uint16_t accent);
  void scene(adventure::Route, bool dim = false, int dx = 0, uint64_t visualSteps = 0);
  void sceneThumbnail(adventure::Route, int cx, int cy);
  void glyph(theme::Icon, int x, int y, uint16_t color);
  void alert(const char*, int y, uint16_t color);
  void progress(uint64_t value, uint32_t total, int y, uint16_t color);
  void hp(const char*, uint8_t value, uint8_t total, int x, uint16_t color);
  void cueRing(const adventure::BattleView&, uint32_t now);
  void actorShadow(int cx, int cy);
  void warning(int cx, int cy, uint16_t color);
  void stamp(const adventure::Stamp&, int top);
  void badges(uint8_t mask, int top);
  bool sprite(uint16_t id, uint32_t now);
  bool sprite(uint16_t id, uint32_t now, Playback&, int cy = 233, int cx = 233,
              uint16_t silhouette = 0, uint8_t maxScale = 3, uint16_t maxHeight = kMaxDisplayHeight);
  bool advance(Playback&, uint16_t id, uint32_t now);
  uint16_t* pixels_ = nullptr;
  uint16_t sceneRow_[456];
  Playback homePlayback_, battlePlayback_, enemyBattlePlayback_, resultPlayback_;
  adventure::Route sceneRoute_ = adventure::Route::Forest;
  uint64_t sceneSteps_ = 0;
  uint32_t sceneLastMs_ = 0;
  bool sceneTiming_ = false;
  uint16_t decodedId_ = 0, decodedFrame_ = UINT16_MAX, decodedScale_ = 0, decodedSilhouette_ = 0;
};
} }
