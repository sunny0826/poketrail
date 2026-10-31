# UI 实现与设计稿维护

本文供维护页面与场景的开发者查找实现和检查入口。视觉规格见[UI 设计](ui-scene-redesign.md)，页面对应关系见[页面清单](ui-scene-redesign-pages.md)，运动示例见[运动设计规格](ui-scene-redesign-motion.md)。

## 当前实现

`screen_theme.hpp` 定义主题，`screens_components.cpp` 提供公共组件，`screens*.cpp` 和 `viewer_screen.cpp` 绘制页面。`Page` 包含 28 个玩法页面与 7 个测试模式页面，共 35 页。普通列表由 `menu_layout.hpp` 统一为每页 3 项，输入和选择由 Runtime 管理，绘制不修改 Engine 或存档。

`tools/design_preview_scenes.py` 提供场景来源；`tools/build_scene_backgrounds.py` 生成普通、暗色与分层 RGB565 数据。`Screens::advanceScene()` 持有展示层进度：首页且屏幕唤醒时每 100ms 推进一次，走路不加速；路线切换归零，其他页面和熄屏期间暂停。绘制使用 456 像素行缓冲，远景和 UI 不动。遭遇、对战和投球使用已保存遭遇路线的静态暗色图。

双角色各自保留原始动画时长与完整像素，按尺寸使用 1 倍或 2 倍整数缩放。Engine 判定后立即更新 HP；胜负提交成功后，页面最多保留 450ms 的最后动作窗口。保存失败立即显示错误，重试同一结果。设计 GIF 的示例 HP 与受击帧不代表当前玩法，读招光环、假动作、狂暴和必杀规则以[使用指南](../usage.md)为准。

## 重建设计稿

先按[开发指南](../development.md)安装依赖。以下是 Pillow 设计预览的生成与检查入口，输出到 `docs/design/ui-scene-redesign/`。当前入口受下述素材解析问题阻塞；修复工具后运行会改写生成图、HTML 与清单，须检查差异，不手工编辑生成物：

```bash
rtk proxy mise exec -- python3 tools/build_ui_design_previews.py
rtk proxy mise exec -- python3 -m unittest test.test_ui_design_previews
```

画廊仍是原 26 页、77 帧和 6 条动画的历史视觉参考。`tools/design_preview.py` 的 `GeneratedAssets` 仍按 1～251 和旧 Sprite／帧字段布局解析数据，无法读取当前 649 种的 zlib 格式。2026-10-31 执行两个设计文档检查时均在 `setUpClass` 报 `generated sprite table must contain IDs 1-251`，尚未执行测试断言。

另一个静态可确认的缺口是页面覆盖：`test_every_firmware_page_is_covered` 比较全部 35 个 `Page`，但清单缺少三张进化／预览页和七张测试模式页，还保留已删除的 `ReleaseConfirm`；测试仍硬编码了 26 页。修复需要同步素材解析、生成器、页面清单和测试；只更新文档不能使该检查通过。

`test_design_document_links_and_frame_count` 直接读取 UI 主文档及 `pages`、`motion`、`implementation` 三个文件。这些入口与 HTML 画廊仍被维护流程使用。设计测试还检查路线、六个捕获关键帧、圆外像素、sprite 像素、联系表、50ms 战斗 GIF 和 100ms 步行动画时间槽；这些是设计产物检查，不能替代 Runtime 验收。

## 验证真实页面

运行 `rtk proxy mise run test-render` 生成真实 Runtime/M5GFX 主机截图。开发时可选单套定位：

```bash
rtk proxy mise exec -- python3 scripts/run_render_tests.py test_home_render
rtk proxy mise exec -- python3 scripts/run_render_tests.py test_battle_render
rtk proxy mise exec -- python3 scripts/run_render_tests.py test_evolution_render
rtk proxy mise exec -- python3 scripts/run_render_tests.py test_test_mode_render
```

首页回归检查默认移动、走路不加速、完整循环、菜单返回、路线切换与熄屏唤醒；交互回归从真实 Button_Class 或 SDL 输入进入 Runtime。修改 UI 后还须读取生成截图，核对圆形边缘、长名称、列表末页和返回入口。主机截图不证明实体屏幕观感、帧率、双角色解码耗时或计步精度。

每次任务交付前仍须运行全量 `rtk proxy mise run test-simulator-e2e`。各类检查的职责、已知失败与设备待验事项统一见[验证说明](../verification.md)。
