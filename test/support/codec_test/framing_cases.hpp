#pragma once
#include "harness.hpp"

// Container framing, checksum, truncation and semantic rejects on a baseline save.
static void framingChecks(Save& s, std::vector<uint8_t>& data, Save& decoded) {
  s.sequence = 0x0102030405060708ULL; s.bootGeneration = 1;
  CHECK(validate(s)); const std::vector<uint8_t> original = encode(s);
  CHECK(original[0] == 0x41 && original[1] == 0x56 && original[2] == 0x44 && original[3] == 0x31);
  CHECK(original[6] == 8 && original[13] == 1);
  CHECK(codec::crc32(reinterpret_cast<const uint8_t*>("123456789"), 9) == 0xcbf43926u);
  CHECK(codec::decode(original.data(), original.size(), decoded) == codec::DecodeStatus::Ok);
  CHECK(encode(decoded) == original && decoded.sequence == s.sequence);
  for (size_t n = 0; n < original.size(); ++n)
    CHECK(codec::decode(original.data(), n, decoded) == codec::DecodeStatus::Corrupt);
  CHECK(codec::decode(nullptr, original.size(), decoded) == codec::DecodeStatus::Corrupt);
  data = original; data.push_back(0);
  CHECK(codec::decode(data.data(), data.size(), decoded) == codec::DecodeStatus::Corrupt);
  data.resize(codec::maxEncodedSize + 1);
  CHECK(codec::decode(data.data(), data.size(), decoded) == codec::DecodeStatus::Corrupt);
  data = original; data[4] = 4;
  CHECK(codec::decode(data.data(), data.size(), decoded) == codec::DecodeStatus::UnsupportedVersion);
  data.resize(codec::maxEncodedSize + 100);
  CHECK(codec::decode(data.data(), data.size(), decoded) == codec::DecodeStatus::UnsupportedVersion);
  for (size_t i = 0; i < original.size(); ++i) {
    if (i == 4 || i == 5) continue;
    data = original; data[i] ^= 0x80;
    CHECK(codec::decode(data.data(), data.size(), decoded) == codec::DecodeStatus::Corrupt);
  }
  CHECK(encode(decoded) == original); // all failures preserve the destination
  data = original; data[22 + 4] = 3; checksum(data);
  CHECK(codec::decode(data.data(), data.size(), decoded) == codec::DecodeStatus::Corrupt);
  Save toggled = s; toggled.stepsIncomplete = true; const auto incomplete = encode(toggled);
  data = original; data[firstPayloadDiff(original, incomplete)] = 2; checksum(data); // bool is 0/1
  CHECK(codec::decode(data.data(), data.size(), decoded) == codec::DecodeStatus::Corrupt);
  Save changed = s; changed.clock.pending.mode = ClockChangeMode::RepairWrongDate;
  const auto mode = encode(changed);
  data = original; data[firstPayloadDiff(original, mode)] = 3; checksum(data);
  CHECK(codec::decode(data.data(), data.size(), decoded) == codec::DecodeStatus::Corrupt);
  std::vector<uint8_t> small(original.size() - 1); size_t n = 12;
  CHECK(!codec::encode(s, small.data(), small.size(), n) && n == 0);
  CHECK(!codec::encode(s, nullptr, 0, n));
  Save bad = s; bad.route = Route(3); reject(bad);
  bad = s; bad.starter = 2; reject(bad);
  bad = s; bad.partner = 151; reject(bad);
  bad = s; bad.owned[31] = 0x08; reject(bad);
  bad = s; bad.registered[31] = 0x08; reject(bad);
  // Owned, walked and captured species must be registered. Start from a baseline
  // where #200 is registered (bit 199 = byte 24, mask 0x80) and clear only that bit.
  Save gated; gated.sequence = 3; gated.bootGeneration = 1; gated.starter = gated.partner = 1;
  gated.rosterRevision = 1; setRegistered(gated, 1); setOwned(gated, 1);
  setRegistered(gated, 200); gated.totalSteps = gated.undatedSteps = gated.explorationCredit = 5000;
  Save owned200 = gated; setOwned(owned200, 200); CHECK(validate(owned200));
  bad = owned200; bad.registered[24] &= uint8_t(~0x80); reject(bad);
  Save walked200 = gated; walked200.progress[199].walked = 1; CHECK(validate(walked200));
  bad = walked200; bad.registered[24] &= uint8_t(~0x80); reject(bad);
  Save caught200 = gated; caught200.captureCounts[199] = 1;
  caught200.firstCapture[199].species = 200;
  caught200.firstCapture[199].at.bootGeneration = 1; caught200.firstCapture[199].at.totalSteps = 500;
  caught200.recent[0] = caught200.firstCapture[199]; caught200.recentSize = 1;
  caught200.nextEncounterId = 2; caught200.explorationCredit = 0;
  CHECK(validate(caught200));
  bad = caught200; bad.registered[24] &= uint8_t(~0x80); reject(bad);
  bad = s; bad.historySize = 15; reject(bad);
  bad = s; bad.recentSize = 33; reject(bad);
  bad = s; bad.phase = Phase::ThrowReady; reject(bad);
  bad = s; bad.encounter.usedBalls = 3; reject(bad);
  bad = s; bad.captureCounts[0] = 1; reject(bad);
  bad = s; bad.clock.pending.active = true; reject(bad);
  bad = s; bad.clock.trusted = true; reject(bad);
  bad = s; bad.today.dayKey = 20260229; reject(bad);
  bad = s; bad.clock.offsetMinutes = 841; reject(bad);
  bad = s; bad.explorationCredit = 1; reject(bad);
  bad = s; bad.firstCapture[0].at.bootGeneration = 2; reject(bad);
  bad = s; bad.nextEncounterId = 2; reject(bad);
  bad = s; bad.clock.anchor.bootGeneration = 2; reject(bad);
  bad = s; bad.clock.pending.transactionId = 1; reject(bad);
  ClockReading extreme; extreme.trusted = true; extreme.dayKey = 20261022; extreme.offsetMinutes = 840;
  extreme.utcSeconds = INT64_MAX; CHECK(!validReading(extreme));
  extreme.utcSeconds = INT64_MIN; extreme.offsetMinutes = -720; CHECK(!validReading(extreme));
  bad = s; bad.clock.pending.active = true; bad.clock.pending.transactionId = 1;
  bad.clock.pending.targetDayKey = 20261022; bad.clock.pending.targetOffsetMinutes = 840;
  bad.clock.pending.targetUtcSeconds = INT64_MAX; reject(bad);
  s.clock.offsetMinutes = -720;
  data = encode(s); CHECK(codec::decode(data.data(), data.size(), decoded) == codec::DecodeStatus::Ok);
  CHECK(decoded.clock.offsetMinutes == -720 && encode(decoded) == data);
}
