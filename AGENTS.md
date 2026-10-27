# 项目协作约定

PokeTrail（宝可旅途）是面向 M5Stack StopWatch（C152）的离线宝可梦步行探险固件，使用 ESP32-S3、Arduino、M5Unified 和 M5GFX。

先阅读 [README](README.md) 了解操作和开发入口。涉及培养、进化或存档迁移时，阅读[玩法契约](docs/design/walking-progression.md)与[验证说明](docs/verification.md)。

## 开发环境

- 使用 [mise.toml](mise.toml) 管理运行时、开发工具和任务。执行前检查项目及上级配置，核对实际工具路径和版本。
- 所有 shell 命令使用 `rtk` 前缀；开发命令通过 `rtk proxy mise exec -- <command>` 或 `rtk proxy mise run <task>` 执行。
- Python 依赖遵循 [requirements-dev.txt](requirements-dev.txt)；ESP32 工具链与固件库遵循 [platformio.ini](platformio.ini)。不要另建全局运行时或包管理入口。
- 主机测试使用系统平台编译器；渲染测试还需要 SDL2 原生库和 `sdl2-config`。缺少依赖时说明限制，不把未运行的检查记为通过。

## 代码归属

- [firmware/apps/20_pokedex](firmware/apps/20_pokedex)：玩法、存档、输入和页面。
- [firmware/shared](firmware/shared)：硬件初始化与板级功能。
- [tools](tools)：素材和玩法目录生成器；[assets](assets)：固定来源、清单与缓存。
- [test](test) 与 [scripts](scripts)：主机回归、渲染检查、构建及设备工具。

`Engine` 负责玩法状态变更，`Runtime` 负责采样、输入、页面和保存调度。页面从状态绘制，不在绘制函数中修改账户或提交存档。保持可在主机测试的逻辑与设备适配分离。

## 实现约束

- 按现有模块完成最小改动，避免无关重构。手写代码文件尽量不超过 500 行，生成数据不受此限制。
- 不手工修改 `*_generated.hpp` 或 `*_generated.cpp`。修改对应来源或生成器，再生成并验证；保留素材来源和哈希校验。
- 当前规则为每场随机消耗 200–500 探索步产生遭遇，精灵球不限量、每场最多投两球（ruleVersion 3，存档 schema4）。旧档升级后仍保留 `ruleVersion < 3` 的在途场继续按旧规则检查和扣除球库存。每种宝可梦拥有永久培养账户，只有携带者获得培养步。
- 进化扣除来源培养步并替换原形态；剩余步数留在来源账户，目标使用自己的账户。目标已拥有时拒绝整笔操作。修改这些规则须同步契约、迁移和行为验证。
- 保留存档版本、CRC 与语义校验。保存失败后重试同一次结果；提交成功前不显示奖励。挂载失败不得自动格式化或覆盖已有存档。

## UI 与交互

- Avoid unnecessary secondary labels and descriptions. Let the layout do the explaining.
- 优先用位置、间距、对齐和选中状态表达层级；只保留用户作决定或完成操作所需的文字。保留必要的按键、错误和不可逆操作提示，并与实际行为一致。
- 按圆屏检查内容边缘、长名称、列表末页和返回入口。物理屏幕为 466×466，当前驱动报告 468×468；不要只按方形画布判断布局可用。
- 保持像素素材的整数倍缩放、原始帧时长和透明规则，避免插值模糊或动画重启。
- UI 改动使用真实 Runtime/M5GFX 渲染截图并读图核对。交互改动验证实际输入路径；报告中区分主机截图与实体设备验证，不用重绘界面代替证据。

## 验证

按改动范围选择已有任务；完整说明见[开发指南](docs/development.md#检查入口)，先构建安装锁定依赖；素材测试另需下载缓存。

| 改动范围 | 命令 |
| --- | --- |
| 图鉴时序、手势与解码 | `rtk proxy mise run test` |
| 玩法、培养、进化、编码与重试 | `rtk proxy mise run test-adventure` |
| 计步与日期 | `rtk proxy mise run test-motion` |
| RTC 驱动与校时恢复 | `rtk proxy mise run test-rtc` |
| 双槽存档与文件系统 | `rtk proxy mise run test-storage` |
| 遭遇目录与进化规则 | `rtk proxy mise run check-adventure-catalog` |
| 动画素材 | `rtk proxy mise run test-sprites` |
| 路线场景与生成物 | `rtk proxy mise run test-scenes` |
| 页面与绘制 | `rtk proxy mise run test-render` |
| 固件构建 | `rtk proxy mise run build` |
| 构建后的启动局部栈帧 | `rtk proxy mise run check-startup-stack` |

固件代码改动完成后运行相关主机检查和固件构建。只改文档时核对命令、链接和事实，不必重建固件。已有检查能覆盖时优先复用；新增测试验证用户可观察的行为。

保留已有验收证据，新一轮输出使用独立目录。最终说明改动、实际运行的检查和未验证事项；主机测试不能证明真实计步、屏幕观感、设备时延或断电恢复。

## 设备与 Git

- 烧录、擦除或初始化文件系统前，确认已有明确授权和目标设备。保留现有出厂备份，不覆盖旧备份或验收产物。
- 开始编辑前查看 Git 状态，保护用户已有的暂存和未暂存改动。
- 仅在用户要求时提交；暂存使用本任务文件的显式路径。不要使用 `git add .`、`git add -A`、`git stash`、强制重置、破坏性清理或强制推送。
- 不在代码、日志或提交中写入 secret、token 或 credential。
