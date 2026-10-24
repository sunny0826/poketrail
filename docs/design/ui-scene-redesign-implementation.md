# 全场景 UI 固件实现与验收

返回[视觉设计与页面覆盖](ui-scene-redesign.md)。

## 生成与检查

```bash
rtk proxy mise exec -- python3 tools/build_ui_design_previews.py
rtk proxy mise exec -- python3 -m unittest test.test_ui_design_previews
```

检查覆盖 26 个 `Page`、三条路线、六个捕获关键帧、PNG 尺寸、圆外像素、真实 sprite 像素、联系表和画廊引用。三条战斗 GIF 检查 50ms 帧时长、循环、关键位移与 HP。三条步行 GIF 按 100ms 时间槽检查步事件范围、停止段背景、固定 UI 和循环接缝。场景测试另检查 228×135 静止合成与分层循环的一致性。重复生成应得到相同文件内容。

设计图保留为视觉规格。实际实现使用 `screen_theme.hpp`、`screens_components.cpp` 和 `screens*.cpp`，`viewer_screen.cpp` 同步样式。验证入口和实机边界见[验证说明](../verification.md)。

## 步行纵深实现（2026-10-24）

`tools/design_preview_scenes.py` 提供分层循环来源，`tools/design_preview_walk.py` 生成三路线步行动图和关键帧。`tools/build_scene_backgrounds.py` 另生成固件远景、中层和近层 RGB565 数组及透明键；原普通／暗色合成图继续服务静止画面、缩略图和战斗。

`Screens::advanceScene()` 持有展示层视觉步数。Runtime 只在首页且屏幕唤醒时传入本批已消费步数；路线切换归零，其他页面和熄屏期间不推进。`screens_components.cpp` 仍使用 456 像素行缓冲，按地面带高度合成向下循环的中层和近层，远景与 UI 不动。绘制不修改 Engine 或存档，暗色场景保留原静态路径。

`test_home_render.cpp` 已通过真实 IMU 合成输入核对三路线移动、停止、完整循环、菜单返回、路线切换和熄屏唤醒，并逐像素检查远景与两层地面。原始截图位于 `.pio/verification/depth-runtime-20261024T121229Z/ui/test_home_render/`，完整十套渲染证据位于 `.pio/verification/depth-final-20261024T122011Z/ui/`。玩法、场景、设计预览和固件构建均通过；实机观感、真实计步、绘制耗时和功耗仍需 C152 验证。

## 固件实现（2026-10-24）

三条精细场景、步行纵深、26 个页面样式和双角色战斗动作已接入真实 Runtime。列表仍每页四项；长名称、最大步数、末页返回、首捕、未知日期、坏档及保存失败使用主机输入回归核对。遭遇和投球背景跟随已保存的遭遇路线。

固件与设计 GIF 的区别如下：

- GIF 在示例输入后的受击帧切换 HP；固件直接显示 Engine 已结算的 HP，不等待受击动画。
- 动作按实际判定时刻计算，在 450ms 内结束。胜负立即提交；提交成功后，最后一击显示到该窗口结束才切页。保存失败立即显示错误，重试使用原结果。
- 下一拍、±70ms 完美判定、±180ms 普通判定和伤害规则不变。跨拍恢复、暂停、计时回绕和按住胜利键不误触投球均有测试。
- 双角色保留各自原动画时长与完整像素，按尺寸使用 1 倍或 2 倍；实现没有加入插值或修改原素材。

九套真实渲染回归及固件构建已通过，156 张主机原始截图保存在 `.pio/verification/lowered-contact-release-20261024/ui/`，原像素 PNG 画廊位于 `.pio/verification/lowered-contact-release-20261024/gallery/index.html`。这些本地证据不随源码公开；可用 `test-render` 重新生成。尚未烧录这一版，圆屏观感、双角色解码时延、采样间隔与堆峰值仍需 C152 验证。
