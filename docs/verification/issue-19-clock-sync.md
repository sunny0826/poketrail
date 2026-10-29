# Issue #19 烧录后校时验证

修复、两次实体烧录、自动校时、重启回读和存档核对已完成。最终独立复核关闭全部已发现问题。验证主体及仍未覆盖的范围如下。本文中的 `.pio/`、`.agent-conductor/` 和 `backups/` 路径是本地证据索引，原始日志、设备备份与协作记录不随源码发布；检查命令见[开发指南](../development.md#检查入口)。

## 修复前状态

源码基线为 `main` 的 `f2eb9c3`。已连接设备为 ESP32-S3，串口 `/dev/cu.usbmodem1101`。2026-10-28 保存了新的 16 MB 完整 Flash 备份，原出厂备份未改动。

- 串口基线（本地 `.pio/verification/issue19-20261028/baseline-serial.log`）：RTC 错误计数为 0，存档序列号 9992，累计步数 13796，初始伙伴 1，携带伙伴 2。
- 存档解码（本地 `.pio/verification/issue19-20261028/before-save.log`）：序列号 9992 的日期为 `2026-01-01 17:22:01 +08:00`，已校准、可信、无异常、无待确认事务。该帧通过 CRC 和真实 codec 语义校验。原始 Flash 扫描同时包含旧数据块，不能仅凭最高序列号认定当前槽；这里采用与串口匹配的序列号。
- 账户基线（本地 `.pio/verification/issue19-20261028/before-account.log`）：保留累计步数、探索账户、伙伴、捕获和培养数据，用于校时后的比较。
- 独立源码审查（本地 `.agent-conductor/issue-19-20261028/investigate-report.md`）：未发现当前烧录或启动路径自动把 RTC 写为 1 月 1 日。烧录脚本没有主机时间同步步骤，格式合法的错误时间会继续使用。

完整备份为 `backups/flash-20261028-143830-72656.bin`，SHA256 为 `b54d741605532b4fff4fc64515a36a0f3b3a082deba6d1555e2cb6f7421de1c6`。备份和本轮 `.pio` 证据保留在本地，不随源码发布。

上述结果证明现场存在错误日期，未证明每一次烧录都重置 RTC，也未确认最初错误日期来自哪次输入或失时。

## 基线检查

以下检查由协调者在修改产品代码前执行：

| 检查 | 结果 |
| --- | --- |
| `rtk proxy mise run test-adventure` | 退出 0 |
| `rtk proxy mise run test-rtc` | 退出 0 |
| `rtk proxy mise exec -- python3 scripts/run_render_tests.py test_adventure_render test_test_mode_render` | 退出 0 |

日志在 `.pio/verification/issue19-20261028/baseline-*.log`。主机测试不证明设备 RTC 备电保持、真实断电恢复或屏幕观感。

## 实现与主机检查

新增 USB 校时协议和主机脚本，烧录成功后自动执行。Runtime 复用既有校时存档事务；过期主机请求在首次 RTC 写入前拒绝，设备错误页与串口重试沿用同一状态。回执丢失时查询必须关联真实事务号；只读查询还比较主机当前 UTC。串口短写保留剩余字节，未发送完的回执不会被下一条命令覆盖。

独立审查在第一版确认五项故障，协调者和实施角色分别修正固件与主机脚本。新增回归在修复前实际失败，修复后通过。完整审查及修复报告保留在 `.agent-conductor/issue-19-20261028/`。

| 检查 | 执行者与结果 |
| --- | --- |
| `test-adventure`、`test-motion`、`test-rtc`、`test-storage` | 独立 verifier，退出 0 |
| 校时协议 | 独立 verifier，44 checks 通过 |
| 修正后主机脚本 | 实施角色，22 tests 通过；协调者另跑同一测试 |
| 修正后 Runtime 校时 | 协调者，177 checks 通过 |
| 修正后 adventure / test-mode 页面回归 | 协调者，同轮退出 0 |
| 固件 USB 回执短写 / 零写 / 队列保护 | 协调者，从实际发送函数编译探针，退出 0 |
| 修正后固件构建 | 协调者，退出 0 |
| 启动局部栈 | 协调者，48 bytes，低于 512 bytes 限制 |
| 完整 `test-render` | 独立 verifier，退出 1，既有 `two peaks wait for confirmation` 断言失败；后半套件未在此完整命令中运行 |
| `code-harness --base HEAD` | 协调者，退出 2，仓库缺少 `.codeharness/code-harness.json`；未形成有效门禁结果 |

完整渲染失败的既有记录见 [Issue #9](issue-09-route-types.md)。本轮没有重写该计步用例。协调者已实际查看 verifier 的 Runtime/M5GFX 日期页截图，日期与年月日字段可见、圆屏内无明显文字裁切；这是主机截图，不是实体屏幕证据。

定向修复证据为 `.pio/verification/issue19-20261028/revision-runtime-red.log`、`revision-runtime-green-2.log`、`build-final.log` 和 `.pio/verification/issue19-host-fix/`。实体结果见下文。

最终复核另发现主机失败后的手动恢复被旧事务干扰，已在新手动事务启动前清除旧主机运行状态。新增真实按键回归先失败于手动保存未完成，修复后纳入 158 项 Runtime 检查。证据：`manual-recovery-red.log` 与 `final-clock-sync.log`。

同帧保存交叉检查另发现待重试事务可能被手动入口覆盖；`saveClock` 现在先检查 `savePending`，保留既有事务。新增同帧输入回归修复前失败、修复后通过；最终定向套件为协议 44、Python 22、Runtime 177，日志 `clock-sync-accepted.log`。

## 实体烧录与回读

协调者在用户再次明确要求“烧录”后，通过 `/dev/cu.usbmodem1101` 完成两次烧录，脚本都自动校时成功。第二次包含同帧保存保护，是设备最终使用版本；原文件系统未擦除或格式化。

- 第一次：设备回执 UTC `1793174606`，本地 `2026-10-28 16:03:26 +08:00`，`day=20261028 cal=1 tr=1 pend=0 anom=0 save=0`，日志 `flash-final.log`。
- 第二次：设备回执 UTC `1793174832`，本地 `2026-10-28 16:07:12 +08:00`，同样可信、无异常或待提交，序列号 10174，日志 `flash-accepted.log`。
- 第二次后独立只读三次回读：16:07:49、16:07:53、16:07:58，时钟持续推进，全部落在查询请求时间区间前后 3 秒内。最后一次读取结束时与电脑相差约 0.78 秒；事务号仍为 `1793174832`、序列号 10175，可信且无待提交。详见 `after-flash-clock.json`。

两个烧录日志均包含 esptool 写入数据哈希验证成功。最终镜像大小及 SHA256 记录于 `accepted-artifacts.json`。

## 重启、固件及存档核对

新备份 `backups/flash-20261028-160841-60194.bin` 为 16 MB，SHA256 `f6ec6a1af05291b329cf4dfdd1356968665c99b78c8bf54df07050176ac845ff`。原出厂备份及修复前备份均保留。读取后 esptool 硬复位设备，随后仅执行查询，未重新校时。

重启后三次时间为 2026-10-28 16:10:16、16:10:18、16:10:20（UTC+8），均可信且无待提交；最后读数与查询结束时电脑时间相差约 0.85 秒。事务号仍为 `1793174832`，说明这轮保持检查没有建立新的校时事务。日志 `after-reboot-clock.json`。

Flash 中 `0x10000` 起的应用字节与本地最终 `firmware.bin` 完全一致；固件 SHA256 为 `644272b86344dd035b4bfc3d1bc3331fb8756e24ebf3b48d7a0766fd256cca05`，证据 `flashed-image-check.json`。

从新 Flash 备份恢复 CRC 合法帧，再用仓库实际 codec 验证；选取与实体查询一致的序列号 10175。校时前相邻保留帧 10162、待提交 10163、与首次校时 ACK 匹配的 10164 构成日期变更证据，不仅按最高序列号猜测活动槽。

- 校时前后累计步数均为 13801，初始伙伴 1、携带伙伴 2。
- 探索账户、遭遇字段、拥有/登记位图、捕获数量和逐物种培养数据逐项相同。
- 旧日期 `20260101` 的 6196 步归入历史，今日 `20261028` 为 0 步，没有把旧步数搬到今天。
- 最早基线是 13796 步。扫描保留帧显示，校时之前的 10159/10160 已累计增加 5 步，对应探索及伙伴培养账户同步增加；本轮未观察这些计步的物理来源。初次与早期基线做完全相等检查因此失败，原 `account-comparison.json` 保留；改用紧邻校时前后的事务帧核对通过，证据 `account-comparison-verified.json`，没有隐藏差异或将其归因于校时。

## 独立复核与边界

Reviewer 的最终 N2 复核报告 `n2-review-report.md` 确认最后一处保存入口保护关闭问题、无新增发现。177 项 Runtime 检查、41 项独立恢复探针及同帧输入探针均通过。前轮发现全部关闭，协调者已读完整报告并核对设备证据。

本轮实体证明自动校时、重复烧录后的校时、硬复位后 RTC 保持、最终应用字节及存档数据。未执行实际断电/备电耗尽、跨午夜、实体故障注入或多设备串口测试，也未拍摄或目视实体屏幕；主机截图不替代这些验证。完整渲染套件的既有断言失败和 code-harness 缺配置仍如上记录，均未宣称通过。未提交或推送代码。
