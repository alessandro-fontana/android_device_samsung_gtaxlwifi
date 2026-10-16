#!/system/bin/sh
#
# SPDX-FileCopyrightText: 2026 The LineageOS Project
# SPDX-License-Identifier: Apache-2.0
#
#   gtaxl-ota.sh check     the partition table has been migrated, with vbmeta
#   gtaxl-ota.sh prepare   before the addon backup
#   gtaxl-ota.sh finish    at the end, after the addon restore
#
# The addon backup does not fit in RAM: prepare bind-mounts /tmp/backupdir
# from USERDATA. If USERDATA is not formatted yet, a scratch ext4 is used and
# wiped again by finish, so that Android still formats it.

LOG=/tmp/gtaxl-ota.log
BK_MNT=/tmp/gtaxl-bk
BK_DEV=/dev/block/by-name/USERDATA
SCRATCH=/tmp/gtaxl-ota-scratch
OTA_DEV=/dev/block/by-name/OTA
# first LBA of USERDATA after the repartition package
USERDATA_FIRST=8300544

log() { echo "gtaxl-ota: $*" | tee -a "$LOG"; }
die() { log "ERROR: $*"; exit 1; }

backup_store() {
	mkdir -p $BK_MNT /tmp/backupdir
	if ! mount -t ext4 $BK_DEV $BK_MNT 2>>"$LOG"; then
		# anything but a wiped head (4 KiB of zeros) is data: leave it alone
		[ "$(head -c 4096 $BK_DEV | tr -d '\000' | head -c 1 | wc -c)" = 0 ] || return 1
		touch $SCRATCH
		mke2fs -F -q -t ext4 $BK_DEV >>"$LOG" 2>&1 || return 1
		mount -t ext4 $BK_DEV $BK_MNT 2>>"$LOG" || return 1
	fi
	mkdir -p $BK_MNT/gtaxl-addon-backup && mount -o bind $BK_MNT/gtaxl-addon-backup /tmp/backupdir
}

# Builds before the seed moved to misc kept it in the first sector of OTA,
# which is zero on the factory table: put it back
ota_sector_restore() {
	[ "$(head -c 8 $OTA_DEV 2>/dev/null)" = GXSEED01 ] || return 0
	dd if=/dev/zero of=$OTA_DEV bs=512 count=1 2>>"$LOG" && log "OTA: old entropy seed erased"
}

backup_store_release() {
	grep -q " $BK_MNT " /proc/mounts || return 0
	umount /tmp/backupdir 2>/dev/null
	rm -rf $BK_MNT/gtaxl-addon-backup
	umount $BK_MNT
	[ -e $SCRATCH ] && dd if=/dev/zero of=$BK_DEV bs=1048576 count=1 2>>"$LOG"
	rm -f $SCRATCH
	sync
}

case $1 in
check)
	part=$(readlink -f $BK_DEV)
	# vbmeta: the first stage verifies vendor against it, and does not
	# mount vendor without it
	[ -e /dev/block/by-name/super ] && [ -e /dev/block/by-name/vbmeta ] && [ -n "$part" ] \
		&& [ "$(cat /sys/class/block/${part##*/}/start 2>/dev/null)" = $USERDATA_FIRST ] \
		|| die "partition table not migrated, nothing written"
	log "partition table: migrated"
	;;
prepare)
	for m in /data /sdcard /cache /metadata; do
		umount "$m" 2>/dev/null
	done
	backup_store || die "no room on USERDATA for the addon backup, nothing written"
	;;
finish)
	backup_store_release
	ota_sector_restore
	;;
*)
	die "usage: $0 check|prepare|finish"
	;;
esac
exit 0
