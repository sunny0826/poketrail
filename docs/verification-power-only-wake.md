# Issue #34 仅电源键唤醒（2026-10-30）

需求为熄屏后仅短按电源键点亮屏幕，触摸、滑动和侧键不唤醒。`Runtime::updateDisplayPower` 仅接受电源键输入，保留唤醒等待、释放门、页面与草稿恢复。CPU 的短暂浅睡和采样机制不变。

验证基线为 `7ab0e87`，分支 `feat/issue-34-power-only-wake`。新增回归在旧规则下失败于 `touch cannot wake the locked screen`，修改后通过。电源套件 13,212 checks，电源页面套件 5,889 checks；覆盖三个熄屏入口、触摸／滑动／侧键不亮屏、同时按住其他输入时电源键唤醒，以及释放后的正常操作。

`test`、`build`、`check-startup-stack` 均通过。15 套渲染检查分批执行，13 套通过；`test_progression_render` 失败于 `two peaks wait for confirmation`，`test_evolution_render` 失败于 `two step peaks remain pending before threshold`。将唯一产品改动替换为基线版本后，两套件仍在相同断言失败，故不计为本轮回归，也不将完整 `test-render` 标记为通过。首页辅助测试的旧侧键唤醒步骤已迁移，复验 219,136 checks 通过。

固件已烧录至本次唯一检测到的 ESP32-S3 设备，写入哈希校验成功，重启后校时返回 `result=ok`、`ready=1`。固件 SHA256 为 `738794e4909ddd514da935e26290bf038b74f000a357cab5bb0079a7177a190c`。用户在实体设备确认：电源键、30 秒无操作、菜单“立即省电”三个熄屏入口，在 USB 与电池模式下均符合要求，且亮屏后触控正常。此结果来自用户观察，不是 Agent 远程触控或录像验收。

本轮日志保存在本地 `.pio/verification/issue34-power-only-20261030/`，包含先失败／后通过、两项基线对照、烧录与设备验收记录。未测量省电电流、唤醒时延或真实计步误差；本轮自行审阅 diff，未执行独立 Agent 审核。
