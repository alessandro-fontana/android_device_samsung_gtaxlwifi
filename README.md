# Android device tree for Samsung Galaxy Tab A 10.1 (2016) Wi-Fi running mainline kernel

SM-T580, codename `gtaxlwifi`, Exynos 7870. Built on `device/mainline/common`,
with the standard first stage init and HALs chosen through its options.

Developed with LLM assistance: commits made with it carry `Assisted-by: LLM`.

The history here is not the development history: the device tree of a
private development repository was split into one commit per
component. Only the last commit has been built and tested.

## Kernel

| Path | URL | Branch |
|------|-----|--------|
| `kernel/mainline/exynos7870-mainline` | https://github.com/alessandro-fontana/linux-exynos7870 | `gtaxlwifi/7.2` |

Linux 7.2.9 with the board's device tree and drivers (display bridge and
panel, MFC, camera interface and sensors, audio, PMIC, charger, sensors).

## Boot chain

Samsung's S-BOOT only starts its own boot image format and cannot be
replaced. `BOOT` and `RECOVERY` therefore hold `bootloader/loader`, built
from source by `bootimg/mkbootimg.mk`: S-BOOT starts it, and it unpacks the
kernel, completes the device tree (command line, serial number, Wi-Fi and
Bluetooth addresses from the EFS partition, rng-seed, charger mode) and
jumps to Linux. S-BOOT and download mode are never modified.

## Partition table

LineageOS needs `super`, `vbmeta`, `metadata` and `misc`, which the factory
table does not have. A separate package converts it once, before the first
install: `tools/repartition/` in
https://github.com/alessandro-fontana/gtaxlwifi-lineageos. The install
package only checks that the table is converted, and keeps the addon backup
on `USERDATA` (`releasetools.py`, `ota/`), since it does not fit in the
recovery's RAM.

## Changes to other projects

Until they are merged, these projects need the changes published in
https://github.com/alessandro-fontana/gtaxlwifi-lineageos (`patches/`):

| Project | Purpose |
|---------|---------|
| `external/minigbm-upstream` | gbm_mesa: fd and 1D buffer fixes, macroblock padding for encoders, optional cached heap for CPU-read RGB buffers |
| `external/v4l2_codec2` | HEVC encoder, MPEG-4/H.263 components, input buffer size property, surface change and AIDL buffer pool fixes |
| `external/tinyhal` | report the presented position, not the written one |
| `frameworks/base` | hwui: optional uniform stretch without a layer, for slow GPUs |
| `hardware/intel/sensors-iio` | RGBC light sensors: use the clear channel |
| `hardware/linaro/libpm` | thermal HAL: deliver thermal events, poll binder from the looper |
| `system/core` | ashmem: memfd without probing (with the backport below) |
| `system/sepolicy`, `external/selinux` | `memfd_file` class (Android 17 backport) |
| `external/libcamera` | TI's Android branch with the software ISP, plus this tablet's patches |

## Proprietary files

`proprietary-files.txt` lists seven files with their sources; extract them
with `./extract-files.py`, from a running installation or from a directory.
