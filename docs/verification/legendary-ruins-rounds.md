# 传说遗迹历史轮次

本文件保存 2026-10-27 首轮验收及随后两轮修复的历史数字，仅作记录；当前状态见[传说遗迹验收](legendary-ruins.md)。

## 首轮验收（2026-10-27）

本轮指定检查均通过，证据在 `.pio/verification/r1/final/`：

| 检查 | 结果 |
| --- | --- |
| `test-adventure` | adventure 426,036；codec 43,499；modern 136；regressions 48；progression 17,037；migration 368；unique 38,377；启动局部栈通过 |
| `test-storage` | file_store 124、device_store 30、allocation 8；`encoded_bytes=21520`、`sizeof_Save=29488`、`workspace_bytes=103264`、`peak_owned_heap=132752` |
| `test` | viewer 201，0 failures |
| `test-render` | 12 套均 0 failures：1614、10169、1060、**10727 adventure UI**、89、383、25217、88369、8368、522751、10685、219144 |
| `check-adventure-catalog` | 251 IDs、124 普通根、122 进化边；12 项 unittest 通过 |
| `build` | SUCCESS；RAM 97,780 / 327,680（29.8%），Flash 7,981,397 / 13,107,200（60.9%），`firmware.bin` 7,981,808 字节 |
| `check-startup-stack` | `Engine::begin` 局部帧 48 字节（limit 512） |

新增真实 Runtime/M5GFX 截图 29 张（`.pio/verification/r1/final/ui/test_adventure_render/ruins-*.ppm`），覆盖：未解锁遗迹、线索列表、线索战准备/对战/暂停/结果、错误顺序冲突、解锁、目标首页与末页、目标混合状态（已完成/当前）、251 与 150 首领详情、4 HP 与 5 HP 首领战、失败重试、已捕获重打、零球等待与重启。血条像素探针：默认线索战双方三段 39px；4/5 HP 首领为 49px 连续轨道；所有截图亮像素均在半径 228 内。

主机断言覆盖：三条线索全部六种完成顺序、普通池全空仍能完成线索、错误顺序零提交与冲突位、正确解锁幂等、重启保留；11 个目标的 `BattleSpec` 与表逐项一致；首领失败、两次投球失败和保存失败只重试当前阶段且 ID/seed 更新；已捕获重打不进入投球、不改图鉴/计数/首球账；零球保留胜利；活动期间步数只入账不创建普通遭遇。旧 schema3 短 payload 仍读默认值，非法目录组合（错误路线、锁定首领、目标不符、DragonTrial origin）被校验拒绝。

未验证或未满足（R1 轮）：实体设备输入、观感、断电恢复与动态堆未测；设计预览仍只覆盖 26 页（固件现有 34 页），该页覆盖断言不计入本轮验收。151、245 的 `DodgeOnly` 已被下一小节的首领可完成性修复取代。

## 首领可完成性修复（2026-10-27）

验收发现 11 个目标中 151、245 的 `DodgeOnly` 只有闪避拍，攻击无法命中，必然在 12 拍判负，无法进入投球。协调者裁决不新增“守满即胜”、不降低捕捉概率，只把这两只的节奏改为 `AlternateDodgeFirst`；HP、节拍、判定窗口、guard 和拍数上限都不变。

先用新端到端用例在旧表上取得失败：对 11 个目标逐一从新档完成三条线索、解锁、选择、开战，按目录节奏用公开 `press`/`tick` 打出 Perfect。旧表下 151、245 均为 `Lost`、`cues=12`、首领 HP 仍为 4，其余 9 只在 cueIndex 1～5（即第 2～6 拍）内胜利；失败日志在 `.pio/verification/r1-boss-fix/before/test-adventure.log`。改表后同一组目标全部胜利并各自完成一次真实投球捕获，逐只结果在 `.pio/verification/r1-boss-fix/after/test-adventure.log` 的 `TARGET` 行。日志里的 `cues` 是从 0 开始的 cueIndex（每次推进前先记录当前索引），不是胜利拍数；失败时的 `cues=12` 是判负上限，同样不能机械加一：

| 目标 | 胜利时 cueIndex（从 0 开始） | 捕获 |
| --- | --- | --- |
| 144 / 146 / 244 | 2 / 1 / 1 | 各 1 次 |
| 145 / 151 / 243 / 245 | 3 | 各 1 次 |
| 150 / 249 / 250 / 251 | 2 / 4 / 5 / 2 | 各 1 次 |

换算成第几拍（cueIndex + 1）依次为 3/2/2、4、3/5/6/3。

改表后指定检查全部通过，证据在 `.pio/verification/r1-boss-fix/after/`：

| 检查 | 结果 |
| --- | --- |
| `test-adventure` | adventure 427,260；codec 43,499；modern 136；regressions 48；progression 17,037；migration 368；unique 38,377；启动局部栈通过 |
| `test-storage` | file_store 124、device_store 30、allocation 8；`encoded_bytes=21520`、`sizeof_Save=29488`、`workspace_bytes=103264`、`peak_owned_heap=132752` |
| `test` | viewer 201，0 failures |
| `test-render` | 12 套均 0 failures：1614、10169、1060、**10795 adventure UI**、89、383、25217、88369、8368、522751、10685、219144 |
| `check-adventure-catalog` | 251 IDs、124 普通根、122 进化边；12 项 unittest 通过 |
| `build` | SUCCESS；RAM 97,780 / 327,680（29.8%），Flash 7,981,393 / 13,107,200（60.9%），`firmware.bin` 7,981,808 字节 |
| `check-startup-stack` | `Engine::begin` 局部帧 48 字节（limit 512） |

151 与 245 新增 6 张真实 Runtime/M5GFX 帧（`ruins-boss-151|245`、`-dodge`、`-attack`）：首领详情、首拍“左键 · 躲避”、第二拍“右键 · 攻击”，逐字符像素 oracle 与 catalog 节奏一致；四格血条仍是 49px 连续轨道，玩家三格不变，亮像素全部在半径 228 内。首领详情页的“节奏 …”行当时有 2 像素被双项菜单面板覆盖，已在下一小节修复。

12 项捕获均先由合法 seed 让首次投球的真实 roll 通过，未改概率或直接写 `captureCounts`；每次捕获后 `captureCounts=1`、首捕卡与图鉴只增加一次、`ballsSpent=1`、重启后账目不变。

## 首领详情页节奏行覆盖修复（2026-10-27）

首领详情页的 `specLine` 三行原本以 y=154/178/202 居中，16 px 字框到 209，而双项菜单面板从 y=208 起，导致“节奏 …”行底部 2 行被面板盖住。修复只把整块 specLine 从 y=166 上移到 y=158（三行中心 146/170/194），文案、字号、缩略图与圆屏安全区都不变。

修复前先用真实 Runtime/M5GFX 帧量化缺陷：`ruins-boss-{251,151,245,150}` 详情页“节奏”行的着墨行只到 207，比同页“节拍”行的 16 行（170–185）少 2 行；修复后回到 186–201，行 202–207 无文字着墨，缩略图下沿 132 与“生命”行首墨 138 之间 133–137 全为背景色。证据在 `.pio/verification/r1-layout-fix/{before,after}/`（PPM、PNG 与 `pixel-summary.txt`）。

新增 `ruinsBossSpec` 断言：251、150、151、245 的详情页逐像素比对“生命 / 节拍 / 节奏”三行真实 efont glyph，任一行被菜单覆盖即失败。同一断言在旧 y 位置实测失败（`before-fail/test-render-subset.log` 的 `FAIL: boss detail keeps the whole HP line`），修复后全部通过。

| 检查 | 结果 |
| --- | --- |
| `test-render` | 12 套 0 failures：1614、10169、1060、**10807 adventure UI**、89、383、25217、88369、8368、522751、10685、219144 |
| `test-adventure` | adventure 427,260；codec 43,499；modern 136；regressions 48；progression 17,037；migration 368；unique 38,377；启动局部栈通过 |
| `build` | SUCCESS；RAM 97,780 / 327,680（29.8%），Flash 7,981,393 / 13,107,200（60.9%），`firmware.bin` 7,981,808 字节 |
| `git diff --check` | 无输出 |

相对首领可完成性修复基线：adventure UI 10795 → 10807（新增 4 个目标 × 3 行断言），其余套件计数、adventure 域检查、RAM、Flash 与 `firmware.bin` 全部不变。主机帧与像素断言不等于设备观感；本轮未做实体设备验证，也未目视核对图片（本会话图片工具不解码 PNG）。
