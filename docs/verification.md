# 验证范围与已知限制

自动检查入口见[开发指南](development.md#检查入口)。主机测试使用实际 Engine、Runtime、codec、RX8130 驱动及 M5GFX；它们覆盖合成输入和故障注入，不等同于实体设备验收。设备硬件概况、开发路径与社区项目见 [StopWatch 开发资源汇总](verification/stopwatch-c152-dev-resources.html)。

## 自动检查覆盖

- 玩法：随机遭遇账目与投球次数、122 条进化边、永久账户、目标冲突、唯一捕获、重放与失败重试。
- 存档：schema1 字节 fixture、schema2/v4 现场编解码、schema2/3 边界造档、schema3 活动尾部 fixture、v1/v4 混合槽与 v2 单槽升级、CRC／语义校验、未知版本拒绝、部分写入和回读故障。
- 计步与时钟：合成连续步态、峰值归属、跨日、回拨、RTC 寄存器错误、启动与校时恢复。
- 输入与页面：真实 `Button_Class` 消抖、双键确认与释放门、触摸、投球等待、图鉴重进、省电与伙伴首页。
- 进化动画：提交后播放、形态交替、跳过、重试与重启、播放中计步；图鉴预览覆盖全部 122 条进化边、伊布五分支与无畏小子三分支、账户不变及存储错误下的返回。
- 素材：来源哈希、生成物一致性、全部 13,496 个 GIF 帧（关都 151 只 8,165＋城都 100 只 5,331）与固件解码及帧时长一致。
- 构建：ESP32-S3 固件与 `Engine::begin()` 局部栈帧。静态 RAM 和局部帧不代表运行时堆峰值或完整任务栈水位。

## 历史证据清理与重新检查

截至 2026-10-27，本地各轮协调报告、审查交付、日志和渲染截图已清理。[历史验证轮次](verification/history.md)保留各轮检查摘要与设备观察，原证据文件已不可读取；检查数量、资源数字和设备观察只适用于对应版本，不能当作当前 HEAD 的验证结果。本次清理只核对文档差异与静态链接，没有重跑主机套件、构建或设备验收。

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

## 已知限制

- 2026-10-25 的设备存档显示今日步数“--”：RTC 已校准且当日有 122 步，但时钟被标为异常。根因是重启清除锚点后，第一次同日读数仍按旧锚点做 120 秒偏差检查，从而把当天锁成异常。该状态已有回归；已有异常存档需重新校时后恢复今日统计，未知日期的步数不回填。
- “资源与数据”页的不完整日桶措辞与首页仍有差异，可能把部分记录显示为普通“今日”数值。
- RTC 清除未完成标记至最终确认间仍有非原子窗口；故障注入不证明实际备电保持和物理断电安全。
- 当前没有首次 LittleFS 格式化入口。挂载失败保留已有数据并限制玩法，不能通过自动格式化掩盖问题。
- 主机脚本当前在 macOS 验证；Linux 和 Windows 尚未实跑。端口自动探测仅支持 macOS，多个 USB 设备须显式指定目标。

以下设备检查仍开放：实体按键／触摸与唤醒、圆屏观感和动画时延、四种携带方式计步误差、真实行走达到单场 200–500 步目标并完成遭遇／捕获／保存的全过程、RTC 保持、满盘与物理断电恢复、动态堆峰值、完整任务栈、采样时延和省电电流。
