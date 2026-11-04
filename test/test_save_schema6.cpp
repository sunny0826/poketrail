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
    CHECK(s.route==Route::Forest);
    CHECK(migrateLegacy(s)&&s.schema==8&&s.sequence==19&&s.route==Route::Kanto);
    for(uint16_t id=252;id<=649;++id)CHECK(!owns(s,id)&&!registered(s,id)&&!s.captureCounts[id-1]&&!s.progress[id-1].walked);
  }
}
static void capacityBoundaries() {
  for(uint16_t schema=1;schema<=8;++schema){
    Save good;const auto bytes=baseline(schema>=6?5:schema);
    CHECK(codec::decode(bytes.data(),bytes.size(),good)==codec::DecodeStatus::Ok);
    good.schema=schema;if(schema>=7)good.route=Route::Kanto;CHECK(validate(good));
    const auto cap=speciesCapacity(schema);
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
  }
  auto s=fixture(649,1000);s->progress[648].walked=1000;s->progress[648].spent=600;
  const auto b=encoded(*s);Save back;CHECK(codec::decode(b.data(),b.size(),back)==codec::DecodeStatus::Ok);
  CHECK(back.partner==649&&owns(back,649)&&registered(back,649)&&back.progress[648].walked==1000&&back.progress[648].spent==600);

}
static void upgradePhases() {
  for(Phase phase:{Phase::EncounterReady,Phase::Battle,Phase::ThrowReady})for(unsigned mode=0;mode<3;++mode){
    auto old=fixture(1,1000);old->schema=5;old->route=Route::Coast;old->progress[0].walked=1000;
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
    if(mode){CHECK(!e.ready());const auto frozen=store.attempt;
      store.before=store.after=false;CHECK(e.retrySave()==CommandResult::Applied&&store.disk==frozen);}
    CHECK(e.ready()&&e.view().schema==8&&e.view().route==Route::Kanto);
    CHECK(e.view().phase==(phase==Phase::Battle?Phase::EncounterReady:phase));
    CHECK(e.view().encounter.species==1&&e.view().encounter.partner==1&&e.view().encounter.id==1&&e.view().encounter.seed==12345);
    CHECK(old->encounter.route==Route::Forest&&e.view().encounter.route==Route::Kanto&&e.view().encounter.usedBalls==old->encounter.usedBalls&&e.view().encounter.ruleVersion==3);
    CHECK(e.view().progress[0].walked==1000&&e.view().progress[251].walked==0&&store.disk!=original);
  }
}
int main(){historicalBytes();capacityBoundaries();upgradePhases();std::printf("schema6: %u checks passed\n",checks);}
