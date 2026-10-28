# 步行状态归属与验收

玩法、培养账户与遭遇规则见[玩法契约](walking-progression.md)，旧档升级见[迁移契约](save-migration.md)。

## 调用者和状态归属

Engine继续唯一修改Save，复用`stage → submit → retrySave`，UI不拆分进化、扣费和资格更新。以下 Engine 方法声明在[adventure.hpp](../../firmware/apps/20_pokedex/adventure.hpp)，`SpeciesProgress`、`EvolveRequest` 和 `availableBalls` 声明在[adventure_model.hpp](../../firmware/apps/20_pokedex/adventure_model.hpp)：

```cpp
struct SpeciesProgress { uint64_t walked, spent; };
struct EvolveRequest { uint16_t from, to; uint64_t rosterRevision; };
EvolutionView evolutionView(uint16_t species) const;
CommandResult evolve(EvolveRequest, const ClockReading&);
CommandResult giveUpCapture(uint64_t encounterId);
uint64_t availableBalls(const Save&);
```

详情读 `evolutionView(partner)`，得到最多五条目标、费用、阻塞原因及 revision；确认时只提交 `{view.from, target, view.revision}`。Engine复核阶段、边、拥有、额度和合法 ClockReading；`trusted=false` 合法，不强迫校时。TargetOwned、InsufficientProgress、Stale 表示目标冲突、培养不足和过期请求；NoBalls 仅用于规则版本小于 3 的旧在途场。

`rosterRevision` 在首次获得当前资格、切换、进化时递增；纯步数及捕获当前已拥有、但从未捕获过的赠送伙伴时不变。最后进化回执保存完整请求、费用和 Stamp；相同请求先返回 Duplicate，再检查其他旧 revision 为 Stale。有效进化同一 candidate 扣来源 spent、替换拥有、登记目标、切换伙伴、更新遭遇伙伴和回执。更早请求可以返回 Stale，不会再扣费。Released 未确认结果时重复放弃返回 Duplicate，旧场 ID 在下一场返回过期错误且不改变状态。

| 模块 | 责任 |
| --- | --- |
| adventure_model、adventure_progression | 账户、校验、只读进化视图与费用 |
| adventure.cpp／steps／capture | 唯一事务、归步、遭遇、投球次数、旧规则扣球和奖励 |
| motion、runtime | 峰值归属、原日期标签、有限队列和用户意图 |
| screens、runtime_input | 只读页面、确认与等待，不直接写存档 |
| 规则 JSON、目录生成器 | 唯一父关系、费用、普通资格与编译期表 |
| save_codec、file_store | 版本解析、CRC、双槽选择和完整回读；不产生游戏奖励 |

Save v2 新增 151 个培养账户、19 字节曾获得位图、rewardBaseSteps、ballsSpent、legacyChargedCount、voidedLegacyCount、rosterRevision、lastEvolution；Encounter 新增 legacyFreeAttempts。Save v3 把物种数组扩到 251：32 字节拥有／曾获得位图、251 个培养账户与捕获次数、251 张首捕卡，新增的 100 项初始为空。Save v4 追加 `encounterTarget`（下一场 200–500 步目标）与 `explorationSpent`（累计已扣探索步），维持 `spent + credit == totalSteps` 账目；新遭遇规则版本为 3（普通 85/92/98、稀有 70/80/90，球不限量），规则版本与存档版本分开处理；schema1/2/3 的旧布局保持可读。

实现入口：[Engine冻结提交与启动迁移](../../firmware/apps/20_pokedex/adventure.cpp)、[双版本语义校验](../../firmware/apps/20_pokedex/adventure_model.cpp)、[计步队列与安全刷步](../../firmware/apps/20_pokedex/adventure_steps.cpp)、[进化事务](../../firmware/apps/20_pokedex/adventure_progression.cpp)、[投球准备与结算](../../firmware/apps/20_pokedex/adventure_capture.cpp)、[捕获练习](../../firmware/apps/20_pokedex/practice_capture.cpp)。新增字段和状态由这些实际调用路径统一处理，页面读取已发布状态。

## 换伙伴前后的步数归属

Detector 在更新 `peakAt_` 时同时冻结本拍 `uint16_t` 不透明归属标签，三个待确认峰分别保存自己的标签。Runtime 每拍传入已发布的 partner；保存未完成期间仍传旧伙伴。确认后把原标签和原 ClockReading 放入 StepEvent，经 Runtime 和 Engine 两层队列传递。`acceptPeak`、`observeSteps` 和刷队时都不重取当前伙伴。

因此两峰 A、切换 B、第三峰 B 的结果是 A/A/B；两次成功切换可得到 A/B/C。进化后迟到的来源峰仍计入来源永久账户，即使该形态当前已不拥有。Engine校验标签为 0 或曾获得物种，保留 bootGeneration+batchId 去重；同一事件不能重复归步或培养。

切换／进化前先按原标签刷已确认队列；刷队保存失败则先重试存档，重新确认意图。操作本身不 reset Detector，不因换伙伴丢掉待确认两峰。现有异常采样、震动抑制和校时清理仍可能丢弃未确认峰，继续显示质量缺口。队列满、掉电前未保存步数不承诺恢复。标签机制已接入真实Detector、Runtime和Engine；主机合成波形覆盖A/A/B、A/B/C、进化后迟到峰及保存失败期间的旧伙伴归属。这些结果不证明设备步数准确。

## 资源与验收

schema1 编码 12,139 字节，schema2 编码 14,672 字节，schema4 与 schema5 编码 21,508 字节，均使用固定字段布局。单槽上限为 24,576 字节（schema4/5 余量 3,068 字节），双槽加候选缓冲共 73,728 字节。`FileStore::Workspace` 继续整体堆分配，主机实测 103,248 字节（含解码用 Save）；`sizeof(Save)=29,472`、`sizeof(Engine)=66,208`。本轮数据及 codec 日志标签的已知错误见[2026-10-27 验证记录](../verification.md#schema4-文档复核与主机检查2026-10-27)，这些主机大小不代替设备堆峰值。

固件 Flash 和静态 RAM 占用以当前构建输出为准；主机 ABI 不能代替 Xtensa 目标大小。`check-startup-stack` 检查 Engine 启动局部栈帧，不能代替完整任务栈或全系统堆峰值测量。

| 验证入口 | 必须观察到的行为 |
| --- | --- |
| 规则生成器和素材检查 | 122边（关都72＋城都50）、124普通根；路线按属性推导，普通池65/50/42，全部251种为122/105/95；保留物种排除、地区形态排除；schema1–4旧掩码与生成头文件逐值一致；动画缩放读显式 [sprite-scales.json](../../assets/sprite-scales.json)，其中 Gen1 的 evolved 集合与冻结的 evolution-gen1 父关系一致，另列无进化链物种；不要求 251 parents 与现动画一致 |
| Engine真实计步入口 | 目标-1/目标、1000、10000、15000；跨日、unknown和重放无丢余数或重复收益；`spent+credit==totalSteps` 恒成立 |
| 122条边逐边门槛 | 费用−1拒绝，费用及费用+1正确替换；永久历史、赠送伙伴唯一捕获时复用余额、目标冲突、伊布五分支、无畏小子三分支和旧迷你龙培养 |
| Detector → Runtime → Engine | 两旧峰后切换、两次切换、进化后迟到峰、保存失败；归属和日期不漂移；异常缺口仍可见 |
| 实际页面与Button_Class | 全部拥有分页、携带标记、五分支分页和确认释放门；胜利等待可留首页、采样和返回同场；251图鉴原操作保留；球图标无数字 |
| 实际投球时间线 | 准备零占用、按下/超时计一次、重复键零占用、成功不能再投、Released与Escaped分开；无限球不再返回NoBalls |
| 捕获练习 | 随机物种、真实对战与两次投球判定，独立对象不读写存档、不写图鉴 |
| codec与双槽故障注入 | 各 phase v1 字节 fixture、schema2/v4/v5 现场编解码、schema2/3 边界造档、schema3 活动尾部 fixture、v4 迁移账目与规则升级、v5 路线修正与旧非法路线拒绝、v1/v5 混合槽和 v2 单槽升级、同序冲突、未知版本、500 步旧首捕保留、旧免费前缀、V=1 后的 ID 边界 |
| 真实提交失败和重启 | 写前、部分写、写后回读失败；完整旧或新快照，重试无双扣/双进化；升级后再次启动保留账目 |
| 溢出与资源 | 总步/培养/花费/ID/revision上界、128队列满、实际编码长度、全部Save副本峰值RAM、完整栈 |
| 设备验收 | 四种携带误差、采样/输入/Flash时延、RTC保持与恢复、电流、满盘/物理掉电、挂载保留和初始化 |

自动检查与设备待验范围统一维护在[验证说明](../verification.md)。主机合成波形、故障注入与截图不代表实体计步、屏幕效果、RTC 保持或断电恢复已经通过。
