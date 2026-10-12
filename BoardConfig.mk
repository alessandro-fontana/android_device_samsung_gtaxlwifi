#
# SPDX-FileCopyrightText: 2026 The LineageOS Project
# SPDX-License-Identifier: Apache-2.0
#

DEVICE_PATH := device/samsung/gtaxlwifi

# Inherit from mainline/common
include device/mainline/common/BoardConfigMainlineCommon.mk

# A/B
AB_OTA_UPDATER := false

# Architecture
TARGET_ARCH := arm64
TARGET_ARCH_VARIANT := armv8-a
TARGET_CPU_ABI := arm64-v8a
TARGET_CPU_ABI2 :=
TARGET_CPU_VARIANT := generic

# Display
TARGET_SCREEN_DENSITY := 240

# Filesystem
TARGET_USERIMAGES_SPARSE_EXT_DISABLED := true
TARGET_USERIMAGES_USE_EXT4 := true

# Graphics (Mesa): no Vulkan, panvk does not support Midgard
BOARD_MESA3D_GALLIUM_DRIVERS := panfrost
BOARD_MESA3D_VULKAN_DRIVERS :=
$(call soong_config_set,minigbm_upstream,platform,all_arm)

# Kernel
include $(DEVICE_PATH)/kernels/exynos7870/board.mk
BOARD_RAMDISK_USE_LZ4 := true

# Kernel modules: the eMMC and dm drivers are built in, and ueventd loads
# the rest by modalias. cfg80211 is loaded by the first stage, before the
# policy: it creates the keyring for the regulatory.db signature.
BOOT_KERNEL_MODULES := cfg80211.ko
BOARD_VENDOR_RAMDISK_KERNEL_MODULES_LOAD := cfg80211.ko
# Platform
TARGET_BOARD_PLATFORM := exynos7870

# SELinux
BOARD_VENDOR_SEPOLICY_DIRS += $(DEVICE_PATH)/sepolicy/vendor
SYSTEM_EXT_PRIVATE_SEPOLICY_DIRS += $(DEVICE_PATH)/sepolicy/private

# VINTF
DEVICE_MANIFEST_FILE := $(DEVICE_PATH)/configs/vintf/manifest.xml

# Kernel command line, which the loader takes from the payload: S-BOOT
# passes only its own. The state is orange as on any unlocked device, which
# also lets the recovery start adbd; S-BOOT does not report one. fbdev
# emulation off keeps the S-BOOT logo up until the boot animation.
BOARD_KERNEL_CMDLINE := \
    $(MAINLINE_COMMON_ANDROIDBOOT_PARAMS) \
    $(MAINLINE_COMMON_KERNEL_PARAMS) \
    androidboot.boot_devices=soc@0/13540000.mmc \
    androidboot.hardware=gtaxlwifi \
    androidboot.verifiedbootstate=orange \
    console=ttynull \
    drm_kms_helper.fbdev_emulation=0 \
    fbcon=rotate:1 \
    panic=10 \
    s3c2410_wdt.tmr_atboot=1
# The late SM5502 reset that redetects the cable briefly opens the USB
# switch: in the recovery it would drop the sideload connection.
GTAXL_RECOVERY_CMDLINE := \
    $(BOARD_KERNEL_CMDLINE) \
    extcon_sm5502.boot_redetect=0

# Boot: S-BOOT starts BOOT (p9, 32 MiB) or RECOVERY (p10); both hold
# bootloader/loader with the kernel, DTB, ramdisk and cmdline.
# The recovery size is not the real one (38 MiB, checked by gtaxl_bootimg.py):
# add_img_to_target_files also builds an unused recovery-two-step.img that
# must fit.
BOARD_BOOTIMAGE_PARTITION_SIZE := 33554432
BOARD_RECOVERYIMAGE_PARTITION_SIZE := 134217728
BOARD_CUSTOM_BOOTIMG := true
BOARD_CUSTOM_BOOTIMG_MK := $(DEVICE_PATH)/bootimg/mkbootimg.mk
# Cache: the CACHE partition (p20), for /cache/recovery. cache.img is not
# installed.
BOARD_CACHEIMAGE_FILE_SYSTEM_TYPE := ext4
BOARD_CACHEIMAGE_PARTITION_SIZE := 209715200

# Partitions: system, product and system_ext writable ext4 with reserved
# space, so that recovery addons can install. The full product reserve does
# not fit in super.
# No BOARD_EXT4_SHARE_DUP_BLOCKS: shared blocks force a read-only mount.
BOARD_FLASH_BLOCK_SIZE := 4096
BOARD_USES_METADATA_PARTITION := true
BOARD_USES_SYSTEM_DLKMIMAGE := true
BOARD_USES_VENDOR_DLKMIMAGE := true
TARGET_COPY_OUT_PRODUCT := product
TARGET_COPY_OUT_SYSTEM_DLKM := system_dlkm
TARGET_COPY_OUT_SYSTEM_EXT := system_ext
TARGET_COPY_OUT_VENDOR := vendor
TARGET_COPY_OUT_VENDOR_DLKM := vendor_dlkm
BOARD_SYSTEM_DLKMIMAGE_FILE_SYSTEM_TYPE := erofs
BOARD_VENDORIMAGE_FILE_SYSTEM_TYPE := erofs
BOARD_VENDOR_DLKMIMAGE_FILE_SYSTEM_TYPE := erofs
BOARD_SYSTEMIMAGE_FILE_SYSTEM_TYPE := ext4
BOARD_PRODUCTIMAGE_FILE_SYSTEM_TYPE := ext4
BOARD_SYSTEM_EXTIMAGE_FILE_SYSTEM_TYPE := ext4
BOARD_PRODUCTIMAGE_MINIMAL_PARTITION_RESERVED_SIZE := true
include vendor/lineage/config/BoardConfigReservedSize.mk

# Super: the former SYSTEM partition (p19); the group leaves 4 MiB for
# liblp metadata and alignment.
BOARD_SUPER_PARTITION_SIZE := 3565158400
BOARD_SUPER_PARTITION_GROUPS := gtaxl
BOARD_GTAXL_SIZE := 3560964096
BOARD_GTAXL_PARTITION_LIST := system product system_ext vendor vendor_dlkm system_dlkm
