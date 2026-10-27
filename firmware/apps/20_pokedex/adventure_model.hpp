#pragma once
#include <array>
#include <cstddef>
#include <cstdint>

namespace pokedex { namespace adventure {
constexpr size_t kSpeciesCount = 251;
constexpr size_t kSpeciesBytes = 32;
constexpr size_t kEvolutionOptions = 5;
constexpr size_t kRecentCount = 32;
constexpr size_t kHistoryCount = 14;
constexpr uint16_t kSchemaVersion = 3;
constexpr uint16_t kRuleVersion = 2;
constexpr uint32_t kEncounterSteps = 5000;
// Activity state ceilings. They keep the optional schema-3 block representable
// and reject impossible values; the activity catalogs tighten them when fixed.
constexpr uint8_t kRuinsClueCount = 3;
constexpr uint8_t kDragonTrialCount = 2;
constexpr uint8_t kDragonTrialStageCount = 8;
constexpr uint8_t kMaxBattleHp = 9;
constexpr uint16_t kMaxWindowBonusMs = 180;
constexpr uint8_t kMaxMissedDodgeGuard = 1;
enum class Route : uint8_t { Forest, Coast, Cave };
enum class EncounterOrigin : uint8_t { Ordinary = 0, Ruins = 1, DragonTrial = 2 };
enum class Phase : uint8_t { Walking, EncounterReady, Battle, ThrowReady, Resolved };
enum class Timing : uint8_t { Miss, Normal, Nice, Perfect };
enum class Action : uint8_t { Attack, Dodge, Both };
enum class Result : uint8_t {
  None, Won, Lost, Retreated, Captured, Escaped, Released, MigrationRetired,
  // Terminal activity outcomes. They reuse the one-byte result slot and only
  // appear in schema-3 saves with a Ruins or DragonTrial encounter.
  RuinsClue, RuinsCleared,
  // DragonTrial: awaiting the stage-upgrade choice, and a replay win that owes
  // no throw or reward.
  DragonUpgrade, DragonCleared
};
enum class ClockChangeMode : uint8_t { Normal, TimezoneChange, RepairWrongDate };
enum class CommandResult : uint8_t {
  Applied, Duplicate, InvalidPhase, SavePending, StorageError, InvalidInput,
  NotReady, BufferFull, Deferred, NoBalls, TargetOwned, InsufficientProgress, Stale
};
struct ClockReading {
  bool trusted = false;
  int64_t utcSeconds = 0;
  int16_t offsetMinutes = 0;
  uint32_t dayKey = 0;
  uint32_t bootGeneration = 0;
  uint32_t monotonicMs = 0;
  uint32_t calibrationEpoch = 0;
};
struct Stamp {
  bool timeKnown = false;
  bool dayComplete = false;
  int64_t utcSeconds = 0;
  int16_t offsetMinutes = 0;
  uint32_t dayKey = 0;
  uint32_t bootGeneration = 0;
  uint32_t monotonicMs = 0;
  uint32_t calibrationEpoch = 0;
  uint64_t dailySteps = 0;
  uint64_t totalSteps = 0;
};
struct SpeciesProgress { uint64_t walked = 0, spent = 0; };
struct EvolveRequest { uint16_t from = 0, to = 0; uint64_t rosterRevision = 0; };
struct EvolutionReceipt { EvolveRequest request; uint64_t cost = 0; Stamp at; };
struct EvolutionOption { uint16_t target = 0; uint32_t cost = 0; CommandResult status = CommandResult::InvalidInput; };
struct EvolutionView {
  uint16_t from = 0; uint64_t revision = 0, walked = 0, available = 0;
  std::array<EvolutionOption, kEvolutionOptions> options; uint8_t count = 0;
};
struct Achievement { bool reached = false; Stamp first; };
struct Capture {
  uint16_t species = 0;
  Route route = Route::Forest;
  Stamp at;
  uint8_t achievementMask = 0;
};
struct DayBucket {
  uint32_t dayKey = 0;
  uint32_t epoch = 0;
  uint64_t steps = 0;
  bool complete = false;
  std::array<Achievement, 3> milestones;
};
struct ClockChangePending {
  bool active = false;
  uint64_t transactionId = 0;
  int64_t targetUtcSeconds = 0;
  int16_t targetOffsetMinutes = 0;
  uint32_t targetDayKey = 0;
  ClockChangeMode mode = ClockChangeMode::Normal;
};
struct ClockState {
  bool calibrated = false;
  bool trusted = false;
  bool anomaly = false;
  int16_t offsetMinutes = 0;
  uint32_t dayHighWater = 0;
  uint32_t calibrationEpoch = 0;
  ClockReading anchor;
  ClockChangePending pending;
};
struct AttemptKey {
  uint64_t encounterId;
  uint8_t attemptIndex;
  AttemptKey(uint64_t id = 0, uint8_t index = 0) : encounterId(id), attemptIndex(index) {}
};
struct ThrowAttempt {
  bool used = false;
  Timing timing = Timing::Miss;
  uint8_t probability = 0;
  uint8_t roll = 0;
  bool captured = false;
  uint16_t ruleVersion = kRuleVersion;
};
struct RuinsState {
  uint8_t clueMask = 0;
  bool unlocked = false;
  uint16_t lastTarget = 0;
};
struct DragonTrialState {
  bool active = false;
  uint8_t trialId = 0;
  uint8_t stageIndex = 0;
  uint8_t stageStartHp = 0;
  uint8_t normalBonus = 0;
  uint8_t perfectBonus = 0;
  uint8_t guard = 0;
};
struct Encounter {
  uint64_t id = 0;
  uint16_t species = 0;  // Final capture target; ordinary opponents equal species.
  uint16_t partner = 0;
  Route route = Route::Forest;
  uint32_t seed = 0;
  uint16_t ruleVersion = kRuleVersion;
  uint8_t usedBalls = 0;
  uint8_t legacyFreeAttempts = 0;
  std::array<ThrowAttempt, 2> attempts;
  uint16_t opponent = 0;  // 0 keeps the legacy default of species.
  EncounterOrigin origin = EncounterOrigin::Ordinary;
};
struct Save {
  std::array<uint8_t, kSpeciesBytes> registered = {{0}};
  std::array<SpeciesProgress, kSpeciesCount> progress;
  uint64_t rewardBaseSteps = 0, ballsSpent = 0, legacyChargedCount = 0;
  uint8_t voidedLegacyCount = 0;
  uint64_t rosterRevision = 0;
  EvolutionReceipt lastEvolution;
  uint16_t schema = kSchemaVersion;
  uint64_t sequence = 0;
  uint16_t starter = 0;
  uint16_t partner = 0;
  Route route = Route::Forest;
  std::array<uint8_t, kSpeciesBytes> owned = {{0}};
  std::array<uint32_t, kSpeciesCount> captureCounts = {{0}};
  std::array<Capture, kSpeciesCount> firstCapture;
  uint64_t totalSteps = 0;
  uint64_t explorationCredit = 0;
  uint64_t undatedSteps = 0;
  bool stepsIncomplete = false;
  DayBucket today;
  std::array<DayBucket, kHistoryCount> history;
  uint8_t historySize = 0;
  uint64_t bestDaySteps = 0;
  std::array<Capture, kRecentCount> recent;
  uint8_t recentSize = 0;
  std::array<Achievement, 3> dailyFirst;
  std::array<Achievement, 3> lifetime;
  ClockState clock;
  uint32_t bootGeneration = 0;
  uint64_t lastStepBatch = 0;
  uint64_t nextEncounterId = 1;
  Phase phase = Phase::Walking;
  Result result = Result::None;
  Encounter encounter;
  uint64_t activityEncounterCount = 0;
  RuinsState ruins;
  DragonTrialState dragonTrial;
};
struct StepEvent {
  uint16_t partner = 0;
  uint64_t batchId = 0;
  ClockReading clock;
};
bool registered(const Save&, uint16_t);
void setRegistered(Save&, uint16_t);
uint64_t availableBalls(const Save&);
bool owns(const Save&, uint16_t species);
void setOwned(Save&, uint16_t species);
bool validDayKey(uint32_t);
bool validReading(const ClockReading&);
bool consistentReading(const ClockReading& anchor, const ClockReading&, bool allowLate);
bool validate(const Save&);
uint32_t mixRandom(uint32_t);
uint8_t throwRoll(const Encounter&, uint8_t attemptIndex);
uint16_t encounterOpponent(const Encounter&);
uint8_t captureProbability(bool rare, Timing);
Stamp makeStamp(const Save&, const ClockReading&);
} }
