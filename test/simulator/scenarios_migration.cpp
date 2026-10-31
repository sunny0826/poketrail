#include "driver.hpp"

namespace simulator { namespace e2e {
void scenarioMigration(Driver& d,const std::string& phase) {
  d.check(d.save().schema==6,"legacy bytes upgraded to schema6");
  if(phase=="activity") {
    d.check(d.save().totalSteps==6293 && d.save().explorationCredit==1293,"frozen activity archive preserves accounting");
    d.check(d.save().phase==Phase::Walking && d.save().encounter.id==0,"obsolete activity encounter retired");
    d.state("activity-upgraded"); d.shot("activity-upgraded"); d.close(); return;
  }
  if(phase=="rule2" || phase=="rule2-reload") {
    d.check(d.save().encounter.ruleVersion==3 && d.save().ballsSpent==0,"historical rule2 upgraded to unlimited rule3");
    if(phase=="rule2") {
      d.check(d.save().encounter.id==1 && d.save().totalSteps==5000,"historical encounter and accounts preserved");
      d.throwBall(false); d.finishCapture(); d.throwBall(false); d.finishCapture();
      d.check(d.save().encounter.usedBalls==2 && d.save().result==Result::Escaped && d.save().ballsSpent==0,"upgraded throws do not debit inventory");
      d.select(0); d.walk(600);
    }
    d.check(d.save().phase==Phase::EncounterReady && d.save().encounter.id==2 && d.save().encounter.ruleVersion==3,"next rule3 encounter persists");
    d.state("rule2-upgraded"); d.shot("rule2-upgraded"); d.close(); return;
  }
  if(phase=="reload") {
    d.check(d.save().encounter.ruleVersion==1,"the released legacy encounter keeps its old rule");
    d.check(d.save().result==Result::Released && d.save().ballsSpent==0,
            "the legacy release persists without spending a ball");
    d.check(d.save().encounter.usedBalls==1,"the earlier legacy attempt survives the release");
    d.state("migration-reloaded"); d.close(); return;
  }
  d.check(d.save().encounter.ruleVersion==1 && d.save().encounter.usedBalls==1,"frozen v1 in-flight encounter retains old attempt");
  d.check(availableBalls(d.save())==0,"legacy inventory begins empty");
  // 旧规则零球：投球页没有菜单，但必须留一个明确出口，否则玩家卡死。
  d.check(d.screen().page==Page::Throw,"legacy encounter reaches the throw page");
  d.check(d.screen().command!=CommandResult::NoBalls,"the empty stock does not block the page itself");
  d.advance(3000);
  d.check(!d.runtime().engine().throwView().active && d.save().encounter.usedBalls==1,
          "waiting on the legacy throw page starts no window and spends no attempt");
  d.key(SDL_SCANCODE_LEFT,true);
  d.key(SDL_SCANCODE_RIGHT,true);
  d.key(SDL_SCANCODE_RIGHT,false);
  d.key(SDL_SCANCODE_LEFT,false);
  d.check(d.screen().page==Page::Result && d.save().result==Result::Released,
          "the legacy exit button ends the encounter instead of trapping the player");
  d.check(d.save().ballsSpent==0 && d.save().encounter.usedBalls==1,
          "ending the legacy encounter spends no ball and keeps the earlier attempt");
  d.state("migration-final"); d.shot("migration-final"); d.close();
}
} }
