# Boot images of gtaxlwifi

S-BOOT, Samsung's signed bootloader, starts BOOT (p9) or RECOVERY (p10) as
Samsung boot images. It ignores the command line in the image, refuses
ramdisks over 14 MiB and expects a DT table next to the kernel. So the
"kernel" of both images is a small loader, `external/gtaxl-loader`, that
starts the real kernel.

## Layout

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

| file | what |
|---|---|
| `mkbootimg.mk` | builds the loader and both images during the Android build |
| `gtaxl_bootimg.py` | writes the images and the DT table |
| `stub.dts` | the DT S-BOOT requires next to the kernel |
