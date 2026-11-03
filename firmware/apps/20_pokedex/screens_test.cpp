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
  else line(s.testMode ? tr(Text::TestWritesOnly) : tr(Text::TestSeparateSave), 180, theme::kMuted);
  testMenu(s, s.testMode ? reset : enter, 3);
}
void Screens::testHub(const ScreenState& s, const Engine&) {
  const char* const labels[] = {tr(Text::InjectSteps), tr(Text::DateTime), tr(Text::TestScenarios), tr(Text::TestFaults),
                                tr(Text::Diagnostics), tr(Text::TestReset), tr(Text::TestExit), tr(Text::Back)};
  title(tr(Text::TestMode), tr(Text::TestSaveOnly), theme::kPurple);
  if (s.testFailure) alert(tr(Text::PreviousActionFailed), 180, theme::kRed);
  else line(tr(Text::MainProgressUnchanged), 180, theme::kMuted);
  testMenu(s, labels, 8);
}
void Screens::testSteps(const ScreenState& s) {
  const char* const labels[] = {tr(Text::AddOneStep), tr(Text::AddTenSteps), tr(Text::AddHundredSteps), tr(Text::AddThousandSteps), tr(Text::AddFiveThousandSteps), tr(Text::Back)};
  title(tr(Text::InjectSteps), tr(Text::NoRealImu), theme::kPurple);
  if (s.testFailure) alert(tr(Text::InjectStepsFailed), 180, theme::kRed);
  else line(tr(Text::AddPresetSteps), 180, theme::kMuted);
  testMenu(s, labels, 6);
}
void Screens::testScenarios(const ScreenState& s) {
  const char* labels[] = {tr(Text::ScenarioAccount),
                          tr(Text::ScenarioNear),
                          tr(Text::ScenarioThrow),
                          tr(Text::ScenarioLegacy), tr(Text::Back)};
  title(tr(Text::TestScenarios), tr(Text::ReloadTestSave), theme::kPurple);
  line(tr(Text::ValidateBeforeWrite), 180, theme::kMuted);
  testMenu(s, labels, 5);
}
void Screens::testFaults(const ScreenState& s) {
  const char* const labels[] = {tr(Text::FaultNone), tr(Text::FaultBefore), tr(Text::FaultAfter), tr(Text::FaultLoad), tr(Text::Back)};
  title(tr(Text::TestFaults), tr(Text::TestCommitsOnly), theme::kPurple);
  line(tr(Text::TakesEffectNow), 180, theme::kMuted);
  for (unsigned i = 0; i < 5; ++i)
    item(s, i, labels[i], menu_layout::kTop, 0, false,
         i < 4 && uint8_t(s.testFault) == i ? tr(Text::InUse) : nullptr);
}
void Screens::testDiagnostics(const ScreenState& s, const Engine& e) {
  char text[100];
  title(tr(Text::TestDiagnostics), "", theme::kPurple);
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
  line(tr(Text::TopBack), 330, theme::kMuted, false);
}
void Screens::testExit(const ScreenState& s) {
  const char* const labels[] = {tr(Text::ConfirmExit), tr(Text::Back)};
  title(tr(Text::TestExit), tr(Text::DeleteTestOnly), theme::kRed);
  if (s.testFailure) alert(tr(Text::TestExitFailed), 185, theme::kRed);
  else line(tr(Text::RestartMainProgress), 180, theme::kMuted);
  testMenu(s, labels, 2);
}
} }
