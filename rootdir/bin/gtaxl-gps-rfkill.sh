#!/vendor/bin/sh
#
# SPDX-FileCopyrightText: 2026 The LineageOS Project
# SPDX-License-Identifier: Apache-2.0
#
# gps.xml needs a fixed GpioNStdbyPath, but the rfkillN index of the GPS
# rfkill-gpio depends on probe order: link its state file into
# /data/vendor/gps. The chip starts off (default-blocked in the kernel DT)
# until gpsd powers it on.

DIR=/sys/devices/platform/gps/rfkill
LINK=/data/vendor/gps/nstdby

for r in $DIR/rfkill*; do
    [ -e "$r/state" ] || continue
    ln -sf "$r/state" $LINK
    echo "gtaxl-gps-rfkill: $LINK -> $r/state"
    exit 0
done

echo "gtaxl-gps-rfkill: no rfkill in $DIR, gpsd will not be able to power the chip"
exit 1
