# Issue #9 按属性生成路线资格（2026-10-28）

返回[验证说明](../verification.md)。

三条路线的遭遇资格改由 [adventure-251.json](../../assets/adventure-251.json) 的 `routeTypes` 按官方属性推导，物种文件删去手写 `routes`；存档升到 schema5，schema 1–4 改用冻结的旧掩码校验，4→5 只修正当前遭遇的路线。分支 `feat/issue-9-route-types`，基于 `master` `8ea8653`。

| 命令 | 结果 |
| --- | --- |
| `rtk proxy mise run check-adventure-catalog` | 0；251 个属性掩码与生成物一致，普通池 65/50/42 |
| `rtk proxy mise run test-adventure` | 0；adventure 151,996；codec 43,376；modern codec 142；regressions 48；progression 17,023；migration 398；unique encounters 38,377；启动栈检查通过 |
| `rtk proxy mise run test-storage` | 0；file_store 124、device_store 30、test_mode_storage 150、allocation 8 |
| `rtk proxy mise run build` | 0，`SUCCESS`；静态 RAM 22,668 字节（6.9%），Flash 7,985,069 字节（60.9%） |

新增用例覆盖：小火龙旧森林遭遇升级后位于洞窟（EncounterReady 与 Resolved）、MigrationRetired 同样修正路线、历史捕获与玩家选定路线保持原值、schema 1–4 的旧非法路线仍被拒绝、schema5 新非法路线在解码时被拒而不自动改写。payload 未增加字段，schema4/5 编码长度均为 21,508 字节。

补充运行 `test-render` 时发现两处既有失败，与本改动无关：用基线 `8ea8653` 源码加当前测试源码复现 `test_progression_render`（“two peaks wait for confirmation”）与 `test_evolution_render`（“two step peaks remain pending before threshold”）的同样失败，原因是 `84461fe` 调整 Detector 后未同步这两个页面用例；其余 12 套渲染套件通过。该问题留给计步负责人处理，本包未改。

未验证：实体设备上的路线池手感、真实 schema4 存档升级、烧录与断电恢复。主机检查不证明真实计步、屏幕观感或设备时延。
