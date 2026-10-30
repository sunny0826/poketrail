[中文](README.md) · [**English**](README.en.md)

<div align="center">

# PokeTrail · 宝可旅途

带着伙伴走路，遇见宝可梦，用节奏对战和投球完成捕捉。<br>
运行在 M5Stack StopWatch（C152）圆屏上的离线步行探险。

<img src="docs/images/readme/home.png" width="360" alt="圆屏首页：妙蛙种子站在森林路线上，标注携带中，下方为今日 4 步。主机 Runtime/M5GFX 渲染，不是实机拍摄。">

[开始使用](#开始使用) · [构建和烧录](#构建和烧录) · [开发指南](docs/development.md) · [验证说明](docs/verification.md)

<sub>开发中的非官方爱好项目 · 画面为主机 Runtime/M5GFX 渲染，不是实机拍摄</sub>

</div>

## 玩法

- **走路探险**：森林、海边、洞窟三条路线，遭遇池各不相同。每走 200–500 步（随机）产生一次遭遇。
- **节奏对战与捕捉**：按节拍攻击或躲避，胜利后最多投两球；精灵球不限量。成功捕获后该种永久退出遭遇池。
- **伙伴培养**：一次携带一只，只有携带者获得培养步，达到条件后手动进化。
- **图鉴**：自由浏览全国图鉴 1～649，涵盖关都、城都、丰缘、神奥与合众，动画为《黑／白》像素素材。

<p align="center">
  <a href="docs/images/readme/battle.png"><img src="docs/images/readme/battle.png" width="234" alt="圆屏对战：妙蛙种子对玛瑙水母，双方有生命格，提示右键攻击。主机 Runtime/M5GFX 渲染，不是实机拍摄。"></a>
  <a href="docs/images/readme/throw.png"><img src="docs/images/readme/throw.png" width="234" alt="圆屏投球：玛瑙水母在瞄准圈中，提示按住蓄力、松手投球。主机 Runtime/M5GFX 渲染，不是实机拍摄。"></a>
  <a href="docs/images/readme/viewer.png"><img src="docs/images/readme/viewer.png" width="234" alt="圆屏图鉴：编号 001／251 的妙蛙种子，属性为草、毒，显示当前拥有和预览进化。主机 Runtime/M5GFX 渲染，不是实机拍摄。"></a>
  <br>
  <sub>节奏对战 · 蓄力投球 · 图鉴</sub>
</p>

<p align="center">
  <a href="docs/images/readme/route-forest.png"><img src="docs/images/readme/route-forest.png" width="234" alt="圆屏首页的森林路线：林间小径与树木。主机 Runtime/M5GFX 渲染，不是实机拍摄。"></a>
  <a href="docs/images/readme/route-coast.png"><img src="docs/images/readme/route-coast.png" width="234" alt="圆屏首页的海边路线：海浪、沙滩与礁石。主机 Runtime/M5GFX 渲染，不是实机拍摄。"></a>
  <a href="docs/images/readme/route-cave.png"><img src="docs/images/readme/route-cave.png" width="234" alt="圆屏首页的洞窟路线：钟乳石与水晶。主机 Runtime/M5GFX 渲染，不是实机拍摄。"></a>
  <br>
  <sub>森林 · 海边 · 洞窟</sub>
</p>

概率、进化费用、按键和存档规则见[使用指南](docs/usage.md)。账户与迁移见[玩法契约](docs/design/walking-progression.md)。

<details>
<summary>画面来源</summary>

全部画面是 2026-10-29 的 251 种版本用主机 Runtime/M5GFX 离屏渲染的截图，不是 C152 实机拍摄，也不能当作实机帧率、观感或验收结果。点图片打开 468×468 原文件。物理圆屏为 466×466，当前 M5GFX 驱动报告 468×468；PNG 只保留圆屏内像素，圆外透明。

| 画面 | 渲染套件 | 截图 |
| --- | --- | --- |
| 首屏首页 | `test_home_render` | `home-today-complete-live`：今日 4 步来自测试中的模拟走路 |
| 森林、海边、洞窟 | `test_home_render` | `home-scene-0`～`home-scene-2`：未校时，所以今日显示 -- |
| 节奏对战、蓄力投球 | `test_adventure_render` | `battle`、`throw-active`：海边路线，对手为玛瑙水母 |
| 图鉴 | `test_adventure_render` | `viewer-entry`：测试档中的初始伙伴 |

电量处的 -- 是因为渲染时没有电池读数。重新生成：

```bash
rtk proxy mise exec -- python3 scripts/run_render_tests.py \
  test_home_render test_adventure_render
```

</details>

## 开始使用

使用项目烧录脚本后，设备会同步烧录电脑的当前时间。存储可用后选择妙蛙种子、小火龙或杰尼龟作为初始伙伴。也可以在设备上手动校时，或暂不校时：累计步数与探索照常增加，未知日期期间的步数不会补填到今日。

首页点按任一侧键或轻触屏幕进入菜单。30 秒无操作后熄屏，计步和保存继续；熄屏后仅短按电源键可点亮屏幕，触摸和侧键不会唤醒。页面操作见[使用指南](docs/usage.md)。

## 构建和烧录

使用 [mise](https://mise.jdx.dev/) 管理开发环境，命令通过 [RTK](https://github.com/rtk-ai/rtk) 执行。主机回归还需系统 C/C++ 编译器，渲染回归需 SDL2 及 `sdl2-config`。当前验证环境为 macOS；Linux 和 Windows 尚未实跑。

```bash
rtk proxy mise trust
rtk proxy mise install
rtk proxy mise exec -- python3 -m pip install -r requirements-dev.txt
rtk proxy mise run build
```

仓库包含完整生成的固件数据，普通构建无需重新下载宝可梦素材。PlatformIO 首次构建需要联网获取固定版本的工具链和库。素材更新、测试命令和代码归属见[开发指南](docs/development.md)。

烧录前确认目标设备，首次覆盖出厂固件前备份整片 Flash：

```bash
rtk proxy mise exec -- python3 scripts/detect_port.py
rtk proxy mise exec -- ./scripts/backup.sh /dev/cu.usbmodemXXXX
rtk proxy mise exec -- ./scripts/flash.sh /dev/cu.usbmodemXXXX
rtk proxy mise exec -- pio device monitor -p /dev/cu.usbmodemXXXX --baud 115200
```

以上端口是 macOS 示例，应替换为实际目标；自动探测仅支持 macOS USB 串口，多个设备时请显式指定。备份脚本读取设备**当前**内容，刷入自定义固件后再备份不会得到出厂镜像。已有出厂备份请长期保留。

烧录脚本构建并写入 bootloader、分区表和应用，不写入文件系统镜像。应用启动后，脚本通过 USB 串口发送电脑当前时间，设备按现有校时事务写入 RTC 和存档，再回读确认。已校准设备保留原时区，首次校准采用电脑当前时区偏移；电脑时间须正确。普通重启沿用 RTC，不重复设置日期。仅使用 PlatformIO 或 esptool 写入固件不会执行这一步。

校时失败时脚本会说明固件已经写入，并以非零状态退出。测试模式、设备忙或存储故障会阻止自动校时。固件要求 `spiffs` 分区已有可挂载的 LittleFS；**当前没有首次格式化入口**。新设备若显示“存储未初始化”，图鉴仍可浏览，步行玩法不可用。请保留原 Flash，在确认备份和分区用途后另行初始化；不要用自动格式化绕过挂载错误。

## 硬件

目标设备为 ESP32-S3R8、16 MB Flash、8 MB PSRAM 的 M5Stack StopWatch C152，使用 CO5300 AMOLED 和 CST820B 触摸。物理圆屏 466×466，当前 M5GFX 驱动报告 468×468，应用留有圆形边缘余量。

- 面向屏幕时，左侧为 `BtnA`（GPIO2），右侧为 `BtnB`（GPIO1）。
- 双击电源键关机由 M5PM1 硬件执行。固件每次开机设置其 LDO 3.3V 电源保持，RTC 在关机期间继续走时，开机不需要重新校时；关机静态功耗待测。
- 手动进入下载模式：连接 USB，长按电源键约 2 秒，内部绿灯亮起后松开。
- v1.0 批次背面 `BAT` 贴纸标错，实际为 5V IN，不能接电池；v1.0.1 已修正。

## 文档与许可

历史固件版本曾在 C152 启动并重载存档；该观察不代表当前 schema6 的设备验收已完成。当前主机检查、设备观察，以及真实计步、实体交互、RTC 保持、断电恢复和功耗的待验项，见[验证说明](docs/verification.md)。

- [使用指南](docs/usage.md)：操作、捕捉、培养、日期与存档。
- [Usage guide (English)](docs/usage.en.md)：English version of the usage guide.
- [开发指南](docs/development.md)：环境、素材生成、测试与模块职责。
- [玩法与迁移契约](docs/design/walking-progression.md)：账户不变量、进化、旧档兼容。
- [验证说明](docs/verification.md)：自动检查范围、当前限制和设备待验项。

自写代码采用 [MIT](LICENSE) 许可证。宝可梦图像、名称、资料快照和第三方库不纳入本项目 MIT 授权；来源与各自声明见 [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)。本项目与 Nintendo、Creatures、GAME FREAK 或 The Pokémon Company 无关联，也不代表其认可。
