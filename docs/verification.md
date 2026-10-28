# 验证范围与已知限制

自动检查入口见[开发指南](development.md#检查入口)。主机测试使用实际 Engine、Runtime、codec、RX8130 驱动及 M5GFX；它们覆盖合成输入和故障注入，不等同于实体设备验收。设备硬件概况、开发路径与社区项目见 [StopWatch 开发资源汇总](verification/stopwatch-c152-dev-resources.html)。

## 自动检查覆盖

- 玩法：随机遭遇账目与投球次数、122 条进化边、永久账户、目标冲突、唯一捕获、重放与失败重试。
- 存档：schema1 字节 fixture、schema2/v4 现场编解码、schema2/3 边界造档、schema3 活动尾部 fixture、v1/v4 混合槽与 v2 单槽升级、CRC／语义校验、未知版本拒绝、部分写入和回读故障。
- 计步与时钟：合成连续步态、峰值归属、跨日、回拨、RTC 寄存器错误、启动与校时恢复。
- 输入与页面：真实 `Button_Class` 消抖、双键确认与释放门、触摸、投球等待、图鉴重进、省电与伙伴首页。
- 测试模式：独立目录与模式标记、快照和空档、RAM-only、存储故障、模拟时钟与 IMU 隔离；控制台的主机输入和渲染覆盖见下方 Issue #7 记录。
- 进化动画：提交后播放、形态交替、跳过、重试与重启、播放中计步；图鉴预览覆盖全部 122 条进化边、伊布五分支与无畏小子三分支、账户不变及存储错误下的返回。
- 素材：来源哈希、生成物一致性、全部 13,496 个 GIF 帧（关都 151 只 8,165＋城都 100 只 5,331）与固件解码及帧时长一致。
- 构建：ESP32-S3 固件与 `Engine::begin()` 局部栈帧。静态 RAM 和局部帧不代表运行时堆峰值或完整任务栈水位。

## 历史证据清理与重新检查

2026-10-27 清理时，之前各轮的本地协调报告、审查交付、日志和渲染截图已删除。[历史验证轮次](verification/history.md)保留各轮检查摘要与设备观察，原证据文件已不可读取；检查数量、资源数字和设备观察只适用于对应版本，不能当作当前 HEAD 的验证结果。该次清理只核对文档差异与静态链接，没有重跑主机套件、构建或设备验收。

按[开发指南](development.md#检查入口)准备依赖后，可用 `rtk proxy mise run test-adventure`、`rtk proxy mise run test-storage`、`rtk proxy mise run test-render` 等对应任务重新检查。渲染任务生成真实 Runtime/M5GFX 截图；需要命令日志时另行保存标准输出、标准错误和退出状态。输出目录由 `MYM5_VERIFICATION_DIR` 指定，未指定时使用任务新建的 `.pio/verification` 时间戳目录。新结果只反映运行时的代码与环境，不能恢复旧轮次报告、PNG 画廊或设备观察。

## schema4 文档复核与主机检查（2026-10-27）

在 `master`、HEAD `e3d6397` 上运行 `rtk proxy mise run test-adventure` 和 `rtk proxy mise run test-storage`，两项退出状态均为 0。环境为 macOS arm64、mise Python 3.14.2、Apple Clang 21.0.0；主机套件默认启用 AddressSanitizer／UndefinedBehaviorSanitizer，启动栈检查单独运行。

| 检查 | 本轮结果 |
| --- | --- |
| `test-adventure` | adventure 151,996；codec 43,376；modern codec 119；regressions 48；progression 17,023；migration 373；unique 38,377，均通过；启动可放入带保护页的 16,384 字节主机栈 |
| `test-storage` | file_store 124、device_store 30、allocation 8，均通过；文件系统由主机替身提供 |
| 设计页面覆盖单测 | 失败：画廊缺 `Evolution`、`EvolutionPreviewTargets`、`EvolutionPreview`，固件为 29 页；测试还保留 26 页断言。本轮仅记录，未修改测试或生成器 |

实测编码长度为 schema1 12,139、schema2 14,672、schema4 21,508 字节；单槽上限仍为 24,576，v4 余量 3,068 字节。主机 `sizeof(Save)=29,472`、`sizeof(Engine)=66,208`、`workspace_bytes=103248`、`peak_owned_heap=132720`；最后一项是测试按 Workspace 加一个 codec 解码用 Save 计算的存储层占用，不是整机动态堆峰值。`test/test_save_codec.cpp` 的输出标签仍写 `v3=21508`，实际来自默认 schema4 的编码；这是日志标签错误，不是 schema3 长度变更。

迁移测试实际覆盖 v1/v4 混合槽、v2 单槽升级和 schema3 活动字节样本；`test_save_codec_modern.cpp` 另覆盖 schema2/3 的现场造档与语义边界。旧首捕样本断言为 500 总步，不应记为 200 步首捕边界测试。活动样本在当前 v4 累计账目约束内可读，不代表任意零消耗活动存档均兼容。

该轮文档审计交付与配套日志已清理；可用 `rtk proxy mise run test-adventure` 和 `rtk proxy mise run test-storage` 重新检查并保存日志。页面检查命令为 `rtk proxy mise exec -- python3 -m unittest test.test_ui_design_previews.DesignPreviewTests.test_every_firmware_page_is_covered`。该轮只修改文档，未运行完整渲染、固件构建或目标栈检查，也未烧录、校时或初始化文件系统；以上主机数据不证明设备上的堆峰值、屏幕表现或断电恢复。

## Issue #11 电源键短按（2026-10-27）

电源键输入直接读取 M5PM1 的 `0x48` 按钮状态：bit0 为当前按下电平，bit7 为读后自清的按下标志。按下沿或仅保留标志的短按产生一次省电／唤醒事件，不执行页面动作；唤醒完成帧不会再次锁屏。

验证对象为 `master`、HEAD `85d470c` 加上述 PM1 输入改动的工作树，主机检查结果如下：

| 命令 | 结果 |
| --- | --- |
| `rtk proxy mise run test-render` | 13 套件零失败；power 13,168 + power pages 5,883 checks |
| `rtk proxy mise run test-adventure` | adventure 151,996；codec 43,376；modern codec 119；regressions 48；progression 17,023；migration 373；unique encounters 38,377，均通过 |
| `rtk proxy mise run build` | `SUCCESS`；RAM 29.8%，Flash 60.8% |
| `rtk proxy mise run check-startup-stack` | PASS；`Engine::begin` 局部帧 48 字节 |

证据目录为 `.pio/verification/issue11-powerdiag-20261027T181520Z/`：`test-render-final.log`、`test-adventure-final.log`、`build-final.log`、`check-startup-stack-final.log` 保留上述命令的完整输出；`flash-fix.log` 保留烧录记录；`power.log`、`power-fix.log` 保留串口诊断。`test-render-final.log` 含 4 条既有 unused-function 警告。

主机覆盖短按省电与唤醒、唤醒不执行页面动作、活动流程保持亮屏，以及暂停、等待投球和校时草稿保留；还覆盖按住不重复触发、相邻按下和唤醒完成帧。

真机已通过用户确认：首页短按电源键后熄屏，再短按亮屏且仍在首页，再短按再次熄屏。串口诊断保存在上述目录的 `power.log` 与 `power-fix.log`；`power.log` 可见按钮状态 `81`（按下及标志）、`01`（按住）、`00`（释放）。三次操作的屏幕结果来自用户观察，串口日志不提供逐次屏幕验收。

尚未测量功耗与唤醒时延数值，也未逐帧验收屏幕观感。主机通过 PM1 寄存器替身验证输入，不执行设备输入适配器或真实 PMIC I/O；上述首页实测不代表所有页面和硬件边界均已验证，主机测试不能替代设备验收。

## Issue #7 设备内测试模式（2026-10-27）

实现基于 HEAD `ce9ed83` 上的核心与 UI 工作树改动，操作见[设备内测试模式](usage-test-mode.md)。当前提供隐藏入口、正式快照／空档、独立测试槽与模式标记、手动步数、模拟校时、四种固定场景、存储故障开关、诊断、重置和退出。正式玩法仍为 schema4／ruleVersion 3、每场随机 200–500 探索步。

以下为最终集成轮（2026-10-27 12:05–12:15 UTC）在退出路径与截图断言修正后重跑的结果；命令退出状态、检查数量和资源数字均已核对日志。

| 实际运行命令 | 退出状态与关键结果 | 日志 |
| --- | --- | --- |
| `rtk proxy mise run test-storage` | 0；file_store 124、device_store 30、test_mode_storage 150、allocation 8 checks 通过 | [最终存储日志](../.pio/verification/issue7-final/test-storage.log) |
| `rtk proxy mise run test-adventure` | 0；test_mode 343 checks 通过，既有玩法、codec、培养与迁移套件通过 | [最终玩法日志](../.pio/verification/issue7-final/test-adventure.log) |
| `rtk proxy mise run test-motion` | 0；motion/calendar 156,175 checks 通过（仅合成信号） | [最终计步日志](../.pio/verification/issue7-final/test-motion.log) |
| `rtk proxy mise run test-rtc` | 0；RTC 适配 39 项、板级启动 9／468 项通过 | [最终 RTC 日志](../.pio/verification/issue7-final/test-rtc.log) |
| `rtk proxy mise run test-render` | 0；14 套件零失败，其中 test-mode UI 248 checks；有 unused-function 警告 | [最终渲染日志](../.pio/verification/issue7-final/test-render.log) |
| `rtk proxy mise run build` | 0，`SUCCESS`；静态 RAM 22,668 字节（6.9%），Flash 7,984,321 字节（60.9%） | [最终构建日志](../.pio/verification/issue7-final/build.log) |
| `rtk proxy mise run check-startup-stack` | 0；`Engine::begin` 局部帧 48 字节（上限 512） | [最终启动栈日志](../.pio/verification/issue7-final/check-startup-stack.log) |

退出测试模式时，`main.cpp` 通过 `Runtime::begin(false)` 调用 `Engine::begin(false)` 加载正式档，不递增或提交 `bootGeneration`／`sequence`；随后用当前读数刷新内存时钟锚点，避免测试模式重启后把正式时钟误判为回拨，正式槽字节保持不变。新增 `test/test_test_mode.cpp::noBootWriteOnModeExit` 断言加载前后存档字节和提交次数不变，并保留总步与初始伙伴；[最终玩法日志](../.pio/verification/issue7-final/test-adventure.log)记录 `PASS test_mode: 343 checks`。进入前的正常 checkpoint 仍可能保存正式进度，比较基线应取该保存完成之后。此主机断言不替代完整设备切换与断电验收。

`check-startup-stack` 使用新的 `Engine::begin(bool)` 符号，仍在最终构建上记录局部帧 48 字节、PASS；这是局部帧，不代表完整任务栈或动态堆水位。

本轮证据目录为 `.pio/verification/issue7-final/`。测试模式截图位于 [ui/test_test_mode_render/](../.pio/verification/issue7-final/ui/test_test_mode_render/)，包含入口、控制台末页、步数、场景、故障、诊断、校时、图鉴、重置、退出和错误页的真实 Runtime/M5GFX 主机渲染。这里的容量与槽位大小来自测试替身，不是设备测量。这些产物在 `.pio/` 下，不随源码发布。

主机检查的具体边界如下：

- `test_mode_storage` 使用 LittleFS/VFS 替身，比较正式槽位字节与哈希，覆盖测试提交、退出清理、标记重启、标记删除失败、坏档／未知版本快照、RAM-only、写前／写后／加载失败和重试状态不串目录。
- `test_mode` 验证模拟时间推进、测试平台不调用真实 IMU 采样及 RTC 写入接口、正式模式恢复委托，并以手动步数事件触发遭遇；修正后的套件另覆盖退出加载正式档时字节和提交次数不变。
- `test_test_mode_render` 使用真实 Runtime、Button_Class 和 M5GFX，验证顶部长按 1 秒不进入、约 2 秒后松手进入、来源确认、分页和返回、固定场景、+1 步触发遭遇并打开遭遇页、模拟校时、图鉴标识、重置与退出。存储和模式宿主为替身，不执行 `main.cpp` 的完整设备切换链；读取故障由测试代码重建 Runtime 触发。

仍有以下实现边界，不能据上述通过记录判定整项 Issue 已验收：

- 当前没有仓库测试样本导入／导出、任意账户编辑、设备内软重启、损坏字节或双槽冲突注入入口；诊断页也未展示 CRC、当前槽位、重试次数或动态内存。“读取失败”只影响后续加载，设备重启会清除该故障开关；渲染测试仍以主机重建 Runtime 触发，不能当作设备内一键复现路径。
- `+1000` 步注入遇提交失败后的精确回放没有端到端回归；当前只把单批注入限制在 Engine 队列容量内，降低溢出丢步风险。

### 设备验证（2026-10-28）

设备验证记录见[测试模式设备验证](verification-test-mode-device.md)。

## 已知限制

- 2026-10-25 的设备存档显示今日步数“--”：RTC 已校准且当日有 122 步，但时钟被标为异常。根因是重启清除锚点后，第一次同日读数仍按旧锚点做 120 秒偏差检查，从而把当天锁成异常。该状态已有回归；已有异常存档需重新校时后恢复今日统计，未知日期的步数不回填。
- “资源与数据”页的不完整日桶措辞与首页仍有差异，可能把部分记录显示为普通“今日”数值。
- RTC 清除未完成标记至最终确认间仍有非原子窗口；故障注入不证明实际备电保持和物理断电安全。
- 当前没有首次 LittleFS 格式化入口。挂载失败保留已有数据并限制玩法，不能通过自动格式化掩盖问题。
- 主机脚本当前在 macOS 验证；Linux 和 Windows 尚未实跑。端口自动探测仅支持 macOS，多个 USB 设备须显式指定目标。

以下设备检查仍开放：除上述首页电源键短按外的实体按键／触摸与唤醒场景、圆屏观感和动画时延、四种携带方式计步误差、真实行走达到单场 200–500 步目标并完成遭遇／捕获／保存的全过程、RTC 保持、满盘与物理断电恢复、动态堆峰值、完整任务栈、采样时延和省电电流。
