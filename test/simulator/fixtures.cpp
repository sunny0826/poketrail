#include "driver.hpp"
#include "../../simulator/storage.hpp"
#include "save_codec.hpp"
#include <stdexcept>
#include <vector>

namespace simulator { namespace e2e {
void makeFixture(const std::string& name, const std::string& directory) {
  if (name == "blank") return;
  Save s; s.sequence=1; s.bootGeneration=1; s.encounterTarget=300;
  s.starter=s.partner=1; s.rosterRevision=1; setRegistered(s,1); setOwned(s,1);
  if (name == "throw" || name == "battle" || name == "legacy") {
    s.totalSteps=800; s.explorationSpent=300; s.explorationCredit=500; s.progress[0].walked=800;
    s.nextEncounterId=2; s.phase=name == "battle" ? Phase::EncounterReady : Phase::ThrowReady;
    s.result=name == "battle" ? Result::None : Result::Won;
    auto& e=s.encounter; e.id=1; e.species=72; e.partner=1; e.route=Route::Coast; e.seed=0x2468ace0u;
    if(name=="legacy") { e.ruleVersion=1; s.totalSteps=2000; s.explorationCredit=1700; s.progress[0].walked=2000; }
  } else if (name == "evolution" || name == "target-owned" || name == "edges") {
    s.totalSteps=7800; s.explorationCredit=7800; s.progress[0].walked=7600;
    setRegistered(s,2); s.progress[1].walked=200; setRegistered(s,4); setOwned(s,4);
    if(name=="target-owned") setOwned(s,2);
    if(name=="edges") for(unsigned id : {133u,439u,649u}) { setRegistered(s,id); setOwned(s,id); }
  } else if (name != "starter" && name != "clock") throw std::runtime_error("unknown fixture: " + name);
  if (!validate(s)) throw std::runtime_error("fixture semantic validation failed: " + name);
  std::vector<uint8_t> bytes(codec::maxEncodedSize); size_t length=0; Save decoded;
  if(!codec::encode(s,bytes.data(),bytes.size(),length) || codec::decode(bytes.data(),length,decoded)!=codec::DecodeStatus::Ok || !validate(decoded))
    throw std::runtime_error("fixture codec validation failed");
  Storage storage(directory); FileStore store(storage);
  if(store.commit(s)!=CommitStatus::Committed || store.load(decoded).status!=LoadStatus::Loaded || !validate(decoded))
    throw std::runtime_error("fixture FileStore verification failed");
}
} }
