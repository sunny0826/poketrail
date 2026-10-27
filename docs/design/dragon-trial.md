# 龙穴试炼

龙穴试炼是 issue #2 的独立连续战斗活动，与传说遗迹没有解锁、线索、顺序或目标上的依赖。它只包含迷你龙 #147 与幼基拉斯 #246 两条试炼，复用普通节奏战斗、投球、捕获动画和双槽存档事务，只新增固定目录、活动状态、Engine 命令与页面。

## 玩家流程

1. 当全部 251 种 `captureCounts` 的总和为 6 的倍数且大于 0 时，主菜单“龙穴试炼”里的两条试炼都可选择。
2. 选择一条后从第 1 阶段开始。每个阶段是一场 12 拍的节奏战斗；阶段之间保留剩余生命。
3. 非最终阶段胜利后三选一：恢复 1 格生命（满血不可选）、普通窗口 +30ms 且完美窗口 +10ms、后续每场首次漏防不扣生命（只能获得一次）。
4. 最终阶段胜利进入现有投球流程。战败只重试当前阶段；两次投球失败只重试最终阶段。
5. 每种只能成功捕获一次。完成状态只从统一 `captureCounts` 派生，活动本身不保存完成标志。

捕获总数 0–5、7–11 等非倍数状态不能开始新试炼，页面显示还差几次捕获。初始赠送、进化、`owned`/`registered` 与步数都不计入；历史多次捕获按实际次数累加。已经开始的对战、强化、投球和结果流程不因总数变化中断。在 6 次捕获时完成一条试炼后总数为 7，另一条暂时关闭，直到 12 次时再次开放。

## 固定目录

来源是 [assets/dragon-trials.json](../../assets/dragon-trials.json)，由 [tools/build_dragon_trials.py](../../tools/build_dragon_trials.py) 校验并生成 [dragon_trial_catalog_generated.hpp](../../firmware/apps/20_pokedex/dragon_trial_catalog_generated.hpp) 与 `.cpp`。生成器固定检查两条试炼、物种、阶段数、每阶段生命与节拍、12 拍、180/70ms 窗口、交替节奏、强化步进与上限；生成物带来源 SHA-256，`--check` 同时核对来源和两份产物，失败不写文件。固件只消费生成表。

| 试炼 | 物种 | 阶段 | 敌方生命 | 节拍 |
| --- | --- | --- | --- | --- |
| 迷你龙 | 147 | 2 | 3 / 4 | 1500 / 1450ms |
| 幼基拉斯 | 246 | 3 | 3 / 4 / 5 | 1500 / 1450 / 1350ms |

两场试炼的玩家参数相同：3 格生命、每场 12 拍、普通 ±180ms、完美 ±70ms、交替攻防。强化上限为普通窗口 +180ms、完美窗口 +60ms，步进固定 30/10ms，护盾上限 1。窗口强化按成对步进保存，非法超限或非步进值由 `validate` 拒绝；实际窗口在战斗时间轴和阶段页上读取生效值。

`dragon_trial_catalog.hpp` 是非生成适配层：物种/阶段查询、资格与剩余次数、强化可用性、`BattleSpec` 组装、结果可重试判定。Engine、校验和页面都读它，不写物种分支。

## 状态与事务

`Save` 继续使用 schema3 的现有活动块，不改变编码形状与长度：

- `Encounter.origin = DragonTrial`，`species` 与 `opponent` 都是该试炼目标；最终捕捉账户取 `species`。
- `DragonTrialState` 保存 `active/trialId/stageIndex/stageStartHp/normalBonus/perfectBonus/guard`。`stageIndex` 始终指向“即将开始”的阶段；非最终阶段胜利时先提交下一阶段索引和剩余生命，强化选择再在同一快照内提交 HP 或窗口/护盾。
- 同一次多阶段尝试只分配一次活动 ID/seed，活动 ID 计入 `activityEncounterCount`，不扣 5000 探索步。
- `Result` 追加 `DragonUpgrade`（等待强化）与 `DragonCleared`（已捕获重打胜利、无奖励）；仍是单字节，schema1/2 仍限旧值。

Engine 独占以下命令，页面只读：

| 命令 | 前置 | 效果 |
| --- | --- | --- |
| `beginDragonTrial` | Walking、捕获总数为 6 的倍数 | 创建第 1 阶段快照与活动遭遇 |
| `chooseDragonUpgrade` | 结果页处于 `DragonUpgrade` | 只应用一个可用强化并提交下一阶段 |
| `retryDragonTrial` | 结果已关闭且可重试 | 败/撤退复用同一 ID/seed；投球失败创建新 ID/seed |
| `acknowledgeActivityResult` | 活动结果已关闭 | 清理遭遇与试炼快照，返回龙穴 |

普通 `acknowledgeResult` 拒绝活动遭遇。探索账只把普通现代遭遇计入 5000 步费用；活动期间走 6000 步只入账，活动关闭后的下一次普通遭遇只扣一次费用。

## 失败、重启与捕捉

- 战败或撤退只关闭当前阶段。快照保留阶段起始 HP、已选强化与阶段索引；重试复用同一活动 ID/seed。
- 中途重启时 `Engine::begin` 把 `Phase::Battle` 恢复为 `EncounterReady`，`startBattle` 按目录和快照恢复当前阶段，而不是默认或第一阶段。
- 最终阶段胜利后 `Phase::ThrowReady`。两次投球失败（或放弃捕捉）后重试会关闭旧结果、创建新活动 ID/seed、清空投球记录并保留最终阶段快照；前置阶段和强化不重复。
- 零球保留 `ThrowReady`，回首页走路获得球后从龙穴入口返回同场；重启同样保留。
- 保存失败继续冻结同一 candidate；`retrySave` 成功后只提交一次阶段推进或投球结果，提交前不显示奖励。
- 最终阶段胜利时若目标已捕获，直接 `DragonCleared`，跳过投球与全部奖励；图鉴、首捕卡、球和 `captureCounts` 都不变。

## 校验不变量

`validate()` 在活动块上联合检查：

- `DragonTrialState.active` 与 `Encounter.origin == DragonTrial` 必须同时成立；`trialId` 必须是 1/2，阶段在该试炼范围内，`stageStartHp` 为 1–3。
- 当前 `species`/`opponent` 等于该试炼制目标；活动 ID 晚于 legacy 边界，`activityEncounterCount` 不超过现代 ID 边界。
- bonus 必须是 30/10 的成对步进且不超过 180/60，`guard` 为 0/1。
- 强化预算：累计强化数为 `normalBonus / 30 + guard`。等待强化时不超过 `stageIndex - 1`，其他当前阶段快照不超过 `stageIndex`；阶段 0 必须是满生命、无窗口、无护盾的初始快照。治疗只改变当前阶段生命、不进入累计计数，所以该值只是上界，不能替换成“选择次数必须相等”。
- 等待强化必须至少剩一个可选强化。固定目录的合法等待总是满生命或伤血，再加未封顶的窗口或未取得的护盾；三项全禁用无法由公开命令产生。
- `phase` 与 `result` 组合：`EncounterReady`/`Battle` 无投球、无结果；`ThrowReady` 只在最终阶段、`Won`、未被捕获；`DragonUpgrade` 只在下阶段快照、未投球、有可选项；`DragonCleared` 必须已捕获；`Lost`/`Retreated`/`Escaped`/`Captured` 的投球数与阶段一致。
- 旧 schema3 payload 在活动块前结束读为 inactive 全零；schema1/2 带任何活动字段、未知 origin、截断或非法 stage/bonus/guard 都被拒绝。

## 页面

主菜单第三页新增“龙穴试炼”（传说遗迹之后、返回探索之前）。页面只保留必要文字，按 466 圆屏检查：

| 页面 | 内容 |
| --- | --- |
| 龙穴试炼 | 捕获总数、本轮开放或还差几次、两条试炼与状态文字、返回 |
| 阶段准备 | 目标缩略图、第几阶段、携带生命、敌方生命与节拍、实际窗口与护盾、开始对战/返回 |
| 强化 | 三个选项与结果文字（已满、已达上限、已获得、下阶段窗口），不可用项同时变灰并由 Engine 拒绝 |
| 战斗 | 复用节奏战斗；时间轴读取实际普通/完美窗口，血条支持 3–5 点 |
| 投球 | 复用普通投球、零球等待和捕获动画 |
| 结果 | 战败/撤退/未能捕获/已放弃提供“重试本阶段 / 返回龙穴”；已捕获重打显示无奖励；捕获成功走统一首捕卡 |

实体按键、触摸、释放门、暂停、熄屏唤醒和零球等待沿用普通流程。返回龙穴只从结果页确认或从阶段页返回；强化页必须做出选择。

## 验证入口

- `test-adventure`：`test/support/adventure_test/dragon_cases.hpp`、`dragon_upgrade_cases.hpp` 与 `dragon_retry_cases.hpp` 覆盖资格 0/5/7/11/6/12/18、赠送/进化/步数不计入、两条阶段表与生命携带、三种强化与上限、强化预算与阶段 0 初始态、重复输入、败/投球/保存失败重试、暂停 checkpoint 写出的 `Phase::Battle` 重启恢复、零球、已捕获重打、混合账、遗迹独立性和非法字段。等待强化与各阶段正例都取自公开命令。
- `test_save_codec`：龙穴 EncounterReady 快照与可到达的等待强化快照的编码/解码往返，以及 origin/stage/bonus/guard/phase/预算的语义拒绝与重算 CRC 的截断拒绝。
- `test-render` 的 `test_adventure_render`：`adventure_ui/dragon_cases.hpp` 与 `dragon_extra_cases.hpp` 用真实 Runtime/M5GFX 覆盖资格不足与开放、两条试炼、2/3 阶段、真实可达的强化禁用态、实际窗口、暂停、战败重试、投球失败重试、零球后补球恢复与捕获、幼基拉斯成功捕获、已捕获重打；文案用 efont 像素 oracle，全部帧要求亮像素在半径 228 内。
- `check-dragon-trials`：生成器 `--check` 与来源/生成物单元测试。
