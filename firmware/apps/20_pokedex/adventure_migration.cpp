#include "adventure_migration.hpp"
#include "encounter_catalog.hpp"
namespace pokedex { namespace adventure {
bool migrateLegacy(Save& s) {
  if (s.schema == kSchemaVersion) return validate(s);
  if (s.schema == 1) {
    if (!validate(s)) return false;
    s.schema = 2;
    s.registered = s.owned;
    for (size_t i = 0; i < 151; ++i) {
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
    if (!validate(s)) return false;
  }
  if (s.schema != 2 && s.schema != 3 && s.schema != 4) return false;
  // Schema 3 widens the catalog. Existing fields and the current encounter stay
  // verbatim; species 152-251 start empty and unregistered.
  if (s.schema == 2) {
    for (size_t i = 151; i < kSpeciesCount; ++i) {
      s.progress[i] = SpeciesProgress();
      s.captureCounts[i] = 0;
      s.firstCapture[i] = Capture();
    }
    s.schema = 3;
    if (!validate(s)) return false;
  }
  // Schema 4 switches to random encounter cadence and unlimited balls. The
  // modern encounter account is reconstructed from the invariant it must keep:
  // spent + credit == totalSteps. Any non-legacy encounter still in play is
  // upgraded to the current rules; finished attempts keep their own version so
  // their stored probability and roll stay verifiable.
  if (s.schema == 3) upgradeToSchema4(s);
  // Schema 5 re-derives route eligibility from species types. A schema-4 save was
  // validated against the frozen masks, so an in-play encounter may sit on a
  // route it no longer belongs to; only that route is repaired.
  if (s.schema == 4) upgradeToSchema5(s);
  return validate(s);
}
void upgradeToSchema4(Save& s) {
  s.explorationSpent = s.totalSteps - s.explorationCredit;
  if (s.encounter.id && s.encounter.ruleVersion == 2) s.encounter.ruleVersion = 3;
  s.encounterTarget = nextEncounterTarget(mixRandom(uint32_t(s.bootGeneration) ^ uint32_t(s.totalSteps)));
  s.schema = 4;
}
void upgradeToSchema5(Save& s) {
  const CatalogEntry* entry = catalogEntry(s.encounter.species);
  if (s.phase != Phase::Walking && s.encounter.id && entry &&
      !(entry->routes & (1u << uint8_t(s.encounter.route)))) {
    for (uint8_t route = 0; route < 3; ++route) {
      if (entry->routes & (1u << route)) {
        s.encounter.route = Route(route);
        break;
      }
    }
  }
  s.schema = 5;
}
} }
