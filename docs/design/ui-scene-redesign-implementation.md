# 全场景 UI 固件实现与验收

返回[视觉设计与页面覆盖](ui-scene-redesign.md)。

## 生成与检查

以下命令用于重建和检查历史设计稿。当前固件已有 29 个 `Page`，设计清单与生成器仍只有 26 页，`test_every_firmware_page_is_covered` 会因缺少 `Evolution`、`EvolutionPreviewTargets`、`EvolutionPreview` 失败；测试还硬编码了 26 页。更新覆盖范围须修改生成器和测试后重新生成，不能手工补改生成画廊。本轮仅更新文档并记录该缺口。

```bash
rtk proxy mise exec -- python3 tools/build_ui_design_previews.py
rtk proxy mise exec -- python3 -m unittest test.test_ui_design_previews
```

除页面集合外，设计稿测试还检查三条路线、六个捕获关键帧、PNG 尺寸、圆外像素、真实 sprite 像素、联系表和画廊引用。三条战斗 GIF 检查 50ms 帧时长、循环、关键位移与 HP。三条步行 GIF 按 100ms 时间槽检查连续默认位移、固定 UI 和循环接缝。场景测试另检查 228×135 静止合成与分层循环的一致性。重复生成应得到相同文件内容；本轮未重跑这些图像检查。

设计图保留为视觉规格。实际实现使用 `screen_theme.hpp`、`screens_components.cpp` 和 `screens*.cpp`，`viewer_screen.cpp` 同步样式。验证入口和实机边界见[验证说明](../verification.md)。

## 默认纵深动画实现（2026-10-24）

`tools/design_preview_scenes.py` 提供分层循环来源，`tools/design_preview_walk.py` 生成三路线默认动图和关键帧。`tools/build_scene_backgrounds.py` 另生成固件远景、中层和近层 RGB565 数组及透明键；原普通／暗色合成图继续服务静止画面、缩略图和战斗。

`Screens::advanceScene()` 持有展示层视觉步数。Runtime 在首页且屏幕唤醒时每 100ms 推进一次，不要求走路输入，走路也不加速；路线切换归零，其他页面和熄屏期间暂停。`screens_components.cpp` 仍使用 456 像素行缓冲，按地面带高度合成向下循环的中层和近层，远景与 UI 不动。绘制不修改 Engine 或存档，暗色场景保留原静态路径。

`test_home_render.cpp` 已核对无输入默认移动、走路不加速、停止走路后继续、完整循环、菜单返回、路线切换和熄屏唤醒，并逐像素检查远景与两层地面。当轮首页截图与十套渲染输出已清理；可用 `rtk proxy mise run test-render` 生成当前版本的截图，单独检查首页可运行 `rtk proxy mise exec -- python3 scripts/run_render_tests.py test_home_render`。当轮玩法、场景、设计预览和固件构建均通过；当轮版本已烧录并确认首页运行，实机观感、真实计步、绘制耗时和功耗仍需 C152 验证。

<a id="固件实现2026-10-24"></a>

## 当前固件实现

三条精细场景、步行纵深、29 个页面和双角色战斗动作已接入真实 Runtime。普通列表由 `menu_layout.hpp` 统一为每页 3 项；新增的真实进化、预览目标选择和预览播放由 `screens.cpp` 分发。长名称、最大步数、末页返回、首捕、未知日期、坏档及保存失败有主机输入回归。遭遇和投球背景跟随已保存的遭遇路线。

固件与设计 GIF 的区别如下：

- GIF 在示例输入后的受击帧切换 HP；固件直接显示 Engine 已结算的 HP，不等待受击动画。
- 动作按实际判定时刻计算，在 450ms 内结束。胜负立即提交；提交成功后，最后一击显示到该窗口结束才切页。保存失败立即显示错误，重试使用原结果。
- 该轮未改判定与伤害规则。之后 issue #21 引入读招光环、假动作、狂暴（±140/±55ms、1.1 秒节拍）、必杀与 5 格敌方生命，规则见[使用指南](../usage.md)。跨拍恢复、暂停、计时回绕和按住胜利键不误触投球均有测试。
- 双角色保留各自原动画时长与完整像素，按尺寸使用 1 倍或 2 倍；实现没有加入插值或修改原素材。

2026-10-24 的角色接地修正轮通过了九套真实渲染回归及固件构建，当轮 156 张主机原始截图与原像素 PNG 画廊已清理。这些历史检查不覆盖后来新增的页面；可用 `rtk proxy mise run test-render` 生成当前版本的新截图，原画廊不会随任务恢复。输出位置与证据边界见[历史证据清理与重新检查](../verification.md#历史证据清理与重新检查)。该轮结束时尚未烧录，后续设备观察见[历史验证轮次](../verification/history.md#历史设备观察)；圆屏观感、双角色解码时延、采样间隔与堆峰值仍需 C152 验证。
