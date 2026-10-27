#include "screens.hpp"
#include "board.hpp"
#include "test_console.hpp"
#include "test_mode.hpp"
#include <cstdio>

namespace pokedex { namespace ui {
using namespace adventure;

void Screens::testMenu(const ScreenState& s, const char* const* labels,
                       unsigned count, int top) {
  for (unsigned i = 0; i < count; ++i) item(s, i, labels[i], top);
}
void Screens::testEntry(const ScreenState& s) {
  const char* const enter[] = {"复制正式存档", "空白测试档", "返回"};
  const char* const reset[] = {"重新复制正式存档", "重置为空档", "返回"};
  title(s.testMode ? "重置测试档" : "进入测试模式",
        s.testMode ? "只替换设备测试档" : "正式存档不变", theme::kPurple);
  if (s.testFailure) alert("测试模式切换失败", 180, theme::kRed);
  else line(s.testMode ? "测试档之外不会写入" : "使用独立测试存档", 180, theme::kMuted);
  testMenu(s, s.testMode ? reset : enter, 3);
}
void Screens::testHub(const ScreenState& s, const Engine&) {
  const char* const labels[] = {"注入步数", "日期与时间", "构造状态", "故障开关",
                                "诊断", "重置测试档", "退出测试模式", "返回"};
  title("测试模式", "仅写入测试存档", theme::kPurple);
  if (s.testFailure) alert("上一次操作未完成", 180, theme::kRed);
  else line("正式进度不受影响", 180, theme::kMuted);
  testMenu(s, labels, 8);
}
void Screens::testSteps(const ScreenState& s) {
  const char* const labels[] = {"+1 步", "+10 步", "+100 步", "+1000 步", "+5000 步", "返回"};
  title("注入步数", "不消费真实 IMU", theme::kPurple);
  if (s.testFailure) alert("当前无法注入步数", 180, theme::kRed);
  else line("按预设追加到测试进度", 180, theme::kMuted);
  testMenu(s, labels, 6);
}
void Screens::testScenarios(const ScreenState& s) {
  const char* labels[] = {testScenarioLabel(TestScenario::AccountBalls),
                          testScenarioLabel(TestScenario::NearEncounter),
                          testScenarioLabel(TestScenario::CurrentThrow),
                          testScenarioLabel(TestScenario::LegacyEncounter), "返回"};
  title("构造状态", "确认后重新加载测试档", theme::kPurple);
  line("写入前校验编码与规则", 180, theme::kMuted);
  testMenu(s, labels, 5);
}
void Screens::testFaults(const ScreenState& s) {
  const char* const labels[] = {"无故障", "写入前失败", "写入后失败", "读取失败", "返回"};
  title("故障开关", "仅作用于测试提交", theme::kPurple);
  line("切换后立即生效", 180, theme::kMuted);
  for (unsigned i = 0; i < 5; ++i)
    item(s, i, labels[i], menu_layout::kTop, 0, false,
         i < 4 && uint8_t(s.testFault) == i ? "使用中" : nullptr);
}
void Screens::testDiagnostics(const ScreenState& s, const Engine& e) {
  char text[100];
  title("测试诊断", "", theme::kPurple);
  const auto& d = s.testDiagnostics;
  line(d.ramOnly ? "模式 测试 · RAM-only" : "模式 测试 · 文件系统", 116, theme::kGold);
  snprintf(text, sizeof(text), "schema %u · rule %u", unsigned(e.view().schema),
           unsigned(e.view().encounter.ruleVersion));
  fitLine(text, 148, 320, theme::kText, false);
  snprintf(text, sizeof(text), "sequence %llu",
           static_cast<unsigned long long>(e.view().sequence));
  fitLine(text, 176, 320, theme::kText, false);
  snprintf(text, sizeof(text), "正式 %u B · 测试 %u B", unsigned(d.officialBytes),
           unsigned(d.testBytes));
  fitLine(text, 204, 320, theme::kText, false);
  snprintf(text, sizeof(text), "容量 %u KB · 可用 %u KB", unsigned(d.capacityBytes / 1024),
           unsigned(d.availableBytes / 1024));
  fitLine(text, 232, 320, theme::kText, false);
  snprintf(text, sizeof(text), "故障 %s", testFaultLabel(s.testFault));
  fitLine(text, 260, 320, theme::kText, false);
  snprintf(text, sizeof(text), "cleanup %s · seed %s",
           d.cleanupFailed ? "失败" : "正常", d.seedFailed ? "失败" : "正常");
  fitLine(text, 288, 320, d.cleanupFailed || d.seedFailed ? theme::kRed : theme::kMuted, false);
  line("顶部返回", 330, theme::kMuted, false);
}
void Screens::testExit(const ScreenState& s) {
  const char* const labels[] = {"确认退出", "返回"};
  title("退出测试模式", "删除设备测试档，正式档不受影响", theme::kRed);
  if (s.testFailure) alert("退出失败，仍处于测试模式", 185, theme::kRed);
  else line("重启后回到正式进度", 180, theme::kMuted);
  testMenu(s, labels, 2);
}
} }
