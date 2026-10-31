#pragma once
#include <SDL.h>
#include "runtime.hpp"
#include "../../simulator/options.hpp"
#include <functional>
#include <memory>
#include <string>

namespace simulator {
class App;
namespace e2e {
using namespace pokedex::ui;
using namespace pokedex::adventure;
class Driver {
 public:
  Driver(const std::string& saveDir, const std::string& output, uint32_t seed, int64_t utc);
  ~Driver();
  const Runtime& runtime() const;
  const Save& save() const { return runtime().engine().view(); }
  const ScreenState& screen() const { return runtime().screen(); }
  uint32_t now() const;
  bool dimmed() const { return runtime().dimmed(); }
  void check(bool, const std::string&);
  void frames(unsigned = 4);
  void advance(uint32_t ms);
  void until(const std::function<bool()>&, uint32_t maxMs, const std::string&);
  void key(SDL_Scancode, bool down, unsigned ticks = 4);
  void press(SDL_Scancode);
  void tap(int x, int y);
  void swipe(int fromX, int toX, int y = 300);
  void select(unsigned index);
  void back();
  void homeMenu();
  void wake();
  void walk(unsigned minimumSteps);
  void winBattle(bool practice = false);
  void replayBattle();
  int64_t rtcUtc() const;
  void throwBall(bool perfect = true, bool practice = false);
  void finishCapture();
  void shot(const std::string&);
  void state(const std::string&);
  enum class Close { Closed, Protected };
  void close(Close expected = Close::Closed);
  void fault(Fault);
  unsigned faultHits() const;
  unsigned writes() const;
  unsigned closeErrors() const { return closeErrors_; }
  void focusLost(bool hostWindow = false);
  void freeze();
  void event(const std::string& kind, const std::string& detail);
 private:
  void push(SDL_Event&, unsigned ticks = 4);
  void mouse(int x, int y, bool down);
  std::string output_;
  FILE* events_ = nullptr;
  unsigned closeErrors_ = 0;
  std::unique_ptr<App> app_;
};
void scenarioJourney(Driver&, const std::string& phase);
void scenarioCapture(Driver&, const std::string& phase);
void scenarioThrows(Driver&, const std::string& phase);
void scenarioRetry(Driver&, const std::string& phase);
void scenarioProgression(Driver&, const std::string& phase);
void scenarioConflict(Driver&, const std::string& phase);
void scenarioIsolation(Driver&, const std::string& phase);
void scenarioClock(Driver&, const std::string& phase);
void scenarioLifecycle(Driver&, const std::string& phase);
void scenarioEdges(Driver&, const std::string& phase);
void scenarioCrash(Driver&, const std::string& phase);
void scenarioMigration(Driver&, const std::string& phase);
void makeFixture(const std::string& name, const std::string& directory);
}
}
