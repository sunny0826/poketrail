# Issue #22 双击关机时间保持验证

修复、主机检查、Reviewer 审查与设备关机验证已完成；跨日关机与关机功耗转后续 issue 跟踪。验证主体与仍未覆盖的范围如下。本地 `.pio/verification/issue22-20261029/` 的命令日志是证据索引，不随源码发布。

## 根因与修复

双击电源键关机由 M5PM1 硬件执行，默认关机序列切断给 RTC（RX8130CE）与 IMU 供电的 L1；RTC 失电后 VLF 置位，固件按契约把读数判为不可信，校时页保留默认值 `2026-01-01`，即用户所见「时间被重置」。存档本身未损坏：`calibrated` 保留，仅 `trusted=false`、今日统计停摆。

修复在 `c152::begin` 末尾对 PM1 `HOLD_CFG(0x07)` 做读-改-写置 LDO 3.3V 电源保持位（0x20）并回读确认，开机打印一行诊断（`pm1 ldo hold=`，失败时标注 read/write/verify 阶段）。关机后 L1 保持供电，RTC 继续走时。PM1 访问失败不重试、不阻塞启动，退化为旧行为。不改动双击关机使能、充电与 IMU 配置。

方案对比（固件接管关机、NVS 恢复时间、不修）与 M5PM1 库源码证据见本地 `.agent-conductor/issue-22-20261029/architect-report.md` 与 `faultfinder-report.md`，不随源码发布。

## 主机检查

基线为 `main` `94a29c2`（改动前），由协调者执行；实施后检查由实施会话与协调者分别运行。

| 检查 | 结果 |
| --- | --- |
| `test-rtc` | 通过：寄存器 676、生命周期 18522、board startup 19 轮 776 checks，0 failures（基线 488 checks） |
| `test-adventure` | 通过，与基线一致 |
| `test-motion` | 通过（`transport.hpp` 的另一消费方编译路径） |
| `build` | 通过；另做 clean 全量重建，ELF 含新诊断字符串 |
| `check-startup-stack` | 通过（`Engine::begin` 48 bytes < 512；不代表 `c152::begin` 栈帧） |
| `test-render`（三段法） | 与基线一致；`test_progression_render`、`test_evolution_render` 的两个失败为 Issue #9 遗留，非本轮引入 |

测试改动：RTC 启动替身与渲染替身补最小 PM1 接口；`Op` 记录 I2C 设备地址，board 阶段断言只访问 `0x6E:0x07` 且不触碰 RTC（0x32），随后清空轨迹保持既有 VLF/VBLF 注入语义；新增 PM1 读、写、回读三档失败注入，断言启动继续、写入至多一次、RX8130 初始化不受影响。

独立审查（Reviewer，快照为工作树差异）：1 项低严重度发现（失败诊断可能打印旧值），已修复为按阶段输出并复验；结论无阻塞。

## 设备验证（2026-10-29，C152，`/dev/cu.usbmodem1101`）

1. 烧录候选固件，`flash.sh` 自动校时：设备 UTC `1793239082`（本地 09:58:02），`cal=1 tr=1`，事务号 `1793239082`。
2. 断开 USB，双击电源键关机，电池放置约 62 分钟（跨 10:00→11:00 整点）。
3. 插回 USB 开机，`sync_clock.py --query`：设备 UTC `1793243522` 与主机一致（判定正常），`cal=1 tr=1 pend=0 anom=0 day=20261029`，**事务号仍为 `1793239082`**——无新校时事务，时间连续性来自 RTC 自身跨关机走时。
4. 关机后 IMU 正常：串口 `sample` 计数推进、`noaccel=0`、`rtc=0` 无错误；存档状态 `total=18834`、partner、序列号正常推进。

证据：`device-flash.log`、`device-query-after-off.log`、`device-postoff-serial.log`。开机诊断行 `pm1 ldo hold=` 因 USB 枚举竞争未直接捕获；时间连续且无新校时事务已间接证明 hold 生效（固件仅开机一条路径设置该位）。

## 未验证事项

- 跨日关机保持：过夜双击关机后日期翻日、今日步数开始新一天（转后续 issue）。
- 关机功耗：hold 使 L1（RTC+IMU）在关机期间持续供电，静态电流与续航代价未测量（转后续 issue）。
- 真实失时（VLF、备电耗尽、RTC I/O 错误）的设备表现未注入；主机测试覆盖无效读数拒绝路径。
- `c152::begin` 栈帧与完整任务栈未单独测量；实体屏幕观感未目视核对。
