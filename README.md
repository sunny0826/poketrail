# M5Stack StopWatch 宝可梦步行探险

给 M5Stack StopWatch（C152）的离线步行游戏：带着伙伴走路，遇见宝可梦，通过节奏对战和投球完成捕捉。保留第一世代 151 只自由图鉴与《黑／白》像素动画。

- 每 **5000 探索步**产生一次遭遇，每 **1000 总步**获得一球，新档零球。
- 一次携带一只伙伴，走路积累培养步，达到条件后手动进化。
- 每种宝可梦成功捕获后永久退出遭遇池；失败、撤退或放弃后仍可遇见。
- 默认首页显示伙伴、路线场景和今日步数；30 秒无操作后熄屏，继续计步和保存。
- LittleFS 双槽存档，保存失败可重试；保留旧版本存档迁移。

这是开发中的非官方爱好项目。固件已在 C152 启动并重载存档；真实计步误差、实体交互、RTC 保持、断电恢复和功耗仍需验证。已知限制见[验证说明](docs/verification.md)。

## 开始使用

存储可用后设置日期与时间，再选择妙蛙种子、小火龙或杰尼龟作为初始伙伴。也可以暂不校时：累计步数与探索照常增加，未知日期期间的步数不会补填到今日。

首页点按任一侧键进入菜单。普通菜单中，左键上移、右键下移，松开后生效；两键按住有重叠时确认。触摸可见按钮也能操作。

| 页面 | 操作 |
| --- | --- |
| 对战／练习 | 左键躲避，右键攻击；左右半屏触摸等价 |
| 暂停 | 触摸顶部，或双键按住 800 毫秒 |
| 投球 | 先准备并松手，再按右键或轻触投出 |
| 图鉴 | 左右键或横滑切换；点伙伴互动；点顶部返回 |
| 省电 | 任意键或触摸唤醒，全部松手后再操作 |

完整操作、培养、日期和保存规则见[使用指南](docs/usage.md)。

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

烧录脚本构建并写入 bootloader、分区表和应用，不写入文件系统。固件要求 `spiffs` 分区已有可挂载的 LittleFS；**当前没有首次格式化入口**。新设备若显示“存储未初始化”，图鉴仍可浏览，步行玩法不可用。请保留原 Flash，在确认备份和分区用途后另行初始化；不要用自动格式化绕过挂载错误。

## 硬件

目标设备为 ESP32-S3R8、16 MB Flash、8 MB PSRAM 的 M5Stack StopWatch C152，使用 CO5300 AMOLED 和 CST820B 触摸。物理圆屏 466×466，当前 M5GFX 驱动报告 468×468，应用留有圆形边缘余量。

- 面向屏幕时，左侧为 `BtnA`（GPIO2），右侧为 `BtnB`（GPIO1）。
- 手动进入下载模式：连接 USB，长按电源键约 2 秒，内部绿灯亮起后松开。
- v1.0 批次背面 `BAT` 贴纸标错，实际为 5V IN，不能接电池；v1.0.1 已修正。

## 文档与许可

- [使用指南](docs/usage.md)：操作、捕捉、培养、日期与存档。
- [开发指南](docs/development.md)：环境、素材生成、测试与模块职责。
- [玩法与迁移契约](docs/design/walking-progression.md)：账户不变量、进化、旧档兼容。
- [验证说明](docs/verification.md)：自动检查范围、当前限制和设备待验项。

自写代码采用 [MIT](LICENSE) 许可证。宝可梦图像、名称、资料快照和第三方库不纳入本项目 MIT 授权；来源与各自声明见 [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)。本项目与 Nintendo、Creatures、GAME FREAK 或 The Pokémon Company 无关联，也不代表其认可。
