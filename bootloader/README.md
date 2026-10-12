# Boot chain of gtaxlwifi

S-BOOT, Samsung's signed bootloader, starts BOOT (p9) or RECOVERY (p10) as
Samsung boot images. It ignores the command line in the image, refuses
ramdisks over 14 MiB and expects a DT table next to the kernel. So the
"kernel" of both images is `loader/`, a small loader that starts the real
kernel.

## Boot image layout

A Samsung header v0 (2 KiB pages, no ramdisk), the "kernel" field, the DT
table built from `stub.dts`, and `SEANDROIDENFORCE`. The "kernel" field is:

| offset | content |
|---|---|
| 0 | `loader.bin`, loaded by S-BOOT at 0x40080000 |
| 0x100000 (`PAYLOAD_OFFSET`) | payload header: magic `GXPAYLD1`, flags (bit 0: RECOVERY), offset and size of each piece |
| 4 KiB aligned | the kernel `Image`, LZ4 legacy frame (`lz4 -l`) |
| 4 KiB aligned | the DTB |
| 4 KiB aligned | the ramdisk |
| 4 KiB aligned | the command line, as text |

`bootimg/gtaxl_bootimg.py` writes the images; `bootimg/mkbootimg.mk` drives it.

## What the loader does

- sets INFORM3 back to "normal boot" (S-BOOT remembers the last mode);
- takes `androidboot.serialno` from S-BOOT's command line;
- adds `androidboot.mode=charger` when the cable switched the tablet on
  (cold start and PMIC power-on source, read over HSI2C);
- reads the Wi-Fi and Bluetooth addresses of the tablet from Samsung's EFS,
  read-only, and puts them in the DTB;
- passes an entropy seed (the one Android keeps in the vendor space of
  `misc`, plus timing jitter, hashed);
- inflates the kernel to 0x50000000, fills `/chosen` of the DTB at 0x4a000000
  and leaves the ramdisk in place;
- goes to the recovery when the payload is bad or the partition table has no
  `super`;
- leaves any error in `/chosen/gtaxl,loader-log`.

## Build

`loader/build.sh <output.bin> [<top of the Android tree>]` builds it with the
tree's clang and lld and libfdt from `external/dtc`; `bootimg/mkbootimg.mk`
calls it during the Android build.

| file | what |
|---|---|
| `loader/` | the loader: entry, MMU, LZ4, PMIC, eMMC (IDMAC), ext4, SHA-256 |
| `stub.dts` | the DT S-BOOT requires next to the kernel; `bootimg/gtaxl_bootimg.py dtbh` builds its table |
