# 步行存档迁移契约

当前玩法见[玩法契约](walking-progression.md)，状态归属和验证矩阵见[状态与验收](walking-state.md)。

## schema1 迁移到 schema2

先以各自版本的长度、CRC和语义验证两槽，再选择有效最高 sequence。旧校验继续按 500，不能直接把常量改为 5000 后读旧档。同序号内容不同、未知 schema、I/O 错误继续阻断并保留文件，不自动格式化。FileStore 的 unknownHeader 和 codec 都要支持 1、2、3，普通空档初始化条件保持不变。

从选中的完整 v1 快照确定性生成 v2，sequence 加一写非当前槽，完整回读成功才开放玩法。保留旧拥有（含高级与活动）、partner、starter、捕获次数、首捕、近期、日桶、累计和校时待确认事务。曾获得由旧拥有与捕获记录建立；培养从 0 开始，无法推测过去携带归属。球采用 base=迁移时旧总步、spent=0，从升级后新走的 1000 步开始发，不追发历史或赠球；升级说明须写清。

探索步采用以下账式：

```text
T = totalSteps
N = nextEncounterId - 1                 全部已发出的遭遇 ID 数
L = legacyChargedCount                  仍计费的旧 500 步场数
V = voidedLegacyCount                   因升级退款的旧场数，仅 0 或 1
K = L + V                              旧规则最后一个遭遇 ID
credit = T - 500*L - 5000*(N-L-V)
```

新档 L=V=0。迁移时，若不关闭旧场，L=N0、V=0；若关闭一场，L=N0−1、V=1。之后 L、V 固定，ID 不倒退或复用。校验 `0<=L`、`V∈{0,1}`、`L+V<=N`；先验证 `L<=T/500`，再验证 `N-L-V<=(T-500*L)/5000`，最后做乘减。历史捕捉上界继续参考已发 ID，不能把 L 当作所有旧场边界；旧场边界是 K。

| 迁移时当前状态 | 处理 |
| --- | --- |
| 无当前场 | 保留原探索余额，后续安全时机按 5000 建场 |
| 未结算普通根 | 迁移保留 ID、seed、路线、rule1 和已投记录；之后按下文唯一捕获兼容规则处理 |
| 未结算高级或活动种 | 写 Resolved／MigrationRetired，保留 ID及失败尝试，退原先消耗的 500 探索步 |
| 已 Resolved | 原捕获或失败原样保留，不退款、不撤销既有成果 |

退款仅针对规则升级关闭的未结算场，不是玩家主动撤退。它不增加 totalSteps，也不产生球。不能把旧信用乘十、清零或限制为 4999。例：总步10000、旧20场、余额0，升级后仍0；总步10000、旧1场、余额9500，旧场结束后开一场新场并余4500。关闭该旧场则余额10000，确认 MigrationRetired 后可逐场换两场；迁移事务不直接重抽。

旧普通场的 `legacyFreeAttempts` 等于迁移前已提交次数，过去免费不追扣，新尝试扣新库存。保留原 rule1 和 roll，不能用 rule2 重算。`0<=free<=used<=2`；仅 ID<=K 的旧场允许 rule1 与免费前缀，新场必须 rule2、free=0。新规则普通资格只作用于新场，不误杀已完成的旧高级／活动捕获。空 Encounter 规范化为 v2 空值。MigrationRetired 仅允许被退款的旧当前场（V=1、ID=K），不可带成功捕获；Released 允许0或1次失败尝试，Escaped仍要求两次。

历史首捕在总步 500–4999 仍合法，校验下界继续为 500。新建场入口保证新捕获来自 5000 消费；不能用全局提高首捕下界破坏真实历史。捕获、赠送与进化仍是不同来源。

若已存在有效 v2，不能再次迁移、重置 base、培养或余额。普通 bootGeneration 更新仍按原事务进行。迁移写前／部分写失败保留旧快照；完整写入但回执失败，重启选择新 v2。此启动冻结同一 candidate、sequence 和字节重试，不重新计算退款或随机。旧固件不能忽略新版槽继续玩。未持久化的按键与 RAM 队列不猜测补写；未完成校时仍走原恢复流程。

### 唯一捕获规则的旧档兼容

唯一捕获规则复用已有字段，schema和新遭遇规则均为2。加载并校验旧快照、完成必要的schema1迁移后，Engine检查未结算场的永久捕获次数。已经捕获过的普通场改为 `Resolved/Released`，保留原ID、探索余额和已投记录，不追加扣球、奖励或退款。schema1过去免费的尝试仍按原迁移规则记入 `legacyFreeAttempts`；这与高级或活动种的500步迁移退款分别处理。

关闭重复场与bootGeneration更新一起保存，保存失败沿用同一candidate重试，提交前不开放玩法。未捕获的普通场仍可继续；Battle恢复为EncounterReady，ThrowReady保持。已经Resolved的成功或失败结果原样保留。历史捕获次数大于1仍可读回，不能再次增加。

## schema2 迁移到 schema3

schema2 存档在同一启动事务里确定性升级到 schema3：先按 v2 语义完整校验，再把 `schema` 改为 3，其余已有关都字段、探索余额账式、球库存、伙伴、当前遭遇（含 legacyFreeAttempts、rule1 痕迹与已投记录）逐项保留。#152–251 的培养账户、拥有／曾获得位、捕获次数与首捕卡初始化为空，不推断历史。位图扩到 32 字节后，保留位以外的填充位必须为 0。

codec 按 `schema` 选择宽度：v1/v2 读 19 字节位图与 151 项数组，v3 读 32／251；解码后内存中的 `Save` 始终为 251 宽，旧版本未覆盖的部分保持空值。`Engine::begin()` 对 v1 依次执行 v1→v2→v3，对 v2 只执行 v2→v3，最终只写一次非当前槽，sequence 加一；写前、部分写、回读失败继续冻结同一 candidate 重试，不重新迁移或清零。旧固件不能读取 schema3。

迁移不改变总步、探索余额、捕获历史、首捕卡、成就或校时待确认事务，也不发放新物种或活动入口。普通池、`captureCounts` 和唯一捕获规则随 v3 目录扩到 251，但已完成的旧捕获与保留物种（遗迹、龙穴试炼）不受影响。

## schema3 迁移到 schema4

schema3 存档在同一启动事务里确定性升级到 schema4：先按 v3 语义完整校验，再重建随机遭遇账目并切换到当前规则。codec 对 v4 在 payload 末尾追加 `encounterTarget`（下一场目标步数）与 `explorationSpent`（累计已扣探索步）两个字段；v1–v3 布局保持不变，`FileStore` 的 unknownHeader 同步接受 4。

迁移账式：`explorationSpent = totalSteps - explorationCredit`，使 `spent + credit == totalSteps` 恒成立；这覆盖旧档按 5000 消耗的历史场次（每场实际扣除上界校验放宽到 5000）。首个 `encounterTarget` 由启动代与总步派生，落在 200–500 区间。总步、余额、捕获历史、培养账户和校时事务逐项保留，不退款、不追发。

规则切换：`kRuleVersion` 升到 3。迁移把在途非 legacy 遭遇的 `ruleVersion` 从 2 升到 3，之后的新遭遇直接以当前规则创建；已投的 attempt 保留自己的 `ruleVersion`，其存储的概率与 roll 继续按投球时点的表复算，validate 要求 `attempt.ruleVersion <= encounter.ruleVersion`。规则 3 起：普通 85/92/98、稀有 70/80/90；概率按 attempt 的版本分表校验。

精灵球不限量：规则 3 的投球不再检查库存、不再递增 `ballsSpent`，`NoBalls` 仅对规则 1–2 的在途遭遇保留。旧账目字段 `rewardBaseSteps`／`ballsSpent` 原样保留并可校验；`ballsSpent < usedBalls - legacyFreeAttempts` 下界仅对规则 1–2 的遭遇要求。schema4 的非 legacy 遭遇必须为当前规则版本；空遭遇（Walking）保持编码原值。

写前、部分写、回读失败继续冻结同一 candidate 重试，不重新迁移、重抽目标或升级规则；旧固件不能读取 schema4。
