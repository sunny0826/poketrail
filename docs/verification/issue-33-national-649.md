# Issue #33 全国图鉴扩容验证（2026-10-30）

验证对象为基线 `c09d18d` 上的 `feat/issue-33-national-649` 工作树。本节只记录本轮结果；[验证范围](../verification.md)中 251 种版本的检查数量不能作为 649 种的验收证据。

## 主机检查

| 命令 | 已观察结果 |
| --- | --- |
| `rtk proxy mise run test-motion` | 退出 0；计步合成信号 23,629、日期 146,215、RTC 适配 40 项通过 |
| `rtk proxy mise run test-rtc` | 退出 0；真实 RX8130 驱动配合主机 I/O 替身，寄存器 676、生命周期 18,522、启动 19／776 项通过 |
| `rtk proxy mise run test-scenes` | 退出 0；三条路线的正常／暗色场景生成物一致，8 项测试通过 |

命令、时间和退出码见局部检查记录 `.pio/verification/issue33-independent-20261030/results.json`；对应日志保存在同一目录。另以 Git 中固定基线源码独立编译旧 codec，重现 schema1–5 的稀疏字节样本，完整输出与存档测试所用样本一致，见 `.pio/verification/issue33-legacy-independent/results.json`。这些样本覆盖历史布局和 CRC；完整迁移、失败重试及非空账户由集成存档回归覆盖。

PSRAM 分配回归先失败、后通过：解码临时 `Save` 改为只从 PSRAM 分配。修复前注入失败仍接受测试场景，见 `.pio/verification/issue33-memory/large-state-red/step-1.log`；修复后同一检查 30 项通过，见 `.pio/verification/issue33-memory/large-state-green/run.log`。

进化规则和遭遇目录均已生成到 649。`build_evolution_rules.py --check`、`build_pokedex.py --check`、`build_adventure_catalog.py --check` 通过。`test_adventure_catalog`、`test_progression_catalog` 和 `test_pokedex` 共 20 项通过。旧 122 条费用、124 个普通资格和 schema 1–4 路线掩码未改。新增 198 条进化边，普通资格共 294 个；37 个新传说／幻兽单独保留，不进入普通遭遇。649 个动画压缩后为 4,973,937 字节。固件解码输出与全部 53,091 个原 GIF 帧的像素和帧时长一致。最终 `build` 成功：RAM 23,036 字节（7.0%），Flash 9,512,653 字节（72.6%），仍使用原 12.5 MiB 应用分区。

主机验收已通过：遭遇目录与 320 条进化边、图鉴、649 个图标、动画逐帧解码、玩法、存档、计步、RTC、校时、场景、15 个渲染套件和启动局部栈检查。渲染截图确认图鉴、649 只伙伴末页和伊布七个进化分支都落在圆屏内。

## 设备启动

2026-10-30 实体设备先因为应用镜像误写入 `0x0` 而无法启动。恢复时引导程序写回 `0x0`、分区表写回 `0x8000`、应用写入 `0x10000`，哈希均已校验；SPIFFS（`0xC90000`）和 coredump（`0xFF0000`）未擦除。NVS 落在误擦范围内，未从备份恢复。

首次正确启动后，主循环因栈金丝雀触发重启。原因是 ROM 解压助手把约 11 KB 的解压状态放在 8 KB 的 Arduino 主任务栈上。修复后解压状态与动画缓存均从 PSRAM 分配。修复版固件写入 `0x10000` 并通过哈希校验；复位后连续观察 18 秒和 60 秒，没有 Guru Meditation、栈金丝雀或重启。串口显示屏幕 468×468、`displayerr=0`、存档已载入（`load=1`、`starter=1`、`partner=8`、`total=32574`），加速度计在采样，空闲后屏幕调光，电池 100% 且正在充电。

仍未验证屏幕画面、真实计步、按键交互、耗电和物理断电后的存档保持。
