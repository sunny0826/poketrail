#include "support/progression/harness.hpp"
#include "file_store.hpp"
#include <algorithm>
#include <fstream>
#include <iterator>
struct Slots : SlotIO {
  std::vector<uint8_t> files[2], attempt;bool before=false,after=false,readback=false;size_t cut=SIZE_MAX;unsigned writes=0;
  LoadStatus prepare() override{return LoadStatus::Loaded;}
  ReadStatus read(unsigned slot,uint8_t* out,size_t cap,size_t& n) override {
    if(readback && writes){readback=false;return ReadStatus::IoError;}
    n=files[slot].size();if(!n)return ReadStatus::Missing;
    std::copy(files[slot].begin(),files[slot].begin()+std::min(cap,n),out);
    return n>cap?ReadStatus::TooLarge:ReadStatus::Ok;
  }
  bool write(unsigned slot,const uint8_t* b,size_t n) override {
    ++writes;attempt.assign(b,b+n);if(before)return false;files[slot].assign(b,b+std::min(cut,n));return !after&&cut>=n;
  }
};
static std::vector<uint8_t> legacy(unsigned kind) {
  char path[128];std::snprintf(path,sizeof(path),"test/fixtures/progression/v1-%u.bin",kind);
  std::ifstream file(path,std::ios::binary);CHECK(bool(file));
  return std::vector<uint8_t>(std::istreambuf_iterator<char>(file),{});
}
static std::unique_ptr<Save> decoded(const std::vector<uint8_t>& bytes) {
  std::unique_ptr<Save> s(new Save);CHECK(codec::decode(bytes.data(),bytes.size(),*s)==codec::DecodeStatus::Ok);return s;
}
static void phases() {
  for(unsigned kind=0;kind<9;++kind){
    Slots slots;slots.files[0]=legacy(kind);const auto original=slots.files[0];auto old=decoded(original);
    CHECK(old->schema==1&&encoded(*old)==original&&original.size()==12139);
    FileStore store(slots);Engine e(store);CHECK(e.begin()==CommandResult::Applied&&e.ready());const auto& s=e.view();
    CHECK(s.schema==4&&s.sequence==11&&s.bootGeneration==2&&slots.files[0]==original&&slots.writes==1);
    CHECK(s.rewardBaseSteps==500&&availableBalls(s)==0&&s.captureCounts==old->captureCounts&&s.owned==old->owned);
    CHECK(s.explorationSpent==s.totalSteps-s.explorationCredit&&s.encounterTarget>=kEncounterMinSteps&&s.encounterTarget<=kEncounterMaxSteps);
    CHECK(s.progress[0].walked==0&&registered(s,1)&&s.rosterRevision==1);
    CHECK(s.voidedLegacyCount==(kind==5||kind==6?1:0)&&s.legacyChargedCount==(kind==5||kind==6?0:1));
    CHECK(s.explorationCredit==(kind==5||kind==6?500:0));
    if(kind==5||kind==6)CHECK(s.phase==Phase::Resolved&&s.result==Result::MigrationRetired&&s.encounter.id==1);
    else if(kind==7)CHECK(s.phase==Phase::Walking&&s.encounter.id==0&&s.encounter.ruleVersion==kRuleVersion);
    else CHECK(s.encounter.ruleVersion==1&&s.encounter.id==1&&s.encounter.seed==old->encounter.seed);
    if(kind==1)CHECK(s.phase==Phase::EncounterReady);
    if(kind==3)CHECK(s.encounter.legacyFreeAttempts==1&&s.encounter.attempts[0].roll==old->encounter.attempts[0].roll);
    if(kind==4)CHECK(s.firstCapture[0].at.totalSteps==500&&s.result==Result::Captured);
    if(kind==8)CHECK(s.clock.pending.active&&s.clock.pending.transactionId==1);
    FileStore second(slots);Engine reboot(second);CHECK(reboot.begin()==CommandResult::Applied);
    CHECK(reboot.view().rewardBaseSteps==500&&reboot.view().voidedLegacyCount==s.voidedLegacyCount&&reboot.view().explorationCredit==s.explorationCredit);
  }
}
static void failures() {
  for(unsigned mode=0;mode<4;++mode){
    Slots slots;slots.files[0]=legacy(5);const auto original=slots.files[0];
    slots.before=mode==0;slots.cut=mode==1?100:SIZE_MAX;slots.after=mode==2;slots.readback=mode==3;
    FileStore store(slots);Engine e(store);CHECK(e.begin()==CommandResult::StorageError&&!e.ready()&&e.savePending());
    CHECK(slots.files[0]==original);const auto candidate=slots.attempt;
    CHECK(e.begin()==CommandResult::SavePending);
    slots.before=slots.after=slots.readback=false;slots.cut=SIZE_MAX;
    CHECK(e.retrySave()==CommandResult::Applied&&e.ready()&&e.view().sequence==11&&e.view().explorationCredit==500);
    CHECK(candidate==slots.files[1]);
    CHECK(slots.files[0]==original);
  }
  for(bool complete:{false,true}){
    Slots slots;slots.files[0]=legacy(6);slots.after=complete;slots.cut=complete?SIZE_MAX:100;
    {FileStore store(slots);Engine e(store);CHECK(e.begin()==CommandResult::StorageError);}
    slots.after=false;slots.cut=SIZE_MAX;FileStore store(slots);Engine e(store);CHECK(e.begin()==CommandResult::Applied);
    CHECK(e.view().schema==4&&e.view().explorationCredit==500&&e.view().voidedLegacyCount==1);
    CHECK(e.view().sequence==(complete?12:11));
  }
}
static void oldThrowsAndBase() {
  MemoryStore store;store.disk=legacy(3);Engine e(store);CHECK(e.begin()==CommandResult::Applied);
  const AttemptKey second(1,2);CHECK(e.beginThrow(second,0)==CommandResult::NoBalls);
  CHECK(steps(e,1000,1)==CommandResult::Applied&&availableBalls(e.view())==1);
  CHECK(e.beginThrow(second,0)==CommandResult::Applied&&e.view().ballsSpent==0);
  CHECK(e.tick(e.throwView().targetMs+181,clockFor(e))==CommandResult::Applied);
  CHECK(e.view().ballsSpent==1&&e.view().encounter.legacyFreeAttempts==1&&e.view().encounter.usedBalls==2&&e.view().result==Result::Escaped);
  CHECK(e.view().encounter.attempts[1].ruleVersion==1&&e.view().encounter.attempts[1].roll==throwRoll(e.view().encounter,2,1));
  CHECK(steps(e,1000,1)==CommandResult::Applied&&availableBalls(e.view())==1);
  Engine reboot(store);CHECK(reboot.begin()==CommandResult::Applied);
  CHECK(reboot.view().rewardBaseSteps==500&&availableBalls(reboot.view())==1&&reboot.view().progress[0].walked==2000);
  MemoryStore empty;empty.disk=legacy(2);Engine zero(empty);CHECK(zero.begin()==CommandResult::Applied);
  CHECK(zero.giveUpCapture(1)==CommandResult::Applied&&zero.view().result==Result::Released&&zero.view().ballsSpent==0);
}
static void creditAndBoundary() {
  for(unsigned count:{1u,20u})for(bool retire:{false,true}){
    if(count==20&&retire)continue;
    auto s=decoded(legacy(retire?5:0));total(*s,10000);s->nextEncounterId=count+1;s->encounter.id=count;
    s->explorationCredit=10000-count*500;
    MemoryStore store;store.disk=encoded(*s);Engine e(store);CHECK(e.begin()==CommandResult::Applied);
    CHECK(e.view().explorationCredit==10000-(count-(retire?1:0))*500);
    if(!retire)CHECK(e.retreat()==CommandResult::Applied);
    CHECK(e.acknowledgeResult(count,17)==CommandResult::Applied);
    if(count==20)CHECK(e.view().phase==Phase::Walking&&e.view().explorationCredit==0);
    else {
      CHECK(e.view().encounter.id==2&&e.view().encounter.ruleVersion==3&&e.view().encounter.legacyFreeAttempts==0);
      // The upgraded save consumed a random encounter target from the banked credit.
      CHECK(e.view().explorationSpent+e.view().explorationCredit==e.view().totalSteps);
      CHECK(e.view().encounterTarget>=kEncounterMinSteps&&e.view().encounterTarget<=kEncounterMaxSteps);
      CHECK(e.view().voidedLegacyCount==(retire?1:0));
      CHECK(validate(e.view()));
      auto corrupt=std::unique_ptr<Save>(new Save(e.view()));corrupt->encounter.ruleVersion=1;CHECK(!validate(*corrupt));
      corrupt.reset(new Save(e.view()));corrupt->voidedLegacyCount=2;CHECK(!validate(*corrupt));
    }
  }
  auto owned=decoded(legacy(4));owned->firstCapture[0]=Capture();owned->captureCounts[0]=0;
  owned->encounter.species=2;owned->encounter.seed=0;while(throwRoll(owned->encounter,1,1)>=85)++owned->encounter.seed;
  owned->encounter.attempts[0].roll=throwRoll(owned->encounter,1,1);owned->captureCounts[1]=1;setOwned(*owned,2);
  Capture c;c.species=2;c.at.bootGeneration=1;c.at.totalSteps=500;owned->firstCapture[1]=owned->recent[0]=c;
  MemoryStore store;store.disk=encoded(*owned);Engine e(store);CHECK(e.begin()==CommandResult::Applied);
  CHECK(e.view().result==Result::Captured&&owns(e.view(),2)&&registered(e.view(),2)&&e.view().voidedLegacyCount==0);
}
static std::vector<uint8_t> version2(uint16_t partner, uint64_t steps) {
  Save s; s.schema=2; s.sequence=5; s.bootGeneration=1; s.starter=1; s.partner=partner;
  s.rosterRevision=1; setRegistered(s,1); setRegistered(s,partner); setOwned(s,partner);
  total(s,steps); s.rewardBaseSteps=0; s.ballsSpent=0; s.legacyChargedCount=0; s.voidedLegacyCount=0;
  s.explorationCredit=steps; s.nextEncounterId=1; s.progress[partner-1].walked=steps;
  CHECK(validate(s)); return encoded(s);
}
static void schema2Upgrade() {
  const auto bytes=version2(4,1000);auto old=decoded(bytes);
  CHECK(old->schema==2&&old->progress[3].walked==1000);
  Slots slots;slots.files[0]=bytes;FileStore store(slots);Engine e(store);CHECK(e.begin()==CommandResult::Applied);
  const auto& s=e.view();
  CHECK(s.schema==4&&s.sequence==6&&slots.writes==1&&slots.files[0]==bytes);
  CHECK(s.partner==4&&s.progress[3].walked==1000&&s.totalSteps==1000&&s.explorationCredit==1000);
  CHECK(s.explorationSpent==0&&s.encounterTarget>=kEncounterMinSteps&&s.encounterTarget<=kEncounterMaxSteps);
  CHECK(s.rewardBaseSteps==0&&s.legacyChargedCount==0&&s.rosterRevision==1);
  for(uint16_t id=152;id<=251;++id)CHECK(!registered(s,id)&&!owns(s,id)&&!s.progress[id-1].walked&&!s.captureCounts[id-1]);
  FileStore second(slots);Engine reboot(second);CHECK(reboot.begin()==CommandResult::Applied&&reboot.view().schema==4);
  CHECK(reboot.view().progress[3].walked==1000&&slots.writes==2);
  Save enc;CHECK(codec::decode(slots.files[0].data(),slots.files[0].size(),enc)==codec::DecodeStatus::Ok);
  CHECK(enc.schema==4&&validate(enc));
}
static void mixedSlots() {
  Slots slots;slots.files[0]=legacy(0);FileStore first(slots);Engine e(first);CHECK(e.begin()==CommandResult::Applied);
  const auto v2=slots.files[1];auto s=decoded(v2);s->sequence=10;slots.files[1]=encoded(*s);
  FileStore conflict(slots);std::unique_ptr<Save> out(new Save);CHECK(conflict.load(*out).status==LoadStatus::Corrupt);
  slots.files[1]=v2;slots.files[1][4]=5;FileStore unknown(slots);CHECK(unknown.load(*out).status==LoadStatus::UnsupportedVersion);
  slots.files[1]=v2;slots.files[1].back()^=1;FileStore corrupt(slots);
  CHECK(corrupt.load(*out).status==LoadStatus::Loaded&&out->schema==1&&out->sequence==10);
  slots.files[0]=legacy(0);slots.files[1]=v2;FileStore chosen(slots);CHECK(chosen.load(*out).status==LoadStatus::Loaded&&out->schema==4);
}
static void activityTail() {
  // A real frame written by the unmerged activity-branch firmware: an ordinary
  // schema-3 payload plus a 22-byte activity block, mid-activity-encounter
  // (species 251, ThrowReady) with ids that consumed no exploration credit.
  char path[128]; std::snprintf(path, sizeof(path), "test/fixtures/progression/schema3-activity.bin");
  std::ifstream file(path, std::ios::binary); CHECK(bool(file));
  const std::vector<uint8_t> frame((std::istreambuf_iterator<char>(file)), {});
  CHECK(frame.size() == 21520);
  Save s; CHECK(codec::decode(frame.data(), frame.size(), s) == codec::DecodeStatus::Ok);
  CHECK(s.schema == 4 && s.sequence == 8555 && s.totalSteps == 6293 && s.explorationCredit == 1293);
  CHECK(s.explorationSpent == 5000 && s.explorationSpent + s.explorationCredit == s.totalSteps);
  CHECK(s.phase == Phase::Walking && !s.encounter.id && s.result == Result::None);
  CHECK(s.partner == 1 && s.progress[0].walked == 6281 && s.encounterTarget >= kEncounterMinSteps);
  CHECK(s.nextEncounterId == 14 && s.ballsSpent == 6 && validate(s));
  MemoryStore store; store.disk = frame; Engine e(store);
  CHECK(e.begin() == CommandResult::Applied && e.ready());
  CHECK(e.view().schema == 4 && e.view().totalSteps == 6293 && e.view().partner == 1);
  CHECK(e.view().phase == Phase::Walking && e.view().explorationCredit == 1293);
  // The zero-cost activity ids stay inside schema-4 bookkeeping bounds.
  CHECK(e.view().explorationSpent + e.view().explorationCredit == e.view().totalSteps);
}
int main(){phases();failures();oldThrowsAndBase();creditAndBoundary();schema2Upgrade();mixedSlots();activityTail();std::printf("migration: %u checks passed\n",checks);}
