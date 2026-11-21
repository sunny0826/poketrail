#include "screens.hpp"
#include "board.hpp"
#include "sprite.hpp"
#include "sprites_generated.hpp"
#include "battery_screen.hpp"
#include <algorithm>
#include <cstdlib>
#if defined(ARDUINO_ARCH_ESP32)
#include <esp_heap_caps.h>
#endif
namespace pokedex { namespace ui {
using namespace adventure;
using i18n::Text;
namespace {
constexpr size_t kSpriteBufferPixels = size_t(kMaxDisplayWidth) * kMaxDisplayHeight;
constexpr size_t kSceneCachePixels = size_t(data::kSceneWidth * data::kSceneScale) *
                                     size_t(data::kSceneHeight * data::kSceneScale);

uint16_t* allocBuffer(size_t pixels) {
  const size_t bytes = pixels * sizeof(uint16_t);
#if defined(ARDUINO_ARCH_ESP32)
  return static_cast<uint16_t*>(heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
#else
  return static_cast<uint16_t*>(std::malloc(bytes));
#endif
}

void freeBuffer(uint16_t*& ptr) {
  if (!ptr) return;
#if defined(ARDUINO_ARCH_ESP32)
  heap_caps_free(ptr);
#else
  std::free(ptr);
#endif
  ptr = nullptr;
}
}
const char* routeName(i18n::Language language, Route r) {
  const Text names[] = {Text::RouteForest, Text::RouteSea, Text::RouteCave, Text::RouteKanto,
                        Text::RouteJohto, Text::RouteHoenn, Text::RouteSinnoh, Text::RouteUnova};
  return i18n::text(language, r <= Route::Unova ? names[uint8_t(r)] : Text::Unknown);
}
unsigned menuCount(const ScreenState& s, const Engine& e) {
  switch (s.page) {
    case Page::Menu: return kMenuItemCount;
    case Page::CarriedPartner: return e.view().partner ? 2 + e.evolutionView(e.view().partner).count : 1;
    case Page::Settings: return 3;
    case Page::DateWelcome: return 4;
    case Page::Starter: return 4;
    case Page::Routes: return kRegionCount + 1;
    case Page::Partners: {
      unsigned n = 1; for (unsigned id = 1; id <= kSpeciesCount; ++id) n += owns(e.view(), id); return n;
    }
    case Page::Encounter: return 4;
    case Page::Pause: return 2;
    case Page::Throw: {
      const auto& v = e.view();
      return (v.encounter.ruleVersion < 3 && !availableBalls(v)) ? 1 : 0;
    }
    case Page::PracticeThrow: return 0;
    case Page::PracticeResult: return 2;
    case Page::PartnerDetail: return 2 + e.evolutionView(s.species).count;
    case Page::EvolutionConfirm: return 2;
    case Page::EvolutionPreviewTargets: return 1 + e.evolutionView(s.evolutionAnim.from).count;
    case Page::Result: return 1;
    case Page::Records: return 5;
    case Page::FirstCaptures: return kSpeciesCount + 1;
    case Page::Recent: return e.view().recentSize + 1;
    case Page::Days: return e.view().historySize + 2;
    case Page::Achievements: return 7;
    case Page::Clock: return 9;
    case Page::Error: return 3;
    case Page::Language: return 3;
    case Page::TestEntry: return 3;
    case Page::TestHub: return 8;
    case Page::TestSteps: return 6;
    case Page::TestScenarios: return 5;
    case Page::TestFaults: return 5;
    case Page::TestExit: return 2;
    default: return 0;
  }
}
int menuTop(const ScreenState& s, const Engine& e) {
  if (s.page == Page::Result)
    return e.view().result == Result::Captured ? menu_layout::kCaptureResultTop : menu_layout::kResultTop;
  return menu_layout::kTop;
}
const menu_layout::Hub* hubFor(Page page) {
  if (page == Page::Menu) return &menu_layout::kMainHub;
  if (page == Page::Records) return &menu_layout::kRecordsHub;
  return nullptr;
}
bool menuSelectable(const ScreenState& s, const Engine& e, unsigned index) {
  return s.page != Page::Menu || index != unsigned(MenuItem::Encounter) || encounterWaiting(e.view());
}
bool homeEvolutionReady(const Engine& e) {
  if (!e.ready()) return false;
  const EvolutionView view = e.evolutionView(e.view().partner);
  for (unsigned i = 0; i < view.count; ++i)
    if (view.options[i].status == CommandResult::Applied) return true;
  return false;
}
bool touchBackEnabled(Page page) {
  switch (page) {
    case Page::Menu: case Page::Settings: case Page::Routes: case Page::Partners:
    case Page::CarriedPartner: case Page::PartnerDetail: case Page::EvolutionConfirm:
    case Page::Records: case Page::FirstCaptures: case Page::Recent: case Page::Days:
    case Page::Achievements: case Page::Clock: case Page::EvolutionPreviewTargets:
    case Page::TestEntry: case Page::TestHub: case Page::TestSteps: case Page::TestScenarios:
    case Page::TestFaults: case Page::TestDiagnostics: case Page::TestExit:
    case Page::Language: return true;
    default: return false;
  }
}
bool isTestPage(Page page) {
  return page == Page::TestEntry || page == Page::TestHub || page == Page::TestSteps ||
         page == Page::TestScenarios || page == Page::TestFaults ||
         page == Page::TestDiagnostics || page == Page::TestExit;
}
void drawGraphicsError(const c152::BatteryStatus& battery, i18n::Language language) {
  M5.Display.startWrite();
  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.fillCircle(233, 233, 227, theme::kBackground);
  M5.Display.drawTriangle(233, 153, 184, 239, 282, 239, theme::kRed);
  M5.Display.setTextDatum(middle_center);
  M5.Display.setFont(&fonts::efontCN_24);
  M5.Display.setTextColor(theme::kRed);
  M5.Display.drawString("!", 233, 208);
  M5.Display.setFont(&fonts::efontCN_16);
  M5.Display.drawString(i18n::text(language, Text::GraphicsError), 233, 290);
  drawBattery(battery);
  M5.Display.endWrite();
}
Screens::~Screens() {
  freeBuffer(spriteSlots_[0].pixels);
  freeBuffer(spriteSlots_[1].pixels);
  freeBuffer(sceneCache_);
}
bool Screens::begin() {
  for (auto& slot : spriteSlots_)
    if (!slot.pixels) slot.pixels = allocBuffer(kSpriteBufferPixels);
  if (!sceneCache_) {
    sceneCache_ = allocBuffer(kSceneCachePixels);
    sceneCacheValid_ = false;
  }
  return spriteBuffersReady();
}
bool Screens::startup(uint32_t elapsed) {
  if (!spriteSlots_[0].pixels) return false;
  M5.Display.startWrite();
  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.fillCircle(233, 233, 227, theme::kBackground);
  M5.Display.fillCircle(233, 207, 91, theme::kPanel);
  M5.Display.fillEllipse(233, 259, 45, 9, theme::kBackground);
  advance(startupPlayback_, 1, elapsed);
  const auto& sprite = data::kSprites[0];
  auto& slot = spriteSlots_[0];
  slot.reset();
  const bool rendered = decodeFrame(sprite, startupPlayback_.state.frame_index, 3,
                                    slot.pixels, kSpriteBufferPixels);
  if (rendered) M5.Display.pushImage(233 - sprite.width * 3 / 2, 207 - sprite.height * 3 / 2,
                                   sprite.width * 3, sprite.height * 3, slot.pixels, kTransparentKey);
  M5.Display.setTextDatum(middle_center);
  M5.Display.setFont(&fonts::Font4);
  M5.Display.setTextSize(2);
  M5.Display.setTextColor(theme::kText);
  M5.Display.drawString("PokeTrail", 233, 333);
  M5.Display.setTextSize(1);
  M5.Display.endWrite();
  return rendered;
}
void Screens::invalidate() {
  battlePlayback_.species = enemyBattlePlayback_.species = resultPlayback_.species = 0;
  for (auto& slot : spriteSlots_) slot.reset();
  spriteUseSeq_ = 0;
}
bool Screens::advance(Playback& playback, uint16_t id, uint32_t now) {
  if (!id || id > data::kSpriteCount) return false;
  const bool changed = playback.species != id;
  if (changed) {
    playback.state = viewer::State(); playback.state.sprite_index = id - 1;
    playback.state.animation_started_ms = now; playback.species = id;
  }
  return viewer::update(playback.state, data::kSprites[id - 1], now) || changed;
}
bool Screens::advanceHome(uint16_t partner, uint32_t now) { return advance(homePlayback_, partner, now); }
bool Screens::advanceResult(uint16_t species, uint32_t now) { return advance(resultPlayback_, species, now); }
bool Screens::advancePreview(uint16_t species, uint32_t now) { return advance(resultPlayback_, species, now); }
bool Screens::advanceScene(Route route, uint32_t now, bool active) {
  if (sceneRoute_ != route || !sceneTiming_) {
    sceneRoute_ = route;
    sceneSteps_ = 0;
    sceneLastMs_ = now;
    sceneTiming_ = true;
    return true;
  }
  if (!active) {
    sceneLastMs_ = now;
    return false;
  }
  const uint32_t elapsed = now - sceneLastMs_;
  if (elapsed < 100) return false;
  const uint32_t steps = elapsed / 100;
  sceneLastMs_ += steps * 100;
  sceneSteps_ += steps;
  return true;
}
void Screens::touchSlot(SpriteSlot& slot) {
  if (spriteUseSeq_ == UINT32_MAX) {
    SpriteSlot& other = (&slot == &spriteSlots_[0]) ? spriteSlots_[1] : spriteSlots_[0];
    other.lastUse = other.id ? 1u : 0u;
    spriteUseSeq_ = 1u;
  }
  slot.lastUse = ++spriteUseSeq_;
}
const Screens::SpriteSlot* Screens::resolveSprite(uint16_t id, uint32_t now, Playback& playback,
                                                  uint16_t silhouette, uint8_t maxScale,
                                                  uint16_t maxHeight, uint16_t& scaleOut) {
  if (!id || id > data::kSpriteCount || !spriteBuffersReady() || !maxScale) return nullptr;
  advance(playback, id, now);
  const auto& s = data::kSprites[id - 1];
  uint16_t scale = displayScale(s);
  if (scale > maxScale) scale = maxScale;
  while (scale > 1 && s.height * scale > maxHeight) --scale;
  if (!scale) return nullptr;
  const uint16_t frame = playback.state.frame_index;
  for (auto& slot : spriteSlots_) {
    if (slot.id == id && slot.frame == frame &&
        slot.scale == scale && slot.silhouette == silhouette) {
      touchSlot(slot);
      scaleOut = scale;
      return &slot;
    }
  }
  SpriteSlot* victim = &spriteSlots_[0];
  if (spriteSlots_[0].id != 0) {
    if (spriteSlots_[1].id == 0 || spriteSlots_[1].lastUse < spriteSlots_[0].lastUse)
      victim = &spriteSlots_[1];
  }
  victim->reset();
  if (!decodeFrame(s, frame, scale, victim->pixels, kSpriteBufferPixels)) return nullptr;
  if (silhouette) {
    const size_t count = size_t(s.width) * scale * s.height * scale;
    for (size_t index = 0; index < count; ++index)
      if (victim->pixels[index] != kTransparentKey) victim->pixels[index] = silhouette;
  }
  victim->id = id;
  victim->frame = frame;
  victim->scale = scale;
  victim->silhouette = silhouette;
  touchSlot(*victim);
  scaleOut = scale;
  return victim;
}
bool Screens::sprite(uint16_t id, uint32_t now) { return sprite(id, now, battlePlayback_); }
bool Screens::sprite(uint16_t id, uint32_t now, Playback& playback, int cy, int cx,
                     uint16_t silhouette, uint8_t maxScale, uint16_t maxHeight) {
  uint16_t scale = 0;
  const SpriteSlot* slot = resolveSprite(id, now, playback, silhouette, maxScale, maxHeight, scale);
  if (!slot) return false;
  const auto& s = data::kSprites[id - 1];
  M5.Display.pushImage(cx - s.width * scale / 2, cy - s.height * scale / 2,
                       s.width * scale, s.height * scale, slot->pixels, kTransparentKey);
  return true;
}
bool Screens::spriteSilhouette(uint16_t id, uint32_t now, Playback& playback, int cy, int cx,
                               uint16_t silhouette, uint8_t maxScale, uint16_t maxHeight) {
  if (!silhouette) return sprite(id, now, playback, cy, cx, 0, maxScale, maxHeight);
  uint16_t scale = 0;
  const SpriteSlot* slot = resolveSprite(id, now, playback, 0, maxScale, maxHeight, scale);
  if (!slot) return false;
  const auto& s = data::kSprites[id - 1];
  const int width = int(s.width) * int(scale);
  const int height = int(s.height) * int(scale);
  const int left = cx - width / 2;
  const int top = cy - height / 2;
  constexpr size_t kRowCapacity = sizeof(sceneRow_) / sizeof(sceneRow_[0]);
  const int chunkRows = std::max(1, int(kRowCapacity / size_t(width)));
  for (int row = 0; row < height; row += chunkRows) {
    const int rows = std::min(chunkRows, height - row);
    const size_t count = size_t(width) * size_t(rows);
    const uint16_t* src = slot->pixels + size_t(row) * size_t(width);
    for (size_t i = 0; i < count; ++i)
      sceneRow_[i] = src[i] == kTransparentKey ? kTransparentKey : silhouette;
    M5.Display.pushImage(left, top + row, width, rows, sceneRow_, kTransparentKey);
  }
  return true;
}
bool Screens::draw(const ScreenState& s, const Engine& e, const BattleView& practice, uint32_t now) {
  language_ = s.language;
  M5.Display.startWrite();
  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.fillCircle(233, 233, 227, theme::kBackground);
  bool rendered = spriteBuffersReady();
  switch (s.page) {
    case Page::Home: rendered = exploration(e, now); break;
    case Page::Battle: case Page::Practice: case Page::Throw:
      battle(s, e, practice, now); break;
    case Page::PracticeThrow: practiceThrow(s, now); break;
    case Page::PracticeResult: practiceResult(s, now); break;
    case Page::Evolution: case Page::EvolutionPreview: rendered = evolution(s, now); break;
    case Page::EvolutionPreviewTargets: evolutionPreviewTargets(s, e); break;
    case Page::FirstCaptures: case Page::Recent: case Page::Days: case Page::Achievements:
      records(s, e); break;
    case Page::Clock: clock(s); break;
    case Page::Language: language(s, e); break;
    case Page::TestEntry: testEntry(s); break;
    case Page::TestHub: testHub(s, e); break;
    case Page::TestSteps: testSteps(s); break;
    case Page::TestScenarios: testScenarios(s); break;
    case Page::TestFaults: testFaults(s); break;
    case Page::TestDiagnostics: testDiagnostics(s, e); break;
    case Page::TestExit: testExit(s); break;
    case Page::CarriedPartner: case Page::PartnerDetail: case Page::EvolutionConfirm:
      progression(s, e); break;
    case Page::Menu: mainMenu(s, e); break;
    case Page::Records: recordsHub(s, e); break;
    default: home(s, e, now); break;
  }
  const bool listPage = s.page == Page::DateWelcome || s.page == Page::Settings || s.page == Page::Clock ||
                        s.page == Page::Partners || s.page == Page::PartnerDetail || s.page == Page::CarriedPartner ||
                        s.page == Page::EvolutionPreviewTargets || s.page == Page::TestHub ||
                        s.page == Page::TestSteps || s.page == Page::TestScenarios ||
                        s.page == Page::TestFaults;
  if (listPage && menuCount(s, e) > menu_layout::kPageSize) pager(s, menuCount(s, e));
  if (touchBackEnabled(s.page)) glyph(theme::Icon::Back, 112, 52, theme::kMuted);
  drawBattery(s.battery);
  if (s.testMode) {
    M5.Display.setTextDatum(middle_center);
    M5.Display.setFont(&fonts::efontCN_16);
    M5.Display.setTextColor(theme::kGold);
    M5.Display.drawString(tr(Text::TestBadge), 233, 452);
  }
  M5.Display.endWrite();
  return rendered;
}
} }
