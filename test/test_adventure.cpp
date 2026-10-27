#include "support/adventure_test/harness.hpp"
#include "support/adventure_test/clock_cases.hpp"
#include "support/adventure_test/step_cases.hpp"
#include "support/adventure_test/battle_cases.hpp"
#include "support/adventure_test/capture_cases.hpp"
#include "support/adventure_test/recovery_cases.hpp"
#include "support/adventure_test/ruins_cases.hpp"
#include "support/adventure_test/ruins_boss_cases.hpp"
#include "support/adventure_test/ruins_boss_winnable_cases.hpp"
#include "support/adventure_test/dragon_cases.hpp"
#include "support/adventure_test/dragon_upgrade_cases.hpp"
#include "support/adventure_test/dragon_retry_cases.hpp"

int main() {
  basicSteps(); battles(); battleSpec(); throwsAndRecords(); failures(); dates(); clockTransactions();
  probabilityBoundaries(); additionalRecovery(); battleCommitRecovery(); ruinsCoreCases(); ruinsBossCases();
  ruinsBossWinnableCases(); dragonCoreCases(); dragonUpgradeCases(); dragonRetryCases();
  std::printf("adventure: %u checks passed; sizeof(Save)=%zu sizeof(Engine)=%zu\n", checks, sizeof(Save), sizeof(Engine));
}
