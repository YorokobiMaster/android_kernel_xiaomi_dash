# android_kernel_xiaomi_dash

English|[简体中文](README_zh.md)

- Linux kernel entry point docs: [README](README)
- How do I submit patches to Android Common Kernels: [ACK_PATCH_SUBMISSION](README_ACK_PATCH_SUBMISSION.md)

This is an unofficial kernel tree for the Redmi Turbo 5 Max (codename `dash`), spun up for my personal ~~LineageOS 23.2 bringup~~ ~~suffering through MediaTek's finest hot garbage~~ tinkering pleasure.

**Heads up:** Known missing kernel module sources required for basic booting and daily usage have been reverse-engineered and filled in, but weird bugs of varying severity may still be lurking around. Also, several modules essential to HyperOS are missing here—**do NOT flash this directly over HyperOS.**

~~If you want those missing modules back, DIY and send a PR (?).~~

Due to copyright and licensing being a total minefield, I’ve tried my best to sort things out. Mismatched copyright headers, wrong licenses, or accidental attribution mess-ups might still exist.

If you spot code that belongs to you and isn't properly credited, please reach out at `root@sandai.me` or open an Issue, and I'll sort it out ASAP. An email is preferred—just in case I spontaneously touch grass and disappear from GitHub one day.

- **Device**: Redmi Turbo 5 Max
- **Codename**: `dash`
- **SoC**: MediaTek Dimensity 9500s / mt6991
- **Current Version**: 6.6.142

## Reimplemented / Injected Modules

These modules were reconstructed through iterative disassembly and reverse-engineering of stock binaries from `OS3.0.305.0.WPLCNXM` (Kernel `6.6.118`) using Codex (`gpt-5.6-sol`) / ChatGPT (`gpt-5.6-pro`), alongside references to previously published code from Xiaomi and the open-source community. Some custom redesigns and patches were added along the way. Heads up.

Also, parts of the implementation were tailored specifically for `dash`'s current bringup needs and might not play nice on generic ROMs. If you plan to cherry-pick this into your own project, check what changed and why first. Feel free to ask if something looks confusing. ~~Not like I’ll necessarily remember why I wrote it though.~~

The table below lists sources that were missing from MiCode's public baseline [dash (9c6256406e2a)](https://github.com/MiCode/MTK_kernel_device_modules/tree/dash-w-oss) and subsequently reimplemented in this repository.

All build targets below have been verified to generate successfully in a full customer build; however, **"it compiles" does not mean "it works perfectly."**

Some notes might be slightly outdated because I got lazy. When in doubt, read the source. ~~Bed good. Code bad. Me sleep forever now.~~

| Build Target | Added Source | Git Commit | Status / Notes |
|---|---|---|---|
| `nt38771_touch_dash.ko` | [`drivers/input/touchscreen/NT38771/nt387xx.c`](https://github.com/YorokobiMaster/android_kernel_xiaomi_dash_device_modules/blob/dash-w-oss/drivers/input/touchscreen/NT38771/nt387xx.c) | [`1a06e9dbcb21`](https://github.com/YorokobiMaster/android_kernel_xiaomi_dash_device_modules/commit/1a06e9dbcb2185806dcfef22f15a67129397fc2b) | Added initial NT38771 baseline; later completed pinctrl, THP, gestures, and power lifecycle routines. |
| `xiaomi_touch_dash.ko` | [`drivers/input/touchscreen/xiaomi_touch/xiaomi_touch_core.c`](https://github.com/YorokobiMaster/android_kernel_xiaomi_dash_device_modules/blob/dash-w-oss/drivers/input/touchscreen/xiaomi_touch/xiaomi_touch_core.c) | [`3cb62b35aec9`](https://github.com/YorokobiMaster/android_kernel_xiaomi_dash_device_modules/commit/3cb62b35aec98f5cf739f1ec832948e4a20916c4) | Restored Xiaomi touch common layer; re-established touchscreen & FOD contracts alongside NT38771. |
| `xiaomi_spi_tee.ko` | [`drivers/input/fingerprint/xiaomi_fp.c`](https://github.com/YorokobiMaster/android_kernel_xiaomi_dash_device_modules/blob/dash-w-oss/drivers/input/fingerprint/xiaomi_fp.c) | [`c2f002345855`](https://github.com/YorokobiMaster/android_kernel_xiaomi_dash_device_modules/commit/c2f002345855765f76a5f0243a07c215c004776f) | Restored Xiaomi fingerprint transport ABI. |
| `simtray.ko` | [`drivers/misc/mediatek/simtray/simtray.c`](https://github.com/YorokobiMaster/android_kernel_xiaomi_dash_device_modules/blob/dash-w-oss/drivers/misc/mediatek/simtray/simtray.c) | [`7d288b0de68b`](https://github.com/YorokobiMaster/android_kernel_xiaomi_dash_device_modules/commit/7d288b0de68bf552f00e12befc9f2824474b9df0) | Added Xiaomi SIM tray status driver. Present in both stock containers. |
| `crash_module.ko` | [`drivers/misc/xiaomi/crash_module/crash_module.c`](https://github.com/YorokobiMaster/android_kernel_xiaomi_dash_device_modules/blob/dash-w-oss/drivers/misc/xiaomi/crash_module/crash_module.c) | [`5c2352895f40`](https://github.com/YorokobiMaster/android_kernel_xiaomi_dash_device_modules/commit/5c2352895f40d6f8273d3c9c53441148b116242a) | Rebuilt matching stock behavior. |
| `debug_ext.ko` | [`drivers/misc/xiaomi/debug_ext/debug_ext.c`](https://github.com/YorokobiMaster/android_kernel_xiaomi_dash_device_modules/blob/dash-w-oss/drivers/misc/xiaomi/debug_ext/debug_ext.c) | [`1f80032df1e5`](https://github.com/YorokobiMaster/android_kernel_xiaomi_dash_device_modules/commit/1f80032df1e5e3f958f6ae53f88c1584c9ea2b16) | Rebuilt matching stock behavior; upstream lineage noted inside source. |
| `hwid.ko` | [`drivers/misc/xiaomi/hwid/hwid.c`](https://github.com/YorokobiMaster/android_kernel_xiaomi_dash_device_modules/blob/dash-w-oss/drivers/misc/xiaomi/hwid/hwid.c) | [`343c81b8eb44`](https://github.com/YorokobiMaster/android_kernel_xiaomi_dash_device_modules/commit/343c81b8eb447b78874f149b4b9124e9c357585f) | Rebuilt stock params and exported symbol ABI; omitted dead stock `/sys/hwid` node intentionally. |
| `mi_memory.ko` | [`drivers/misc/xiaomi/mi_memory/mi_memory.c`](https://github.com/YorokobiMaster/android_kernel_xiaomi_dash_device_modules/blob/dash-w-oss/drivers/misc/xiaomi/mi_memory/mi_memory.c) | [`0e9dd2f365e3`](https://github.com/YorokobiMaster/android_kernel_xiaomi_dash_device_modules/commit/0e9dd2f365e3b4daeb16c8e4887b4ba1662c4b5c) | Rebuilt matching stock behavior; aligned with UFS diagnostic provider later. |
| `mi_stack.ko` | [`drivers/misc/xiaomi/mi_stack/mi_stack.c`](https://github.com/YorokobiMaster/android_kernel_xiaomi_dash_device_modules/blob/dash-w-oss/drivers/misc/xiaomi/mi_stack/mi_stack.c) | [`1dbbb7b07cf6`](https://github.com/YorokobiMaster/android_kernel_xiaomi_dash_device_modules/commit/1dbbb7b07cf69226a198f7eb012fa3b0320adb85) | Part of `dash` diagnostic stack recreation. |
| `mi_ubt.ko` | [`drivers/misc/xiaomi/mi_ubt/mi_ubt.c`](https://github.com/YorokobiMaster/android_kernel_xiaomi_dash_device_modules/blob/dash-w-oss/drivers/misc/xiaomi/mi_ubt/mi_ubt.c) | [`1dbbb7b07cf6`](https://github.com/YorokobiMaster/android_kernel_xiaomi_dash_device_modules/commit/1dbbb7b07cf69226a198f7eb012fa3b0320adb85) | Part of `dash` diagnostic stack recreation. |
| `mi_ubt_test.ko` | [`drivers/misc/xiaomi/mi_ubt_test/mi_ubt_test.c`](https://github.com/YorokobiMaster/android_kernel_xiaomi_dash_device_modules/blob/dash-w-oss/drivers/misc/xiaomi/mi_ubt_test/mi_ubt_test.c) | [`1dbbb7b07cf6`](https://github.com/YorokobiMaster/android_kernel_xiaomi_dash_device_modules/commit/1dbbb7b07cf69226a198f7eb012fa3b0320adb85) | Part of `dash` diagnostic stack recreation. |
| `perf_helper.ko` | [`drivers/misc/xiaomi/perf_helper/perf_helper.c`](https://github.com/YorokobiMaster/android_kernel_xiaomi_dash_device_modules/blob/dash-w-oss/drivers/misc/xiaomi/perf_helper/perf_helper.c) | [`f61679798165`](https://github.com/YorokobiMaster/android_kernel_xiaomi_dash_device_modules/commit/f61679798165773fa62176edef840592ca51f83d) | Rebuilt matching stock behavior; later completed memory reclaim logic. Present in both stock containers. |
| `mi_thermal_interface.ko` | [`drivers/thermal/xiaomi/mi_thermal_interface.c`](https://github.com/YorokobiMaster/android_kernel_xiaomi_dash_device_modules/blob/dash-w-oss/drivers/thermal/xiaomi/mi_thermal_interface.c) | [`4256a6112fb3`](https://github.com/YorokobiMaster/android_kernel_xiaomi_dash_device_modules/commit/4256a6112fb3cd7c687fd96e1e40e145fbf6a95a) | Missing from MiCode's `dash` tree; merged back into the unified device-module build. |
| `ufs-mediatek-mod-ise.ko` | [`drivers/ufs/ufs-mediatek-xiaomi.c`](https://github.com/YorokobiMaster/android_kernel_xiaomi_dash_device_modules/blob/dash-w-oss/drivers/ufs/ufs-mediatek-xiaomi.c) | [`c7890f5d60af`](https://github.com/YorokobiMaster/android_kernel_xiaomi_dash_device_modules/commit/c7890f5d60af88eca16d352583bcb7ebafe6b5df) | Added Xiaomi UFS diagnostic provider; statically linked into the composite module rather than built as a standalone `.ko`. |

## Related Repositories

Full builds and device bringups require the following trees:

- [Kernel build](https://github.com/YorokobiMaster/android_kernel_xiaomi_dash_build)
- [Bazel MGK rules](https://github.com/YorokobiMaster/android_kernel_xiaomi_dash_bazel_mgk_rules)
- [MediaTek device modules](https://github.com/YorokobiMaster/android_kernel_xiaomi_dash_device_modules)
- [MediaTek kernel modules](https://github.com/YorokobiMaster/android_kernel_xiaomi_dash_mtk_modules)

~~No prebuilt boot images provided. Build it yourself.~~

## Disclaimer

**YOUR WARRANTY IS NOW VOID.**

This project is provided **"AS IS"** without warranty of any kind. I do NOT guarantee that it will:

- Compile cleanly on your machine
- Boot, stay alive, or run stably
- Play nice with your specific firmware, vendor blob set, or bootloader revision
- Not cause sudden data loss, bootloops, hard bricks, or thermonuclear war
- Prevent your device from catching fire, exploding, summoning demons, or dying permanently etc.

Only proceed if you actually understand the Android boot chain, partition maps, kernel building, and unbricking/EDL recovery workflows. **Any risk of building, flashing, or bricking your phone rests entirely on you.**

This project is NOT affiliated with or endorsed by Xiaomi, MediaTek, Google, or the LineageOS project.

## Licensing

Individual files remain subject to their original upstream licenses. See [COPYING](COPYING) and any applicable license notices.

In addition (and purely in spirit), please observe the non-legally-binding moral terms of the [Don't Be A Dick License v1.2](DBAD.md) and [The Fuck Around And Find Out License v0.2](FAFOL.md).

**TL;DR:** DON'T BE A DICK, DIY your own stuff, and remember: FUCK AROUND AND FIND OUT.