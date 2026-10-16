#!/vendor/bin/sh
#
# SPDX-FileCopyrightText: 2026 The LineageOS Project
# SPDX-License-Identifier: Apache-2.0
#
# Entropy seed for the next boot, in the vendor space of misc (2 KiB in);
# the boot loader passes it to the kernel, which credits it as entropy.
#
#   bytes 0..7    "GXSEED01"
#   bytes 8..39   seed
#
#   gtaxl-entropy-seed.sh clear   early in the boot: a seed is used once
#   gtaxl-entropy-seed.sh write   once the pool is mature
#
# The signature is cleared first and written last, so an interrupted write
# never leaves a seed marked valid.

NODE=/dev/block/by-name/misc
SIG=256		# 2048 / 8
SEED=257

clear_sig() {
    dd if=/dev/zero of=$NODE bs=8 seek=$SIG count=1 conv=notrunc 2>/dev/null
}

case $1 in
clear)
    clear_sig
    ;;
write)
    clear_sig && sync &&
        dd if=/dev/urandom of=$NODE bs=8 seek=$SEED count=4 conv=notrunc 2>/dev/null && sync &&
        echo -n 'GXSEED01' | dd of=$NODE bs=8 seek=$SIG count=1 conv=notrunc 2>/dev/null && sync
    ;;
*)
    exit 1
    ;;
esac
