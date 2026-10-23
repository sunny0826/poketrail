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
| `test-adventure` | 玩法、培养、迁移、日期、codec 与重试 |
| `test-motion` | 合成步态、日期和设备时钟适配 |
| `test-rtc` | 真实 RX8130 驱动、初始化与校时故障 |
| `test-storage` | 双槽、I/O、文件系统与分配失败 |
| `check-adventure-catalog` | 151 目录、73 普通根与 72 进化边 |
| `test-sprites` | GIF 哈希、完整帧和固件逐像素解码 |
| `test-icons` | PNG 哈希、透明规则与生成像素 |
| `test-render` | 真实 Runtime／M5GFX 页面和输入回归 |
| `build` | ESP32-S3 固件 |
| `check-startup-stack` | 构建后 Engine 启动局部栈帧 |

渲染、玩法、计步、RTC 和存储任务支持 `MYM5_VERIFICATION_DIR` 指定本轮结果目录；未指定时使用 `.pio/verification` 下的时间戳目录。不要覆盖已有验收产物。可选单个渲染套件，例如：

```bash
rtk proxy mise exec -- python3 scripts/run_render_tests.py test_home_render
```

详细覆盖与限制见[验证说明](verification.md)。提交前按变更范围运行相关检查；固件变更还应运行 `build`。UI 变更须读取真实渲染截图，交互变更须验证实际输入路径。

## 模块归属

| 位置 | 职责 |
| --- | --- |
| [main.cpp](../firmware/apps/20_pokedex/main.cpp) | 设备输入、IMU、时钟和存档装配 |
| [runtime.cpp](../firmware/apps/20_pokedex/runtime.cpp) 及同名前缀文件 | 采样、页面、输入、省电和保存调度 |
| [adventure.hpp](../firmware/apps/20_pokedex/adventure.hpp) 及同名前缀文件 | Engine 唯一修改玩法状态，提交与重试 |
| [save_codec.cpp](../firmware/apps/20_pokedex/save_codec.cpp)、[file_store.cpp](../firmware/apps/20_pokedex/file_store.cpp) | 版本、语义校验、CRC、双槽和回读 |
| [screens.cpp](../firmware/apps/20_pokedex/screens.cpp) 及同名前缀文件 | 从已发布状态绘制页面 |
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
| 进化规则 | `rtk proxy mise exec -- python3 tools/build_evolution_rules.py` |
| 遭遇目录 | `rtk proxy mise exec -- python3 tools/build_adventure_catalog.py` |

遭遇目录入口为 [adventure-gen1.json](../assets/adventure-gen1.json)，`speciesFiles` 列出 [adventure-routes](../assets/adventure-routes) 内的手工配置。每种按 `routes` 的第一条路线存放，跨路线资格仍全部保留；生成器合并后按编号排序，重复或缺失编号会报错。

动画缓存位于 `assets/sprites/black-white`，缩略图位于 `assets/sprites/icons`，均不纳入 Git。来源、版本和权利声明见[第三方声明](../THIRD_PARTY_NOTICES.md)。指定子集必须使用独立输出路径，防止覆盖完整图鉴：

```bash
rtk proxy mise exec -- python3 tools/build_sprites.py --ids 1,4,7 --out /tmp/sprites_generated.hpp
```

151 只动画共 8165 个时序帧，保留原画布与时长；16 色 RGB565 调色板与 RLE 数据存于 Flash，相同帧共享数据。设备只解码当前帧，透明键为 `0xF81F`，黑白像素保留。

基础形态使用 2 倍整数缩放，中间和最终形态优先 3 倍；超出 336×270 时回退 2 倍。按时间定位动画帧，不插值，不因点击反馈重启。进化关系仅使用编号 1～151，缩放不代表实际身高比例。

串口每 10 秒输出采样、最大间隔、输入轮询、质量缺口、RTC 错误、绘制与保存耗时。`maxgap`、`input`、`render`、`commit` 的单位为毫秒。锁定 IMU 接口不能区分 I/O 失败和无新样本，显示 `imuio=not-exposed`；日志不代替真实计步和屏幕观察。

## 发布文件

发布范围为当前源码、生成数据、测试与 fixture、资料清单、文档和许可。`.pio/`、原始素材缓存、Flash 备份、本地报告和系统缓存由 [.gitignore](../.gitignore) 排除。历史验收记录保存在本地证据目录，不作为新用户构建前置条件。

发布前核对 `git status` 和实际暂存内容，避免把旧版本索引当作当前工作树。不要上传个人备份或设备存档，也不要覆盖旧备份。代码 MIT 许可不能代替宝可梦图像和名称的权利授权。
