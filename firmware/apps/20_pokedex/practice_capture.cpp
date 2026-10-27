#include "practice_capture.hpp"
#include "encounter_catalog.hpp"
namespace pokedex { namespace adventure {

void CapturePractice::begin(uint32_t nowMs, uint32_t seed) {
  const uint16_t species = uint16_t(1 + mixRandom(seed ^ 0x9e3779b9u) % kSpeciesCount);
  begin(species, nowMs, seed);
}

void CapturePractice::begin(uint16_t species, uint32_t nowMs, uint32_t seed) {
  if (!species || species > kSpeciesCount) return;
  battle_ = Battle();
  encounter_ = Encounter();
  encounter_.species = species;
  encounter_.seed = seed;
  encounter_.id = (uint64_t(mixRandom(seed)) << 32) | mixRandom(seed ^ 0x85ebca6bu);
  throw_ = PracticeThrowView();
  stage_ = PracticeStage::Battle;
  result_ = Result::None;
  usedBalls_ = 0;
  battle_.begin(nowMs, seed);
}

void CapturePractice::reset() {
  battle_ = Battle();
  encounter_ = Encounter();
  throw_ = PracticeThrowView();
  stage_ = PracticeStage::Idle;
  result_ = Result::None;
  usedBalls_ = 0;
}

void CapturePractice::settleBattle() {
  if (battle_.view().active) return;
  if (battle_.view().result == Result::Won) stage_ = PracticeStage::ThrowReady;
  else { result_ = Result::Lost; stage_ = PracticeStage::Result; }
}

PracticeOutcome CapturePractice::tick(uint32_t nowMs) {
  if (stage_ == PracticeStage::Battle) {
    battle_.tick(nowMs);
    settleBattle();
  } else if (stage_ == PracticeStage::Throwing && throw_.active &&
             timeAfter(nowMs, throw_.targetMs + 180)) {
    return resolve(throw_.targetMs + 181);
  }
  return PracticeOutcome();
}

bool CapturePractice::press(Action action, uint32_t nowMs) {
  if (stage_ != PracticeStage::Battle) return false;
  const bool accepted = battle_.press(action, nowMs);
  settleBattle();
  return accepted;
}

bool CapturePractice::requestPause() {
  return stage_ == PracticeStage::Battle && battle_.requestPause();
}

bool CapturePractice::resume(uint32_t nowMs) {
  return stage_ == PracticeStage::Battle && battle_.resume(nowMs);
}

bool CapturePractice::beginThrow(uint32_t nowMs) {
  if (stage_ != PracticeStage::ThrowReady || usedBalls_ >= kPracticeBalls) return false;
  throw_.active = true;
  throw_.attemptIndex = uint8_t(usedBalls_ + 1);
  throw_.targetMs = nowMs + 1500;
  stage_ = PracticeStage::Throwing;
  return true;
}

PracticeOutcome CapturePractice::pressThrow(uint32_t nowMs) {
  if (stage_ != PracticeStage::Throwing || !throw_.active) return PracticeOutcome();
  return resolve(nowMs);
}

PracticeOutcome CapturePractice::resolve(uint32_t pressMs) {
  const CatalogEntry* entry = catalogEntry(encounter_.species);
  if (!entry) return PracticeOutcome();
  PracticeOutcome out;
  out.valid = true;
  out.attemptIndex = throw_.attemptIndex;
  out.timing = throwTiming(pressMs, throw_.targetMs);
  out.probability = captureProbability(encounter_.ruleVersion, entry->rare, out.timing);
  out.roll = throwRoll(encounter_, out.attemptIndex, encounter_.ruleVersion);
  out.captured = out.roll < out.probability;
  out.at = pressMs;
  ThrowAttempt& attempt = encounter_.attempts[out.attemptIndex - 1];
  attempt.used = true;
  attempt.timing = out.timing;
  attempt.probability = out.probability;
  attempt.roll = out.roll;
  attempt.captured = out.captured;
  attempt.ruleVersion = encounter_.ruleVersion;
  usedBalls_ = out.attemptIndex;
  throw_.active = false;
  if (out.captured) { result_ = Result::Captured; stage_ = PracticeStage::Result; }
  else if (usedBalls_ >= kPracticeBalls) { result_ = Result::Escaped; stage_ = PracticeStage::Result; }
  else stage_ = PracticeStage::ThrowReady;
  return out;
}

} }
