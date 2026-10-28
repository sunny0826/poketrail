# 开发指南

## 环境与首次构建

在仓库根目录执行命令。先安装 [mise](https://mise.jdx.dev/) 与 [RTK](https://github.com/rtk-ai/rtk)，再安装项目工具：

```bash
rtk proxy mise trust
rtk proxy mise install
rtk proxy mise exec -- python3 -m pip install -r requirements-dev.txt
rtk proxy mise run build
```

[mise.toml](../mise.toml) 固定 Python 3.14.2、Node 24.22.1、pipx 1.18.4 和 PlatformIO 6.2.0；[requirements-dev.txt](../requirements-dev.txt) 固定 esptool 5.6.0 与 Pillow 12.1.0。ESP32 工具链和 Arduino 由 [platformio.ini](../platformio.ini) 的 Espressif32 6.10.0 管理，库版本为 M5Unified 0.2.19、M5GFX 0.2.27、M5PM1 1.0.6、M5IOE1 1.0.8。

PlatformIO 使用当前用户的 `~/.platformio` 工具缓存，项目依赖位于 `.pio/libdeps/20_pokedex`。首次构建需要联网；完整生成数据随仓库提供，固件编译不需要下载原始图片。

主机测试使用系统编译器，建议 Clang，可用 `CC`／`CXX` 指定。渲染还需要系统 SDL2 原生库及 `sdl2-config`，这些不由 Python 或 PlatformIO 安装。当前实际验证环境是 macOS；其他系统应先核对编译器、SDL2 和 USB 串口支持。

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
| `test-rtc` | 真实 RX8130 驱动、初始化与校时故障 |
| `test-storage` | 双槽、I/O、文件系统与分配失败；测试目录隔离、模式标记、重启恢复、清理失败、RAM-only 与存储故障开关 |
| `check-adventure-catalog` | 251 目录、属性推导路线掩码、124 普通根与 122 进化边 |
| `test-pokedex` | 251 资料、Gen2 来源哈希与 Gen1 快照一致性 |
| `test-sprites` | 251 个 GIF 哈希、完整帧和固件逐像素解码 |
| `test-icons` | 251 个 PNG 哈希、透明规则与生成像素 |
| `test-scenes` | 三路线场景来源、RGB565 像素与生成物 |
| `test-render` | 14 套真实 Runtime／M5GFX 页面、战斗与进化动画、图鉴预览、输入回归；含测试模式入口、控制台、场景、标识与退出 |
| `build` | ESP32-S3 固件 |
| `check-startup-stack` | 构建后 Engine 启动局部栈帧 |

渲染、玩法、计步、RTC 和存储任务支持 `MYM5_VERIFICATION_DIR` 指定本轮结果目录；未指定时使用 `.pio/verification` 下的时间戳目录。不要覆盖已有验收产物。可选单个渲染套件，例如：

```bash
rtk proxy mise exec -- python3 scripts/run_render_tests.py test_home_render
rtk proxy mise exec -- python3 scripts/run_render_tests.py test_battle_render
rtk proxy mise exec -- python3 scripts/run_render_tests.py test_evolution_render
rtk proxy mise exec -- python3 scripts/run_render_tests.py test_evolution_preview_render
rtk proxy mise exec -- python3 scripts/run_render_tests.py test_test_mode_render
```

`test_battle_render` 通过真实按键、Runtime 和 M5GFX 核对双角色原帧时长、攻击与躲避位移、即时 HP、最终动作窗口、暂停恢复和保存失败。`test_evolution_render` 覆盖保存后播放、形态交替、跳过与释放门、重试与重启、播放中计步及圆屏边界。`test_evolution_preview_render` 遍历全部 122 条进化边，核对新档、坏档、未拥有和零培养步下的预览、伊布五分支与无畏小子三分支、连续刷新、原条目返回、账户不变及真实进化重试隔离。输出是主机截图，不表示设备已达到目标帧率。

`test_test_mode_render` 覆盖资源页和错误页顶部长按、来源确认、控制台翻页与返回、四种固定场景、手动 +1 步触发遭遇、模拟校时、故障选择、重置和退出。测试使用主机输入与存储替身，不执行 `main.cpp` 的完整设备切换链。操作入口见[设备内测试模式](usage-test-mode.md)，已有日志和未覆盖边界见[Issue #7 验证记录](verification.md#issue-7-设备内测试模式2026-10-27)。

详细覆盖与限制见[验证说明](verification.md)。提交前按变更范围运行相关检查；固件变更还应运行 `build`。UI 变更须读取真实渲染截图，交互变更须验证实际输入路径。

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
| [test](../test)、[scripts](../scripts) | 回归、构建及设备工具 |

培养、进化和兼容修改先读[玩法契约](design/walking-progression.md)。schema1、旧字节 fixture、RTC 故障和按键回归仍是当前契约的一部分。页面不写账户、不直接提交存档。

## 生成数据与动画

`*_generated.hpp/.cpp` 随源码发布，必须通过生成器更新。不要手工改生成物。

| 数据 | 更新入口 |
| --- | --- |
| 动画 | `rtk proxy mise run sprites` |
| 缩略图 | `rtk proxy mise run icons`，初次用 `icons-download` |
| 中文资料 | `rtk proxy mise run pokedex` |
| 路线场景 | `rtk proxy mise run scenes` |
| 进化规则 | `rtk proxy mise exec -- python3 tools/build_evolution_rules.py` |
| 遭遇目录 | `rtk proxy mise exec -- python3 tools/build_adventure_catalog.py` |

遭遇目录入口为 [adventure-251.json](../assets/adventure-251.json)，`routeTypes` 声明属性到路线的归属，`speciesFiles` 列出 [adventure-routes](../assets/adventure-routes) 内的物种配置。物种文件只保留编号、名称、稀有度和权重，路线由生成器读取 `pokedex-gen1.json`／`pokedex-gen2.json` 的 `pokemon_type_name` 推导；双属性按位或，251 种都必须得到非空掩码。`ordinaryRoots` 显式声明普通资格，`reserved` 记录后续独立活动的保留物种；生成器合并后按编号排序，重复、缺失编号、手写 `routes`、未归属物种或路线映射缺口都会报错。生成器同时写出 schema1–4 的冻结旧掩码 `route_masks_v4_generated.hpp`；两份头文件都不得手改。旧的 `adventure-gen1.json` 与关都三份路线文件保留为冻结的关都快照，当前生成器只消费 `adventure-251.json`。

动画缓存位于 `assets/sprites/black-white`，缩略图位于 `assets/sprites/icons`，均不纳入 Git。来源、版本和权利声明见[第三方声明](../THIRD_PARTY_NOTICES.md)。动画和缩略图清单按世代拆分为 Gen1／Gen2 两份，固定来源均为 PokeAPI sprites 提交 `97a7832d307311c0e1c6ea9035308bf80cc94f81`。指定子集必须使用独立输出路径，防止覆盖完整图鉴：

```bash
rtk proxy mise exec -- python3 tools/build_sprites.py --ids 1,4,7 --out /tmp/sprites_generated.hpp
```

1～251 共 251 只动画、13496 个时序帧，保留原画布与时长；16 色 RGB565 调色板与 RLE 数据存于 Flash，相同帧共享数据。设备只解码当前帧，透明键为 `0xF81F`，黑白像素保留。未知图腾只收录 #201 默认形态。最大原画布为 #249 洛奇亚的 153×94，因此源画布上限为 160。

整数缩放类别由 [sprite-scales.json](../assets/sprite-scales.json) 显式冻结：Gen1 保留当前 72 个中间／最终形态与 16 个无进化链物种；Gen2 的 42 个中间／最终形态、27 个无进化链物种与 31 个基础形态分别列出。生成器不再从进化父关系推导缩放，避免后续目录扩张改变已有画面。基础形态使用 2 倍整数缩放，中间和最终形态与目录内无进化链物种优先 3 倍；超出 336×270 或页面高度限制时回退 2 倍。页面再按可用高度限制：首页避免 3 倍角色占用步数区，Viewer 为属性栏留出空间，遭遇和结果页使用 1 倍或 2 倍。双角色战斗中，原图不超过 68×65 的角色用 2 倍，其他用 1 倍。所有页面保留完整原画布和透明像素，不插值。

三路线场景通过 `tools/design_preview_scenes.py` 生成，清单位于 `assets/scene-backgrounds.json`。普通与暗色合成图，以及首页远景／中层／近层均为 228×135 RGB565，绘制时逐行放大 2 倍；场景数组存于 Flash，只需 456 像素的行缓冲。首页由 `Screens::advanceScene()` 持有视觉步数，屏幕唤醒且首页可见时每 100ms 默认推进一次，地面向下循环，路线切换归零；走路不加速，其他页面和熄屏期间暂停。该进度不进入 Engine 或存档。遭遇、战斗和投球使用已保存遭遇路线的静态暗色图，选择新路线不改变当前遭遇背景。

第二世代中文名、属性、身高和体重来自 PokeAPI 提交 `f38eb24b70f8462976154f534e79bf2bdd7af847` 的四份 CSV，保存在 [pokedex-source](../assets/pokedex-source)，哈希和字节数见同目录 `manifest.json`；[build_pokedex.py](../tools/build_pokedex.py) 只读本地缓存生成 [pokedex-gen2.json](../assets/pokedex-gen2.json)，普通构建不访问网络。

动画按原始帧时长推进，点击和战斗动作不会重启动画。双角色各自持有播放位置，共用解码缓冲；白闪和残影只改变本帧颜色与位置。Engine 与 HP 在判定后立即更新，结束战斗时页面保留最多 450ms 动作窗口，失败保存立即转到错误页。判定窗口和下一拍时间不变。进化关系使用编号 1～251，缩放不代表实际身高比例。

串口每 10 秒输出采样、最大间隔、输入轮询、质量缺口、RTC 错误、绘制与保存耗时。`maxgap`、`input`、`render`、`commit` 的单位为毫秒。锁定 IMU 接口不能区分 I/O 失败和无新样本，显示 `imuio=not-exposed`；日志不代替真实计步和屏幕观察。

## 发布文件

单 app 分区表 [partitions_single_app_16MB.csv](../partitions_single_app_16MB.csv) 把 factory app 放在 `0x10000`、大小 `0xC80000`，SPIFFS 保持 `0xC90000`／`0x360000`，coredump 保持 `0xFF0000`／`0x10000`。素材仍编入 app，不迁移到文件系统。

发布范围为当前源码、生成数据、测试与 fixture、资料清单、文档和许可。`.pio/`、原始素材缓存、Flash 备份、本地报告和系统缓存由 [.gitignore](../.gitignore) 排除。本地历史验收证据已清理，不作为新用户构建前置条件；历史摘要与重新检查方式见[验证说明](verification.md#历史证据清理与重新检查)。

发布前核对 `git status` 和实际暂存内容，避免把旧版本索引当作当前工作树。不要上传个人备份或设备存档，也不要覆盖旧备份。代码 MIT 许可不能代替宝可梦图像和名称的权利授权。
