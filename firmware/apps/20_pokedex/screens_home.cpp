#include "screens.hpp"
#include "board.hpp"
#include "encounter_catalog.hpp"
#include "sprites_generated.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
namespace pokedex { namespace ui {
using namespace adventure;
using i18n::Text;
namespace {
void bookIcon() {
  M5.Display.drawRect(207, 124, 52, 44, theme::kGold);
  M5.Display.drawLine(233, 124, 233, 168, theme::kGold);
  for (int y : {135, 145, 155}) {
    M5.Display.drawLine(214, y, 226, y, theme::kGold);
    M5.Display.drawLine(240, y, 252, y, theme::kGold);
  }
}
}
void Screens::language(const ScreenState& s, const Engine& e) {
  title(tr(Text::LanguageTitle), "", theme::kMint);
  if (e.savePending()) alert(tr(Text::RetrySaveFirst), 180, theme::kGold);
  else if (s.command != CommandResult::Applied && s.command != CommandResult::Duplicate)
    line(s.command == CommandResult::Deferred && e.view().clock.pending.active ?
         tr(Text::FinishClockFirst) : progressionMessage(s.command), 180, theme::kGold);
  else if (s.languagePersistence == LanguagePersistence::SessionOnly)
    line(tr(Text::SessionOnly), 180, theme::kGold);
  item(s, 0, tr(Text::LanguageChinese), menu_layout::kTop, 0, false,
       s.language == i18n::Language::Chinese ? tr(Text::InUse) : nullptr);
  item(s, 1, tr(Text::LanguageEnglish), menu_layout::kTop, 0, false,
       s.language == i18n::Language::English ? tr(Text::InUse) : nullptr);
  item(s, 2, tr(Text::Back));
}
void Screens::menu(const ScreenState& s, const Engine& e) {
  const int top = menuTop(s, e);
  const auto phase = e.view().phase;
  char dexLabel[32], captureLabel[48];
  snprintf(dexLabel, sizeof(dexLabel), tr(Text::PokedexCount), unsigned(kSpeciesCount));
  snprintf(captureLabel, sizeof(captureLabel), tr(Text::FirstCapturesCount), unsigned(kSpeciesCount));
  const char* const home[] = {phase == Phase::Walking ? tr(Text::NoEncounter) : phase == Phase::ThrowReady ? tr(Text::ResumeCatch) : tr(Text::ResumeEncounter), tr(Text::ChooseRegion), tr(Text::MyPartners), tr(Text::CarriedPartner), dexLabel, tr(Text::Resources), tr(Text::CapturePractice), tr(Text::SleepNow), tr(Text::LanguageMenu), tr(Text::ReturnExploration)};
  const char* const resources[] = {tr(Text::CaptureWalkRecords), tr(Text::DateTime), tr(Text::Back)};
  const char* const date[] = {tr(Text::SetDateTime), tr(Text::SkipDateTime), tr(Text::BrowsePokedex), tr(Text::LanguageMenu)};
  const char* const starters[] = {speciesName(1), speciesName(4), speciesName(7), tr(Text::Back)};
  const char* const routes[] = {tr(Text::RouteKanto), tr(Text::RouteJohto), tr(Text::RouteHoenn), tr(Text::RouteSinnoh), tr(Text::RouteUnova), tr(Text::Back)};
  const char* const encounter[] = {tr(Text::StartBattle), tr(Text::CapturePractice), tr(Text::ChoosePartner), tr(Text::BattleLater)};
  const char* const pause[] = {tr(Text::Continue), tr(Text::ConfirmRetreat)};
  const char* const records[] = {captureLabel, tr(Text::RecentThirtyTwo), tr(Text::TodayAndHistory), tr(Text::DailyLifetime), tr(Text::Back)};
  const char* const error[] = {tr(Text::RetrySaveLoad), tr(Text::BrowsePokedex), tr(Text::LanguageMenu)};
  const char* const* labels = nullptr;
  switch (s.page) {
    case Page::Menu: labels = home; break;
    case Page::Resources: labels = resources; break;
    case Page::DateWelcome: labels = date; break;
    case Page::Starter: {
      const uint16_t ids[] = {1, 4, 7, 0};
      for (unsigned i = 0; i < 4; ++i) item(s, i, starters[i], top, ids[i]);
      return;
    }
    case Page::Routes: labels = routes; break;
    case Page::Encounter: labels = encounter; break;
    case Page::Pause: labels = pause; break;
    case Page::Records: labels = records; break;
    case Page::Error: labels = error; break;
    case Page::Partners: {
      unsigned row = 0;
      for (unsigned id = 1; id <= kSpeciesCount; ++id) if (owns(e.view(), id)) {
        item(s, row++, speciesName(id), top, id, id == e.view().partner);
      }
      item(s, row, tr(Text::Back), top); return;
    }
    case Page::Result: item(s, 0, tr(Text::AcceptResult), top); return;
    default: return;
  }
  for (unsigned i = 0; i < menuCount(s, e); ++i) item(s, i, labels[i], top);
}
void Screens::home(const ScreenState& s, const Engine& e, uint32_t now) {
  const auto& v = e.view();
  char text[100];
  switch (s.page) {
    case Page::Menu:
      fitLine(tr(Text::WalkingAdventure), 52, 140, theme::kText);
      break;
    case Page::Resources: {
      title(tr(Text::Resources), "", theme::kGold);
      snprintf(text, sizeof(text), tr(Text::StepsRoute), v.clock.trusted ? tr(Text::Today) : tr(Text::LastRecord),
               static_cast<unsigned long long>(v.today.steps), routeName(v.route));
      M5.Display.setFont(&fonts::efontCN_16);
      if (s.language == i18n::Language::English && M5.Display.textWidth(text) > 300) {
        snprintf(text, sizeof(text), tr(Text::StepsContext), v.clock.trusted ? tr(Text::Today) : tr(Text::LastRecord),
                 static_cast<unsigned long long>(v.today.steps));
        line(text, 80, theme::kMint);
        line(routeName(v.route), 110, theme::kMint);
      } else line(text, 80, theme::kMint);
      captureBadge(v, 312, 110);
      const EvolutionView progress = e.evolutionView(v.partner);
      snprintf(text, sizeof(text), "%s · %s", speciesName(v.partner), progress.count ? tr(Text::Training) : tr(Text::FinalForm));
      line(text, 132, theme::kMuted);
      if (!progress.count) snprintf(text, sizeof(text), tr(Text::WalkedSteps), static_cast<unsigned long long>(progress.walked));
      else snprintf(text, sizeof(text), tr(Text::TrainingRatio), static_cast<unsigned long long>(progress.available),
                    static_cast<unsigned long>(progress.options[0].cost));
      line(text, 156, theme::kText);
      if (e.stepsIncomplete()) alert(tr(Text::StepsIncomplete), 183, theme::kRed);
      else if (e.loadResult().degraded) alert(tr(Text::BackupRestored), 183, theme::kGold);
      else if (v.phase == Phase::Walking) {
        if (!remainingEncounters(v, v.route)) {
          bool all = true;
          for (uint8_t i = 0; i < kRegionCount; ++i) all = all && !remainingEncounters(v, regionAt(i));
          alert(all ? tr(Text::AllCaught) : tr(Text::RegionCaught), 183, theme::kGold);
        } else {
          snprintf(text, sizeof(text), tr(Text::NextEncounterSteps), static_cast<unsigned long long>(
            v.explorationCredit < v.encounterTarget ? v.encounterTarget - v.explorationCredit : 0));
          alert(text, 183, theme::kGold);
        }
      } else alert(tr(Text::EncounterWaiting), 183, theme::kGold);
      break;
    }
    case Page::DateWelcome:
      title(tr(Text::ConfirmDate), v.clock.pending.active ? tr(Text::ClockPending) : tr(Text::OfflineClock), theme::kGold);
      M5.Display.drawCircle(233, 133, 29, theme::kGold);
      M5.Display.drawLine(233, 111, 233, 133, theme::kGold);
      M5.Display.drawLine(233, 133, 249, 142, theme::kGold);
      line(tr(Text::SkipKeepsSteps), 174, theme::kText);
      line(tr(Text::CaptureTimeUnknown), 194, theme::kMuted);
      break;
    case Page::Starter:
      title(tr(Text::ChooseStarter), tr(Text::StarterOnce), theme::kGold);
      if (s.selected < 3) {
        const uint16_t ids[] = {1, 4, 7};
        actorShadow(233, 151);
        sprite(ids[s.selected], now, resultPlayback_, 151, 233, 0, 2);
      }
      break;
    case Page::Routes: {
      const Route region = s.selected < kRegionCount ? regionAt(uint8_t(s.selected)) : v.route;
      title(tr(Text::ChooseRegion), tr(Text::NextEncounterOnly), theme::routeColor(region));
      sceneThumbnail(region, 233, 151);
      break;
    }
    case Page::Partners: {
      title(tr(Text::MyPartners), "", theme::kMint);
      unsigned owned = 0, row = 0;
      for (uint16_t id = 1; id <= kSpeciesCount; ++id) if (owns(v, id)) ++owned;
      snprintf(text, sizeof(text), tr(Text::OwnedCount), owned); line(text, 80, theme::kMint);
      for (uint16_t id = 1; id <= kSpeciesCount; ++id) if (owns(v, id) && row++ == s.selected) {
        M5.Display.drawCircle(233, 136, 48, theme::kOutline);
        thumbnail(id, 233, 137, 2); break;
      }
      break;
    }
    case Page::Encounter:
      scene(v.encounter.route, true);
      title(tr(Text::PokemonFound), speciesName(v.encounter.species), theme::kGold);
      actorShadow(233, 147);
      sprite(v.encounter.species, now, resultPlayback_, 147, 233, 0, 2, 110);
      break;
    case Page::Pause:
      title(tr(Text::Paused), s.practice ? tr(Text::PracticeFree) : tr(Text::ResumeNextBeat), theme::kGold);
      M5.Display.fillRect(216, 120, 13, 44, theme::kGold);
      M5.Display.fillRect(239, 120, 13, 44, theme::kGold);
      break;
    case Page::Result:
      title(v.result == Result::Captured ? tr(Text::CaptureSuccess) : v.result == Result::Lost ? tr(Text::BattleOver) :
            v.result == Result::Retreated ? tr(Text::Retreated) : v.result == Result::Released ?
            (v.captureCounts[v.encounter.species - 1] ? tr(Text::AlreadyCaughtEnd) : tr(Text::CaptureAbandoned)) :
            v.result == Result::MigrationRetired ? tr(Text::LegacySettled) : tr(Text::PokemonLeft),
            speciesName(v.encounter.species),
            v.result == Result::Captured || v.result == Result::MigrationRetired ? theme::kGold : theme::kMuted);
      if (v.result == Result::Captured && v.recentSize) {
        const auto& c = v.recent[v.recentSize - 1];
        const bool first = v.captureCounts[v.encounter.species - 1] == 1;
        if (first) {
          const auto& sp = data::kSprites[v.encounter.species - 1];
          const int scale = sp.height * 2 <= 120 ? 2 : 1;
          const int radiusX = sp.width * scale / 2 + 12;
          const int radiusY = std::min(64, sp.height * scale / 2 + 12);
          for (unsigned k = 0; k < 12; ++k) {
            const float a = 6.2831853f * (float(now % 8000) / 8000.0f) + float(k) * 0.5235988f;
            M5.Display.fillCircle(233 + int(radiusX * std::cos(a)), 168 + int(radiusY * std::sin(a)), 2, theme::kGold);
          }
        }
        actorShadow(233, 168);
        sprite(v.encounter.species, now, resultPlayback_, 168, 233, 0, 2, 120);
        if (first) line(tr(Text::FirstCapture), 246, theme::kGold);
        calendar::CivilTime local;
        if (c.at.timeKnown && calendar::fromUtc(c.at.utcSeconds, c.at.offsetMinutes, local))
          snprintf(text, sizeof(text), "%s · %04d-%02d-%02d %02d:%02d", routeName(c.route),
                   local.year, local.month, local.day, local.hour, local.minute);
        else snprintf(text, sizeof(text), tr(Text::RouteTimeUnknown), routeName(c.route));
        line(text, 270, theme::kMuted);
        badges(c.achievementMask, 294);
      } else {
        M5.Display.drawCircle(233, 164, 52, theme::kOutline);
        thumbnail(v.encounter.species, 233, 164, 2);
        if (v.result == Result::MigrationRetired) {
          line(tr(Text::MigrationRefund), 249, theme::kGold);
          line(tr(Text::UnlimitedBalls), 277, theme::kText);
        } else if (v.result == Result::Released && !v.captureCounts[v.encounter.species - 1]) {
          if (v.encounter.ruleVersion < 3 && !availableBalls(v)) {
            line(tr(Text::NoBalls), 249, theme::kRed);
            line(tr(Text::EncounterEnded), 277, theme::kMuted);
          } else {
            line(tr(Text::NoRefund), 249, theme::kRed);
          }
        } else line(tr(Text::KeepExploring), 249, theme::kMuted);
      }
      menu(s, e); return;
    case Page::Records:
      title(tr(Text::CaptureWalkRecords), "", theme::kGold);
      snprintf(text, sizeof(text), tr(Text::BestDaySteps), static_cast<unsigned long long>(v.bestDaySteps));
      line(text, 80, theme::kGold); bookIcon();
      break;
    case Page::Error:
      title(e.savePending() ? tr(Text::SaveUnavailable) : tr(Text::StorageUnavailable), e.savePending() ? tr(Text::ResultUnconfirmed) : tr(Text::GameplayPaused), theme::kRed);
      warning(233, 133, theme::kRed);
      if (e.savePending()) {
        line(tr(Text::RetrySameResult), 184, theme::kGold);
      } else {
        const char* message = tr(Text::StorageIoFailed);
        switch (e.loadResult().status) {
          case LoadStatus::NeedsProvisioning: message = tr(Text::StorageUninitialized); break;
          case LoadStatus::Corrupt: message = tr(Text::CorruptPreserved); break;
          case LoadStatus::UnsupportedVersion: message = tr(Text::NewerSave); break;
          default: break;
        }
        line(message, 184, theme::kGold);
      }
      break;
    default: break;
  }
  menu(s, e);
}
} }
