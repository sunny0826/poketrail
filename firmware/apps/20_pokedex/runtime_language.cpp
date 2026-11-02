#include "runtime.hpp"
#include "viewer_screen.hpp"

namespace pokedex { namespace ui {
void Runtime::syncLanguage() {
  const bool hasLanguage = engine_.ready() || engine_.savePending();
  const i18n::Language next = hasLanguage ? engine_.language() : screen_.language;
  const bool temporary = !hasLanguage || (testHost_ && testHost_->ramOnly());
  const LanguagePersistence persistence = temporary ? LanguagePersistence::SessionOnly :
                                                     LanguagePersistence::Saved;
  if (screen_.language != next || screen_.languagePersistence != persistence) dirty_ = true;
  screen_.language = next;
  screen_.languagePersistence = persistence;
  viewer_screen::setLanguage(next);
}

void Runtime::openLanguage() {
  if (!safe()) return;
  languageReturn_ = screen_.page;
  languageReturnSelection_ = screen_.selected;
  syncLanguage();
  navigate(Page::Language);
  screen_.selected = unsigned(screen_.language);
}

void Runtime::activateLanguage(unsigned item) {
  if (item >= 2) { leaveLanguage(); return; }
  if (engine_.savePending()) {
    screen_.command = adventure::CommandResult::SavePending;
    waitForRelease();
    return;
  }
  const auto language = static_cast<i18n::Language>(item);
  if (!engine_.ready()) screen_.language = language;
  else screen_.command = engine_.selectLanguage(language);
  syncLanguage();
  dirty_ = true;
  waitForRelease();
  if (engine_.savePending()) navigate(Page::Error);
}

void Runtime::leaveLanguage() {
  navigate(languageReturn_);
  screen_.selected = languageReturnSelection_;
}
} }
