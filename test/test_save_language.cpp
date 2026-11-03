#include "support/progression/harness.hpp"
#include "adventure_migration.hpp"
#include <algorithm>
#include <fstream>
#include <iterator>
using pokedex::i18n::Language;

static void checksum(std::vector<uint8_t>& bytes) {
  std::vector<uint8_t> covered(bytes.begin(), bytes.begin() + 18);
  covered.insert(covered.end(), bytes.begin() + 22, bytes.end());
  const uint32_t crc = codec::crc32(covered.data(), covered.size());
  for (unsigned i = 0; i < 4; ++i) bytes[18 + i] = uint8_t(crc >> (8 * i));
}
static void historicalWidths() {
  const size_t sizes[] = {0, 12139, 14672, 21498, 21508, 21508, 48672, 48672, 48673};
  for (uint16_t schema = 1; schema <= 8; ++schema) {
    Save save; save.schema = schema; save.encounterTarget = schema >= 4 ? 300 : 0;
    if (schema < 7) save.route = Route::Forest;
    if (schema == 1) {
      save.encounter.ruleVersion = 1;
      for (auto& attempt : save.encounter.attempts) attempt.ruleVersion = 1;
    }
    const auto bytes = encoded(save);
    CHECK(bytes.size() == sizes[schema]);
    Save decoded; decoded.language = Language::English;
    CHECK(codec::decode(bytes.data(), bytes.size(), decoded) == codec::DecodeStatus::Ok);
    CHECK(decoded.schema == schema && decoded.language == Language::Chinese);
    CHECK(encoded(decoded) == bytes);
    save.language = Language::English;
    CHECK(validate(save) == (schema == 8));
    if (schema < 8) {
      size_t length = 99; std::vector<uint8_t> buffer(codec::maxEncodedSize);
      CHECK(!codec::encode(save, buffer.data(), buffer.size(), length) && length == 0);
    }
  }
}
static void tailAndSemanticValidation() {
  auto old = fixture(649, 1000); old->schema = 7; old->route = Route::Unova;
  old->progress[648].walked = 1000; old->progress[648].spent = 600;
  const auto before = encoded(*old);
  Save modern = *old; modern.schema = 8; modern.language = Language::English;
  const auto bytes = encoded(modern);
  CHECK(bytes.size() == before.size() + 1 && bytes.back() == 1);
  CHECK(std::equal(before.begin() + 22, before.end(), bytes.begin() + 22));
  Save decoded;
  CHECK(codec::decode(bytes.data(), bytes.size(), decoded) == codec::DecodeStatus::Ok);
  CHECK(decoded.language == Language::English && encoded(decoded) == bytes);
  for (uint8_t bad : {2, 127, 255}) {
    auto forged = bytes; forged.back() = bad; checksum(forged);
    CHECK(codec::decode(forged.data(), forged.size(), decoded) == codec::DecodeStatus::Corrupt);
    modern.language = Language(bad); CHECK(!validate(modern));
  }
  auto truncated = bytes; truncated.pop_back();
  truncated[14] = uint8_t(truncated.size() - 22);
  truncated[15] = uint8_t((truncated.size() - 22) >> 8); checksum(truncated);
  CHECK(codec::decode(truncated.data(), truncated.size(), decoded) == codec::DecodeStatus::Corrupt);
  auto future = bytes; future[4] = 9; checksum(future);
  CHECK(codec::decode(future.data(), future.size(), decoded) == codec::DecodeStatus::UnsupportedVersion);
  CHECK(speciesCapacity(8).count == 649 && speciesCapacity(9).count == 0);
}
static void frozenSchema7() {
  std::ifstream input("test/fixtures/simulator/schema7-progress.bin", std::ios::binary);
  CHECK(bool(input));
  const std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(input)), {});
  CHECK(bytes.size() == 48672);
  Save old; CHECK(codec::decode(bytes.data(), bytes.size(), old) == codec::DecodeStatus::Ok);
  CHECK(old.schema == 7 && old.language == Language::Chinese && encoded(old) == bytes);
  CHECK(old.progress[0].walked == 7600 && old.progress[1].walked == 200);
  Save migrated = old; CHECK(migrateLegacy(migrated));
  Save expected = old; expected.schema = 8;
  CHECK(encoded(migrated) == encoded(expected));
  for (Route region : {Route::Kanto, Route::Johto, Route::Hoenn, Route::Sinnoh, Route::Unova}) {
    Save regional = old; regional.route = region; expected = regional; expected.schema = 8;
    CHECK(migrateLegacy(regional) && encoded(regional) == encoded(expected));
  }
}
int main() {
  historicalWidths(); tailAndSemanticValidation(); frozenSchema7();
  std::printf("save language: %u checks passed\n", checks);
}
