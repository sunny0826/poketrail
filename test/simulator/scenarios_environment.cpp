#include "driver.hpp"

namespace simulator { namespace e2e {
namespace {
void start(Driver& d) {
  if(d.screen().page==Page::DateWelcome) { d.select(1); d.select(0); }
  d.check(d.save().starter==1,"starter available");
}
void clockPage(Driver& d) {
  d.homeMenu(); d.select(4); d.select(1);
  d.check(d.screen().page==Page::Clock,"clock settings visible");
}
}
void scenarioClock(Driver& d,const std::string& phase) {
  if(phase=="reload") {
    d.advance(1200);
    d.check(d.save().clock.calibrated && d.save().clock.trusted,"clock trusted after process restart");
    d.check(!d.save().clock.pending.active,"clock transaction completed");
    d.state("reloaded"); d.shot("clock-reloaded"); d.close(); return;
  }
  start(d);
  if(phase=="captured") {
    d.check(d.save().captureCounts[589]==1 && !d.save().firstCapture[589].at.timeKnown,
            "unknown-date capture loaded before calibration");
  }
  if(phase!="midnight") d.walk(10);
  const auto unknown=d.save().undatedSteps;
  d.check(!d.save().clock.trusted && (phase=="midnight" || unknown>0),"uncalibrated walking remains undated");
  clockPage(d); d.shot("clock-before"); d.select(7);
  d.until([&]{return d.save().clock.trusted;},2000,"UI clock calibration trusted");
  d.check(d.save().undatedSteps==unknown,"clock does not backfill undated steps");
  if(phase=="midnight") {
    const auto day=d.save().today.dayKey; const auto total=d.save().totalSteps;
    d.walk(140);
    d.check(d.save().today.dayKey>day,"walking crosses midnight");
    d.check(d.save().totalSteps>=total+140,"midnight preserves cumulative walking");
    d.check(d.save().historySize>0,"midnight archives previous day");
    clockPage(d); d.select(2); d.press(SDL_SCANCODE_LEFT); d.press(SDL_SCANCODE_RETURN);
    const Save before=d.save(); const int64_t utcBefore=d.rtcUtc();
    d.select(7); d.advance(1200);
    d.check(d.save().clock.trusted && d.save().clock.calibrationEpoch==before.clock.calibrationEpoch+1,"explicit backward clock starts one new epoch");
    d.check(d.rtcUtc()<utcBefore-86330 && d.rtcUtc()>utcBefore-86470,"UI moved RTC back one day");
    d.check(d.save().today.dayKey==day && d.save().today.steps==0 &&
            d.save().today.epoch==d.save().clock.calibrationEpoch,"backward calibration starts empty selected day");
    d.check(d.save().totalSteps==before.totalSteps && d.save().explorationCredit==before.explorationCredit &&
            d.save().undatedSteps==before.undatedSteps,"backward calibration preserves walking accounts");
    d.check(d.save().historySize==before.historySize+1 &&
            d.save().history[d.save().historySize-1].steps==before.today.steps,"backward calibration archives prior day");
  } else {
    const auto dated=d.save().today.steps; d.walk(10);
    d.check(d.save().today.steps>dated,"new walking uses calibrated date");
    d.check(d.save().undatedSteps==unknown,"earlier unknown steps remain unknown");
  }
  if(phase=="captured") d.check(d.save().captureCounts[589]==1 && !d.save().firstCapture[589].at.timeKnown,
                                 "calibration preserves unknown capture timestamp");
  d.state("clock-final"); d.shot("clock-final"); d.close(); d.state("closed");
}
void scenarioLifecycle(Driver& d,const std::string& phase) {
  start(d);
  if(phase=="sleep") {
    d.advance(31000); d.check(d.dimmed(),"idle display sleeps");
    d.tap(233,238); d.check(d.dimmed(),"touch cannot wake"); d.wake();
    d.check(!d.dimmed(),"power wakes");
    const auto beforeWalk = d.save().totalSteps;
    d.press(SDL_SCANCODE_W); d.press(SDL_SCANCODE_TAB);
    d.focusLost(true);
    d.advance(6000);
    d.check(d.save().totalSteps >= beforeWalk + 8,"host focus changes do not interrupt scripted walking");
    d.focusLost(); d.advance(2000);
    const auto stopped = d.save().totalSteps;
    d.advance(3000);
    d.check(d.save().totalSteps == stopped,"scripted focus loss stops walking");
    d.key(SDL_SCANCODE_RIGHT,true); d.focusLost();
    d.homeMenu(); d.select(7); d.shot("awake-after-focus"); d.close();
  } else if(phase=="close-save" || phase=="close-clock") {
    d.walk(10);
    d.fault(phase=="close-clock" ? Fault::ClockWrite : Fault::BeforeWrite);
    d.close(Driver::Close::Protected); d.check(d.closeErrors()==1 && d.faultHits()>0,"failed close retains window and reports error");
    d.frames(); d.shot("protected-close"); d.fault(Fault::None); d.close();
    d.check(d.closeErrors()==1,"recovered close succeeds");
  } else if(phase=="throw-focus" || phase=="throw-close") {
    d.check(d.screen().page==Page::Throw,"throw fixture ready");
    d.tap(233,238); d.key(SDL_SCANCODE_RIGHT,true);
    d.check(d.screen().charging,"throw held");
    if(phase=="throw-focus") {
      d.focusLost(); d.check(!d.runtime().engine().throwView().active,"focus loss releases and settles held throw");
      d.check(d.save().encounter.usedBalls==1,"focus loss uses one throw"); d.finishCapture(); d.close();
    } else { d.close(); d.check(d.save().encounter.usedBalls==1,"close settles one held throw"); }
  } else d.check(false,"unknown lifecycle phase");
  d.state("closed");
}
} }
