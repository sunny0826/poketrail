#include "adventure_migration.hpp"
#include "encounter_catalog.hpp"
namespace pokedex { namespace adventure {
bool migrateLegacy(Save& s) {
  if (s.schema != 1) return s.schema == 2 && validate(s);
  if (!validate(s)) return false;
  s.schema = 2;
  s.registered = s.owned;
  for (size_t i = 0; i < kSpeciesCount; ++i) {
    if (s.captureCounts[i]) setRegistered(s, uint16_t(i + 1));
    s.progress[i] = SpeciesProgress();
  }
  s.rewardBaseSteps = s.totalSteps; s.ballsSpent = 0;
  s.legacyChargedCount = s.nextEncounterId - 1; s.voidedLegacyCount = 0;
  s.rosterRevision = s.starter ? 1 : 0;
  s.lastEvolution = EvolutionReceipt();
  if (s.phase == Phase::Walking) s.encounter = Encounter();
  else {
    s.encounter.legacyFreeAttempts = s.encounter.usedBalls;
    if (s.phase != Phase::Resolved && !ordinaryEncounter(s.encounter.species)) {
      --s.legacyChargedCount; s.voidedLegacyCount = 1;
      s.explorationCredit += 500;
      s.phase = Phase::Resolved; s.result = Result::MigrationRetired;
    } else if (s.phase == Phase::Battle) s.phase = Phase::EncounterReady;
  }
  return validate(s);
}
} }
