# 第三方内容与许可

[MIT 许可证](LICENSE)适用于本项目自写代码和文档。以下内容保留原有权利与许可，不因转换为 C++ 数组或打包进固件而变成本项目的 MIT 内容。

## 宝可梦图像与名称

动画、缩略图和精灵球图标来自 [PokeAPI/sprites](https://github.com/PokeAPI/sprites/tree/97a7832d307311c0e1c6ea9035308bf80cc94f81)，固定提交 `97a7832d307311c0e1c6ea9035308bf80cc94f81`。编号 1～251 的动画与缩略图分别记录在 Gen1／Gen2 清单中。

- 动画：`sprites/pokemon/versions/generation-v/black-white/animated/{id}.gif`，Gen1 记录在 [animations-gen1.json](assets/animations-gen1.json)，Gen2 记录在 [animations-gen2.json](assets/animations-gen2.json)。
- 缩略图：`sprites/pokemon/versions/generation-vii/icons/{id}.png`，Gen1 记录在 [icons-gen1.json](assets/icons-gen1.json)，Gen2 记录在 [icons-gen2.json](assets/icons-gen2.json)。
- 精灵球：`sprites/items/poke-ball.png`，与 Gen1 缩略图一同记录在 [icons-gen1.json](assets/icons-gen1.json)。
- 对应生成物：`firmware/apps/20_pokedex/sprites_generated.hpp/.cpp` 和 `icons_generated.hpp/.cpp`。

上游[原始许可声明](docs/licenses/pokeapi-sprites-LICENCE.txt)同时写明：

> All image contents within are Copyright The Pokémon Company.
>
> This repository is distributed under CC0 1.0 Universal

保留完整声明不代表已取得图像权利人的额外授权。CC0 也不授予商标权；本项目不能把宝可梦图像与名称重新授权为 MIT。Pokémon 及相关角色名称是各自权利人的商标。本项目为非官方爱好项目，与 Nintendo、Creatures、GAME FREAK 或 The Pokémon Company 无关联。

## 进化资料

[assets/evolution-source](assets/evolution-source) 内三份 CSV 来自 [PokeAPI/pokeapi](https://github.com/PokeAPI/pokeapi/tree/f38eb24b70f8462976154f534e79bf2bdd7af847/data/v2/csv)，固定提交 `f38eb24b70f8462976154f534e79bf2bdd7af847`，版本和 SHA256 见[来源清单](assets/evolution-source/manifest.json)。

上游采用 BSD 三条款形式的许可，版权为 Paul Hallett 与 PokéAPI contributors。原文保存在 [pokeapi-LICENSE.md](docs/licenses/pokeapi-LICENSE.md)，适用于保留的来源及其派生进化目录；分发固件时也须保留该声明。游戏中等级差 ×500 步、道具／交换 10000 步是本项目玩法规则，并非官方游戏参数。

## 中文资料快照

[pokedex-gen1.json](assets/pokedex-gen1.json) 保存 151 种中文名称、属性、身高和体重，来源标注为 [dex.pokemon.cn 图鉴](https://dex.pokemon.cn/play/pokedex)，从早期生成数据保留。该快照没有随附明确的再分发许可证，本项目不对其主张 MIT 授权，也不声称已核实权利人授权。

[pokedex-gen2.json](assets/pokedex-gen2.json) 保存第二世代 152～251 的中文名称、属性、身高和体重，由 [build_pokedex.py](tools/build_pokedex.py) 从 [pokedex-source](assets/pokedex-source) 内的四份 PokeAPI CSV 离线生成。CSV 固定提交 `f38eb24b70f8462976154f534e79bf2bdd7af847`，版本、URL、字节数和 SHA256 见同目录 `manifest.json`；上游许可与进化资料相同，见 [pokeapi-LICENSE.md](docs/licenses/pokeapi-LICENSE.md)。生成器同时核对 Gen1 快照与同一来源一致。纯主机测试验证资料与来源，不改变第一世代既有再分发说明。

[assets/sprite-scales.json](assets/sprite-scales.json) 是本项目手写的整数缩放类别表，不是上游素材。

## 固件依赖

以下依赖由 PlatformIO 获取，其源码不随本仓库复制。构建、修改或分发含这些依赖的固件时，应保留对应许可；版本由 [platformio.ini](platformio.ini) 固定。

| 依赖 | 上游 |
| --- | --- |
| Arduino-ESP32 / ESP-IDF | [espressif/arduino-esp32](https://github.com/espressif/arduino-esp32)、[espressif/esp-idf](https://github.com/espressif/esp-idf) |
| M5Unified | [m5stack/M5Unified](https://github.com/m5stack/M5Unified) |
| M5GFX（含字体与其他第三方组件） | [m5stack/M5GFX](https://github.com/m5stack/M5GFX) |
| M5PM1 | [m5stack/M5PM1](https://github.com/m5stack/M5PM1) |
| M5IOE1 | [m5stack/M5IOE1](https://github.com/m5stack/M5IOE1) |

主机渲染使用 [SDL2](https://www.libsdl.org/)，素材生成使用 [Pillow](https://python-pillow.github.io/)，设备工具使用 [esptool](https://github.com/espressif/esptool)。这些开发依赖不被本项目重新许可。发布预编译固件前，应另行汇总实际链接的库、字体和 SDK 的二进制分发要求。
