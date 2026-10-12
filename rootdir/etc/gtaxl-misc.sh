#!/system/bin/sh
#
# SPDX-FileCopyrightText: 2026 The LineageOS Project
# SPDX-License-Identifier: Apache-2.0
#
# On the factory partition table there is no misc partition and recovery
# fails to clear the BCB at every start: provide a RAM-backed loop device.
# Nothing reads it; with a real misc partition this does nothing.
M=/dev/block/by-name/misc
[ -e $M ] && exit 0
truncate -s 1048576 /tmp/gtaxl-misc.img || exit 0
L=$(losetup -f --show /tmp/gtaxl-misc.img) || exit 0
ln -s "$L" $M
