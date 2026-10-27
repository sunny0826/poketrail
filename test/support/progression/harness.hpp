#pragma once
#include "adventure.hpp"
#include "encounter_catalog.hpp"
#include "save_codec.hpp"
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <vector>
using namespace pokedex::adventure;
static unsigned checks = 0;
#define CHECK(x) do { ++checks; if (!(x)) { std::fprintf(stderr, "line %d: %s\n", __LINE__, #x); std::exit(1); } } while(0)
inline std::vector<uint8_t> encoded(const Save& s) {
  std::vector<uint8_t> b(codec::maxEncodedSize);size_t n=0;
  CHECK(codec::encode(s,b.data(),b.size(),n));b.resize(n);return b;
}
struct MemoryStore : Store {
  std::vector<uint8_t> disk,attempt; bool before=false,after=false;unsigned commits=0;
  LoadResult load(Save& out) override {
    if(disk.empty())return LoadResult(LoadStatus::Empty);
    return LoadResult(codec::decode(disk.data(),disk.size(),out)==codec::DecodeStatus::Ok?LoadStatus::Loaded:LoadStatus::Corrupt);
  }
  CommitStatus commit(const Save& s) override {
    ++commits;attempt=encoded(s);if(before)return CommitStatus::IoError;
    disk=attempt;return after?CommitStatus::IoError:CommitStatus::Committed;
  }
};
inline ClockReading clockFor(const Engine& e) {ClockReading r;r.bootGeneration=e.view().bootGeneration;return r;}
inline CommandResult steps(Engine& e,unsigned count,uint16_t partner) {
  for(unsigned i=0;i<count;++i){StepEvent x;x.batchId=e.view().lastStepBatch+1;x.clock=clockFor(e);x.partner=partner;
    const auto r=e.observeSteps(&x,1,123);if(r!=CommandResult::Applied)return r;}
  return CommandResult::Applied;
}
inline void total(Save& s,uint64_t n) {
  s.totalSteps=s.undatedSteps=s.explorationCredit=n;
  const uint64_t limits[]={10000,50000,100000};
  for(unsigned i=0;i<3;++i)if(n>=limits[i]){s.lifetime[i].reached=true;s.lifetime[i].first.bootGeneration=1;s.lifetime[i].first.totalSteps=limits[i];}
}
inline std::unique_ptr<Save> fixture(uint16_t partner,uint64_t n=0) {
  std::unique_ptr<Save> s(new Save);s->sequence=1;s->bootGeneration=1;s->starter=1;s->partner=partner;
  s->rosterRevision=1;s->encounterTarget=300;setRegistered(*s,1);setRegistered(*s,partner);setOwned(*s,partner);total(*s,n);
  CHECK(validate(*s));return s;
}
inline EvolveRequest request(const Engine& e,uint16_t from,uint16_t to) {
  EvolveRequest r;r.from=from;r.to=to;r.rosterRevision=e.view().rosterRevision;return r;
}
inline void win(Engine& e) {
  CHECK(e.startBattle(0)==CommandResult::Applied);
  while(e.view().phase==Phase::Battle){const uint32_t t=e.battleView().targetMs;
    CHECK(e.press(e.battleView().expected,t)==CommandResult::Applied);
    if(e.view().phase==Phase::Battle)CHECK(e.tick(t+181,clockFor(e))==CommandResult::Applied);}
  CHECK(e.view().phase==Phase::ThrowReady);
}
