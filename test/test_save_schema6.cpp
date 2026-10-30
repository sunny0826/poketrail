#include "support/progression/harness.hpp"
#include "adventure_migration.hpp"
#include <algorithm>
// Captured from c09d18d's 251-species codec before changing any implementation.
// Every unspecified byte is zero, including the CRC-covered empty accounts.
struct Byte { size_t offset; uint8_t value; };
static std::vector<uint8_t> baseline(unsigned schema) {
  const size_t lengths[] = {0,12139,14672,21498,21508,21508};
  std::vector<uint8_t> b(lengths[schema]);
  const Byte common[]={{0,65},{1,86},{2,68},{3,49},{6,19},{22,1},{27,1}};
  for(auto x:common)b[x.offset]=x.value;
  b[4]=uint8_t(schema);
  const Byte one[]={{14,85},{15,47},{18,50},{19,48},{20,65},{21,71},{24,1},{12083,1},{12095,1},{12122,1},{12130,1},{12137,1}};
  const Byte two[]={{14,58},{15,57},{18,10},{19,42},{20,34},{21,204},{24,1},{12083,1},{12095,1},{12122,3},{12130,3},{12137,3},{12139,1},{14599,1}};
  const Byte wide[]={{24,251},{58,4},{17296,1},{17308,1},{17335,3},{17343,3},{17350,3},{17352,1},{17383,4},{21425,1}};
  if(schema==1)for(auto x:one)b[x.offset]=x.value;
  if(schema==2)for(auto x:two)b[x.offset]=x.value;
  if(schema>=3){
    for(auto x:wide)b[x.offset]=x.value;
    b[14]=schema==3?228:238;b[15]=83;
    const uint8_t crc[][4]={{254,186,234,14},{218,53,78,120},{8,17,152,102}};
    for(unsigned i=0;i<4;++i)b[18+i]=crc[schema-3][i];
    if(schema>=4){b[21498]=44;b[21499]=1;}
  }
  return b;
}
static void historicalBytes() {
  for(unsigned schema=1;schema<=5;++schema){
    const auto b=baseline(schema);Save s;
    CHECK(codec::decode(b.data(),b.size(),s)==codec::DecodeStatus::Ok);
    CHECK(s.schema==schema&&encoded(s)==b);
    CHECK(s.partner==(schema>=3?251:1));
    CHECK(migrateLegacy(s)&&s.schema==6&&s.sequence==19);
    for(uint16_t id=252;id<=649;++id)CHECK(!owns(s,id)&&!registered(s,id)&&!s.captureCounts[id-1]&&!s.progress[id-1].walked);
    const auto migrated=encoded(s);CHECK(migrated.size()==48672);
  }
}
static void capacityBoundaries() {
  for(uint16_t schema=1;schema<=6;++schema){
    Save good;const auto bytes=baseline(schema==6?5:schema);
    CHECK(codec::decode(bytes.data(),bytes.size(),good)==codec::DecodeStatus::Ok);
    good.schema=schema;CHECK(validate(good));
    const auto cap=speciesCapacity(schema);CHECK(cap.bytes==(cap.count+7)/8);
    Save bad=good;bad.partner=uint16_t(cap.count+1);CHECK(!validate(bad));
    bad=good;bad.encounter.species=uint16_t(cap.count+1);CHECK(!validate(bad));
    bad=good;bad.encounter.partner=uint16_t(cap.count+1);CHECK(!validate(bad));
    bad=good;bad.owned[cap.bytes-1]|=0x80;CHECK(!validate(bad));
    bad=good;bad.registered[cap.bytes-1]|=0x80;CHECK(!validate(bad));
    bad=good;bad.lastEvolution.request.to=uint16_t(cap.count+1);CHECK(!validate(bad));
    if(schema<6){
      bad=good;bad.progress[cap.count].walked=1;CHECK(!validate(bad));
      bad=good;bad.firstCapture[cap.count].route=Route::Cave;CHECK(!validate(bad));
      bad=good;setOwned(bad,uint16_t(cap.count+1));CHECK(!validate(bad));
    }
    CHECK(savedRouteMask(uint16_t(cap.count+1),schema)==0);
  }
  CHECK(speciesCapacity(0).count==0&&speciesCapacity(7).count==0);
  auto s=fixture(649,1000);s->progress[648].walked=1000;s->progress[648].spent=600;
  const auto b=encoded(*s);Save back;CHECK(codec::decode(b.data(),b.size(),back)==codec::DecodeStatus::Ok);
  CHECK(back.partner==649&&owns(back,649)&&registered(back,649)&&back.progress[648].walked==1000&&back.progress[648].spent==600);
  CHECK(b.size()==48672);std::printf("schema6 encoded=%zu Save=%zu Engine=%zu\n",b.size(),sizeof(Save),sizeof(Engine));
}
static void highEvolution() {
  for(uint16_t target:{134,135,136,196,197,470,471}){
    auto s=fixture(133,11000);s->progress[132].walked=10500;
    setRegistered(*s,target);s->progress[target-1].walked=500;
    MemoryStore store;store.disk=encoded(*s);Engine e(store);CHECK(e.begin()==CommandResult::Applied);
    CHECK(e.evolutionView(133).count==7);
    CHECK(e.evolve(request(e,133,target),clockFor(e))==CommandResult::Applied);
    CHECK(e.view().partner==target&&!owns(e.view(),133)&&owns(e.view(),target));
    CHECK(e.view().progress[132].walked==10500&&e.view().progress[132].spent==10000&&e.view().progress[target-1].walked==500);
    Engine reboot(store);CHECK(reboot.begin()==CommandResult::Applied&&reboot.view().partner==target);
    auto both=fixture(133,10000);both->progress[132].walked=10000;setOwned(*both,target);setRegistered(*both,target);
    MemoryStore conflict;conflict.disk=encoded(*both);Engine c(conflict);CHECK(c.begin()==CommandResult::Applied);
    const auto before=encoded(c.view());CHECK(c.evolve(request(c,133,target),clockFor(c))==CommandResult::TargetOwned&&encoded(c.view())==before);
  }
  CHECK(evolutionRule(290,292)&&evolutionRule(290,292)->steps==10000);
}
static void upgradePhases() {
  for(Phase phase:{Phase::EncounterReady,Phase::Battle,Phase::ThrowReady})for(unsigned mode=0;mode<3;++mode){
    auto old=fixture(1,1000);old->schema=5;old->progress[0].walked=1000;
    old->phase=phase;old->result=phase==Phase::ThrowReady?Result::Won:Result::None;
    old->nextEncounterId=2;old->explorationSpent=300;old->explorationCredit=700;
    old->encounter.id=1;old->encounter.species=1;old->encounter.partner=1;old->encounter.seed=12345;
    if(phase==Phase::ThrowReady){
      auto& a=old->encounter.attempts[0];a.used=true;a.timing=Timing::Miss;
      a.roll=throwRoll(old->encounter,1,3);old->encounter.usedBalls=1;
    }
    MemoryStore store;store.disk=encoded(*old);const auto original=store.disk;
    store.before=mode==1;store.after=mode==2;Engine e(store);
    CHECK(e.begin()==(mode?CommandResult::StorageError:CommandResult::Applied));
    if(mode){CHECK(!e.ready()&&e.view().schema==6);const auto frozen=store.attempt;
      store.before=store.after=false;CHECK(e.retrySave()==CommandResult::Applied&&store.disk==frozen);}
    CHECK(e.ready()&&e.view().schema==6&&e.view().sequence==old->sequence+1);
    CHECK(e.view().phase==(phase==Phase::Battle?Phase::EncounterReady:phase));
    CHECK(e.view().encounter.species==1&&e.view().encounter.partner==1&&e.view().encounter.id==1&&e.view().encounter.seed==12345);
    CHECK(e.view().encounter.route==old->encounter.route&&e.view().encounter.usedBalls==old->encounter.usedBalls&&e.view().encounter.ruleVersion==3);
    CHECK(e.view().progress[0].walked==1000&&e.view().progress[251].walked==0&&store.disk!=original);
  }
}
int main(int argc,char**){historicalBytes();capacityBoundaries();upgradePhases();if(argc==1)highEvolution();std::printf("schema6: %u checks passed\n",checks);}
