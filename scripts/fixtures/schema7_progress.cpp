#include "adventure_model.hpp"
#include "save_codec.hpp"
#include <cstdio>
#include <vector>

int main(int argc, char** argv) {
  using namespace pokedex::adventure;
  if (argc != 2) return 1;
  Save s;
  s.sequence = 1;
  s.bootGeneration = 1;
  s.encounterTarget = 300;
  s.starter = s.partner = 1;
  s.rosterRevision = 1;
  setRegistered(s, 1); setOwned(s, 1);
  s.totalSteps = s.explorationCredit = 7800;
  s.progress[0].walked = 7600;
  setRegistered(s, 2); s.progress[1].walked = 200;
  setRegistered(s, 4); setOwned(s, 4);
  if (s.schema != 7 || !validate(s)) return 2;
  std::vector<uint8_t> bytes(codec::maxEncodedSize);
  size_t length = 0;
  if (!codec::encode(s, bytes.data(), bytes.size(), length)) return 3;
  FILE* file = std::fopen(argv[1], "wb");
  if (!file) return 4;
  const bool written = std::fwrite(bytes.data(), 1, length, file) == length;
  return std::fclose(file) == 0 && written ? 0 : 5;
}
