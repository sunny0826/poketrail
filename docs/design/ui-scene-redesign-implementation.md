# UI 实现与设计稿维护

本文供维护页面与场景的开发者查找实现和检查入口。视觉规格见[UI 设计](ui-scene-redesign.md)，页面对应关系见[页面清单](ui-scene-redesign-pages.md)，运动示例见[运动设计规格](ui-scene-redesign-motion.md)。

## 当前实现

`screen_theme.hpp` 定义主题，`screens_components.cpp` 提供公共组件，`screens*.cpp` 和 `viewer_screen.cpp` 绘制页面。`Page` 包含 29 个玩法页面、7 个测试模式页面和1个语言页面，共 37 页。普通列表由 `menu_layout.hpp` 统一为每页 3 项，输入和选择由 Runtime 管理，绘制不修改 Engine 或存档。

`tools/design_preview_scenes.py` 汇总关都、城都、丰缘、神奥、合众五个地区的场景来源（共用绘制工具在 `tools/scene_paint.py`，各地区分别在 `tools/scene_kanto.py`、`scene_johto.py`、`scene_hoenn.py`、`scene_sinnoh.py`、`scene_unova.py`）；`tools/build_scene_backgrounds.py` 按固件地区顺序生成普通、暗色与分层 RGB565 数据。`Screens::advanceScene()` 持有展示层进度：首页且屏幕唤醒时每 100ms 推进一次，走路不加速；地区切换归零，其他页面和熄屏期间暂停。绘制使用 456 像素行缓冲，远景和 UI 不动。遭遇、对战和投球使用已保存遭遇地区的静态暗色图，捕获练习使用对手所属地区。“选择地区”菜单两页列出五个地区，选中项的强调色取 `theme::routeColor`，标题下方显示选中地区（选到“返回”时为当前地区）的场景缩略图。

双角色各自保留原始动画时长与完整像素，按尺寸使用 1 倍或 2 倍整数缩放。Engine 判定后立即更新 HP；胜负提交成功后，页面最多保留 450ms 的最后动作窗口。保存失败立即显示错误，重试同一结果。设计 GIF 的示例 HP 与受击帧不代表当前玩法，读招提示圈、假动作、狂暴和必杀规则以[对战与投球](../usage-battle.md)为准。

## 重建设计稿

先按[开发指南](../development.md)安装依赖。Pillow 设计预览输出到 `docs/design/ui-scene-redesign/`，只作为历史视觉参考；当前生成入口仍受下述素材解析问题阻塞，不能作为发布或 Runtime 验收入口：

```bash
rtk proxy mise exec -- python3 tools/build_ui_design_previews.py
```

画廊仍是原 26 页、77 帧和 6 条动画的历史视觉参考。`tools/design_preview.py` 的 `GeneratedAssets` 按 1～251 和旧 Sprite／帧字段布局解析数据，无法读取当前 649 种的 zlib 格式。五地区改版后 `design_preview_scenes.py` 只提供 `kanto`～`unova`，而 `design_preview_screens.py`、`design_preview_walk.py`、`design_preview_battle.py` 仍请求 `forest`、`coast`、`cave`。清单也缺少后续新增页面，并保留已删除的 `ReleaseConfirm`。

2026-11-04 按[测试保留标准](../verification/test-policy.md)删除设计稿测试：页面数、文档内容、固定坐标及示例动画不能证明实际产品行为。历史图片、画廊和原始验收证据继续保留；删除测试不表示旧生成器的问题已修复。维护画廊时需同步素材解析、地区名和页面清单，再检查生成差异，不手工修改生成物。

## 验证真实页面

`test-render` 当前保留 12 套真实 M5GFX 检查入口，其中使用 Runtime 的套件按实际场景输出主机截图。开发时选择受影响的渲染或 SDL 场景：

```bash
rtk proxy mise exec -- python3 scripts/run_render_tests.py test_home_render
rtk proxy mise exec -- python3 scripts/run_render_tests.py test_capture_render
rtk proxy mise exec -- python3 scripts/run_render_tests.py test_test_mode_render
rtk proxy mise run test-simulator-e2e -- --case battle-cues --case battle-fixed
rtk proxy mise run test-simulator-e2e -- --case progression --case isolation --case edges
rtk proxy mise run test-simulator-e2e -- --case regions --case region-exhausted
```

首页渲染检查日期可信状态和极大计数的可读性；地区切换、捕获记录与耗尽提示由 SDL 场景检查。睡眠期间计步与失败恢复由 `test_power_pages_render` 保留。独立战斗／进化／预览渲染套件和场景视差像素公式检查已删除；历史截图继续保留，不能当作当前入口输出。

交互检查从真实 Button_Class 或 SDL 输入进入 Runtime。修改 UI 后还须读取对应截图，核对圆形边缘、长名称、列表末页和返回入口；每个套件只对自己的行为断言负责，不能以截图存在代表视觉通过。主机截图不证明实体屏幕观感、帧率、双角色解码耗时或计步精度。

按影响范围运行定向 E2E；只有命中全量触发条件才运行 `rtk proxy mise run test-simulator-e2e`。触发边界、各类检查的职责、已知失败与设备待验事项统一见[验证说明](../verification.md)。
