#!/usr/bin/env python3
#
# SPDX-FileCopyrightText: 2026 The LineageOS Project
# SPDX-License-Identifier: Apache-2.0
#
"""Builds the boot images of gtaxlwifi (SM-T580) inside the Android build.

The image "kernel" is bootloader/loader followed by its payload: a header,
the LZ4 kernel, the DTB, the ramdisk and the command line.
"""

import argparse
import hashlib
import os
import struct
import subprocess
import sys
import tempfile

PAGE = 2048
# Samsung mkbootimg defaults
BASE = 0x10000000
KERNEL_OFFSET = 0x00008000
RAMDISK_OFFSET = 0x01000000
SECOND_OFFSET = 0x00f00000
TAGS_OFFSET = 0x00000100
SEANDROID = b"SEANDROIDENFORCE"
# S-BOOT's limit on the "kernel" field
KERNEL_FIELD_MAX = 0x3d7ff00
RECOVERY_PARTITION = 77824 * 512    # RECOVERY, p10: 38 MiB


def pad(data, size=PAGE):
    return data + b"\0" * ((size - len(data) % size) % size)


def samsung_bootimg(kernel, dt):
    """A boot image as S-BOOT wants it: header v0 with dt_size, no ramdisk."""
    sha = hashlib.sha1()
    for blob in (kernel, b"", b"", dt):   # kernel, ramdisk, second, dt
        sha.update(blob)
        sha.update(struct.pack("<I", len(blob)))
    header = struct.pack(
        "<8s10I16s512s32s1024s",
        b"ANDROID!",
        len(kernel), BASE + KERNEL_OFFSET,
        0, BASE + RAMDISK_OFFSET,
        0, BASE + SECOND_OFFSET,
        BASE + TAGS_OFFSET, PAGE, len(dt), 0,
        b"", b"", sha.digest(), b"")
    return pad(header) + pad(kernel) + pad(dt) + SEANDROID


BOOT_PARTITION = 65536 * 512        # BOOT, p9: 32 MiB
PAYLOAD_OFFSET = 0x100000           # bootloader/loader/loader.h
PAYLOAD_RECOVERY = 1


def cmd_loader(args):
    """The loader (bootloader/loader) as the "kernel" of a Samsung boot
    image, followed by its payload: header, LZ4 kernel, DTB, ramdisk and
    command line. For BOOT (Android) or RECOVERY (--recovery)."""
    with open(args.loader, "rb") as f:
        loader = f.read()
    if len(loader) > PAYLOAD_OFFSET:
        sys.exit("loader over PAYLOAD_OFFSET")
    with tempfile.TemporaryDirectory() as tmp:
        lz = os.path.join(tmp, "Image.lz4")
        subprocess.run([args.lz4, "-l", "-12", "--favor-decSpeed", "-f", "-q",
                        args.kernel, lz], check=True)
        with open(lz, "rb") as f:
            kernel_lz4 = f.read()
    with open(args.kernel, "rb") as f:
        kernel_raw_size = len(f.read())
    pieces = []
    for path in (args.dtb, args.ramdisk, args.cmdline):
        with open(path, "rb") as f:
            pieces.append(f.read())
    dtb, ramdisk, cmdline = pieces
    header_size = 64
    offsets, blob = [], b""
    for data in (kernel_lz4, dtb, ramdisk, cmdline):
        blob += b"\0" * ((-(header_size + len(blob))) % 4096)   # page aligned
        offsets.append(header_size + len(blob))
        blob += data
    flags = PAYLOAD_RECOVERY if args.recovery else 0
    header = struct.pack("<8s10I", b"GXPAYLD1", flags,
                         offsets[0], len(kernel_lz4), kernel_raw_size,
                         offsets[1], len(dtb), offsets[2], len(ramdisk),
                         offsets[3], len(cmdline))
    header += b"\0" * (header_size - len(header))
    kernel_field = loader + b"\0" * (PAYLOAD_OFFSET - len(loader)) + header + blob
    if len(kernel_field) > KERNEL_FIELD_MAX:
        sys.exit("kernel field over S-BOOT's limit")
    with open(args.dt, "rb") as f:
        dt = f.read()
    img = samsung_bootimg(kernel_field, dt)
    limit = RECOVERY_PARTITION if args.recovery else BOOT_PARTITION
    if len(img) > limit:
        sys.exit("%d bytes, the partition holds %d" % (len(img), limit))
    with open(args.output, "wb") as f:
        f.write(img)


# Samsung DT table: S-BOOT picks the DTB whose entry matches the board. The
# codes are those of model_info-platform "android" and model_info-subtype
# "samsung" in Samsung's device tree, as hardware/samsung's dtbhtool keeps
# them per device.
DTBH_PLATFORM_CODE = 0x50a6
DTBH_SUBTYPE_CODE = 0x217584da


def fdt_root_u32(dtb, name):
    """A u32 property of the root node of a flattened device tree."""
    magic, _, off_struct, off_strings = struct.unpack(">4I", dtb[:16])
    if magic != 0xd00dfeed:
        sys.exit("not a DTB")
    pos, depth = off_struct, 0
    while True:
        (tag,) = struct.unpack(">I", dtb[pos:pos + 4])
        pos += 4
        if tag == 1:                                    # FDT_BEGIN_NODE
            depth += 1
            pos = (dtb.index(b"\0", pos) + 4) & ~3
        elif tag == 2:                                  # FDT_END_NODE
            depth -= 1
        elif tag == 3:                                  # FDT_PROP
            size, nameoff = struct.unpack(">2I", dtb[pos:pos + 8])
            pos += 8
            start = off_strings + nameoff
            prop = dtb[start:dtb.index(b"\0", start)].decode()
            if depth == 1 and prop == name and size == 4:
                return struct.unpack(">I", dtb[pos:pos + 4])[0]
            pos = (pos + size + 3) & ~3
        elif tag == 4:                                  # FDT_NOP
            continue
        else:                                           # FDT_END
            sys.exit("%s not in the DTB" % name)


def cmd_dtbh(args):
    """The DT table S-BOOT requires next to the kernel: a DTBH header with
    one entry, then the DTB, each padded to a page."""
    with open(args.dtb, "rb") as f:
        dtb = f.read()
    entry = struct.pack("<8I", fdt_root_u32(dtb, "model_info-chip"),
                        DTBH_PLATFORM_CODE, DTBH_SUBTYPE_CODE,
                        fdt_root_u32(dtb, "model_info-hw_rev"),
                        fdt_root_u32(dtb, "model_info-hw_rev_end"),
                        PAGE, len(pad(dtb)), 0x20)
    table = pad(struct.pack("<4s2I", b"DTBH", 2, 1) + entry) + pad(dtb)
    with open(args.output, "wb") as f:
        f.write(table)


def main():
    p = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    sub = p.add_subparsers(dest="cmd", required=True)
    ld = sub.add_parser("loader")
    ld.add_argument("--loader", required=True)
    ld.add_argument("--kernel", required=True)
    ld.add_argument("--dtb", required=True)
    ld.add_argument("--ramdisk", required=True)
    ld.add_argument("--cmdline", required=True)
    ld.add_argument("--dt", required=True)
    ld.add_argument("--lz4", required=True)
    ld.add_argument("--recovery", action="store_true")
    ld.add_argument("--output", required=True)
    dt = sub.add_parser("dtbh")
    dt.add_argument("--dtb", required=True)
    dt.add_argument("--output", required=True)
    args = p.parse_args()
    {"loader": cmd_loader, "dtbh": cmd_dtbh}[args.cmd](args)


if __name__ == "__main__":
    main()
