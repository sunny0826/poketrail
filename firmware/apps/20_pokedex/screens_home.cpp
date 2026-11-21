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
void Screens::drawResultHero(uint16_t species, bool first, uint32_t now) {
  if (first) {
    const auto& sp = data::kSprites[species - 1];
    const int scale = sp.height * 2 <= 120 ? 2 : 1;
    const int rx = sp.width * scale / 2 + 12, ry = std::min(64, sp.height * scale / 2 + 12);
    for (unsigned k = 0; k < 12; ++k) {
      const float a = 6.2831853f * (float(now % 8000) / 8000.0f) + float(k) * 0.5235988f;
      M5.Display.fillCircle(233 + int(rx * std::cos(a)), 168 + int(ry * std::sin(a)), 2, theme::kGold);
    }
  }
  actorShadow(233, 168);
  sprite(species, now, resultPlayback_, 168, 233, 0, 2, 120);
  if (first) line(tr(Text::FirstCapture), 246, theme::kGold);
}
void Screens::menu(const ScreenState& s, const Engine& e) {
  const int top = menuTop(s, e);
  for (unsigned i = 0; i < menuCount(s, e); ++i) drawMenuRow(s, e, i, top);
}
void Screens::home(const ScreenState& s, const Engine& e, uint32_t now) {
  const auto& v = e.view();
  char text[100];
  switch (s.page) {
    case Page::Settings:
      title(tr(Text::Settings), "", theme::kMint);
      hubIcon(theme::HubIcon::Settings, 233, 126, theme::kMint, theme::kBackground, v);
      if (!v.clock.trusted) alert(tr(Text::TimeNotSet), 178, theme::kGold);
      break;
    case Page::DateWelcome:
      title(tr(Text::ConfirmDate), v.clock.pending.active ? tr(Text::ClockPending) : "", theme::kGold);
      M5.Display.drawCircle(233, 133, 29, theme::kGold);
      M5.Display.drawLine(233, 111, 233, 133, theme::kGold);
      M5.Display.drawLine(233, 133, 249, 142, theme::kGold);
      line(tr(Text::SkipKeepsSteps), 182, theme::kMuted);
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
      title(tr(Text::ChooseRegion), pendingEncounterCount(v) ? tr(Text::NextEncounterOnly) : "", theme::routeColor(region));
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
      title(tr(Text::Paused), "", theme::kGold);
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
        drawResultHero(v.encounter.species, v.captureCounts[v.encounter.species - 1] == 1, now);
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
        } else if (v.result == Result::Released && !v.captureCounts[v.encounter.species - 1] &&
                   v.encounter.ruleVersion < 3 && !availableBalls(v)) {
          line(tr(Text::NoBalls), 249, theme::kRed);
        }
      }
      menu(s, e); return;
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
void Screens::drawLanguageRow(const ScreenState& s, const Engine&, unsigned index, int top) {
  if (index == 0)
    item(s, 0, tr(Text::LanguageChinese), top, 0, false,
         s.language == i18n::Language::Chinese ? tr(Text::InUse) : nullptr);
  else if (index == 1)
    item(s, 1, tr(Text::LanguageEnglish), top, 0, false,
         s.language == i18n::Language::English ? tr(Text::InUse) : nullptr);
  else item(s, 2, tr(Text::Back), top);
}
void Screens::drawMenuRow(const ScreenState& s, const Engine& e, unsigned index, int top) {
  static const Text kSettings[] = {Text::DateTime, Text::LanguageMenu, Text::Back};
  static const Text kDate[] = {Text::SetDateTime, Text::SkipDateTime, Text::BrowsePokedex, Text::LanguageMenu};
  static const Text kRoutes[] = {Text::RouteKanto, Text::RouteJohto, Text::RouteHoenn,
                                 Text::RouteSinnoh, Text::RouteUnova, Text::Back};
  static const Text kEncounter[] = {Text::StartBattle, Text::CapturePractice, Text::SwitchPartner, Text::BattleLater};
  static const Text kPause[] = {Text::Continue, Text::ConfirmRetreat};
  static const Text kError[] = {Text::RetrySaveLoad, Text::BrowsePokedex, Text::LanguageMenu};
  switch (s.page) {
    case Page::Settings: item(s, index, tr(kSettings[index]), top); break;
    case Page::DateWelcome: item(s, index, tr(kDate[index]), top); break;
    case Page::Starter: {
      const uint16_t ids[] = {1, 4, 7, 0};
      item(s, index, index < 3 ? speciesName(ids[index]) : tr(Text::Back), top, ids[index]);
      break;
    }
    case Page::Routes: item(s, index, tr(kRoutes[index]), top); break;
    case Page::Encounter: item(s, index, tr(kEncounter[index]), top); break;
    case Page::Pause: item(s, index, tr(kPause[index]), top); break;
    case Page::Error: item(s, index, tr(kError[index]), top); break;
    case Page::Partners: {
      unsigned row = 0;
      for (unsigned id = 1; id <= kSpeciesCount; ++id)
        if (owns(e.view(), id) && row++ == index) {
          item(s, index, speciesName(id), top, id, id == e.view().partner);
          return;
        }
      item(s, index, tr(Text::Back), top);
      break;
    }
    case Page::Result: item(s, 0, tr(Text::AcceptResult), top); break;
    default: break;
  }
}
void Screens::drawHomeGroup(const ScreenState& s, const Engine& e, uint32_t now, bool full) {
  const auto& v = e.view();
  auto drawLangStatus = [&]() {
    if (e.savePending()) alert(tr(Text::RetrySaveFirst), 180, theme::kGold);
    else if (s.command != CommandResult::Applied && s.command != CommandResult::Duplicate)
      line(s.command == CommandResult::Deferred && v.clock.pending.active ?
           tr(Text::FinishClockFirst) : progressionMessage(s.command), 180, theme::kGold);
    else if (s.languagePersistence == LanguagePersistence::SessionOnly)
      line(tr(Text::SessionOnly), 180, theme::kGold);
  };
  if (s.page == Page::Language) {
    if (full) {
      title(tr(Text::LanguageTitle), "", theme::kMint);
      drawLangStatus();
      for (unsigned i = 0; i < 3; ++i) drawLanguageRow(s, e, i, menu_layout::kTop);
    } else {
      if (e.savePending() != snap_.savePending || s.command != snap_.command ||
          v.clock.pending.active != snap_.clockPending ||
          s.languagePersistence != snap_.languagePersistence) {
        WriteScope sc;
        clearRegion(40, 162, 388, 36, theme::kBackground);
        drawLangStatus();
      }
      updateListRows(s, e, menu_layout::kTop, false, 3, &Screens::drawLanguageRow);
    }
    snap_.savePending = e.savePending();
    snap_.command = s.command;
    snap_.clockPending = v.clock.pending.active;
    snap_.languagePersistence = s.languagePersistence;
    return;
  }
  auto redrawBody = [&]() {
    WriteScope sc;
    clearRegion(0, 36, 468, 405, theme::kBackground);
    home(s, e, now);
    if (hasPager(s.page) && menuCount(s, e) > menu_layout::kPageSize) pager(s, menuCount(s, e));
    if (touchBackEnabled(s.page)) glyph(theme::Icon::Back, 112, 52, theme::kMuted);
  };
  if (full) {
    home(s, e, now);
  } else {
    switch (s.page) {
      case Page::Settings:
        if (v.clock.trusted != snap_.clockTrusted) {
          WriteScope sc;
          clearRegion(40, 162, 388, 34, theme::kBackground);
          if (!v.clock.trusted) alert(tr(Text::TimeNotSet), 178, theme::kGold);
        }
        updateListRows(s, e, menu_layout::kTop, true, 3, &Screens::drawMenuRow);
        break;
      case Page::DateWelcome:
        if (v.clock.pending.active != snap_.clockPending) {
          WriteScope sc;
          clearRegion(70, 68, 328, 22, theme::kBackground);
          if (v.clock.pending.active) fitLine(tr(Text::ClockPending), 80, 300, theme::kGold, false);
        }
        updateListRows(s, e, menu_layout::kTop, true, 4, &Screens::drawMenuRow);
        break;
      case Page::Starter: {
        const uint16_t ids[] = {1, 4, 7};
        if (s.selected < 3) advance(resultPlayback_, ids[s.selected], now);
        const uint16_t curFrame = s.selected < 3 ? resultPlayback_.state.frame_index : UINT16_MAX;
        if ((s.selected < 3 && (s.selected != snap_.selected || curFrame != snap_.animFrame)) ||
            (s.selected >= 3 && snap_.selected < 3)) {
          WriteScope sc;
          clearRegion(104, 92, 258, 116, theme::kBackground);
          if (s.selected < 3) {
            M5.Display.setClipRect(104, 92, 258, 116);
            actorShadow(233, 151);
            sprite(ids[s.selected], now, resultPlayback_, 151, 233, 0, 2);
            M5.Display.clearClipRect();
          }
        }
        updateListRows(s, e, menu_layout::kTop, false, 4, &Screens::drawMenuRow);
        break;
      }
      case Page::Routes: {
        const Route region = s.selected < kRegionCount ? regionAt(uint8_t(s.selected)) : v.route;
        const bool hasPending = pendingEncounterCount(v) > 0;
        if (region != snap_.route || hasPending != snap_.encounterWaiting) {
          WriteScope sc;
          if (hasPending != snap_.encounterWaiting || (hasPending && region != snap_.route)) {
            clearRegion(70, 68, 328, 22, theme::kBackground);
            if (hasPending) fitLine(tr(Text::NextEncounterOnly), 80, 300, theme::routeColor(region), false);
          }
          if (region != snap_.route) sceneThumbnail(region, 233, 151);
        }
        updateListRows(s, e, menu_layout::kTop, false, kRegionCount + 1, &Screens::drawMenuRow);
        break;
      }
      case Page::Partners: {
        const unsigned count = menuCount(s, e), owned = count - 1;
        if (v.rosterRevision != snap_.rosterRevision || v.partner != snap_.partner) {
          redrawBody();
        } else {
          if (s.selected != snap_.selected && (s.selected < owned || snap_.selected < owned)) {
            WriteScope sc;
            clearRegion(184, 88, 98, 98, theme::kBackground);
            unsigned row = 0;
            for (uint16_t id = 1; id <= kSpeciesCount; ++id)
              if (owns(v, id) && row++ == s.selected) {
                M5.Display.drawCircle(233, 136, 48, theme::kOutline);
                thumbnail(id, 233, 137, 2);
                break;
              }
          }
          updateListRows(s, e, menu_layout::kTop, true, count, &Screens::drawMenuRow);
        }
        break;
      }
      case Page::Encounter:
        if (v.encounter.route != snap_.route || v.encounter.species != snap_.species) redrawBody();
        else {
          advance(resultPlayback_, v.encounter.species, now);
          if (resultPlayback_.state.frame_index != snap_.animFrame) {
            const auto box = spriteBounds(v.encounter.species, 233, 147, 2, 110);
            WriteScope sc;
            restoreSceneRegion(box.x, box.y, box.width, std::min(box.height, 208 - box.y), v.encounter.route, true);
            M5.Display.setClipRect(0, 92, 468, 116);
            sprite(v.encounter.species, now, resultPlayback_, 147, 233, 0, 2, 110);
            M5.Display.clearClipRect();
          }
          updateListRows(s, e, menu_layout::kTop, false, 4, &Screens::drawMenuRow, true, v.encounter.route, true);
        }
        break;
      case Page::Pause: updateListRows(s, e, menu_layout::kTop, false, 2, &Screens::drawMenuRow); break;
      case Page::Error:
        if (e.savePending() != snap_.savePending || e.loadResult().status != snap_.loadStatus) redrawBody();
        else updateListRows(s, e, menu_layout::kTop, false, 3, &Screens::drawMenuRow);
        break;
      case Page::Result: {
        const bool legacyNoBalls = v.result == Result::Released && !v.captureCounts[v.encounter.species - 1] &&
                                   v.encounter.ruleVersion < 3 && !availableBalls(v);
        if (v.result != snap_.result || v.encounter.species != snap_.species ||
            v.recentSize != snap_.recentSize || legacyNoBalls != snap_.legacyNoBalls) {
          redrawBody();
        } else if (v.result == Result::Captured && v.recentSize) {
          WriteScope sc;
          clearRegion(72, 100, 322, 156, theme::kBackground);
          drawResultHero(v.encounter.species, v.captureCounts[v.encounter.species - 1] == 1, now);
        }
        break;
      }
      default: break;
    }
  }
  snap_.clockTrusted = v.clock.trusted;
  snap_.clockPending = v.clock.pending.active;
  snap_.route = s.page == Page::Routes ? (s.selected < kRegionCount ? regionAt(uint8_t(s.selected)) : v.route) :
                s.page == Page::Encounter ? v.encounter.route : v.route;
  snap_.encounterWaiting = pendingEncounterCount(v) > 0;
  snap_.rosterRevision = v.rosterRevision;
  snap_.partner = v.partner;
  snap_.species = (s.page == Page::Encounter || s.page == Page::Result) ? v.encounter.species : 0;
  snap_.savePending = e.savePending();
  snap_.loadStatus = e.loadResult().status;
  snap_.result = v.result;
  snap_.recentSize = v.recentSize;
  snap_.legacyNoBalls = v.result == Result::Released && v.encounter.species &&
                        !v.captureCounts[v.encounter.species - 1] &&
                        v.encounter.ruleVersion < 3 && !availableBalls(v);
  snap_.animFrame = (s.page == Page::Starter && s.selected < 3) || s.page == Page::Encounter ||
                    (s.page == Page::Result && v.result == Result::Captured)
                        ? resultPlayback_.state.frame_index : UINT16_MAX;
}
} }
