#
# SPDX-FileCopyrightText: 2026 The LineageOS Project
# SPDX-License-Identifier: Apache-2.0
#

DEVICE_PATH := device/samsung/gtaxlwifi

# Options of device/mainline/common
TARGET_AUDIO_HAL := tinyhal
TARGET_AUDIO_POLICY := common-handheld-7.0
TARGET_CAMERA_PROVIDER_HAL := libcamera
TARGET_DRM_HWCOMPOSER_INSIDE_APEX := false
TARGET_GRAPHICS_ALLOCATOR_HAL := minigbm-upstream
TARGET_HEALTH_HAL := custom
TARGET_HOSTAPD_AND_WPA_SUPPLICANT_FORM := legacy
TARGET_MEDIA_C2_HAL := v4l2_codec2
TARGET_MINIGBM_UPSTREAM_ENABLE_GBM_MESA_DRIVER := true
TARGET_POWER_HAL := perfmgr-lineage
TARGET_SENSORS_HAL := iio
TARGET_THERMAL_HAL := linaro-libpm
# The Wi-Fi variant has no vibration motor
TARGET_HAS_VIBRATOR := false
include device/mainline/common/optional/options.mk
$(call inherit-product, device/mainline/common/mainline_common.mk)

# Graphics. minigbm needs no vendor.minigbm.device: ueventd closes
# renderD128, and it falls through to renderD129 (panfrost).
PRODUCT_VENDOR_PROPERTIES += \
    ro.hardware.gralloc=minigbm_upstream \
    ro.opengles.version=196609 \
    ro.vendor.graphics.card.name=exynos \
    ro.vendor.graphics.render.name=panfrost \
    vendor.hwc.drm.device=/dev/dri/card0
# SoC
PRODUCT_VENDOR_PROPERTIES += \
    ro.soc.manufacturer=samsung \
    ro.soc.model=exynos7870

# Proprietary
$(call inherit-product, vendor/samsung/gtaxlwifi/gtaxlwifi-vendor.mk)

# Characteristics
PRODUCT_CHARACTERISTICS := tablet

# Audio: TinyHAL opens /vendor/etc/audio.<ro.product.vendor.device>.xml
PRODUCT_COPY_FILES += \
    $(DEVICE_PATH)/configs/audio/audio.gtaxlwifi.xml:$(TARGET_COPY_OUT_VENDOR)/etc/audio.gtaxlwifi.xml \
    $(DEVICE_PATH)/configs/audio/primary_audio_policy_configuration.xml:$(TARGET_COPY_OUT_VENDOR)/etc/primary_audio_policy_configuration.xml

# ueventd: imported by ueventd.mainline.rc
PRODUCT_COPY_FILES += \
    $(DEVICE_PATH)/configs/init/ueventd.gtaxlwifi.rc:$(TARGET_COPY_OUT_VENDOR)/etc/ueventd.gtaxlwifi.rc

# Recovery: a larger /tmp for the addon backup, and an in-RAM misc (the
# factory partition table has none)
PRODUCT_COPY_FILES += \
    $(DEVICE_PATH)/rootdir/etc/init/gtaxl-recovery.rc:$(TARGET_COPY_OUT_RECOVERY)/root/system/etc/init/gtaxl-recovery.rc \
    $(DEVICE_PATH)/rootdir/etc/gtaxl-misc.sh:$(TARGET_COPY_OUT_RECOVERY)/root/system/etc/gtaxl-misc.sh

# APEX compression: super is too small for uncompressed APEXes. It is the
# default today, but RELEASE_APEX_MOUNT_BEFORE_DATA would turn it off.
OVERRIDE_PRODUCT_COMPRESSED_APEX := true

# Dalvik heap
$(call inherit-product, frameworks/native/build/tablet-10in-xhdpi-2048-dalvik-heap.mk)

# Dynamic partitions: recovery and addons need ro.boot.dynamic_partitions
PRODUCT_USE_DYNAMIC_PARTITIONS := true
PRODUCT_USE_DYNAMIC_PARTITION_SIZE := true

# Images: boot.img is built by bootimg/mkbootimg.mk
PRODUCT_BUILD_BOOT_IMAGE := true
PRODUCT_BUILD_RAMDISK_IMAGE := true

# Init
PRODUCT_COPY_FILES += \
    $(DEVICE_PATH)/configs/fstab/fstab.gtaxlwifi:$(TARGET_COPY_OUT_RAMDISK)/fstab.gtaxlwifi \
    $(DEVICE_PATH)/configs/fstab/fstab.gtaxlwifi:$(TARGET_COPY_OUT_VENDOR)/etc/fstab.gtaxlwifi \
    $(DEVICE_PATH)/configs/init/init.gtaxlwifi.rc:$(TARGET_COPY_OUT_VENDOR)/etc/init/hw/init.gtaxlwifi.rc
PRODUCT_PACKAGES += \
    vndservicemanager \
    zram.rc
# Keep the build's model (SM-T580) instead of the device tree's
$(call soong_config_set,mainline_common_libinit,set_properties_from,none)

# Kernel
PRODUCT_OTA_ENFORCE_VINTF_KERNEL_REQUIREMENTS := false

# Memory: ashmem through memfd. vendor_init cannot set sys.use_memfd.
PRODUCT_SYSTEM_PROPERTIES += \
    persist.sys.zram_enabled=1 \
    sys.use_memfd=true

# Shipping API level
PRODUCT_SHIPPING_API_LEVEL := 33

# Soong namespaces
PRODUCT_SOONG_NAMESPACES += \
    $(DEVICE_PATH)

# Storage
$(call inherit-product, $(SRC_TARGET_DIR)/product/emulated_storage.mk)

# OTA: extracted by the updater to /tmp/install
PRODUCT_COPY_FILES += \
    $(DEVICE_PATH)/ota/gtaxl-ota.sh:install/bin/gtaxl-ota.sh
