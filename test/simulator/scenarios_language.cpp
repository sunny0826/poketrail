#include "driver.hpp"
#include "progression_support.hpp"
#include "../support/rendered_text.hpp"

namespace simulator { namespace e2e {
using pokedex::i18n::Language;
namespace {
void english(Driver& d, const char* text, uint16_t color = theme::kText) {
  d.frames();
  d.check(rendered_text::contains(text, color), std::string("rendered English: ") + text);
}
void openLanguage(Driver& d) {
  const Page source = d.screen().page;
  const unsigned item = source == Page::DateWelcome ? 3 : source == Page::Error ? 2 : 8;
  d.select(item);
  d.check(d.screen().page == Page::Language && menuCount(d.screen(), d.runtime().engine()) == 3,
          "real keys open three language choices");
}
void choose(Driver& d, Language language) {
  d.select(unsigned(language));
  d.check(d.runtime().language() == language && d.save().language == language &&
          !d.runtime().engine().savePending(), "language is committed before publication");
}
void normalJourney(Driver& d, const std::string& phase) {
  if (phase == "new") {
    d.check(d.screen().page == Page::DateWelcome && d.runtime().language() == Language::Chinese,
            "blank save starts Chinese before starter selection");
    d.swipe(320, 140);
    d.check(d.screen().selected == 3, "welcome language entry is reachable on its second page");
    d.shot("welcome-language-page");
    d.tap(233, 238);
    d.check(d.screen().page == Page::Language, "touch opens welcome language entry");
    choose(d, Language::English); english(d, "Language");
    d.shot("language-english");
    d.select(2);
    d.check(d.screen().page == Page::DateWelcome && d.screen().selected == 3,
            "Back restores welcome page and selection");
    openLanguage(d); d.back();
    d.check(d.screen().page == Page::DateWelcome && d.screen().selected == 3,
            "top back restores welcome language entry");
    d.select(1);
    d.check(d.screen().page == Page::Starter, "English welcome continues to starter");
    english(d, "Choose your starter"); d.shot("english-starter"); d.select(0);
    d.check(d.screen().page == Page::Home && d.save().starter == 1, "English starter selection persists");
    english(d, "Bulbasaur"); d.shot("english-home");
  } else if (phase == "english") {
    d.check(d.save().starter == 1 && d.runtime().language() == Language::English, "new process restores English and partner");
    english(d, "Bulbasaur");
    d.homeMenu(); d.shot("english-menu"); english(d, "My partners");
    openLanguage(d); d.back();
    d.check(d.screen().page == Page::Menu && d.screen().selected == 8, "back restores menu language selection");
    openLanguage(d); choose(d, Language::Chinese); d.select(2); d.select(9);
    d.check(d.screen().page == Page::Home, "tenth main menu item returns home");
    d.shot("chinese-restored");
  } else {
    d.check(d.runtime().language() == Language::Chinese && d.save().starter == 1,
            "new process restores Chinese and unchanged partner");
    english(d, "妙蛙种子"); d.shot("chinese-reload");
  }
  d.state("language-final"); d.close();
}
void retry(Driver& d, const std::string& phase) {
  if (phase == "retry-reload") {
    d.check(d.runtime().language() == Language::English && d.save().starter == 1,
            "language retry survives new process");
    english(d, "Bulbasaur"); d.state("language-final"); d.close(); return;
  }
  d.homeMenu(); openLanguage(d);
  const Save before = d.save();
  d.fault(phase == "before" ? Fault::BeforeWrite : phase == "after" ? Fault::AfterWrite : Fault::ReadBack);
  d.select(1);
  d.check(d.screen().page == Page::Error && d.runtime().engine().savePending() &&
          d.runtime().language() == Language::Chinese && d.save().language == Language::Chinese,
          "failed language commit retains Chinese error page");
  d.check(d.faultHits() > 0, "language save fault was exercised");
  d.shot("language-save-failed"); d.state("language-failed");
  openLanguage(d); d.select(0); d.select(1);
  d.check(d.runtime().language() == Language::Chinese,
          "pending language choices cannot publish a replacement candidate");
  d.shot("language-pending-locked"); d.back(); d.fault(Fault::None); d.select(0);
  d.check(d.runtime().language() == Language::English && !d.runtime().engine().savePending(),
          "retry publishes original English candidate");
  progression_support::sameAccounts(d, before, "language retry");
  d.shot("language-retry-saved"); d.state("language-final"); d.close();
}
void unavailable(Driver& d, const std::string& phase) {
  d.check(d.screen().page == Page::Error && !d.runtime().engine().ready() &&
          d.runtime().language() == Language::Chinese, "unavailable save starts in Chinese");
  if (phase == "error-reload") { d.state("language-final"); d.shot("language-error-reload"); d.close(); return; }
  openLanguage(d); d.select(1);
  d.check(d.runtime().language() == Language::English &&
          d.screen().languagePersistence == LanguagePersistence::SessionOnly, "unavailable save allows session-only English");
  english(d, "Session only", theme::kGold); d.shot("language-session-only");
  d.select(2); d.select(1);
  d.check(d.screen().page == Page::Viewer, "error menu opens English viewer");
  english(d, "Bulbasaur"); english(d, "Grass / Poison", theme::kBlue);
  d.shot("language-error-viewer"); d.back();
  d.check(d.screen().page == Page::Error, "temporary viewer returns to the storage error");
  d.state("language-final"); d.close();
}
void migration(Driver& d, const std::string& phase) {
  d.check(d.save().schema == kSchemaVersion && d.save().route == Route::Kanto && d.save().totalSteps == 7800 &&
          d.save().explorationCredit == 7800 && d.save().explorationSpent == 0 &&
          d.save().progress[0].walked == 7600 && d.save().progress[1].walked == 200 &&
          d.save().partner == 1 && owns(d.save(), 4) && !owns(d.save(), 2),
          "frozen schema7 byte fixture retains region, ownership and training accounts");
  d.check(d.runtime().language() == (phase == "schema7" ? Language::Chinese : Language::English),
          "historical bytes default Chinese; later English choice survives restart");
  if (phase == "schema7") { d.homeMenu(); openLanguage(d); choose(d, Language::English); }
  d.shot("language-schema7"); d.state("language-final"); d.close();
}
}
void scenarioLanguage(Driver& d, const std::string& phase) {
  if (phase == "new" || phase == "english" || phase == "chinese") normalJourney(d, phase);
  else if (phase == "error" || phase == "error-reload") unavailable(d, phase);
  else if (phase == "schema7" || phase == "schema7-reload") migration(d, phase);
  else retry(d, phase);
}
} }
