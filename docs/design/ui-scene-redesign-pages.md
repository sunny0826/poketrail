# 全场景页面覆盖

返回[视觉设计](ui-scene-redesign.md)。下表按 [screens.hpp](../../firmware/apps/20_pokedex/screens.hpp) 的 `Page` 枚举列出当前 37 页，并链接到对应行为契约、实现或主机测试。原 251 种三路线 Pillow 设计画廊已退役清理，页面验证以真实 Runtime/M5GFX 渲染测试与 SDL E2E 为准。

| Page | 状态与验证入口 |
| --- | --- |
| `Home` | 关都～合众五地区场景（见[视觉设计](ui-scene-redesign.md#地区场景)）、遭遇／待捕捉横幅、日期与极大计数、步数缺失、恢复警告与可进化按钮；见[首页渲染](../../test/test_home_render.cpp)与[伙伴与进化 E2E](../../test/simulator/scenarios_progression.cpp) |
| `Menu` | 一屏宫格（遭遇横幅 + 6 格 + 返回探索）与恢复警告；见[菜单宫格渲染检查](../../test/test_touch_menu_render.cpp) |
| `Settings` | 设置页（日期与时间、语言 / Language、返回）；见[菜单宫格渲染检查](../../test/test_touch_menu_render.cpp) |
| `DateWelcome` | 首次日期确认与未完成校时恢复；见[按键菜单渲染](../../test/test_button_menu_render.cpp)与[时钟 E2E](../../test/simulator/scenarios_environment.cpp) |
| `Starter` | 初始伙伴选择（妙蛙种子 / 小火龙 / 杰尼龟）；见[按键菜单渲染](../../test/test_button_menu_render.cpp)与[捕获 E2E](../../test/simulator/scenarios_capture.cpp) |
| `Routes` | 两页“选择地区”（关都～合众五地区预览）；见[地区选择 E2E](../../test/simulator/scenarios_regions.cpp) |
| `Partners` | 伙伴列表、长名称与末页返回；见[菜单输出渲染](../../test/test_touch_menu_render.cpp)与[边界 E2E](../../test/simulator/scenarios_environment.cpp) |
| `Encounter` | 遭遇预告；见[捕获 E2E](../../test/simulator/scenarios_capture.cpp) |
| `Battle` | 攻击／躲避／完美判定、假动作、必杀与狂暴；见[战斗 E2E](../../test/simulator/scenarios_battle.cpp) |
| `Practice` | 捕获练习对战；见[隔离 E2E](../../test/simulator/scenarios_environment.cpp) |
| `PracticeThrow` | 练习投球（不消耗精灵球）；见[隔离 E2E](../../test/simulator/scenarios_environment.cpp) |
| `PracticeResult` | 练习结算（不收入图鉴或记录）；见[隔离 E2E](../../test/simulator/scenarios_environment.cpp) |
| `Pause` | 实战与练习暂停；见[按键菜单渲染](../../test/test_button_menu_render.cpp) |
| `Throw` | 瞄准蓄力与三幕捕获动画；见[捕获渲染](../../test/test_capture_render.cpp)与[捕获规格](ui-scene-redesign-motion.md) |
| `Result` | 首捕、战败、撤退、已捕获后放弃、放弃新捕捉、迁移结清与离开；见[捕获 E2E](../../test/simulator/scenarios_capture.cpp)与[迁移 E2E](../../test/simulator/scenarios_migration.cpp) |
| `Records` | 2×2 记录宫格；见[菜单宫格渲染检查](../../test/test_touch_menu_render.cpp) |
| `FirstCaptures` | 永久首捕卡与初始赠送空态；见[捕获 E2E](../../test/simulator/scenarios_capture.cpp) |
| `Recent` | 最近捕捉；见[按键菜单渲染](../../test/test_button_menu_render.cpp)与[捕获 E2E](../../test/simulator/scenarios_capture.cpp) |
| `Days` | 每日步数与每日里程碑；见[页面实现](../../firmware/apps/20_pokedex/screens_records.cpp) |
| `Achievements` | 累计里程碑（已达成 / 未达成）；见[页面实现](../../firmware/apps/20_pokedex/screens_records.cpp) |
| `Clock` | 日期与时间浏览、编辑与校时错误恢复；见[校时渲染](../../test/test_clock_sync_render.cpp) |
| `Error` | 保存重试、未初始化、坏档、较新版本与画面不可用；见[启动渲染](../../test/test_startup_render.cpp)与[冒险渲染](../../test/test_adventure_render.cpp) |
| `Viewer` | 图鉴浏览（当前拥有 / 曾经获得 / 未获得）、互动与动画内存不足；见[图鉴渲染](../../test/test_render.cpp)与[边界 E2E](../../test/simulator/scenarios_environment.cpp) |
| `PartnerDetail` | 进化就绪、已保存、最终形态、目标已拥有与步数不足；见[伙伴 E2E](../../test/simulator/scenarios_progression.cpp) |
| `CarriedPartner` | 当前携带者、培养账户、切换入口、进化费用与不可用原因；见[玩法契约](walking-progression.md)与[伙伴 E2E](../../test/simulator/scenarios_progression.cpp) |
| `EvolutionConfirm` | 进化确认；见[伙伴 E2E](../../test/simulator/scenarios_progression.cpp) |
| `Evolution` | 保存成功后的真实进化动画；见[动画契约](evolution-animation.md)与[进化 E2E](../../test/simulator/scenarios_progression.cpp) |
| `EvolutionPreviewTargets` | 伊布七分支、无畏小子三分支预览选择；见[预览契约](evolution-animation.md)与[预览隔离 E2E](../../test/simulator/scenarios_progression.cpp) |
| `EvolutionPreview` | 不改变账户、结束返回原图鉴条目的预览动画；见[预览契约](evolution-animation.md)与[预览隔离 E2E](../../test/simulator/scenarios_progression.cpp) |
| `TestEntry` | 测试模式进入／重置时选择正式快照或空档；见[测试模式指南](../usage-test-mode.md)与[测试模式渲染](../../test/test_test_mode_render.cpp) |
| `TestHub` | 测试控制台；见[页面实现](../../firmware/apps/20_pokedex/screens_test.cpp) |
| `TestSteps` | 手动步数注入；见[测试模式指南](../usage-test-mode.md) |
| `TestScenarios` | 四种固定场景构造；见[场景构造](../../firmware/apps/20_pokedex/test_console.cpp) |
| `TestFaults` | 存储故障选择；见[测试模式指南](../usage-test-mode.md) |
| `TestDiagnostics` | 模式、容量、版本与故障诊断；见[页面实现](../../firmware/apps/20_pokedex/screens_test.cpp) |
| `TestExit` | 测试模式退出确认；见[测试模式指南](../usage-test-mode.md) |
| `Language` | 中文／English 选择、当前标记、“仅本次生效”提示与待保存限制；见[语言契约](i18n.md)与[语言 E2E](../../test/simulator/scenarios_language.cpp) |
