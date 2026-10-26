# 传说遗迹验收

本文件记录当前轮次的验收数值、修复状态和未验证项。首轮验收与后续修复的历史数字保存在[历史轮次](legendary-ruins-rounds.md)，不代表当前代码。设计见[传说遗迹](../design/legendary-ruins.md)。

传说遗迹按固定目录实现：三条线索（10/54/74，森林胜利、海边两次 Perfect、洞窟无伤）、顺序海边→森林→洞窟、11 个目标首领参数、失败只重试当前阶段。Engine 独占线索提交、顺序校验、目标选择、首领推进、结果确认与捕捉结算；活动 ID 与普通遭遇共用序列但不扣 5000 步。设计见[传说遗迹](../design/legendary-ruins.md)。

## 当前状态：R1 文件拆分后（2026-10-27）

文件职责拆分后的最新 clean build 使用 193 个编译单元，RAM 97,780 / 327,680（29.8%），Flash 7,981,741 / 13,107,200（60.9%），`firmware.bin` 7,982,144 字节。`test-adventure` 的 44 行结果与 12 套渲染计数和审查修复轮一致，272 张渲染帧逐文件相同；pre-commit 通过。证据在 `.pio/verification/r1-split/`。

### 审查修复记录

独立审查确认 7 项（1 P1、4 P2、2 P3），本轮逐项修复：

1. 结果菜单的绘制条目、`menuCount`、`menuTop` 与按键动作统一改用 `ruinsActivityRetryable`。已捕获目标重打后 `Lost`/`Retreated` 仍显示“重试本阶段 / 返回遗迹”两项，重试会建立新尝试而不是确认失败；绘制移入数组仍有效的分支内直接画完并返回，消除局部标签数组结束生命周期后的读取。普通结果仍是单行“确认结果 · 继续探索”。
2. 遗迹“继续遭遇”按 `Phase` 分派：`ThrowReady` 清除同场 deferred 标记并进入投球页，只有 `EncounterReady` 才进入遭遇页。零球与已用一球暂离后都能从遗迹恢复同一遭遇，ID/seed/usedBalls 不变。
3. 目录新增只读的 `ruinsClueOrderPosition()`（海边 1、森林 2、洞窟 3）。线索结果页、线索列表行与选中详情显示该位次，解谜提示改为“按线索标出的顺序”。收集顺序仍任意，答案不变。
4. 线索位与终态一致：`RuinsClue` 必须已置位本场线索，未结算、`Lost`、`Retreated` 不得置位；其他路线位保持合法。真实成功/失败快照的单字段变异在 `validate`、`encode`、重算 CRC 的 `decode` 与 `Engine::begin` 均被拒绝。
5. 遗迹首页的目标名与生命/节拍/节奏摘要上移到 y=172/196，完整位于菜单面板（上沿 208）之上、线索 chips 之下。
6. 首领可完成性表格的 `cues` 明确为从 0 开始的 `cueIndex`，并给出换算后的第 2～6 拍；失败 `cues=12` 不换算。
7. `sizeof(Engine)` 按实测改为 66288。

修复前用新回归在旧实现上取得失败：`captured-failure-menu`（`FAIL: captured retreat draws the retry action`）、`deferred-throw-resume`（`FAIL: ruins hub resumes the deferred throw, not the battle page`）、`clue-order-visibility`（`FAIL: forest clue row announces its position on the selected row`）、`order-and-capture`（`FAIL: hub keeps the whole target name`）；codec modern 在旧实现上停在 `line 22: !validate(s)`。日志在 `.pio/verification/r1-review-fix/before/`。

修复后指定命令全部 exit 0（`.pio/verification/r1-review-fix/after/`）：

| 检查 | 结果 |
| --- | --- |
| `test-adventure` | adventure 427,261；codec 43,499；modern 228；regressions 48；progression 17,037；migration 368；unique 38,377；`sizeof(Save)=29488`、`sizeof(Engine)=66288`；启动局部栈通过 |
| `test-storage` | file_store 124、device_store 30、allocation 8；`encoded_bytes=21520`、`two_slots=43040`、`sizeof_Save=29488`、`workspace_bytes=103264`、`peak_owned_heap=132752` |
| `test` | viewer 201，0 failures |
| `test-render` | 12 套均 0 failures：1614、10169、1060、**11153 adventure UI**、89、383、25217、88369、8368、522751、10685、219144 |
| `check-adventure-catalog` | 251 IDs、124 普通根、122 进化边；12 项 unittest 通过 |
| `pio run -t clean` + `build` | 全量重建 SUCCESS；RAM 97,780 / 327,680（29.8%），Flash 7,981,761 / 13,107,200（60.9%），`firmware.bin` 7,982,176 字节 |
| `check-startup-stack` | `Engine::begin` 局部帧 48 字节（limit 512） |
| `git diff --check` | exit 0，无输出 |

新增真实 Runtime/M5GFX 帧 6 张：`ruins-captured-retreat`、`ruins-captured-lost`、`ruins-resume-zero-balls`、`ruins-resume-used-ball`、`ruins-clue-positions`、`ruins-unlocked-after-order`。全部 41 张遗迹帧的非零像素仍在半径 228 内。断言逐像素比对菜单行、线索位次与首页摘要的 efont 字形；摘要行若被面板覆盖会立即失败。OCR 复核读出“目标 时拉比 / 生命 4 / 节拍 1.40 / 交替”“第2位 · 森林”“第1位 海边”“第3位 洞窟”“按线索标出的顺序”“本场剩余 1 次”。仍不替代实体设备观感与按键时延。
