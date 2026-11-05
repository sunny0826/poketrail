#include "driver.hpp"
#include "progression_support.hpp"
#include "encounter_catalog.hpp"
#include "screen_theme.hpp"
#include "../support/rendered_text.hpp"

namespace simulator { namespace e2e {
namespace {
void exhaustionPage(Driver& d, const char* text, const char* shot) {
  d.homeMenu(); d.frames();
  d.check(d.screen().page==Page::Menu && d.screen().selected==unsigned(MenuItem::Region) &&
          rendered_text::contains(text, theme::kGold, 78, 130),
          "menu banner explains exhaustion without claiming every species was caught");
  d.shot(shot);
  progression_support::toHome(d,"exhausted menu");
}
void checkNoNewEncounter(Driver& d, bool all=false) {
  const uint64_t credit=d.save().explorationCredit, spent=d.save().explorationSpent;
  const uint64_t total=d.save().totalSteps, next=d.save().nextEncounterId;
  const uint16_t partner=d.save().partner;
  const uint64_t trained=d.save().progress[partner-1].walked;
  d.walk(500);
  d.check(d.save().phase==Phase::Walking && d.save().nextEncounterId==next &&
          d.save().explorationSpent==spent &&
          d.save().explorationCredit-credit==d.save().totalSteps-total &&
          d.save().progress[partner-1].walked-trained==d.save().totalSteps-total,
          "exhausted pool preserves exploration credit and continues partner training");
  exhaustionPage(d,all ? "暂无新的普通宝可梦" : "本地区暂无新遭遇 · 可换地区","history-exhausted");
}
void englishExhaustion(Driver& d, bool all) {
  d.homeMenu(); d.select(MenuItem::Settings); d.select(1); d.select(1); d.select(2);
  progression_support::toHome(d,"English language selected");
  exhaustionPage(d,all ? "No new regular Pokémon" : "No new encounters · Try another","history-exhausted-english");
}
void evolutionState(Driver& d) {
  d.check(d.save().partner==2 && !owns(d.save(),1) && registered(d.save(),1) && registered(d.save(),2),
          "evolved starter and permanent history survive");
  d.check(d.save().progress[0].spent==7500 && d.save().progress[0].walked==8000,
          "source retains 500 training steps without recapture");
  d.check(capturedSpeciesCount(d.save())==0 && d.save().recentSize==0,
          "gifts and evolution do not manufacture capture records");
  d.check(remainingEncounters(d.save(),Route::Kanto)==0,"acquired Kanto pool is empty");
}
} // namespace

void scenarioEncounterHistory(Driver& d, const std::string& phase) {
  d.frames();
  if (phase=="evolution") {
    d.check(d.save().partner==1 && d.save().captureCounts[0]==0,"gifted starter was never captured");
    progression_support::homeEvolution(d,1,2,"retire gifted starter");
    const Save before=d.save();
    d.fault(Fault::BeforeWrite); d.select(0);
    d.check(d.screen().page==Page::Error && d.save().partner==1 &&
            d.save().registered==before.registered,"failed evolution publishes no new history");
    d.fault(Fault::None); d.select(0);
    progression_support::finishEvolution(d,"retried starter evolution");
    progression_support::toHome(d,"evolved starter");
    evolutionState(d); checkNoNewEncounter(d,true);
  } else if (phase=="evolution-reload") {
    evolutionState(d); checkNoNewEncounter(d,true); englishExhaustion(d,true);
  } else if (phase=="branches") {
    d.check(registered(d.save(),237) && !registered(d.save(),236),"sparse history has Hitmontop but no Tyrogue");
    d.check(registered(d.save(),26) && !registered(d.save(),25) && !registered(d.save(),172),
            "sparse Raichu history omits both ancestral forms");
    checkNoNewEncounter(d); englishExhaustion(d,false);
    d.homeMenu(); d.select(MenuItem::Region); d.select(regionIndex(Route::Kanto));
    d.until([&]{return d.screen().page==Page::Home;},4000,"switch to Kanto");
    d.check(d.save().phase==Phase::EncounterReady &&
            (d.save().encounter.species==106 || d.save().encounter.species==107),
            "banked steps encounter an unacquired sibling, never a retired species");
    d.homeMenu(); d.select(MenuItem::Encounter);
    d.check(d.screen().page==Page::Encounter,"actual sibling encounter opens");
    d.shot("history-sibling-encounter");
  } else if (phase=="branches-reload") {
    d.check(d.save().phase==Phase::EncounterReady &&
            (d.save().encounter.species==106 || d.save().encounter.species==107),
            "new process restores the sibling encounter from disk");
    d.shot("history-branches-reload");
  } else if (phase=="pending") {
    d.check(d.screen().page==Page::Throw && d.save().encounter.species==1 &&
            !owns(d.save(),1) && registered(d.save(),1) && registered(d.save(),2),
            "pre-change encounter of an evolved gift still loads into throw page");
    d.throwBall(true); d.finishCapture(); waitSettled(d,"grandfathered capture settles");
    d.check(d.screen().page==Page::Result && d.save().result==Result::Captured &&
            d.save().captureCounts[0]==1 && d.save().firstCapture[0].species==1,
            "existing encounter can still capture its retired species once");
    d.check(d.save().progress[0].walked-d.save().progress[0].spent==500,"capture preserves the source account");
    d.shot("history-grandfathered-capture");
  } else if (phase=="pending-reload") {
    d.check(d.save().result==Result::Captured && d.save().captureCounts[0]==1 && owns(d.save(),1),
            "grandfathered result persists without duplicate capture");
    const auto credit=d.save().explorationCredit;
    acknowledge(d);
    d.check(d.save().phase==Phase::Walking && d.save().explorationCredit==credit,
            "confirming the grandfathered result retains credit without refilling");
    d.walk(1);
    d.check(d.save().phase==Phase::EncounterReady && d.save().encounter.species!=1,
            "next real encounter excludes the grandfathered source");
    d.shot("history-next-encounter");
  } else d.check(false,"unknown encounter-history phase");
  d.state("history-end"); d.close();
}
} }
