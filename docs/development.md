# 开发指南

## 环境与首次构建

在仓库根目录执行命令。先安装 [mise](https://mise.jdx.dev/) 与 [RTK](https://github.com/rtk-ai/rtk)，再安装项目工具：

```bash
rtk proxy mise trust
rtk proxy mise install
rtk proxy mise exec -- python3 -m pip install -r requirements-dev.txt
rtk proxy mise run build
```

[mise.toml](../mise.toml) 固定 Python 3.14.2、Node 24.22.1、pipx 1.18.4、PlatformIO 6.2.0、RTK 1.1.0 和 actionlint 1.7.12；[requirements-dev.txt](../requirements-dev.txt) 固定 esptool 5.6.0 与 Pillow 12.1.0。ESP32 工具链和 Arduino 由 [platformio.ini](../platformio.ini) 的 Espressif32 6.10.0 管理，库版本为 M5Unified 0.2.19、M5GFX 0.2.27、M5PM1 1.0.6、M5IOE1 1.0.8。

PlatformIO 使用当前用户的 `~/.platformio` 工具缓存，项目依赖位于 `.pio/libdeps/20_pokedex`。首次构建需要联网；完整生成数据随仓库提供，固件编译不需要下载原始图片。

主机测试使用系统编译器，建议 Clang，可用 `CC`／`CXX` 指定。动画解码需要 zlib 头文件与链接库；渲染和模拟器还需要 SDL2 原生库及 `sdl2-config`，这些系统依赖不由 Python 或 PlatformIO 安装。当前实际验证环境是 macOS；其他系统应先核对编译器、SDL2 和 USB 串口支持。

## 桌面模拟

完成首次构建并安装 SDL2 后，运行 `rtk proxy mise run simulate` 打开桌面窗口。支持键盘、鼠标、模拟走路、时间快进和独立双槽存档。操作与平台限制见[桌面模拟指南](simulator.md)。模拟器与离屏渲染共用 [host_build.py](../scripts/host_build.py) 的编译入口和 Runtime 源文件清单。

E2E 入口为 `rtk proxy mise run test-simulator-e2e`，可加 `-- --case capture` 选择场景或 `-- --suite core` 覆盖 P0。验收范围按下文交付规则选择；覆盖与限制见[模拟器 E2E](design/simulator-e2e.md)。

## GitHub Actions

[Simulator E2E 工作流](../.github/workflows/simulator-e2e.yml) 在每个 PR、`main` 推送和手动触发时运行，包括仅修改文档的 PR。Ubuntu 24.04 使用 Clang、SDL2 和 Xvfb，mise 安装项目固定工具，PlatformIO 按 `platformio.ini` 安装库，再执行无筛选的 `rtk proxy mise run test-simulator-e2e`。CI 不构建或烧录设备固件。

测试成功或失败都会上传本次目录中的结果、事件、截图和测试存档，artifact 名称为 `simulator-e2e-<run-id>-<attempt>`，保留 7 天。依赖或编译失败时可能尚无测试证据，原因见步骤日志。PR 更新会取消同一 PR 的旧运行。工作流通过 `rtk proxy mise run check-ci` 检查。

## 新功能与任务交付

测试先说明独立的产品／逻辑假设，再选择验证层级；只保留这些假设与构建发布保障，详见[测试保留标准](verification/test-policy.md)。已有真实用户路径覆盖的普通成功流程不重复添加低层测试，不检查内部实现形状或固定整屏像素。

新功能必须随实现新增对应 E2E 测试：在 [test/simulator](../test/simulator) 新增场景或扩展已有场景的操作链和断言，接入 [runner](../scripts/run_simulator_e2e.py) 的默认全量运行，并更新[覆盖矩阵](design/simulator-e2e-coverage.md)。测试复用真实 SDL → Runtime 路径；涉及持久状态时验证新进程恢复。

默认按影响范围运行专项检查和相关 `--case`，包括新增功能、局部 UI、单流程修复和单场景测试调整。纯文档、注释、说明截图只核对内容、链接与格式，不触发 E2E，也不中断运行中的验收。

全量仅用于定向检查不足以覆盖的跨流程核心变化、存档协议与迁移、全局 E2E 基础设施变化，或明确的完整回归要求；边界见[验证指南](verification.md#每次任务的-e2e-验收要求)。主 agent 先说明触发理由，再自己或指定一个验收 agent 统一执行，子 agent 默认只做定向检查。

失败先排障并定向复测，后续改动只重验受影响范围；只有再次命中全量条件才重跑。可组合未受影响的已有结果与修复后的定向结果，但须分别列出，不称为最终全量通过。`--build-only` 不证明行为通过，失败或阻塞必须如实报告，保留各轮证据。

## 检查入口

先运行 `build` 安装锁定依赖，RTC、渲染与模拟器检查会直接编译这些库中的实现。素材测试还需要原图缓存：

```bash
rtk proxy mise run sprites
rtk proxy mise run icons-download
```

这两条命令只在缺少缓存时联网，下载后核对仓库清单中的 SHA256。后续 `icons` 默认离线；改变来源和清单须单独评审，不能为消除哈希错误随意重写清单。

| 任务（`rtk proxy mise run …`） | 覆盖范围 |
| --- | --- |
| `test` | 图鉴时序、手势边界、解码与整数缩放；交替／替换／损坏来源后的像素正确性 |
| `test-adventure` | 玩法、培养、迁移、日期、codec 与重试；TestModePlatform 的模拟时间和 IMU／RTC 隔离 |
| `test-motion` | 合成步态、待确认峰归属、日期转换与跨日边界 |
| `test-clock-sync` | USB 校时协议、主机重试、Runtime 校时事务与存档保护 |
| `test-rtc` | 真实 RX8130 驱动、板级启动、初始化与校时故障恢复 |
| `test-storage` | 双槽、I/O、文件系统与分配失败；测试目录隔离、模式标记、重建恢复、清理失败、RAM-only 与存储故障开关 |
| `check-adventure-catalog` | 649 目录、五地区普通池与实际 C++ 抽样、320 条进化边与费用；固定来源及当前／历史生成物 |
| `test-pokedex` | 固定 CSV 来源哈希、篡改拒绝、649 英文名称及属性集合、中文资料与英文表生成物 |
| `test-sprites` | 649 个 GIF 来源、原始帧像素／时长与固件解码；子集独立输出及发布文件保护 |
| `test-icons` | 649 个 PNG 来源哈希、透明规则、生成像素及缺来源时拒绝发布 |
| `test-scenes` | 生成器 `--check` 核对五地区普通／暗色／分层数据和清单 |
| `test-render` | 12 套 M5GFX 入口：文字、圆屏输出、残影、首页日期与极大计数、步峰归属、输入释放、慢保存、睡眠／唤醒、校时事务、测试控制台及图形失败错误 |
| `test-simulator-e2e` | 48 场（P0 27、P1 21）：启动、语言、捕获、培养进化、开战伙伴绑定、失败重试、强杀恢复、旧档迁移、地区与遭遇历史、时钟和练习隔离；真实文件及新进程重载 |
| `build` | ESP32-S3 固件 |
| `check-startup-stack` | 构建后目标 ELF 的 Engine 启动局部栈帧 |
| `check-ci` | GitHub Actions 工作流语法与表达式 |

当前没有独立 typecheck 任务，C++ 类型检查由实际主机与固件编译承担；只构建模拟器使用 `rtk proxy mise run simulate -- --build-only`。独立 smoke／`test-simulator` 入口已删除，集成行为由 E2E 检查。

渲染、玩法、计步、RTC、校时和存储任务支持 `MYM5_VERIFICATION_DIR` 指定本轮结果目录；未指定时使用 `.pio/verification` 下的时间戳目录。不要覆盖已有验收产物。可选单个渲染套件或定向 E2E，例如：

```bash
rtk proxy mise exec -- python3 scripts/run_render_tests.py test_home_render
rtk proxy mise exec -- python3 scripts/run_render_tests.py test_capture_render
rtk proxy mise exec -- python3 scripts/run_render_tests.py test_progression_render
rtk proxy mise exec -- python3 scripts/run_render_tests.py test_test_mode_render
rtk proxy mise run test-simulator-e2e -- --case battle-cues --case battle-fixed
rtk proxy mise run test-simulator-e2e -- --case progression --case isolation --case edges
```

战斗、进化、预览、缩略图和基础电源的五套独立渲染入口已删除。相关用户操作由 E2E 覆盖；`test_progression_render` 保留尚未确认的步峰跨伙伴切换／进化时的账目归属，`test_power_pages_render` 保留睡眠继续计步、草稿恢复与失败重试等边界。目录与玩法测试仍检查全部 320 条进化规则，但不再逐条进入 UI 预览。具体场景以[覆盖矩阵](design/simulator-e2e-coverage.md)为准，主机截图不证明设备帧率或完整视觉验收。

设备内测试模式的操作入口见[使用指南](usage-test-mode.md)；控制台操作覆盖、平台隔离及未验边界见[测试模式验证说明](verification/test-mode.md)。

详细覆盖与限制见[验证说明](verification.md)。固件变更还须运行 `build`；UI 与交互变更须核对真实渲染截图及输入路径。

烧录后自动校时、单独校时与只读查询的命令见[时钟恢复](design/clock-recovery.md#单独校时与查询)。

## 模块归属

| 位置 | 职责 |
| --- | --- |
| [main.cpp](../firmware/apps/20_pokedex/main.cpp) | 设备输入、IMU、时钟、存档装配与测试模式切换时的 Runtime 重建；熄屏空闲时在下一次采样前 light sleep |
| [runtime.cpp](../firmware/apps/20_pokedex/runtime.cpp) 及同名前缀文件 | 采样、页面、输入、省电和保存调度 |
| [adventure.hpp](../firmware/apps/20_pokedex/adventure.hpp) 及同名前缀文件 | Engine 唯一修改玩法状态，提交与重试 |
| [save_codec.cpp](../firmware/apps/20_pokedex/save_codec.cpp)、[file_store.cpp](../firmware/apps/20_pokedex/file_store.cpp) | 版本、语义校验、CRC、双槽和回读 |
| [device_store.cpp](../firmware/apps/20_pokedex/device_store.cpp) | LittleFS 所有者；正式／测试路径、模式标记、快照、清理、RAM-only 与存储故障注入 |
| [test_mode.hpp](../firmware/apps/20_pokedex/test_mode.hpp)、[test_mode.cpp](../firmware/apps/20_pokedex/test_mode.cpp) | 测试模式、模拟时钟状态、故障类型、诊断数据与宿主接口 |
| [test_mode_platform.hpp](../firmware/apps/20_pokedex/test_mode_platform.hpp)、[test_mode_platform.cpp](../firmware/apps/20_pokedex/test_mode_platform.cpp) | 测试模式使用静止合成 IMU 样本与模拟时钟，隔离真实 IMU 读取和硬件 RTC 写入 |
| [test_console.hpp](../firmware/apps/20_pokedex/test_console.hpp)、[test_console.cpp](../firmware/apps/20_pokedex/test_console.cpp) | 构造四种固定测试场景，经 codec 往返与语义校验后提交测试档 |
| [runtime_input.cpp](../firmware/apps/20_pokedex/runtime_input.cpp)、[screens_test.cpp](../firmware/apps/20_pokedex/screens_test.cpp) | 隐藏入口与测试控制请求、测试页面绘制；模式切换请求交给 `main.cpp`，页面不直接写档 |
| [screens.cpp](../firmware/apps/20_pokedex/screens.cpp) 及同名前缀文件 | 从已发布状态绘制页面；公共组件位于 `screens_components.cpp` |
| [locale.hpp](../firmware/apps/20_pokedex/locale.hpp)、[i18n.hpp](../firmware/apps/20_pokedex/i18n.hpp)、[pokedex_text.hpp](../firmware/apps/20_pokedex/pokedex_text.hpp) | 语言值、成对界面文案与物种名称／属性查询；语言状态和存档规则见[中英文界面](design/i18n.md) |
| [screen_theme.hpp](../firmware/apps/20_pokedex/screen_theme.hpp) | 页面与 Viewer 的颜色和图标类型 |
| [battle_presentation.hpp](../firmware/apps/20_pokedex/battle_presentation.hpp) | 从 `BattleView` 判定动作及 450ms 显示窗口，定义提示圈、下一拍预告轨道与倒计时的布局，不结算玩法 |
| [game.cpp](../firmware/apps/20_pokedex/game.cpp)、[sprite.cpp](../firmware/apps/20_pokedex/sprite.cpp) | 不依赖 Arduino 的图鉴时序与解码 |
| [firmware/shared](../firmware/shared) | C152 初始化、电源与反馈 |
| [tools](../tools)、[assets](../assets) | 生成器、固定来源、资料与哈希清单 |
| [simulator](../simulator) | SDL 桌面窗口、输入与时钟适配、独立主机存档；复用固件 Runtime |
| [test](../test)、[scripts](../scripts) | 回归、构建及设备工具 |

设备冷启动在板级初始化后构造 Runtime，调用 `startStartup()` 先分配绘图缓冲并显示首帧，再初始化 RTC、挂载存储并执行 `begin()`。`Screens::begin()` 复用已分配缓冲。启动页按平台时钟至少显示 1200ms，循环期间持续采样并消耗输入；结束后等待释放再操作原目的页面。普通 Runtime 重建不调用 `startStartup()`；存储错误沿用错误页，图形分配失败直接显示图形错误。启动绘制不增加存档字段或提交。

培养、进化和兼容修改先读[玩法契约](design/walking-progression.md)。schema1、旧字节 fixture、RTC 故障和按键回归仍是当前契约的一部分。页面不写账户、不直接提交存档。

## 生成数据与动画

生成入口、固定来源、目录规则、解码和整数缩放见[素材维护参考](assets.md)。`*_generated.hpp/.cpp` 必须通过对应生成器更新，不手工修改。

串口每 10 秒输出采样、最大间隔、输入轮询、质量缺口、RTC 错误、绘制与保存耗时。`maxgap`、`input`、`render`、`commit` 的单位为毫秒。锁定 IMU 接口不能区分 I/O 失败和无新样本，显示 `imuio=not-exposed`；日志不代替真实计步和屏幕观察。

## 发布文件

单 app 分区表 [partitions_single_app_16MB.csv](../partitions_single_app_16MB.csv) 把 factory app 放在 `0x10000`、大小 `0xC80000`，SPIFFS 保持 `0xC90000`／`0x360000`，coredump 保持 `0xFF0000`／`0x10000`。素材仍编入 app，不迁移到文件系统。

发布范围为当前源码、生成数据、测试与 fixture、资料清单、文档和许可。`.pio/`、原始素材缓存、Flash 备份、本地报告和系统缓存由 [.gitignore](../.gitignore) 排除。本地验收数据不作为新用户构建前置条件；新一轮输出与已有原始证据应分开保存，见[证据说明](verification.md#生成和保留本轮证据)。

发布前核对 `git status` 和实际暂存内容，避免把旧版本索引当作当前工作树。不要上传个人备份或设备存档，也不要覆盖旧备份。代码 MIT 许可不能代替宝可梦图像和名称的权利授权。
