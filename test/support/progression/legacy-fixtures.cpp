#include "save_codec.hpp"
#include "encounter_catalog.hpp"
#include <memory>
#include <cstdio>
#include <cstdlib>
using namespace pokedex::adventure;
int main() {
  for (unsigned kind=0;kind<9;++kind) {
    std::unique_ptr<Save> s(new Save); s->schema=1; s->sequence=10; s->bootGeneration=1;
    s->starter=s->partner=1; setOwned(*s,1);
    s->totalSteps=s->undatedSteps=500; s->nextEncounterId=2;
    Encounter& e=s->encounter; e.id=1;e.partner=1;e.species=kind==5?2:kind==6?147:1;e.ruleVersion=1;
    const auto* entry=catalogEntry(e.species);
    for(unsigned route=0;route<3;++route) if(entry->routes&(1u<<route)){e.route=Route(route);break;}
    s->phase=Phase::EncounterReady;
    if(kind==1)s->phase=Phase::Battle;
    if(kind==2||kind==3){s->phase=Phase::ThrowReady;s->result=Result::Won;}
    if(kind==3){e.usedBalls=1;e.attempts[0].used=true;e.attempts[0].ruleVersion=1;e.attempts[0].roll=throwRoll(e,1);}
    if(kind==4){s->phase=Phase::Resolved;s->result=Result::Captured;
      while(throwRoll(e,1)>=85)++e.seed;
      e.usedBalls=1;auto& a=e.attempts[0];a.used=true;a.ruleVersion=1;a.timing=Timing::Perfect;a.probability=85;a.roll=throwRoll(e,1);a.captured=true;
      s->captureCounts[0]=1;auto& c=s->firstCapture[0];c.species=1;c.at.bootGeneration=1;c.at.totalSteps=500;
      s->recentSize=1;s->recent[0]=c;
    }
    if(kind==7){s->phase=Phase::Walking;s->encounter=Encounter();}
    if(kind==8){s->clock.pending.active=true;s->clock.pending.transactionId=1;s->clock.pending.targetUtcSeconds=1792627200;s->clock.pending.targetDayKey=20261022;}
    uint8_t out[codec::maxEncodedSize];size_t n=0;if(!codec::encode(*s,out,sizeof(out),n))return 2;
    char name[128];std::snprintf(name,sizeof(name),"test/fixtures/progression/v1-%u.bin",kind);
    FILE* f=std::fopen(name,"wb");if(!f)return 3;std::fwrite(out,1,n,f);std::fclose(f);
  }
}
