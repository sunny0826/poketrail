# 在桌面运行 PokeTrail

桌面模拟器运行与固件相同的 Runtime、玩法、计步器、存档 codec 和 M5GFX 绘制代码。SDL2 显示圆屏，键盘和鼠标提供输入，无需连接设备。

## 启动

先按[开发指南](development.md#环境与首次构建)安装 mise 工具与 Python 依赖，并运行一次 `build` 获取锁定的固件库。主机还需要系统 C/C++ 编译器、zlib 头文件与链接库、SDL2 原生库和 `sdl2-config`。macOS 可以通过系统包管理器安装 SDL2，例如 `rtk brew install sdl2`；运行时仍由 mise 管理。

在仓库根目录执行：

```bash
rtk proxy mise run simulate
```

首次启动会编译主机程序；后续启动复用 `.pio/host-build` 下的目标文件，源文件或头文件变化会触发重编译。窗口初次显示校时引导，选择校时或暂不校时，再选择初始伙伴。

## 操作

| 操作 | 桌面输入 |
| --- | --- |
| 左侧键／躲避 | `←` 或 `A` |
| 右侧键／攻击 | `→` 或 `D` |
| 同时按两键／菜单确认 | `Enter`，也可同时按左右键 |
| 双键长按 | 按住 `Enter` |
| 触摸／滑动 | 鼠标左键点按、按住或水平拖动圆屏 |
| 蓄力投球 | 按住右侧键或触屏，松开投球 |
| 开关模拟走路 | `W` |
| 切换 1 倍／16 倍模拟时间 | `Tab` |
| 熄屏／唤醒 | `P` |
| 截图 | `F12` |
| 保存并退出 | 关闭窗口；macOS 也可按 `⌘Q` |

`W` 产生合成步态，再由实际计步器识别，不直接改步数账户。开始走路后需要数步确认节奏。`Tab` 加速整个模拟时钟，包括动画、节拍、计步和熄屏倒计时；对战前建议切回 1 倍。倍速表示每轮推进的逻辑时间，不是桌面性能保证。

30 秒无游戏输入后，圆屏会按固件规则熄屏；按 `P` 唤醒。切换到其他窗口时，模拟器释放按键和触摸，并关闭走路与快进，避免输入卡住。

截图保存为 BMP，位于 `.simulator/screenshots/本次启动时间/`。截图包含真实 M5GFX 圆屏和模拟器操作栏。

## 存档与时钟

默认存档在 `.simulator/save/`，包含 `save-a.bin`、`save-b.bin` 和模拟时钟文件。使用实际 FileStore 的双槽、CRC、语义校验与回读；同一个目录只能由一个模拟器进程使用。该目录已加入 Git 忽略，不读取或写入实体设备。

正常关闭会保存待提交步数；战斗中关闭会先请求暂停，蓄力投球中关闭会先处理释放，再保存退出。保存失败时窗口保留，修复目录权限后可再次关闭。强制结束进程不能保证未保存的步数被保留。

模拟时钟首次以电脑时间开始；手动校时只改变模拟器。退出时保存模拟时间相对电脑时间的偏移，因此快进后的日期在重新启动时仍延续，关闭期间也会走时。

换一个目录可以新开游戏，保留原档：

```bash
rtk proxy mise run simulate -- --save-dir .simulator/another-save
```

## 验证与边界

`test-simulator` 检查基础集成；`test-simulator-e2e` 检查捕获、培养进化、保存失败重试、强杀恢复、旧档迁移和时钟。结构与覆盖边界见[模拟器 E2E](design/simulator-e2e.md)。

新功能必须新增对应 E2E 覆盖；每次任务最终改动完成后、报告完成前必须运行全量 `test-simulator-e2e`，包括仅修改文档的任务。下面的 `--suite core`、`--case` 和 `--build-only` 用于开发调试，不能代替交付前全量运行。完整规则见[验证指南](verification.md#每次任务的-e2e-验收要求)。

```bash
rtk proxy mise run test-simulator
rtk proxy mise run test-simulator-e2e
rtk proxy mise run test-simulator-e2e -- --suite core
rtk proxy mise run test-simulator-e2e -- --case capture --case progression
rtk proxy mise run simulate -- --build-only
```

`test-simulator` 创建独立验收目录，通过 SDL 事件队列驱动真实窗口，检查初始伙伴、菜单、滑动、图鉴、熄屏唤醒、模拟走路、遭遇、对战暂停、失焦和关闭保存，再启动第二个进程读取同一份存档。它还将首页截图的圆屏像素与 Runtime 画布逐个比对，检查目录互斥和损坏存档保护。结果位于 `.pio/verification/simulator-时间戳/`；也可用 `MYM5_VERIFICATION_DIR` 指定一个尚不存在的目录。截图同时保留 BMP 和 PNG。

E2E 每个场景使用独立目录，每次 reload 都启动新进程；失败不会自动重试。`--suite core` 只选 P0，`--case` 可重复指定；`--build-only` 只编译。结果在 `.pio/verification/simulator-e2e-时间戳/`，也可用 `MYM5_VERIFICATION_DIR` 指定尚不存在的目录。`manifest.json` 记录源码和可执行文件哈希、固定种子与时间、编译器及 SDL 版本；根目录的 `result.json` 汇总场景通过或失败，各启动目录的 `result.json` 记录退出码和墙钟耗时。每次启动保留事件、业务快照、截图、进程日志和真实双槽文件。

SDL 队列输入不证明操作系统键鼠注入；原生输入和人工视觉验收须分别记录。固定截图比较只覆盖指定画面，不能代替对本轮 UI 改动的读图核对。存储故障接缝只由测试驱动使用；桌面正常运行仍显示原有错误提示并在关闭保存失败时保持窗口。

本地主机验证使用 macOS；Ubuntu 24.04 的 X11/Xvfb 全量 E2E 由 [GitHub Actions](../.github/workflows/simulator-e2e.yml) 执行，通过状态以对应提交的 CI 结果为准。Linux 真实桌面人工操作尚未验收。存储适配使用 POSIX 文件锁，暂不支持原生 Windows。模拟器复用 [test/support](../test/support) 的主机 M5 接口；构建清单位于 [host_build.py](../scripts/host_build.py)，渲染测试和模拟器共用同一份固件源文件列表。

这是应用层桌面模拟，不模拟 ESP32 指令、真实 IMU、硬件 RTC、LittleFS 挂载、电池、声音或震动。设备内隐藏测试控制台的存储切换也未接入桌面版；模拟走路使用 `W`，新档使用 `--save-dir`。主机截图与输入检查不能证明设备性能、真实计步或断电恢复。
