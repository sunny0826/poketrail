# 步行存档迁移契约

当前玩法见[玩法契约](walking-progression.md)，状态归属和验证矩阵见[状态与验收](walking-state.md)。

下文按版本保留 schema3 起的迁移规则；schema1→2、schema2→3 的双槽选择、500／5000 步账式和发球规则见[早期存档迁移](save-migration-early.md)，只适用于那些历史版本。当前启动会在同一事务中继续升级到 schema8：目录扩容见 [schema5→6](#schema5-迁移到-schema6)，地区切换见 [schema6→7](#schema6-迁移到-schema7)，语言字段见 [schema7→8](#schema7-迁移到-schema8)，不会把中间版本单独写入存档。

## schema3 迁移到 schema4

schema3 存档在同一启动事务里确定性升级到 schema4：先按 v3 语义完整校验，再重建随机遭遇账目并切换到当前规则。codec 对 v4 在 payload 末尾追加 `encounterTarget`（下一场目标步数）与 `explorationSpent`（累计已扣探索步）两个字段；v1–v3 布局保持不变，`FileStore` 的 unknownHeader 同步接受 4。

迁移账式：`explorationSpent = totalSteps - explorationCredit`，使 `spent + credit == totalSteps` 恒成立；这覆盖旧档按 5000 消耗的历史场次（每场实际扣除上界校验放宽到 5000）。首个 `encounterTarget` 由启动代与总步派生，落在 200–500 区间。总步、余额、捕获历史、培养账户和校时事务逐项保留，不退款、不追发。

当前 `validCapture` 对捕获记录使用 200 总步下界，保留旧档的 500 步首捕；schema4 新场在创建时实际扣除 200–500 探索步的随机目标，不再按 5000 步建场。

规则切换：`kRuleVersion` 升到 3。迁移把已有规则 2 遭遇的 `ruleVersion` 升到 3，之后的新遭遇直接以当前规则创建；已投的 attempt 保留自己的 `ruleVersion`，其存储的概率与 roll 继续按投球时点的表复算，validate 要求 `attempt.ruleVersion <= encounter.ruleVersion`。规则 3 起：普通 85/92/98、稀有 70/80/90；概率按 attempt 的版本分表校验。

精灵球不限量：规则 3 的投球不再检查库存、不再递增 `ballsSpent`，`NoBalls` 仅对规则 1–2 的在途遭遇保留。旧账目字段 `rewardBaseSteps`／`ballsSpent` 原样保留并可校验；`ballsSpent < usedBalls - legacyFreeAttempts` 下界仅对规则 1–2 的遭遇要求。schema4 的非 legacy 遭遇必须为当前规则版本；空遭遇（Walking）保持编码原值。

写前、部分写、回读失败继续冻结同一 candidate 重试，不重新迁移、重抽目标或升级规则；旧固件不能读取 schema4。

## schema4 迁移到 schema5

schema4 存档在同一启动事务里确定性升级到 schema5：先按 v4 语义完整校验，再把 `schema` 写成 5。payload 不增加字段，`encounterTarget` 与 `explorationSpent` 继续读写，编码长度与 schema4 相同。

schema5 把路线资格从物种文件的手写 `routes` 改为按属性推导。为使旧档继续按当时的目录校验，schema 1–4 的路线校验使用基线 `8ea8653` 冻结的旧掩码（由生成器写到 `route_masks_v4_generated.hpp`），schema 5 使用新掩码。非法旧路线在 1–4 校验中仍然失败，迁移不能靠关掉校验通过。

4 到 5 只做一件事：当前遭遇不在 Walking 且 `encounter.id` 非 0 时，若其路线不在新掩码里，就按 Forest、Coast、Cave 的顺序改成第一个命中位。EncounterReady、Battle、ThrowReady、Resolved 与 MigrationRetired 都同样修正路线；物种、seed、已投记录、结果、账目、玩家选定的 `Save.route`、历史 `Capture.route`、培养账户和伙伴都不改。Walking 没有当前场，不处理。例：小火龙原为森林遭遇，Fire 只属洞窟，升级后该场改到洞窟，捕获历史中的森林记录保持原值。

写入后仍须通过 v5 语义校验；schema5 存档中不合法的路线会被拒绝，而不是在加载时自动改写。写前、部分写、回读失败继续冻结同一 candidate 重试；旧固件不能读取 schema5。

## schema5 迁移到 schema6

schema6 把目录扩到全国编号 #1–649：拥有／曾获得位图各 82 字节，培养账户、捕获次数和首捕卡各 649 项。codec 对 schema1–2 继续读写 151 项／19 字节，对 schema3–5 继续读写 251 项／32 字节；不根据当前目录长度猜测历史格式。旧版本不能含超出对应范围的物种 ID、非空数组尾部或位图填充位；未知版本、CRC 和语义校验继续生效。

完整校验 schema5 后，将版本改为 6。新增 #252–649 保持未拥有、未登记、零培养、零捕获和空首捕卡；既有账户、总步、探索账目、捕获历史、伙伴及在途遭遇保留，不补发历史培养或新物种。新宝宝父关系不取消旧 124 种普通资格。schema5→6 不改变当前场的物种、路线、seed、ID、投球次数或规则版本；启动恢复仍按既有规则将 Battle 恢复为 EncounterReady，关闭已捕获的重复在途场。

schema1–5 的所有必要迁移和启动更新合并为一次非当前槽提交。写入失败沿用同一个候选、sequence 与结果重试，提交成功前不开放玩法。旧固件不能读取 schema6，不应降级后忽略新版槽继续游戏。schema6 的编码长度与内存占用须从本轮测试和目标构建输出确认，检查边界见[验证说明](../verification.md)。

## schema6 迁移到 schema7

schema7 把按属性划分的森林、海边、洞窟改为五个世代地区。字节布局与 schema6 相同，只改变 `route` 字节的取值含义：`Route` 保留 0–2（Forest、Coast、Cave）作历史值，3–7 依次为关都、城都、丰缘、神奥、合众，编号范围由目录生成的 `kRegionRanges` 给出（#1–151、#152–251、#252–386、#387–493、#494–649）。

完整校验 schema6 后执行三项改动，再把版本写成 7：

- 玩家选定的 `Save.route` 一律改为关都。旧路线与地区没有一一对应，不按属性猜测。
- 当前遭遇不在 Walking 且 `encounter.id` 非 0 时，其路线改为该物种所属地区。EncounterReady、Battle、ThrowReady、Resolved 与 MigrationRetired 都同样处理；物种、seed、ID、已投记录、结果、规则版本和账目不改，可以继续对战或投球。例：海边的 #72 改到关都，#158 改到城都。
- 历史 `Capture.route`（首捕卡与最近捕捉）保留森林、海边或洞窟原值，界面照旧显示当时的名称。

schema7 的校验：`Save.route` 必须是地区；在途遭遇的路线必须等于物种所属地区；`Capture.route` 可为 0–7 任一值；大于 7 的字节、地区之外的选定路线或与物种不符的遭遇路线都判为损坏，不在加载时改写。schema1–6 继续按各自版本的路线规则校验（1–4 用冻结旧掩码，5–6 用属性掩码）。

普通遭遇只从当前地区编号范围内、未捕获的普通物种抽取（schema7 当时的资格判定；当前新建遭遇按曾获得及其祖先排除，见[玩法契约](walking-progression.md)）；地区捕完时保持 Walking，不扣探索步，换到仍有物种的地区后按已有余额建场。进化不受地区限制，初始伙伴仍为关都三只。

schema1–6 的必要迁移与启动更新合并为一次非当前槽提交；写入失败沿用同一候选、sequence 与字节重试，提交成功前不开放玩法。旧固件不能读取 schema7。[迁移测试](../../test/test_migration.cpp)覆盖三种在途阶段的 6→7 升级与写失败重试；模拟器 E2E 的 `migration-schema6` 使用由提交 `05432f6` 的 schema6 编码器生成的[冻结字节](../../test/fixtures/simulator/manifest.json)，经真实启动升级后继续投球，并在新进程中核对关都与森林两条记录。

## schema7 迁移到 schema8

schema8 在 schema7 payload 尾部追加一字节 `language`：0 为中文、1 为英文。649 项账户、地区、header、CRC 算法和 ruleVersion 3 不变；编码长度比等价 schema7 多一字节。schema1–7 继续使用各自原始布局，旧格式对象只能携带默认中文，防止编码时静默丢弃英文选择。

完整校验 schema7 后，只设置中文语言并把版本改为 8；已选地区、拥有／登记、培养、捕获历史、总步、探索余额、在途场和投球记录保持原值。启动仍执行原有 bootGeneration、clock.trusted 和 Battle 恢复规则，迁移本身不重复执行 6→7 的关都重置。

非法语言字节即使 CRC 正确也判为损坏。混合 schema7／8 槽沿用 sequence 选择与冲突检查，未知 schema9 拒绝读取；不得降级后绕过新版槽覆盖存档。启动保存失败继续冻结同一候选重试。

语言选择使用现有 Engine 提交事务，只在公开状态更新后切换显示。存储无法加载时的临时语言不写档。行为与状态归属见[中英文界面](i18n.md)。

## 活动分支固件存档的兼容读取

未合并的活动分支固件（legendary-ruins、dragon-trial）在 schema3 payload 末尾追加了 22 字节活动块，并可能持有超出普通池的当前遭遇：这些活动遭遇消耗遭遇 ID 但不消耗探索步，schema3 的固定扣步账式对它们不成立。解码器识别「schema3 + 恰好 22 字节尾部」的帧时跳过活动块；若当前遭遇不在普通池，就清空该遭遇、回到 Walking 并清空结果，然后由解码器就地升级 schema4、schema5，启动时再由 Engine 升级到 schema8。总步、培养账户、捕获历史与伙伴保留，活动进度（遗迹线索、试炼阶段）随尾部块一起丢弃。

升级到 schema4 后仍须通过 v4 语义校验，再按上文进入 4 到 5 并通过 v5 校验。令 `modern = nextEncounterId - 1 - legacyChargedCount - voidedLegacyCount`、`modernSpent = explorationSpent - 500 × legacyChargedCount`，当前校验要求 `200 × modern <= modernSpent <= 5000 × modern`；这是累计账目区间，并非允许每场零消耗。已纳入测试的[活动固件字节样本](../../test/fixtures/progression/schema3-activity.bin)满足该区间，[迁移测试](../../test/test_migration.cpp)验证其 6293 总步、1293 探索余额、培养与伙伴保留。不能据此承诺任意活动分支存档都能迁移；未来活动格式需另订迁移契约。
