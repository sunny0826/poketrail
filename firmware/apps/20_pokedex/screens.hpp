#pragma once
#include "adventure.hpp"
#include "calendar.hpp"
#include "game.hpp"
#include "menu_layout.hpp"
#include "board_power.hpp"
namespace pokedex { namespace ui {
enum class Page : uint8_t {
  Home, Menu, DateWelcome, Starter, Routes, Partners, Encounter, Battle, Practice,
  Pause, Throw, Result, Records, FirstCaptures, Recent, Days, Achievements,
  Clock, Error, Viewer, PartnerDetail, EvolutionConfirm, ReleaseConfirm
};
struct ScreenState {
  Page page = Page::Home;
  c152::BatteryStatus battery;
  uint16_t selected = 0;
  bool editing = false;
  bool clockError = false;
  bool practice = false;
  uint16_t species = 0;
  adventure::EvolveRequest evolution;
  adventure::CommandResult command = adventure::CommandResult::Applied;
  bool evolved = false;
  calendar::CivilTime civil = {2026, 1, 1, 12, 0, 0};
  int offset = 480;
  adventure::ClockChangeMode clockMode = adventure::ClockChangeMode::Normal;
};
const char* routeName(adventure::Route);
const char* speciesName(uint16_t id);
const char* progressionMessage(adventure::CommandResult);
unsigned menuCount(const ScreenState&, const adventure::Engine&);
void drawGraphicsError(const c152::BatteryStatus&);
class Screens {
 public:
  ~Screens();
  bool begin();
  void invalidate();
  bool advanceHome(uint16_t partner, uint32_t now);
  bool draw(const ScreenState&, const adventure::Engine&,
            const adventure::BattleView& practice, uint32_t now);
 private:
  struct Playback { viewer::State state; uint16_t species = 0; };
  bool exploration(const adventure::Engine&, uint32_t now);
  void home(const ScreenState&, const adventure::Engine&);
  void progression(const ScreenState&, const adventure::Engine&);
  void battle(const ScreenState&, const adventure::Engine&,
              const adventure::BattleView&, uint32_t);
  void records(const ScreenState&, const adventure::Engine&);
  void clock(const ScreenState&);
  void menu(const ScreenState&, const adventure::Engine&, int top = menu_layout::kTop);
  void item(const ScreenState&, unsigned index, const char*, int top = menu_layout::kTop,
            uint16_t species = 0, bool carried = false);
  void thumbnail(uint16_t species, int x, int y, unsigned scale = 1);
  void ballCount(uint64_t count, int y);
  void line(const char*, int y, uint16_t color = 0xFFFF, bool large = false);
  void stamp(const adventure::Stamp&, int top);
  void badges(uint8_t mask, int top);
  bool sprite(uint16_t id, uint32_t now);
  bool sprite(uint16_t id, uint32_t now, Playback&);
  bool advance(Playback&, uint16_t id, uint32_t now);
  uint16_t* pixels_ = nullptr;
  Playback homePlayback_, battlePlayback_;
  uint16_t decodedId_ = 0, decodedFrame_ = UINT16_MAX;
};
} }
