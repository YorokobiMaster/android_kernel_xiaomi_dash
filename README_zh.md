# android_kernel_xiaomi_dash

[English](README.md)|简体中文

- Linux 内核入口说明：[README](README)
- How do I submit patches to Android Common Kernels：[ACK_PATCH_SUBMISSION](README_ACK_PATCH_SUBMISSION.md)

这是 Redmi Turbo 5 Max（代号 `dash`）的非官方内核仓库，用于个人 ~~LineageOS 23.2 设备适配工程~~ ~~尝尝发哥答辩咸淡~~ 美美把玩。

先声明，目前已补充已知的、启动与日常运行所依赖的缺失模块源码，但仍可能有潜在的严重程度不同的问题。并且，这里缺少部分 HyperOS 的必要模块，请勿将其直接应用于 HyperOS 上。

~~如果你想要这些缺少的模块，Do It Yourself，最好再 Pull Request 回来（？）。~~

由于牵扯的版权比较多，我已尽可能的尝试整理，不能排除出现版权头、许可证不匹配，以及我冒领版权等情况。

如果你发现某些代码的版权并不属于我，请及时联系 `root@sandai.me` 或直接开 Issues，我看到后会及时处理，非常感谢。建议发邮箱，以防某天我突然消失不见。

- 设备名: Redmi Turbo 5 Max
- 代号: `dash`
- SoC: MediaTek Dimensity 9500s / mt6991
- 当前版本：6.6.142

## 补充模块

这些模块均由 Codex: gpt-5.6-sol / ChatGPT: gpt-5.6-pro 反复对 `OS3.0.305.0.WPLCNXM`、内核版本 `6.6.118` 中的对应产物进行反汇编与逆向分析，或参考小米及其他项目曾经公开过的代码重新实现，并可能在此基础上额外加入了一些设计与修复。请你知晓。

另外，其中部分实现和修改是针对 dash 当前实际需求做的，并不一定适用于大多数 ROM。如果你想拿去玩，建议先看看我具体改了什么，以及为什么。不懂可以来问我。~~虽然我也可能不记得了就是。~~

下表仅列出相对于 MiCode/MTK_kernel_device_modules 的 [dash](https://github.com/MiCode/MTK_kernel_device_modules/tree/dash-w-oss) 公开基线 `9c6256406e2a`，经确认原本缺失、且后来实际补入当前仓库的源码。

目前已经在完整的 customer build 中确认能够找到下表所列的全部构建产物；但需要注意，**构建通过并不代表相关模块都已经能够正常工作。**

部分状态/说明可能对不上，摸了，请你结合源码自己再看看。~~我好喜欢床啊。床好舒服，我要和床过一辈子。~~

| 对应构建产物 | 补入源码 | Git 记录 | 状态/说明 |
|---|---|---|---|
| `nt38771_touch_dash.ko` | [`drivers/input/touchscreen/NT38771/nt387xx.c`](https://github.com/YorokobiMaster/android_kernel_xiaomi_dash_device_modules/blob/dash-w-oss/drivers/input/touchscreen/NT38771/nt387xx.c) | [`1a06e9dbcb21`](https://github.com/YorokobiMaster/android_kernel_xiaomi_dash_device_modules/commit/1a06e9dbcb2185806dcfef22f15a67129397fc2b) | 新增 NT38771 基线，后续补齐 pinctrl、THP、手势和电源生命周期。 |
| `xiaomi_touch_dash.ko` | [`drivers/input/touchscreen/xiaomi_touch/xiaomi_touch_core.c`](https://github.com/YorokobiMaster/android_kernel_xiaomi_dash_device_modules/blob/dash-w-oss/drivers/input/touchscreen/xiaomi_touch/xiaomi_touch_core.c) | [`3cb62b35aec9`](https://github.com/YorokobiMaster/android_kernel_xiaomi_dash_device_modules/commit/3cb62b35aec98f5cf739f1ec832948e4a20916c4) | 补回 Xiaomi touch 公共层，并与 NT38771 一起恢复 dash 触控/FOD 合同。 |
| `xiaomi_spi_tee.ko` | [`drivers/input/fingerprint/xiaomi_fp.c`](https://github.com/YorokobiMaster/android_kernel_xiaomi_dash_device_modules/blob/dash-w-oss/drivers/input/fingerprint/xiaomi_fp.c) | [`c2f002345855`](https://github.com/YorokobiMaster/android_kernel_xiaomi_dash_device_modules/commit/c2f002345855765f76a5f0243a07c215c004776f) | 补回 Xiaomi 指纹 transport ABI。 |
| `simtray.ko` | [`drivers/misc/mediatek/simtray/simtray.c`](https://github.com/YorokobiMaster/android_kernel_xiaomi_dash_device_modules/blob/dash-w-oss/drivers/misc/mediatek/simtray/simtray.c) | [`7d288b0de68b`](https://github.com/YorokobiMaster/android_kernel_xiaomi_dash_device_modules/commit/7d288b0de68bf552f00e12befc9f2824474b9df0) | 补入 Xiaomi SIM tray 状态驱动。Stock 两个容器中均有该模块。 |
| `crash_module.ko` | [`drivers/misc/xiaomi/crash_module/crash_module.c`](https://github.com/YorokobiMaster/android_kernel_xiaomi_dash_device_modules/blob/dash-w-oss/drivers/misc/xiaomi/crash_module/crash_module.c) | [`5c2352895f40`](https://github.com/YorokobiMaster/android_kernel_xiaomi_dash_device_modules/commit/5c2352895f40d6f8273d3c9c53441148b116242a) | 按 stock 行为重建。 |
| `debug_ext.ko` | [`drivers/misc/xiaomi/debug_ext/debug_ext.c`](https://github.com/YorokobiMaster/android_kernel_xiaomi_dash_device_modules/blob/dash-w-oss/drivers/misc/xiaomi/debug_ext/debug_ext.c) | [`1f80032df1e5`](https://github.com/YorokobiMaster/android_kernel_xiaomi_dash_device_modules/commit/1f80032df1e5e3f958f6ae53f88c1584c9ea2b16) | 按 stock 行为重建；源码内另有来源说明。 |
| `hwid.ko` | [`drivers/misc/xiaomi/hwid/hwid.c`](https://github.com/YorokobiMaster/android_kernel_xiaomi_dash_device_modules/blob/dash-w-oss/drivers/misc/xiaomi/hwid/hwid.c) | [`343c81b8eb44`](https://github.com/YorokobiMaster/android_kernel_xiaomi_dash_device_modules/commit/343c81b8eb447b78874f149b4b9124e9c357585f) | 重建 stock 参数和导出符号 ABI；有意省略无效的 stock `/sys/hwid` 接口。 |
| `mi_memory.ko` | [`drivers/misc/xiaomi/mi_memory/mi_memory.c`](https://github.com/YorokobiMaster/android_kernel_xiaomi_dash_device_modules/blob/dash-w-oss/drivers/misc/xiaomi/mi_memory/mi_memory.c) | [`0e9dd2f365e3`](https://github.com/YorokobiMaster/android_kernel_xiaomi_dash_device_modules/commit/0e9dd2f365e3b4daeb16c8e4887b4ba1662c4b5c) | 按 stock 行为重建，后续与 UFS 诊断 provider 对齐。 |
| `mi_stack.ko` | [`drivers/misc/xiaomi/mi_stack/mi_stack.c`](https://github.com/YorokobiMaster/android_kernel_xiaomi_dash_device_modules/blob/dash-w-oss/drivers/misc/xiaomi/mi_stack/mi_stack.c) | [`1dbbb7b07cf6`](https://github.com/YorokobiMaster/android_kernel_xiaomi_dash_device_modules/commit/1dbbb7b07cf69226a198f7eb012fa3b0320adb85) | dash 诊断模块重建的一部分。 |
| `mi_ubt.ko` | [`drivers/misc/xiaomi/mi_ubt/mi_ubt.c`](https://github.com/YorokobiMaster/android_kernel_xiaomi_dash_device_modules/blob/dash-w-oss/drivers/misc/xiaomi/mi_ubt/mi_ubt.c) | [`1dbbb7b07cf6`](https://github.com/YorokobiMaster/android_kernel_xiaomi_dash_device_modules/commit/1dbbb7b07cf69226a198f7eb012fa3b0320adb85) | dash 诊断模块重建的一部分。 |
| `mi_ubt_test.ko` | [`drivers/misc/xiaomi/mi_ubt_test/mi_ubt_test.c`](https://github.com/YorokobiMaster/android_kernel_xiaomi_dash_device_modules/blob/dash-w-oss/drivers/misc/xiaomi/mi_ubt_test/mi_ubt_test.c) | [`1dbbb7b07cf6`](https://github.com/YorokobiMaster/android_kernel_xiaomi_dash_device_modules/commit/1dbbb7b07cf69226a198f7eb012fa3b0320adb85) | dash 诊断模块重建的一部分。 |
| `perf_helper.ko` | [`drivers/misc/xiaomi/perf_helper/perf_helper.c`](https://github.com/YorokobiMaster/android_kernel_xiaomi_dash_device_modules/blob/dash-w-oss/drivers/misc/xiaomi/perf_helper/perf_helper.c) | [`f61679798165`](https://github.com/YorokobiMaster/android_kernel_xiaomi_dash_device_modules/commit/f61679798165773fa62176edef840592ca51f83d) | 按 stock 行为重建，后续补齐 reclaim 行为。Stock 两个容器中均有该模块。 |
| `mi_thermal_interface.ko` | [`drivers/thermal/xiaomi/mi_thermal_interface.c`](https://github.com/YorokobiMaster/android_kernel_xiaomi_dash_device_modules/blob/dash-w-oss/drivers/thermal/xiaomi/mi_thermal_interface.c) | [`4256a6112fb3`](https://github.com/YorokobiMaster/android_kernel_xiaomi_dash_device_modules/commit/4256a6112fb3cd7c687fd96e1e40e145fbf6a95a) | MiCode dash 基线缺失，补入统一 device-module 构建。 |
| `ufs-mediatek-mod-ise.ko` | [`drivers/ufs/ufs-mediatek-xiaomi.c`](https://github.com/YorokobiMaster/android_kernel_xiaomi_dash_device_modules/blob/dash-w-oss/drivers/ufs/ufs-mediatek-xiaomi.c) | [`c7890f5d60af`](https://github.com/YorokobiMaster/android_kernel_xiaomi_dash_device_modules/commit/c7890f5d60af88eca16d352583bcb7ebafe6b5df) | 补入 Xiaomi UFS 诊断 provider；它链接进已有聚合模块，不生成单独的 `.ko`。 |


## 配套仓库

构建和设备适配还依赖以下仓库：

- [Kernel build](https://github.com/YorokobiMaster/android_kernel_xiaomi_dash_build)
- [Bazel MGK rules](https://github.com/YorokobiMaster/android_kernel_xiaomi_dash_bazel_mgk_rules)
- [MediaTek device modules](https://github.com/YorokobiMaster/android_kernel_xiaomi_dash_device_modules)
- [MediaTek kernel modules](https://github.com/YorokobiMaster/android_kernel_xiaomi_dash_mtk_modules)

~~没有直接提供内核成品的必要。~~

## 免责声明

本项目按现状提供，不保证：

- 成功编译
- 启动或稳定运行
- 与任意固件、vendor 或 bootloader 版本兼容
- 不会造成数据丢失、启动失败或设备损坏
- 手机着火，爆炸，丢进天德池，坠落深渊，再起不能

请在理解 Android 启动链、分区结构、内核构建和设备恢复方法后再使用。任何构建、刷写及设备操作风险均由使用者自行承担。

本项目与 Xiaomi、MediaTek、Google 及 LineageOS 项目没有隶属或官方支持关系。

## 许可证

各文件继续遵循其原有许可证。详细信息见 [COPYING](COPYING) 以及任何适用的许可声明。

如果你喜欢，请查看不叠加于许可证的 [Don't Be A Dick License v1.2](DBAD.md) 与 [The Fuck Around And Find Out License v0.2](FAFOL.md) 的道德使用条件。

别当傻逼，自己折腾，后果自负。
