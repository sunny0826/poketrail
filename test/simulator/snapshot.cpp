#include "driver.hpp"
#include <fstream>
#include <stdexcept>

namespace simulator { namespace e2e {
void Driver::state(const std::string& name) {
  const auto& s = save();
  std::ofstream o(output_ + "/" + name + ".json");
  o << "{\"at\":" << now() << ",\"page\":" << int(screen().page)
    << ",\"phase\":" << int(s.phase) << ",\"result\":" << int(s.result)
    << ",\"sequence\":" << s.sequence << ",\"bootGeneration\":" << s.bootGeneration
    << ",\"starter\":" << s.starter << ",\"partner\":" << s.partner
    << ",\"totalSteps\":" << s.totalSteps << ",\"explorationCredit\":" << s.explorationCredit
    << ",\"explorationSpent\":" << s.explorationSpent << ",\"encounterId\":" << s.encounter.id
    << ",\"species\":" << s.encounter.species << ",\"seed\":" << s.encounter.seed
    << ",\"usedBalls\":" << unsigned(s.encounter.usedBalls) << ",\"ballsSpent\":" << s.ballsSpent
    << ",\"rosterRevision\":" << s.rosterRevision << ",\"recentSize\":" << unsigned(s.recentSize)
    << ",\"undatedSteps\":" << s.undatedSteps << ",\"today\":{\"day\":" << s.today.dayKey << ",\"steps\":" << s.today.steps << ",\"epoch\":" << s.today.epoch << "}"
    << ",\"rtcUtc\":" << rtcUtc()
    << ",\"clock\":{\"utc\":" << s.clock.anchor.utcSeconds << ",\"offset\":" << s.clock.offsetMinutes
    << ",\"trusted\":" << s.clock.trusted << ",\"epoch\":" << s.clock.calibrationEpoch << "}"
    << ",\"evolution\":{\"from\":" << s.lastEvolution.request.from << ",\"to\":" << s.lastEvolution.request.to << ",\"cost\":" << s.lastEvolution.cost << "}"
    << ",\"attempts\":[";
  for (unsigned i=0; i<2; ++i) {
    const auto& a=s.encounter.attempts[i]; if(i) o << ',';
    o << "{\"used\":" << a.used << ",\"timing\":" << int(a.timing) << ",\"roll\":" << unsigned(a.roll)
      << ",\"probability\":" << unsigned(a.probability) << ",\"captured\":" << a.captured << "}";
  }
  o << "],\"history\":[";
  for (unsigned i=0; i<s.historySize; ++i) {
    if(i) o << ',';
    o << "{\"day\":" << s.history[i].dayKey << ",\"steps\":" << s.history[i].steps
      << ",\"epoch\":" << s.history[i].epoch << "}";
  }
  o << "],\"accounts\":["; bool comma=false;
  for (unsigned id=1; id<=kSpeciesCount; ++id) if (registered(s,id)) {
    if(comma) o << ','; comma=true;
    o << "{\"id\":" << id << ",\"owned\":" << owns(s,id) << ",\"captures\":" << s.captureCounts[id-1]
      << ",\"walked\":" << s.progress[id-1].walked << ",\"spent\":" << s.progress[id-1].spent
      << ",\"firstKnown\":" << s.firstCapture[id-1].at.timeKnown << ",\"firstUtc\":" << s.firstCapture[id-1].at.utcSeconds << "}";
  }
  o << "]}\n";
  if (!o) throw std::runtime_error("snapshot write failed");
  event("state", name);
}
} }
