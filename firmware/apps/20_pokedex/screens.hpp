#pragma once
#include "adventure.hpp"
#include "i18n.hpp"
#include "pokedex_text.hpp"
#include "calendar.hpp"
#include "capture_anim.hpp"
#include "evolution_anim.hpp"
#include "game.hpp"
#include "menu_layout.hpp"
#include "scene_backgrounds_generated.hpp"
#include "board_power.hpp"
#include "screen_theme.hpp"
#include "test_mode.hpp"
namespace pokedex { namespace ui {
enum class Page : uint8_t {
  Home, Menu, Settings, DateWelcome, Starter, Routes, Partners, Encounter, Battle, Practice,
  PracticeThrow, PracticeResult,
  Pause, Throw, Result, Records, FirstCaptures, Recent, Days, Achievements,
  Clock, Error, Viewer, PartnerDetail, EvolutionConfirm, Evolution,
  EvolutionPreviewTargets, EvolutionPreview,
  TestEntry, TestHub, TestSteps, TestScenarios, TestFaults, TestDiagnostics, TestExit, Language,
  CarriedPartner
};
enum class LanguagePersistence : uint8_t { Saved, SessionOnly };
enum class PressedAux : uint8_t { None, Back, PrevPage, NextPage, HomeEvolve };
constexpr uint16_t kPanelPressed = theme::rgb(0x24, 0x4A, 0x3E);
struct ScreenState {
  i18n::Language language = i18n::Language::Chinese;
  LanguagePersistence languagePersistence = LanguagePersistence::Saved;
  Page page = Page::Home;
  c152::BatteryStatus battery;
  uint16_t selected = 0;
  int16_t pressedIndex = -1;
  PressedAux pressedAux = PressedAux::None;
  struct TouchPress {
    bool active = false;
    bool invalidated = false;
    Page page = Page::Home;
    int16_t startX = 0, startY = 0;
    int16_t startIndex = -1;
    PressedAux startAux = PressedAux::None;
  } touchPress;
  bool editing = false;
  bool clockError = false;
  bool clockNeedsSync = false;
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
  bool throwCommitting = false;
  bool throwCancelNoticeActive = false;
  uint32_t throwCancelNoticeUntilMs = 0;
  uint32_t battleEffectStartMs = 0;
  bool battleEffectOverride = false;
  uint16_t practiceSpecies = 0;
  uint32_t practiceTargetMs = 0;
  uint8_t practiceUsedBalls = 0;
  bool practiceThrowing = false;
  adventure::Result practiceResult = adventure::Result::None;
};
const char* routeName(i18n::Language, adventure::Route);
using i18n::speciesName;
const char* progressionMessage(i18n::Language, adventure::CommandResult);
enum class MenuItem : uint8_t { Encounter, Region, Partner, Pokedex, Records, Practice, Settings, Explore };
constexpr unsigned kMenuItemCount = 8;
unsigned menuCount(const ScreenState&, const adventure::Engine&);
int menuTop(const ScreenState&, const adventure::Engine&);
const menu_layout::Hub* hubFor(Page);
inline bool encounterWaiting(const adventure::Save& v) {
  return v.phase == adventure::Phase::EncounterReady || v.phase == adventure::Phase::ThrowReady;
}
inline bool throwCancelNotice(const ScreenState& s, uint32_t now) {
  return s.throwCancelNoticeActive && int32_t(s.throwCancelNoticeUntilMs - now) > 0;
}
bool menuSelectable(const ScreenState&, const adventure::Engine&, unsigned index);
constexpr menu_layout::Rect kHomeEvolveButton = {173, 67, 120, 30};
constexpr int kHomeEvolveTouchPad = 8;
constexpr int kHomeEvolveTouchTop = kHomeEvolveButton.y - kHomeEvolveTouchPad < 60
                                        ? 60
                                        : kHomeEvolveButton.y - kHomeEvolveTouchPad;
constexpr menu_layout::Rect kHomeEvolveTouchArea = {
    kHomeEvolveButton.x - kHomeEvolveTouchPad,
    kHomeEvolveTouchTop,
    kHomeEvolveButton.width + kHomeEvolveTouchPad * 2,
    kHomeEvolveButton.y + kHomeEvolveButton.height + kHomeEvolveTouchPad - kHomeEvolveTouchTop,
};
bool homeEvolutionReady(const adventure::Engine&);
struct TouchHit {
  int16_t index;
  PressedAux aux;
};
TouchHit touchHit(const ScreenState&, const adventure::Engine&, int x, int y);
constexpr int kBallSlotY = 82;
constexpr int ballSlotX(unsigned index) { return 213 + 40 * int(index); }
inline bool sceneEdgeVisible(unsigned sourceX, unsigned sourceY) {
  const unsigned last = data::kSceneHeight - 1;
  const unsigned edge = sourceY < last - sourceY ? sourceY : last - sourceY;
  const unsigned shifted = sourceX + 2 * (sourceY & 1);
  if (edge == 0) return shifted % 4 == 0;
  if (edge == 1) return (sourceX + sourceY) % 2 == 0;
  if (edge == 2) return shifted % 4 != 1;
  return true;
}
bool touchBackEnabled(Page);
bool isTestPage(Page);
void drawGraphicsError(const c152::BatteryStatus&, i18n::Language);
class Screens {
 public:
  Screens() = default;
  Screens(const Screens&) = delete;
  Screens& operator=(const Screens&) = delete;
  ~Screens();
  bool begin();
  bool startup(uint32_t elapsed);
  void invalidate();
  bool advanceHome(uint16_t partner, uint32_t now);
  bool advanceResult(uint16_t species, uint32_t now);
  bool advancePreview(uint16_t species, uint32_t now);
  bool advanceScene(adventure::Route, uint32_t now, bool active);
  static bool hasPager(Page page);
  bool draw(const ScreenState&, const adventure::Engine&,
            const adventure::BattleView& practice, uint32_t now);
 private:
  const char* tr(i18n::Text id) const { return i18n::text(language_, id); }
  const char* speciesName(uint16_t id) const { return i18n::speciesName(language_, id); }
  const char* routeName(adventure::Route route) const { return ui::routeName(language_, route); }
  const char* progressionMessage(adventure::CommandResult result) const { return ui::progressionMessage(language_, result); }
  struct Playback { viewer::State state; uint16_t species = 0; };
  void drawResultHero(uint16_t species, bool first, uint32_t now);
  bool exploration(const ScreenState&, const adventure::Engine&, uint32_t now);
  void homeEvolveButton(bool pressed);
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
  void ballSlots(uint8_t used);
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
  void menu(const ScreenState&, const adventure::Engine&);
  void mainMenu(const ScreenState&, const adventure::Engine&);
  void recordsHub(const ScreenState&, const adventure::Engine&);
  void encounterBanner(const ScreenState&, const adventure::Save&);
  void hubTiles(const ScreenState&, const menu_layout::Hub&, const i18n::Text* labels,
                const theme::HubIcon* icons, const adventure::Save&);
  void hubTile(const ScreenState&, const menu_layout::Hub&, unsigned index, const char* label,
               int labelWidth, theme::HubIcon, const adventure::Save&);
  void hubExit(const ScreenState&, const menu_layout::Hub&, const char* label, theme::HubIcon,
               const adventure::Save&);
  void hubIcon(theme::HubIcon, int x, int y, uint16_t color, uint16_t background, const adventure::Save&);
  void item(const ScreenState&, unsigned index, const char*, int top = menu_layout::kTop,
            uint16_t species = 0, bool carried = false, const char* note = nullptr, bool disabled = false);
  void panelFrame(const menu_layout::Rect&, int radius, bool active, bool pressed, uint16_t accent);
  void backButton(const ScreenState&);
  void pagerArrow(const ScreenState&, bool next);
  void pager(const ScreenState&, unsigned count);
  void thumbnail(uint16_t species, int x, int y, unsigned scale = 1);
  void captureBadge(const adventure::Save&, int x, int y);
  void encounterBadge(const adventure::Save&, int x, int y);
  void line(const char*, int y, uint16_t color = theme::kText, bool large = false);
  void fitLine(const char*, int y, int width, uint16_t color, bool large = true);
  void title(const char*, const char* subtitle, uint16_t accent);
  void scene(adventure::Route, bool dim = false, int dx = 0, uint64_t visualSteps = 0);
  void sceneThumbnail(adventure::Route, int cx, int cy);
  void glyph(theme::Icon, int x, int y, uint16_t color);
  void alert(const char*, int y, uint16_t color);
  void progress(uint64_t value, uint32_t total, int y, uint16_t color);
  void hp(const char*, uint8_t value, uint8_t total, bool enemy, uint16_t color);
  void aim(uint16_t species, uint32_t now, bool timing, int32_t remaining, bool committing = false);
  void cueRing(const adventure::BattleView&, uint32_t now);
  void actorShadow(int cx, int cy);
  void warning(int cx, int cy, uint16_t color);
  void stamp(const adventure::Stamp&, int top);
  void badges(uint8_t mask, int top);
  struct SpriteSlot {
    uint16_t* pixels = nullptr;
    uint16_t id = 0;
    uint16_t frame = UINT16_MAX;
    uint16_t scale = 0;
    uint16_t silhouette = 0;
    uint32_t lastUse = 0;
    void reset() { id = 0; frame = UINT16_MAX; scale = 0; silhouette = 0; lastUse = 0; }
  };
  bool spriteBuffersReady() const {
    return spriteSlots_[0].pixels != nullptr && spriteSlots_[1].pixels != nullptr;
  }
  void touchSlot(SpriteSlot&);
  const SpriteSlot* resolveSprite(uint16_t id, uint32_t now, Playback&,
                                  uint16_t silhouette, uint8_t maxScale,
                                  uint16_t maxHeight, uint16_t& scaleOut);
  bool sprite(uint16_t id, uint32_t now);
  bool sprite(uint16_t id, uint32_t now, Playback&, int cy = 233, int cx = 233,
              uint16_t silhouette = 0, uint8_t maxScale = 3, uint16_t maxHeight = kMaxDisplayHeight);
  bool spriteSilhouette(uint16_t id, uint32_t now, Playback&, int cy, int cx,
                        uint16_t silhouette, uint8_t maxScale = 3,
                        uint16_t maxHeight = kMaxDisplayHeight);
  bool advance(Playback&, uint16_t id, uint32_t now);
  struct DrawSnapshot {
    bool valid = false;
    Page page = Page::Home;
    i18n::Language language = i18n::Language::Chinese;
    LanguagePersistence languagePersistence = LanguagePersistence::Saved;
    bool testMode = false;
    c152::BatteryStatus battery;
    uint16_t selected = 0;
    int16_t pressedIndex = -1;
    PressedAux pressedAux = PressedAux::None;
    bool editing = false;
    bool clockError = false;
    bool clockNeedsSync = false;
    bool testFailure = false;
    bool evolved = false;
    bool savePending = false;
    bool legacyNoBalls = false;
    bool captureActive = false;
    bool prevCaptureBurst = false;
    bool showBallSlots = false;
    bool evoRevealed = false;
    bool homePartnerRendered = false;
    bool homeEvolvable = false;
    bool todayKnown = false;
    bool todayComplete = false;
    bool clockTrusted = false;
    bool clockPending = false;
    bool routeExhausted = false;
    bool allExhausted = false;
    bool encounterWaiting = false;
    bool firstCapture = false;
    bool throwCommitting = false;
    bool timingActive = false;
    adventure::Phase phase = adventure::Phase::Walking;
    adventure::Result result = adventure::Result::None;
    adventure::Route route = adventure::Route::Forest;
    adventure::CommandResult command = adventure::CommandResult::Applied;
    adventure::LoadStatus loadStatus = adventure::LoadStatus::Empty;
    adventure::ClockChangeMode clockMode = adventure::ClockChangeMode::Normal;
    adventure::TestFault testFault = adventure::TestFault::None;
    adventure::TestDiagnostics testDiagnostics;
    calendar::CivilTime civil = {};
    int offset = 0;
    int battleShake = 0;
    uint8_t usedBalls = 0;
    uint8_t playerHp = 0;
    uint8_t enemyHp = 0;
    uint8_t battleFooterKind = 0;
    uint8_t countdown = 0;
    uint8_t throwPromptKind = 0;
    uint8_t homeFooterKind = 0;
    uint8_t todayMilestonesMask = 0;
    uint8_t achievementsMask = 0;
    uint8_t selectedDayMilestonesMask = 0;
    bool selectedDayComplete = false;
    uint16_t partner = 0;
    uint16_t species = 0;
    uint16_t enemy = 0;
    uint16_t evoFrom = 0;
    uint16_t evoTo = 0;
    uint16_t capturedCount = 0;
    uint16_t pendingEncounters = 0;
    uint16_t recentSize = 0;
    uint16_t historySize = 0;
    uint16_t selectedCaptureCount = 0;
    uint16_t alertTarget = 0;
    uint16_t battleFooterColor = 0;
    uint16_t animFrame = UINT16_MAX;
    uint32_t rosterRevision = 0;
    uint32_t encounterTarget = 0;
    uint32_t todayDayKey = 0;
    uint32_t schema = 0;
    uint32_t ruleVersion = 0;
    uint32_t alertCost = 0;
    uint64_t sequence = 0;
    uint64_t bestDaySteps = 0;
    uint64_t explorationCredit = 0;
    uint64_t todaySteps = 0;
    uint64_t totalSteps = 0;
    uint64_t walked = 0;
    uint64_t available = 0;
    uint64_t selectedDaySteps = 0;
    uint64_t sceneSteps = 0;
    const char* battleFooterText = nullptr;
    adventure::CommandResult alertStatus = adventure::CommandResult::Applied;
    uint8_t alertKind = 0;
    menu_layout::Rect spriteRect = {0, 0, 0, 0};
  };
  using RowDrawer = void (Screens::*)(const ScreenState&, const adventure::Engine&, unsigned index, int top);
  struct WriteScope {
    WriteScope();
    ~WriteScope();
  };
  bool dispatchPage(const ScreenState&, const adventure::Engine&,
                    const adventure::BattleView& practice, uint32_t now, bool full);
  void clearRegion(int x, int y, int w, int h, uint16_t bg = theme::kBackground);
  void restoreSceneRegion(int x, int y, int w, int h, adventure::Route route, bool dim);
  void restoreBackdrop(int x, int y, int w, int h, adventure::Route route, bool dim, bool hasScene);
  menu_layout::Rect spriteBounds(uint16_t id, int cx, int cy, uint8_t maxScale, uint16_t maxHeight) const;
  void updateListRows(const ScreenState&, const adventure::Engine&, int top, bool pagerOn,
                      unsigned count, RowDrawer drawer, bool hasScene = false,
                      adventure::Route sceneRoute = adventure::Route::Forest, bool dim = false);
  void drawTitleCleared(const ScreenState&, const char* heading, const char* subtitle,
                        uint16_t accent, bool clearSubtitle = true);
  void drawMainMenu(const ScreenState&, const adventure::Engine&, bool full);
  void drawRecordsHub(const ScreenState&, const adventure::Engine&, bool full);
  bool drawHome(const ScreenState&, const adventure::Engine&, uint32_t now, bool full);
  void drawBattle(const ScreenState&, const adventure::Engine&,
                  const adventure::BattleView& practice, uint32_t now, bool full);
  void drawThrow(const ScreenState&, const adventure::Engine&, uint32_t now, bool full);
  void drawThrowPrompt(uint8_t promptKind);
  void drawThrowStagePartial(const ScreenState&, adventure::Route route, uint16_t species,
                             bool timing, int32_t remaining, bool committing, uint8_t usedBalls,
                             bool legacyNoBalls, uint32_t now, bool allowExitHint);
  void drawPractice(const ScreenState&, const adventure::Engine&,
                    const adventure::BattleView& practice, uint32_t now, bool full);
  bool drawEvolution(const ScreenState&, uint32_t now, bool full);
  void drawEvolutionTargets(const ScreenState&, const adventure::Engine&, bool full);
  void drawClock(const ScreenState&, const adventure::Engine&, bool full);
  void drawProgression(const ScreenState&, const adventure::Engine&, bool full);
  void drawRecords(const ScreenState&, const adventure::Engine&, bool full);
  void drawTest(const ScreenState&, const adventure::Engine&, bool full);
  void drawHomeGroup(const ScreenState&, const adventure::Engine&, uint32_t now, bool full);
  void drawMenuRow(const ScreenState&, const adventure::Engine&, unsigned index, int top);
  void drawLanguageRow(const ScreenState&, const adventure::Engine&, unsigned index, int top);
  void drawProgressionRow(const ScreenState&, const adventure::Engine&, unsigned index, int top);
  void drawEvolutionTargetRow(const ScreenState&, const adventure::Engine&, unsigned index, int top);
  void drawClockRow(const ScreenState&, const adventure::Engine&, unsigned index, int top);
  void drawTestRow(const ScreenState&, const adventure::Engine&, unsigned index, int top);
  void drawPracticeResultRow(const ScreenState&, const adventure::Engine&, unsigned index, int top);
  i18n::Language language_ = i18n::Language::Chinese;
  SpriteSlot spriteSlots_[2];
  uint32_t spriteUseSeq_ = 0;
  uint16_t* sceneCache_ = nullptr;
  uint16_t sceneRow_[456];
  Playback startupPlayback_, homePlayback_, battlePlayback_, enemyBattlePlayback_, resultPlayback_;
  adventure::Route sceneRoute_ = adventure::Route::Forest;
  adventure::Route sceneCacheRoute_ = adventure::Route::Forest;
  uint64_t sceneSteps_ = 0;
  uint32_t sceneLastMs_ = 0;
  bool sceneTiming_ = false;
  bool sceneCacheDim_ = false;
  bool sceneCacheValid_ = false;
  DrawSnapshot snap_;
};
} }
