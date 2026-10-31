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

运行 `rtk proxy mise run test-simulator-e2e` 验证完整用户旅程、保存失败重试和进程强杀恢复。开发时可用 `-- --suite core` 只跑 P0，或 `-- --case capture` 选单场景；任务交付前必须运行无筛选的全量套件。覆盖与限制见[模拟器 E2E](design/simulator-e2e.md)。

## GitHub Actions

[Simulator E2E 工作流](../.github/workflows/simulator-e2e.yml) 在每个 PR、`main` 推送和手动触发时运行，包括仅修改文档的 PR。Ubuntu 24.04 使用 Clang、SDL2 和 Xvfb，mise 安装项目固定工具，PlatformIO 按 `platformio.ini` 安装库，再执行无筛选的 `rtk proxy mise run test-simulator-e2e`。CI 不构建或烧录设备固件。

测试成功或失败都会上传本次目录中的结果、事件、截图和测试存档，artifact 名称为 `simulator-e2e-<run-id>-<attempt>`，保留 7 天。依赖或编译失败时可能尚无测试证据，原因见步骤日志。PR 更新会取消同一 PR 的旧运行。工作流通过 `rtk proxy mise run check-ci` 检查。

## 新功能与任务交付

新功能必须随实现新增对应 E2E 测试：在 [test/simulator](../test/simulator) 新增场景或扩展已有场景的操作链和断言，接入 [runner](../scripts/run_simulator_e2e.py) 的默认全量运行，并更新[覆盖矩阵](design/simulator-e2e.md#覆盖矩阵)。测试复用真实 SDL → Runtime 路径；涉及持久状态时验证新进程恢复。

每次仓库任务完成最终改动后、报告完成前，都必须运行 `rtk proxy mise run test-simulator-e2e`，包括仅修改文档、配置或测试的任务。`--case`、`--suite core`、`--build-only` 或以前的通过记录不能代替本轮全量结果。失败或阻塞须报告为未完成验收，并保留新一轮证据。详细要求见[验证指南](verification.md#每次任务的-e2e-验收要求)。

## 检查入口

先运行 `build` 安装锁定依赖，RTC、计步适配和渲染测试会直接编译这些库中的实现。素材测试还需要原图缓存：

```bash
rtk proxy mise run sprites
rtk proxy mise run icons-download
```

这两条命令只在缺少缓存时联网，下载后核对仓库清单中的 SHA256。后续 `icons` 默认离线；改变来源和清单须单独评审，不能为消除哈希错误随意重写清单。

| 任务（`rtk proxy mise run …`） | 覆盖范围 |
| --- | --- |
| `test` | 图鉴时序、手势、解码和整数缩放 |
| `test-adventure` | 玩法、培养、迁移、日期、codec 与重试；测试时钟、IMU／RTC 隔离、手动步数事件触发遭遇与退出加载不写正式档 |
| `test-motion` | 合成步态、日期和设备时钟适配 |
| `test-clock-sync` | USB 校时协议、主机重试、Runtime 校时事务与存档保护 |
| `test-rtc` | 真实 RX8130 驱动、初始化与校时故障 |
| `test-storage` | 双槽、I/O、文件系统与分配失败；测试目录隔离、模式标记、重启恢复、清理失败、RAM-only 与存储故障开关 |
| `check-adventure-catalog` | 649 目录、属性路线掩码、普通资格与 320 条进化边；旧 124 种资格、122 条费用和历史路线不变 |
| `test-pokedex` | 649 资料、Gen2–5 来源哈希与 Gen1／Gen2 快照一致性 |
| `test-sprites` | 649 个 GIF 哈希、完整帧和固件逐像素解码 |
| `test-icons` | 649 个 PNG 哈希、透明规则与生成像素 |
| `test-scenes` | 三路线场景来源、RGB565 像素与生成物 |
| `test-render` | 15 套真实 Runtime／M5GFX 页面、战斗与进化动画、图鉴预览、输入回归；含测试模式入口、控制台、场景、标识与退出 |
| `test-simulator` | SDL 桌面输入、图鉴、走路遭遇、窗口退出、双槽存档重启与目录隔离 |
| `test-simulator-e2e` | 捕获、培养进化、失败重试、强杀恢复、旧档迁移、时钟和练习隔离；真实文件及新进程重载 |
| `build` | ESP32-S3 固件 |
| `check-startup-stack` | 构建后 Engine 启动局部栈帧 |

渲染、玩法、计步、RTC、校时和存储任务支持 `MYM5_VERIFICATION_DIR` 指定本轮结果目录；未指定时使用 `.pio/verification` 下的时间戳目录。不要覆盖已有验收产物。可选单个渲染套件，例如：

```bash
rtk proxy mise exec -- python3 scripts/run_render_tests.py test_home_render
rtk proxy mise exec -- python3 scripts/run_render_tests.py test_battle_render
rtk proxy mise exec -- python3 scripts/run_render_tests.py test_evolution_render
rtk proxy mise exec -- python3 scripts/run_render_tests.py test_evolution_preview_render
rtk proxy mise exec -- python3 scripts/run_render_tests.py test_test_mode_render
```

`test_battle_render` 通过真实按键、Runtime 和 M5GFX 核对双角色原帧时长、攻击与躲避位移、即时 HP、最终动作窗口、暂停恢复和保存失败。`test_evolution_render` 覆盖保存后播放、形态交替、跳过与释放门、重试与重启、播放中计步及圆屏边界。`test_evolution_preview_render` 遍历全部 320 条进化边，核对新档、坏档、未拥有和零培养步下的预览、伊布七分支与无畏小子三分支、连续刷新、原条目返回、账户不变及真实进化重试隔离。输出是主机截图，不表示设备已达到目标帧率。

`test_test_mode_render` 覆盖资源页和错误页顶部长按、来源确认、控制台翻页与返回、四种固定场景、手动 +1 步触发遭遇、模拟校时、故障选择、重置和退出。测试使用主机输入与存储替身，不执行 `main.cpp` 的完整设备切换链。操作入口见[设备内测试模式](usage-test-mode.md)，未覆盖边界见[测试模式验证说明](verification.md#测试模式的验证边界)。

详细覆盖与限制见[验证说明](verification.md)。每次任务交付前运行全量 E2E，并按变更范围运行专项检查；固件变更还须运行 `build`。只改文档不必重建固件，仍须核对命令、链接和事实并运行全量 E2E。UI 变更须读取真实渲染截图，交互变更须验证实际输入路径。

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
| [screen_theme.hpp](../firmware/apps/20_pokedex/screen_theme.hpp) | 页面与 Viewer 的颜色和图标类型 |
| [battle_presentation.hpp](../firmware/apps/20_pokedex/battle_presentation.hpp) | 从 `BattleView` 判定动作及 450ms 显示窗口，不结算玩法 |
| [game.cpp](../firmware/apps/20_pokedex/game.cpp)、[sprite.cpp](../firmware/apps/20_pokedex/sprite.cpp) | 不依赖 Arduino 的图鉴时序与解码 |
| [firmware/shared](../firmware/shared) | C152 初始化、电源与反馈 |
| [tools](../tools)、[assets](../assets) | 生成器、固定来源、资料与哈希清单 |
| [simulator](../simulator) | SDL 桌面窗口、输入与时钟适配、独立主机存档；复用固件 Runtime |
| [test](../test)、[scripts](../scripts) | 回归、构建及设备工具 |

培养、进化和兼容修改先读[玩法契约](design/walking-progression.md)。schema1、旧字节 fixture、RTC 故障和按键回归仍是当前契约的一部分。页面不写账户、不直接提交存档。

## 生成数据与动画

生成入口、固定来源、目录规则、解码和整数缩放见[素材维护参考](assets.md)。`*_generated.hpp/.cpp` 必须通过对应生成器更新，不手工修改。

串口每 10 秒输出采样、最大间隔、输入轮询、质量缺口、RTC 错误、绘制与保存耗时。`maxgap`、`input`、`render`、`commit` 的单位为毫秒。锁定 IMU 接口不能区分 I/O 失败和无新样本，显示 `imuio=not-exposed`；日志不代替真实计步和屏幕观察。

## 发布文件

单 app 分区表 [partitions_single_app_16MB.csv](../partitions_single_app_16MB.csv) 把 factory app 放在 `0x10000`、大小 `0xC80000`，SPIFFS 保持 `0xC90000`／`0x360000`，coredump 保持 `0xFF0000`／`0x10000`。素材仍编入 app，不迁移到文件系统。

发布范围为当前源码、生成数据、测试与 fixture、资料清单、文档和许可。`.pio/`、原始素材缓存、Flash 备份、本地报告和系统缓存由 [.gitignore](../.gitignore) 排除。本地验收数据不作为新用户构建前置条件；新一轮输出与已有原始证据应分开保存，见[证据说明](verification.md#生成和保留本轮证据)。

发布前核对 `git status` 和实际暂存内容，避免把旧版本索引当作当前工作树。不要上传个人备份或设备存档，也不要覆盖旧备份。代码 MIT 许可不能代替宝可梦图像和名称的权利授权。
