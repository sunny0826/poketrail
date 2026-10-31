#include "driver.hpp"
#include <cstdio>
#include <exception>
#include <map>

int main(int argc,char** argv) {
  using namespace simulator::e2e;
  try {
    if(argc==4 && std::string(argv[1])=="--fixture") { makeFixture(argv[2],argv[3]); return 0; }
    if(argc!=7) throw std::runtime_error("usage: e2e SAVE OUTPUT SCENARIO PHASE SEED UTC");
    Driver d(argv[1],argv[2],std::stoul(argv[5]),std::stoll(argv[6]));
    try {
      const std::string name=argv[3],phase=argv[4];
      using Scenario=void(*)(Driver&,const std::string&);
      const std::map<std::string,Scenario> scenarios={
        {"journey",scenarioJourney},{"capture",scenarioCapture},{"throws",scenarioThrows},
        {"retry",scenarioRetry},{"progression",scenarioProgression},{"conflict",scenarioConflict},
        {"isolation",scenarioIsolation},{"clock",scenarioClock},{"lifecycle",scenarioLifecycle},
        {"edges",scenarioEdges},{"crash",scenarioCrash},{"migration",scenarioMigration}};
      if(name=="load") {
        if(phase=="error") d.check(!d.runtime().engine().ready() && d.screen().page==Page::Error,"bad save rejected");
        else d.check(d.runtime().engine().ready() && d.save().starter!=0,"valid save loaded");
        d.state("loaded"); d.shot("loaded"); d.close();
      } else {
        const auto it=scenarios.find(name); d.check(it!=scenarios.end(),"scenario registered"); it->second(d,phase);
      }
      d.state("final"); d.event("complete","pass");
    } catch(const std::exception& e) {
      d.event("failure",e.what());
      try { d.state("failure"); d.shot("failure"); } catch(...) {}
      throw;
    }
    return 0;
  } catch(const std::exception& e) { std::fprintf(stderr,"E2E failed: %s\n",e.what()); return 1; }
}
