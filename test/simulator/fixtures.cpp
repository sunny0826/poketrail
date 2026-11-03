#include "driver.hpp"
#include "../../simulator/storage.hpp"
#include "encounter_catalog.hpp"
#include "save_codec.hpp"
#include <stdexcept>
#include <vector>

namespace simulator { namespace e2e {
void makeFixture(const std::string& requested, const std::string& directory) {
  const bool english = requested.compare(0, 8, "english-") == 0;
  const std::string name = english ? requested.substr(8) : requested;
  if (name == "blank") return;
  if (name == "language-corrupt" || name == "language-future") {
    Storage storage(directory);
    std::vector<uint8_t> bytes(24, 0);
    if (name == "language-future") {
      bytes[0] = 'A'; bytes[1] = 'V'; bytes[2] = 'D'; bytes[3] = '1'; bytes[4] = 99;
    }
    if (!storage.write(0, bytes.data(), bytes.size()) || !storage.write(1, bytes.data(), bytes.size()))
      throw std::runtime_error("cannot write unavailable-language fixture");
    FILE* clock = std::fopen((directory + "/clock.txt").c_str(), "w");
    if (!clock) throw std::runtime_error("cannot write fixture clock");
    std::fputs("0\n", clock); std::fclose(clock); return;
  }
  Save s; s.sequence=1; s.bootGeneration=1; s.encounterTarget=300;
  s.starter=s.partner=1; s.rosterRevision=1; setRegistered(s,1); setOwned(s,1);
  if (name == "throw" || name == "battle" || name == "legacy") {
    s.totalSteps=800; s.explorationSpent=300; s.explorationCredit=500; s.progress[0].walked=800;
    s.nextEncounterId=2; s.phase=name == "battle" ? Phase::EncounterReady : Phase::ThrowReady;
    s.result=name == "battle" ? Result::None : Result::Won;
    auto& e=s.encounter; e.id=1; e.species=72; e.partner=1; e.route=Route::Kanto; e.seed=0x2468ace0u;
    if(name=="legacy") { e.ruleVersion=1; s.totalSteps=2000; s.explorationCredit=1700; s.progress[0].walked=2000; }
  } else if (name == "evolution" || name == "target-owned" || name == "edges") {
    s.totalSteps=7800; s.explorationCredit=7800; s.progress[0].walked=7600;
    setRegistered(s,2); s.progress[1].walked=200; setRegistered(s,4); setOwned(s,4);
    if(name=="target-owned") setOwned(s,2);
    if(name=="edges") for(unsigned id : {133u,439u,649u}) { setRegistered(s,id); setOwned(s,id); }
  } else if (name == "sinnoh-done") {
    unsigned captures=0;
    for(uint16_t id=387; id<=493; ++id) {
      if(!ordinaryEncounter(id)) continue;
      setRegistered(s,id); setOwned(s,id); s.captureCounts[id-1]=1;
      Capture& c=s.firstCapture[id-1]; c.species=id; c.route=Route::Sinnoh;
      c.at.bootGeneration=1; c.at.totalSteps=++captures*250;
    }
    s.explorationCredit=400; s.explorationSpent=captures*250;
    s.totalSteps=s.undatedSteps=s.explorationSpent+s.explorationCredit; s.progress[0].walked=s.totalSteps;
    s.nextEncounterId=captures+1; s.route=Route::Sinnoh;
  } else if (name != "starter" && name != "clock") throw std::runtime_error("unknown fixture: " + name);
  if (english) {
    s.language = pokedex::i18n::Language::English;
    if (name == "edges") for (uint16_t id : {115, 160}) { setRegistered(s, id); setOwned(s, id); }
  }
  if (!validate(s)) throw std::runtime_error("fixture semantic validation failed: " + name);
  std::vector<uint8_t> bytes(codec::maxEncodedSize); size_t length=0; Save decoded;
  if(!codec::encode(s,bytes.data(),bytes.size(),length) || codec::decode(bytes.data(),length,decoded)!=codec::DecodeStatus::Ok || !validate(decoded))
    throw std::runtime_error("fixture codec validation failed");
  Storage storage(directory); FileStore store(storage);
  if(store.commit(s)!=CommitStatus::Committed || store.load(decoded).status!=LoadStatus::Loaded || !validate(decoded))
    throw std::runtime_error("fixture FileStore verification failed");
}
} }
