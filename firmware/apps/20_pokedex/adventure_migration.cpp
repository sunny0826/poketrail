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
  if (s.schema < 2 || s.schema > 9 || !validate(s)) return false;
  if (s.schema == 2) {
    for (size_t i = 151; i < speciesCapacity(3).count; ++i) {
      s.progress[i] = SpeciesProgress();
      s.captureCounts[i] = 0;
      s.firstCapture[i] = Capture();
    }
    s.schema = 3;
    if (!validate(s)) return false;
  }
  if (s.schema == 3) upgradeToSchema4(s);
  if (s.schema == 4) upgradeToSchema5(s);
  if (s.schema == 5) {
    if (!validate(s)) return false;
    s.schema = 6;
  }
  if (s.schema == 6) {
    if (!validate(s)) return false;
    s.route = Route::Kanto;
    if (s.phase != Phase::Walking && s.encounter.id) s.encounter.route = regionOf(s.encounter.species);
    s.schema = 7;
  }
  if (s.schema == 7) {
    if (!validate(s)) return false;
    s.language = i18n::Language::Chinese;
    s.schema = 8;
  }
  if (s.schema == 8) {
    if (!validate(s)) return false;
    s.schema = 9;
  }
  if (s.schema == 9) {
    if (!validate(s)) return false;
    s.schema = 10;
  }
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
