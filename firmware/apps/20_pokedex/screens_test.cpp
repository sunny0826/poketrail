#include "screens.hpp"
#include "board.hpp"
#include "test_mode.hpp"
#include <cstdio>

namespace pokedex { namespace ui {
using namespace adventure;
using i18n::Text;

void Screens::testMenu(const ScreenState& s, const char* const* labels,
                       unsigned count, int top) {
  for (unsigned i = 0; i < count; ++i) item(s, i, labels[i], top);
}
void Screens::testEntry(const ScreenState& s) {
  const char* const enter[] = {tr(Text::TestCopy), tr(Text::TestBlank), tr(Text::Back)};
  const char* const reset[] = {tr(Text::TestRecopy), tr(Text::TestResetBlank), tr(Text::Back)};
  title(s.testMode ? tr(Text::TestReset) : tr(Text::TestEnter),
        s.testMode ? tr(Text::TestReplaceOnly) : tr(Text::MainSaveUnchanged), theme::kPurple);
  if (s.testFailure) alert(tr(Text::TestSwitchFailed), 180, theme::kRed);
  testMenu(s, s.testMode ? reset : enter, 3);
}
void Screens::testHub(const ScreenState& s, const Engine&) {
  const char* const labels[] = {tr(Text::InjectSteps), tr(Text::DateTime), tr(Text::TestScenarios), tr(Text::TestFaults),
                                tr(Text::Diagnostics), tr(Text::TestReset), tr(Text::TestExit), tr(Text::Back)};
  title(tr(Text::TestMode), "", theme::kPurple);
  if (s.testFailure) alert(tr(Text::PreviousActionFailed), 180, theme::kRed);
  testMenu(s, labels, 8);
}
void Screens::testSteps(const ScreenState& s) {
  const char* const labels[] = {tr(Text::AddOneStep), tr(Text::AddTenSteps), tr(Text::AddHundredSteps), tr(Text::AddThousandSteps), tr(Text::AddFiveThousandSteps), tr(Text::Back)};
  title(tr(Text::InjectSteps), tr(Text::NoRealImu), theme::kPurple);
  if (s.testFailure) alert(tr(Text::InjectStepsFailed), 180, theme::kRed);
  testMenu(s, labels, 6);
}
void Screens::testScenarios(const ScreenState& s) {
  const char* labels[] = {tr(Text::ScenarioAccount),
                          tr(Text::ScenarioNear),
                          tr(Text::ScenarioThrow),
                          tr(Text::ScenarioLegacy), tr(Text::Back)};
  title(tr(Text::TestScenarios), tr(Text::ReloadTestSave), theme::kPurple);
  testMenu(s, labels, 5);
}
void Screens::testFaults(const ScreenState& s) {
  const char* const labels[] = {tr(Text::FaultNone), tr(Text::FaultBefore), tr(Text::FaultAfter), tr(Text::FaultLoad), tr(Text::Back)};
  title(tr(Text::TestFaults), tr(Text::TestCommitsOnly), theme::kPurple);
  for (unsigned i = 0; i < 5; ++i)
    item(s, i, labels[i], menu_layout::kTop, 0, false,
         i < 4 && uint8_t(s.testFault) == i ? tr(Text::InUse) : nullptr);
}
void Screens::testDiagnostics(const ScreenState& s, const Engine& e) {
  title(tr(Text::TestDiagnostics), "", theme::kPurple);
  char text[100];
  const auto& d = s.testDiagnostics;
  line(d.ramOnly ? tr(Text::TestRam) : tr(Text::TestFilesystem), 116, theme::kGold);
  snprintf(text, sizeof(text), "schema %u · rule %u", unsigned(e.view().schema),
           unsigned(e.view().encounter.ruleVersion));
  fitLine(text, 148, 320, theme::kText, false);
  snprintf(text, sizeof(text), "sequence %llu",
           static_cast<unsigned long long>(e.view().sequence));
  fitLine(text, 176, 320, theme::kText, false);
  snprintf(text, sizeof(text), tr(Text::SaveBytes), unsigned(d.officialBytes),
           unsigned(d.testBytes));
  fitLine(text, 204, 320, theme::kText, false);
  snprintf(text, sizeof(text), tr(Text::StorageCapacity), unsigned(d.capacityBytes / 1024),
           unsigned(d.availableBytes / 1024));
  fitLine(text, 232, 320, theme::kText, false);
  const Text faults[] = {Text::FaultNone, Text::FaultBefore, Text::FaultAfter, Text::FaultLoad};
  snprintf(text, sizeof(text), tr(Text::FaultStatus), tr(faults[uint8_t(s.testFault)]));
  fitLine(text, 260, 320, theme::kText, false);
  snprintf(text, sizeof(text), "cleanup %s · seed %s",
           d.cleanupFailed ? tr(Text::Failed) : tr(Text::Okay), d.seedFailed ? tr(Text::Failed) : tr(Text::Okay));
  fitLine(text, 288, 320, d.cleanupFailed || d.seedFailed ? theme::kRed : theme::kMuted, false);
}
void Screens::testExit(const ScreenState& s) {
  const char* const labels[] = {tr(Text::ConfirmExit), tr(Text::Back)};
  title(tr(Text::TestExit), tr(Text::DeleteTestOnly), theme::kRed);
  if (s.testFailure) alert(tr(Text::TestExitFailed), 185, theme::kRed);
  testMenu(s, labels, 2);
}
void Screens::drawTestRow(const ScreenState& s, const Engine&, unsigned index, int top) {
  switch (s.page) {
    case Page::TestEntry: {
      const char* const enter[] = {tr(Text::TestCopy), tr(Text::TestBlank), tr(Text::Back)};
      const char* const reset[] = {tr(Text::TestRecopy), tr(Text::TestResetBlank), tr(Text::Back)};
      item(s, index, (s.testMode ? reset : enter)[index], top);
      break;
    }
    case Page::TestHub: {
      const char* const labels[] = {tr(Text::InjectSteps), tr(Text::DateTime), tr(Text::TestScenarios), tr(Text::TestFaults),
                                    tr(Text::Diagnostics), tr(Text::TestReset), tr(Text::TestExit), tr(Text::Back)};
      item(s, index, labels[index], top);
      break;
    }
    case Page::TestSteps: {
      const char* const labels[] = {tr(Text::AddOneStep), tr(Text::AddTenSteps), tr(Text::AddHundredSteps),
                                    tr(Text::AddThousandSteps), tr(Text::AddFiveThousandSteps), tr(Text::Back)};
      item(s, index, labels[index], top);
      break;
    }
    case Page::TestScenarios: {
      const char* const labels[] = {tr(Text::ScenarioAccount), tr(Text::ScenarioNear),
                                    tr(Text::ScenarioThrow), tr(Text::ScenarioLegacy), tr(Text::Back)};
      item(s, index, labels[index], top);
      break;
    }
    case Page::TestFaults: {
      const char* const labels[] = {tr(Text::FaultNone), tr(Text::FaultBefore), tr(Text::FaultAfter), tr(Text::FaultLoad), tr(Text::Back)};
      item(s, index, labels[index], top, 0, false, index < 4 && uint8_t(s.testFault) == index ? tr(Text::InUse) : nullptr);
      break;
    }
    case Page::TestExit: {
      const char* const labels[] = {tr(Text::ConfirmExit), tr(Text::Back)};
      item(s, index, labels[index], top);
      break;
    }
    default: break;
  }
}
void Screens::drawTest(const ScreenState& s, const Engine& e, bool full) {
  if (s.page == Page::TestDiagnostics) {
    const auto& d = s.testDiagnostics;
    const auto& v = e.view();
    const bool changed = full || d.ramOnly != snap_.testDiagnostics.ramOnly ||
                         d.officialBytes != snap_.testDiagnostics.officialBytes ||
                         d.testBytes != snap_.testDiagnostics.testBytes ||
                         d.capacityBytes != snap_.testDiagnostics.capacityBytes ||
                         d.availableBytes != snap_.testDiagnostics.availableBytes ||
                         d.cleanupFailed != snap_.testDiagnostics.cleanupFailed ||
                         d.seedFailed != snap_.testDiagnostics.seedFailed ||
                         v.schema != snap_.schema ||
                         v.encounter.ruleVersion != snap_.ruleVersion ||
                         v.sequence != snap_.sequence ||
                         s.testFault != snap_.testFault;
    if (full) testDiagnostics(s, e);
    else if (changed) {
      WriteScope sc;
      clearRegion(40, 100, 388, 204, theme::kBackground);
      testDiagnostics(s, e);
      glyph(theme::Icon::Back, 112, 52, theme::kMuted);
    }
    snap_.testDiagnostics = d;
    snap_.schema = v.schema;
    snap_.ruleVersion = v.encounter.ruleVersion;
    snap_.sequence = v.sequence;
    snap_.testFault = s.testFault;
    return;
  }
  if (full) {
    switch (s.page) {
      case Page::TestEntry: testEntry(s); break;
      case Page::TestHub: testHub(s, e); break;
      case Page::TestSteps: testSteps(s); break;
      case Page::TestScenarios: testScenarios(s); break;
      case Page::TestFaults: testFaults(s); break;
      case Page::TestExit: testExit(s); break;
      default: break;
    }
  } else {
    if (s.testFailure != snap_.testFailure &&
        (s.page == Page::TestEntry || s.page == Page::TestHub ||
         s.page == Page::TestSteps || s.page == Page::TestExit)) {
      WriteScope sc;
      clearRegion(40, 162, 388, 40, theme::kBackground);
      if (s.testFailure) {
        const Text msg = s.page == Page::TestEntry ? Text::TestSwitchFailed :
                         s.page == Page::TestHub ? Text::PreviousActionFailed :
                         s.page == Page::TestSteps ? Text::InjectStepsFailed : Text::TestExitFailed;
        alert(tr(msg), s.page == Page::TestExit ? 185 : 180, theme::kRed);
      }
    }
    if (s.page == Page::TestFaults && s.testFault != snap_.testFault &&
        menu_layout::first(snap_.selected) == menu_layout::first(s.selected)) {
      const unsigned first = menu_layout::first(s.selected);
      for (unsigned idx : {unsigned(snap_.testFault), unsigned(s.testFault)}) {
        if (idx >= first && idx < first + menu_layout::kPageSize &&
            idx != s.selected && idx != snap_.selected) {
          WriteScope sc;
          const auto b = menu_layout::rowBounds(idx - first, menu_layout::kTop);
          clearRegion(b.x, b.y, b.width, b.height, theme::kBackground);
          drawTestRow(s, e, idx, menu_layout::kTop);
        }
      }
    }
    updateListRows(s, e, menu_layout::kTop, hasPager(s.page), menuCount(s, e), &Screens::drawTestRow);
  }
  snap_.testFailure = s.testFailure;
  snap_.testFault = s.testFault;
}
} }
