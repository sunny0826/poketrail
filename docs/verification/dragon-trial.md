# 龙穴试炼验收

本文件记录 issue #2 龙穴试炼当前轮次的验收数值、证据与未验证项。设计见[龙穴试炼](../design/dragon-trial.md)，共享存档说明见[存档迁移](../design/save-migration.md)。

## 当前状态：R2 审查修复与首捕卡联合布局（2026-10-27）

两条试炼、固定目录、资格、阶段推进、三选一强化、战斗/投球重试、零球、已捕获重打、schema3 校验、Runtime 页面与文档均已落地。R2 独立验证为 PASS；独立审查提出的 R2-R1（无法完成的强化存档）与 R2-R2（全禁用页面覆盖误述）已修复。复审发现的 R2-R3（上移里程碑后覆盖路线文字）在本轮由路线、每日、累计三行联合布局修复，并用五张真实捕获帧的联合 glyph 断言保护。指定检查全部 exit 0，相对 R1 基线没有新增失败。

- 分支 `dragon-trial`，基线 `legendary-ruins` @ `28f0ce73a232a2defbe0805b98814b21aa9fd2c3`；未 commit / push / PR，未操作设备，未再派 Agent。
- 未修改 issue #1 的提交历史、遗迹玩法、其他 assets/生成器或设备配置。
- 本轮证据目录 `.pio/verification/r2-capture-layout/`，`before/` 保留联合断言在旧位置的失败与旧帧，`after/` 为修复后的命令、五帧 PNG 与逐行像素范围；上一轮 `.pio/verification/r2-review-fix/` 保留不改。

## 固定目录与生成

- 来源 [assets/dragon-trials.json](../../assets/dragon-trials.json)，生成器 [tools/build_dragon_trials.py](../../tools/build_dragon_trials.py)，产物 `dragon_trial_catalog_generated.hpp/.cpp`。
- 生成器校验两条试炼、物种 147/246、阶段数 2/3、HP 3/4 与 3/4/5、节拍、12 拍、180/70ms、交替节奏、强化步进 30/10 与上限 180/60、guard 1；产物带来源 SHA-256，`--check` 失败即非零退出。
- `check-dragon-trials` 与 `check-adventure-catalog` 都会执行生成检查；4 项来源/产物单元测试通过。

## 命令结果

证据目录 `.pio/verification/r2-capture-layout/after/`。所有命令以 `rtk proxy mise run` 执行。

| 命令 | 结果 |
| --- | --- |
| `test-adventure` | exit 0；adventure **434,247**；codec 43,542（v3=21,520）；modern 228；regressions 48；progression 17,037；migration 368；unique 38,377；`sizeof(Save)=29,488`、`sizeof(Engine)=66,288`；16 KiB 启动栈通过 |
| `test-storage` | exit 0；file_store 124、device_store 30、allocation 8；`encoded_bytes=21,520`、`two_slots=43,040`、`workspace_bytes=103,264`、`peak_owned_heap=132,752` |
| `test` | exit 0；201 viewer checks |
| `test-render` | exit 0；12 套 0 failures：1614、10169、1061、**11,612 adventure UI**、89、383、25217、88369、8368、522751、10685、219144 |
| `check-adventure-catalog` | exit 0；251 IDs、124 普通根、122 进化边；16 项 unittest 通过（含 4 项龙穴来源） |
| `check-dragon-trials` | exit 0；生成物与来源一致，4 项 unittest 通过 |
| `build` | exit 0；增量构建（仅 `screens_home.cpp` 重编并链接）；RAM 97,780 / 327,680（29.8%）；Flash **7,986,825** / 13,107,200（60.9%）；`firmware.bin` **7,987,232** 字节 |
| `check-startup-stack` | exit 0；`Engine::begin` 局部帧 48 字节（limit 512） |
| `git diff --cached --check` | exit 0 |

静态 RAM 与 R1 相同；Flash 比 R1 最终拆分后的 7,981,741 增加 **5,084** 字节，`firmware.bin` 比 7,982,144 增加 5,088 字节。R2 首轮曾引用早期 R1 数字 7,981,397，本轮统一改用拆分后的最终基线。编码长度 21,520、`sizeof(Save)` 与 `sizeof(Engine)` 均不变。

## 行为覆盖

领域用例 `test/support/adventure_test/dragon_cases.hpp`、`dragon_upgrade_cases.hpp` 与 `dragon_retry_cases.hpp`：

- 捕获总数 0/5/7/11 不能开始，6/12/18 两条均可选；赠送、全拥有/曾获得、培养步与 10 万步都不改变资格；6 完成一条后总数为 7，另一条关闭。
- 迷你龙 2 阶段（3/4、1500/1450ms）与幼基拉斯 3 阶段（3/4/5、1500/1450/1350ms）逐项与目录一致；三阶段用例携带 2 HP、治疗回到 3、再用护盾吸收一次漏防。
- 满血拒绝治疗、窗口 30/10 成对上限 180/60、护盾只能一次、重复选择返回 `InvalidPhase`/`InvalidInput`，选择立即提交（commit 计数增加）。
- 强化预算：等待强化时累计强化数不超过 `stageIndex - 1`，其他当前阶段快照不超过 `stageIndex`；阶段 0 必须是 3 HP、0/0、无护盾的初始快照；等待页必须至少剩一个可选强化。治疗只改变当前阶段生命、不留累计痕迹，因此该计数是上界而不是选择总数的精确值。正例全部由公开战斗与选择命令产生并逐阶段经过真实 codec 往返；超预算与阶段 0 改写被 `validate`、`encode` 拒绝。审查给出的全禁用组合同时超出预算，等待页“至少一个可选项”的断言与 Engine/页面共用同一 helper。
- 审查给出的“满血 + 180/60 + guard=1、等待第 1 阶段强化”存档改为拒绝用例：同一字段在合法帧内重算 CRC 后 `decode` 为 `Corrupt`，`Engine::begin` 返回 `StorageError` 且不 ready，不再出现“载入成功后三个按钮都拒绝”。
- 战败复用同一 ID/seed 与阶段快照；暂停后 checkpoint 真正写出 `Phase::Battle` 存档（回读确认），重启把它恢复为同一阶段的 `EncounterReady` 并按 4 HP / 210/80ms / guard 重开；写前/写后保存失败都冻结同一 candidate，`retrySave` 只推进一次且不重复扣球或抽签。
- 两次投球失败新建 ID/seed、清空投球记录、保留最终阶段快照与强化；零球跨重启保留 ThrowReady；已捕获重打直接 `DragonCleared`，不计图鉴/首捕/球/捕获次数。
- 活动走 6000 步只入账，遗迹解锁与目标不受影响；活动关闭后普通遭遇只扣一次 5000 步，ID/A 序列连续。
- 非法 origin、trial/stage、起始生命、bonus、guard、phase/result、预算与阶段 0 初始态被 `validate` 与 `encode` 拒绝；旧 schema3 默认无试炼，schema1/2 带活动字段被拒。

存档用例：龙穴 EncounterReady 快照与可到达的等待强化快照经真实 codec 往返；对快照字节重算 CRC 后修改起始生命、bonus、active 或强化预算仍被语义校验拒绝。

Runtime 额外覆盖：零球后在同一 Runtime 继续走满 1000 步、领到一球、从龙穴入口恢复同一场并投球捕获；幼基拉斯最终阶段从投球页走到统一首捕卡。二者都复用现有投球与捕获路径，不新增弱断言。

## Runtime 截图与读图

目录 `.pio/verification/r2-capture-layout/after/render/ui/test_adventure_render/`，共 **28 张** `dragon-*.ppm`；五张首捕帧 PNG 在 `.pio/verification/r2-capture-layout/after/png/`，路线与里程碑的逐行像素范围在 `layout-rows.log` 与 `time-route-gap.log`。上一轮的 OCR 摘要保留在 `.pio/verification/r2-review-fix/after/ocr/`。

| 覆盖 | 帧 |
| --- | --- |
| 资格不足 | `dragon-blocked-zero/five/seven/eleven` |
| 两条试炼与开放窗口 | `dragon-hub-6`、`dragon-hub-12`、`dragon-stage-dratini`、`dragon-stage-larvitar` |
| 阶段与强化 | `dragon-stage2-upgraded`、`dragon-upgrade-full/hurt/guard/disabled` |
| 战斗与暂停 | `dragon-battle-stage1/upgraded/final`、`dragon-pause` |
| 失败与重试入口 | `dragon-defeat`、`dragon-escaped` |
| 投球与零球 | `dragon-throw-ready`、`dragon-throw-larvitar`、`dragon-zero-balls`、`dragon-zero-balls-resume`、`dragon-zero-balls-captured` |
| 末态 | `dragon-captured`、`dragon-captured-larvitar`、`dragon-replay-cleared`、`dragon-hub-after-capture` |

核对方法与结果：

- 28 张帧逐像素检查所有非零像素仍在半径 228 内，`outside_lit=0`。
- 文案断言使用真实 efont 像素 oracle：目标行 trailing note（`还差 N 次`、`可挑战`、`已完成`）、阶段页 `第 1 / 2 阶段` 与 `窗口 210/80ms · 护盾 1`、强化页 `已满/可选/已获得`、`210/80`、投球页 `暂无精灵球`、重打页 `不改变图鉴与捕获次数`。这些断言会因面板覆盖或文字改色立即失败。
- `dragon-upgrade-disabled` 由真实流程生成：幼基拉斯第一阶段胜利后选护盾，第二阶段满血胜利后等待第二次选择；此时治疗与护盾显示禁用，窗口行仍可选并显示 `210/80`。全禁用组合在固定目录中不可达，也不再作为合法存档或页面覆盖的通过样例。
- OCR 复核（chi_sim，帧先反色放大 3 倍）读出：`龙穴试炼 / 捕获 6 的倍数 · 本轮开放 / 已捕获 6 次 / 迷你龙 可挑战 / 幼基拉斯 可挑战 / 返回`；`还差 6 次` 与 `已捕获 0 次`；`第 1 / 2 阶段 / 生命 我方3 / 3 / 敌方 3 · 节拍 1.50 秒 / 窗口 180/70ms / 开始对战 / 返回龙穴`；`窗口 210/80ms`；`选择强化 / 第 2 阶段完成 / 生命 我方3 / 3 / 窗口 180/70ms / 下一阶段 第 3 阶段 / 恢复 1 生命 已满 / 窗口 +30/10 210/80 / 首次漏防保护 已获得`；`试炼失败 / 第 2 / 2 阶段 · 可重试 / 生命与强化均已保留 / 重试本阶段 / 返回龙穴`；`未能捕获 / 最终阶段 · 可重试`；`已完成 · 无奖励 / 不改变图鉴与捕获次数 / 不消耗精灵球`；`已捕获 7 次 / 迷你龙 已完成 / 幼基拉斯 还差 5 次`；零球页 `每 1000 步获得一球`，恢复页 `本场剩余 2 次`；首捕页 `捕捉成功 / 确认结果 返回龙穴`。
- 首捕卡的四行现在依次为：时间（居中 270，字形 262–277）、路线（居中 286，字形 278–293）、每日里程碑（304，字形 296–311）、累计里程碑（330，字形 322–337）；确认面板从 342 开始，四行与面板均不重叠。复审时路线仍在 290（字形 282–297），与每日行的 296–311 重叠 2 像素；普通首捕、遗迹首领、迷你龙、幼基拉斯和零球补球五张真实捕获帧的像素范围一致。联合断言同时比对路线、每日、累计三行的完整 efont glyph：在旧位置运行时 `first-ball-capture` 的 `capture card keeps the whole route row` 失败，改为 286 后五帧全部通过。逐行范围见 `layout-rows.log` 与 `time-route-gap.log`。
- 图像工具在本会话仍不能直接内联解码图片，以上“读图”由像素 oracle、圆屏统计、逐帧像素审计和 OCR 组成，不替代维护者的完整视觉判断。

## 回归与菜单变化

主菜单新增“龙穴试炼”后从 9 项变为 10 项，“返回探索”移到第 4 页。`test_home_render`、`test_touch_menu_render`、`test_thumbnail_render` 与 `test/support/home_ui/*` 已同步为 10 项/4 页；`test-render` 全部 12 套通过。普通战斗与练习轨迹没有改动。

## 未验证项

1. 实体设备：真实按键/触摸时延、466 圆屏观感、连续战斗手感、暂停与熄屏唤醒的硬件行为、断电恢复、动态堆峰值和完整任务栈未测。主机截图与 OCR 不是设备验收。
2. 真实 6/12/18 次捕获需要实机长流程；主机用例用合法历史捕获账户构造资格，未替代真实步数与投球节奏。
3. “三个强化都不可用”的页面状态在固定目录中不可达：第二阶段等待时至少窗口仍可强化，窗口封顶需要累计六次强化而当前每个非最终阶段只给一次选择。本轮不再用人工组合冒充合法页面；该组合被校验拒绝，页面只覆盖真实可达的“治疗/护盾禁用、窗口可选”状态。`finishDragonBattle` 的无可用强化自动推进分支仍只在代码层存在，未被 UI 触达。
4. 设计预览脚本 `test/test_ui_design_previews.py` 的页面覆盖缺口在基线已存在，本轮不运行、不计入通过项，也未生成设计预览帧。
5. `DragonUpgrade` 等新结果值只对 schema3 合法；未用旧固件读取新存档（现有兼容方向为新固件读旧档）。
6. 真实断电发生在阶段提交或投球提交之间的窗口未测；只验证了写前/写后存储错误与同一 candidate 重试。主机 checkpoint 明确写入 `Phase::Battle`，但真实掉电的写入原子性仍由双槽与文件系统测试间接支撑。
7. 首捕卡路线与里程碑的上移只按 468×468 主机渲染核对；实体 466 圆屏的观感与触控命中未重测。
