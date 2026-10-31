# 生成数据与动画

环境与检查入口见[开发指南](development.md)。`*_generated.hpp/.cpp` 随源码发布，必须通过生成器更新。不要手工改生成物。

| 数据 | 更新入口 |
| --- | --- |
| 动画 | `rtk proxy mise run sprites` |
| 缩略图 | `rtk proxy mise run icons`，初次用 `icons-download` |
| 中文资料 | `rtk proxy mise run pokedex` |
| 路线场景 | `rtk proxy mise run scenes` |
| 进化规则 | `rtk proxy mise exec -- python3 tools/build_evolution_rules.py` |
| 遭遇目录 | `rtk proxy mise exec -- python3 tools/build_adventure_catalog.py` |

## 遭遇目录

遭遇目录入口为 [adventure-649.json](../assets/adventure-649.json)，`routeTypes` 声明属性到路线的归属，`speciesFiles` 列出 [adventure-routes](../assets/adventure-routes) 内的物种配置。物种文件只保留编号、名称、稀有度和权重；生成器读取 Gen1–Gen5 图鉴的 `pokemon_type_name`，按位或双属性得到路线掩码，649 种都必须非空。`ordinaryRoots` 显式声明普通资格，`reserved` 记录活动保留物种；重复或缺失编号、手写 `routes`、未归属物种和映射缺口都会报错。原 [adventure-251.json](../assets/adventure-251.json)、旧物种配置与 schema1–4 的 `route_masks_v4_generated.hpp` 保持冻结；新增宝宝父关系不取消旧 124 种普通资格。生成头文件不得手改。

## 素材与缩放

动画缓存位于 `assets/sprites/black-white`，缩略图位于 `assets/sprites/icons`，均不纳入 Git。来源、版本和权利声明见[第三方声明](../THIRD_PARTY_NOTICES.md)。动画和缩略图清单按世代拆分为 Gen1–Gen5 五份，固定来源均为 PokeAPI sprites 提交 `97a7832d307311c0e1c6ea9035308bf80cc94f81`。指定子集必须使用独立输出路径，防止覆盖完整图鉴：

```bash
rtk proxy mise exec -- python3 tools/build_sprites.py --ids 1,4,7 --out /tmp/sprites_generated.hpp
```

1～649 共 649 只动画、53,091 个时序帧，保留原画布与时长。RGB565 调色板和 zlib 压缩的索引数据存于 Flash，相同帧共享数据；生成器支持最多 256 色，当前素材最多 24 色。解码器用两个槽缓存素材的全部去重帧索引，再将所需帧展开为整数倍像素。同屏双方可复用各自的索引；第三个来源替换较久未用的槽，失败的解压不会作为有效缓存使用。ESP32 的解压状态和索引缓存使用 PSRAM，各槽复用已分配容量；当前素材单份最大 1,245,870 字节，两槽保留容量最多 2,491,740 字节，不含解压状态、像素缓冲和其他运行时内存。透明键为 `0xF81F`，黑白像素保留，未知图腾只收录 #201 默认形态。当前最大宽度为 153、最大高度为 117（来自不同物种），源画布上限为 160。数量可从生成表复算，逐帧一致性由 `test-sprites` 检查。

整数缩放类别由 [sprite-scales.json](../assets/sprite-scales.json) 按 Gen1–Gen5 显式列出。旧世代的类别保持冻结；生成器不从当前进化父关系推导缩放，避免新增宝宝关系改变已有画面。基础形态使用 2 倍整数缩放，中间和最终形态与目录内无进化链物种优先 3 倍；超出 336×270 或页面高度限制时回退 2 倍。页面再按可用高度限制：首页避免 3 倍角色占用步数区，Viewer 为属性栏留出空间，遭遇和结果页使用 1 倍或 2 倍。双角色战斗中，原图不超过 68×65 的角色用 2 倍，其他用 1 倍。所有页面保留完整原画布和透明像素，不插值。

## 场景、资料与播放

三路线场景通过 `tools/design_preview_scenes.py` 生成，清单位于 `assets/scene-backgrounds.json`。普通与暗色合成图，以及首页远景／中层／近层均为 228×135 RGB565，绘制时逐行放大 2 倍；场景数组存于 Flash，只需 456 像素的行缓冲。首页由 `Screens::advanceScene()` 持有视觉步数，屏幕唤醒且首页可见时每 100ms 默认推进一次，地面向下循环，路线切换归零；走路不加速，其他页面和熄屏期间暂停。该进度不进入 Engine 或存档。遭遇、战斗和投球使用已保存遭遇路线的静态暗色图，选择新路线不改变当前遭遇背景。

第二至第五世代中文名、属性、身高和体重来自 PokeAPI 提交 `f38eb24b70f8462976154f534e79bf2bdd7af847` 的四份 CSV，保存在 [pokedex-source](../assets/pokedex-source)，哈希和字节数见同目录 `manifest.json`；[build_pokedex.py](../tools/build_pokedex.py) 只读本地缓存生成各世代图鉴 JSON，保留既有 Gen1／Gen2 快照，普通构建不访问网络。

动画按原始帧时长推进，点击和战斗动作不会重启动画。双角色各自持有播放位置，共用解码缓冲；白闪和残影只改变本帧颜色与位置。Engine 与 HP 在判定后立即更新，结束战斗时页面保留最多 450ms 动作窗口，失败保存立即转到错误页。判定窗口和下一拍时间不变。进化关系使用编号 1～649，缩放不代表实际身高比例。
