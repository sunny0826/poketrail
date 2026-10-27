#pragma once
#include "adventure_store.hpp"
#include "battle.hpp"
namespace pokedex { namespace adventure {
struct ThrowView {
  bool active = false;
  AttemptKey key;
  uint32_t targetMs = 0;
};
// The route puzzle returns the first conflicting position instead of changing
// state. A wrong guess costs no ball, step or unlock.
struct RuinsOrderResult {
  CommandResult status = CommandResult::InvalidInput;
  uint8_t conflict = 0;
};
class Engine {
 public:
  explicit Engine(Store& store) : store_(store) {}
  CommandResult begin();
  const Save& view() const { return state_; }
  const BattleView& battleView() const { return battle_.view(); }
  const ThrowView& throwView() const { return throw_; }
  LoadResult loadResult() const { return loadResult_; }
  bool ready() const { return ready_; }
  bool savePending() const { return pending_; }
  bool stepsIncomplete() const { return overflow_ || state_.stepsIncomplete || (pending_ && candidate_.stepsIncomplete); }
  size_t bufferedSteps() const { return queueSize_; }
  EvolutionView evolutionView(uint16_t species) const;
  CommandResult evolve(EvolveRequest, const ClockReading&);
  CommandResult giveUpCapture(uint64_t encounterId);
  CommandResult chooseStarter(uint16_t species);
  CommandResult selectPartner(uint16_t species);
  CommandResult selectRoute(Route);
  CommandResult observeSteps(const StepEvent*, size_t count, uint32_t seedForNext);
  CommandResult observeClock(const ClockReading&);
  CommandResult checkpoint();
  CommandResult retrySave();
  CommandResult startBattle(uint32_t nowMs);
  CommandResult press(Action, uint32_t nowMs);
  CommandResult tick(uint32_t nowMs, const ClockReading&);
  CommandResult requestPause();
  CommandResult resume(uint32_t nowMs);
  CommandResult retreat();
  CommandResult beginThrow(AttemptKey, uint32_t nowMs);
  CommandResult pressThrow(AttemptKey, uint32_t pressMs, const ClockReading&);
  CommandResult acknowledgeResult(uint64_t encounterId, uint32_t seedForNext);
  CommandResult beginClockChange(uint64_t transactionId, ClockChangeMode,
                                 const ClockReading& target);
  CommandResult finishClockChange(uint64_t transactionId, const ClockReading& verified);
  // Legendary ruins. The Engine owns clue submission, order checking, target
  // selection, boss progression and the activity result transaction.
  CommandResult beginRuinsClue(uint8_t clueIndex, uint32_t seedForEncounter);
  RuinsOrderResult solveRuinsOrder(const uint8_t* order);
  CommandResult selectRuinsTarget(uint16_t species);
  CommandResult beginRuinsBoss(uint32_t seedForEncounter);
  CommandResult retryRuinsActivity(uint64_t encounterId, uint32_t seedForEncounter);
  CommandResult acknowledgeActivityResult(uint64_t encounterId);
  // Dragon cave trials. The catalog decides species, stage HP, beats and upgrade
  // ceilings; the Engine owns qualification, stage advancement and retries.
  CommandResult beginDragonTrial(uint8_t trialId, uint32_t seedForEncounter);
  CommandResult chooseDragonUpgrade(uint8_t choice);
  CommandResult retryDragonTrial(uint64_t encounterId, uint32_t seedForEncounter);
 private:
  CommandResult guard() const;
  void stage();
  CommandResult submit();
  CommandResult flushSteps();
  CommandResult enqueue(const StepEvent*, size_t, uint32_t);
  bool applyStep(Save&, const StepEvent&);
  bool applyClock(Save&, const ClockReading&);
  void archiveDay(Save&);
  void createEncounter(Save&, uint32_t seed);
  void openActivity(Save&, uint16_t species, uint16_t opponent, Route, uint32_t seed,
                    EncounterOrigin origin);
  CommandResult startActivity(uint16_t species, uint16_t opponent, Route, uint32_t seed);
  CommandResult finishBattle();
  CommandResult finishDragonBattle(const Encounter&, Result outcome);
  CommandResult resolveThrow(AttemptKey, Timing, const ClockReading&);
  void awardCapture(Save&, const ClockReading&);
  void updateAchievements(Save&, const ClockReading&);
  Store& store_;
  Save state_;
  Save candidate_;
  LoadResult loadResult_;
  bool ready_ = false;
  bool pending_ = false;
  bool dirty_ = false;
  bool overflow_ = false;
  bool bootPending_ = false;
  Battle battle_;
  ThrowView throw_;
  static constexpr size_t kStepQueueCapacity = 128;
  std::array<StepEvent, kStepQueueCapacity> queue_;
  size_t queueSize_ = 0;
  uint32_t queueSeed_ = 0;
  uint64_t clockTransaction_ = 0;
};
} }
