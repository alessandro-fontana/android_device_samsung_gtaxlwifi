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

# Camera
PRODUCT_PACKAGES += \
    android.hardware.camera.flash-autofocus.prebuilt.xml \
    gtaxlwifi-camera-features.xml

# Media: the example v4l2_codec2 configuration declares 4K and secure codecs
# the 7870 MFC does not have. media_codecs_c2.xml is installed by the
# ffmpeg Codec2 HAL of mainline/common, so the list takes the default name.
TARGET_V4L2_CODEC2_USE_EXAMPLE_CONFIGURATION := false
PRODUCT_COPY_FILES += \
    $(DEVICE_PATH)/configs/media/media_codecs.xml:$(TARGET_COPY_OUT_VENDOR)/etc/media_codecs.xml \
    $(DEVICE_PATH)/configs/media/media_profiles_V1_0.xml:$(TARGET_COPY_OUT_VENDOR)/etc/media_profiles_V1_0.xml
# Pool mask: the example one plus GRALLOC (bit 17). Basic pool blocks have
# no stable id, so v4l2_codec2 cannot bind them to V4L2 buffers.
PRODUCT_VENDOR_PROPERTIES += \
    ro.vendor.v4l2_codec2.decode_concurrent_instances=4 \
    ro.vendor.v4l2_codec2.encode_concurrent_instances=4 \
    debug.stagefright.c2-poolmask=0xf70000
# Every decoder input buffer is mapped whole into the MFC's 256 MB IOVA
# space: 16 MB ones (the default) do not fit, 3 MB is the MFC's own stream
# buffer for 1080p.
PRODUCT_VENDOR_PROPERTIES += \
    ro.vendor.v4l2_codec2.decoder.input_buffer_size=3145728
# Each codec must also be listed in media_codecs.xml. The 7870 MFC
# firmware has no VP9.
PRODUCT_VENDOR_PROPERTIES += \
    ro.vendor.v4l2_codec2.decoder.supported.h263=true \
    ro.vendor.v4l2_codec2.decoder.supported.h264=true \
    ro.vendor.v4l2_codec2.decoder.supported.hevc=true \
    ro.vendor.v4l2_codec2.decoder.supported.mpeg4=true \
    ro.vendor.v4l2_codec2.decoder.supported.vp8=true \
    ro.vendor.v4l2_codec2.encoder.supported.h263=true \
    ro.vendor.v4l2_codec2.encoder.supported.h264=true \
    ro.vendor.v4l2_codec2.encoder.supported.hevc=true \
    ro.vendor.v4l2_codec2.encoder.supported.mpeg4=true \
    ro.vendor.v4l2_codec2.encoder.supported.vp8=true

# Camera metadata FMQ sizes (the HAL defaults): without them the HAL reads
# ro.camera.*, which a vendor service cannot access.
PRODUCT_VENDOR_PROPERTIES += \
    ro.vendor.camera.req.fmq.size=1048576 \
    ro.vendor.camera.res.fmq.size=1048576

# Restart the codec and camera services when the graphics allocator restarts
PRODUCT_COPY_FILES += \
    $(DEVICE_PATH)/rootdir/etc/init/gtaxl-allocator-restart.rc:$(TARGET_COPY_OUT_VENDOR)/etc/init/gtaxl-allocator-restart.rc

# Camera tuning for the libcamera software ISP
PRODUCT_PACKAGES += \
    libcamera_ipa_simple_uncalibrated.yaml \
    libcamera_ipa_simple_s5k4h5yc.yaml \
    libcamera_ipa_simple_sr259.yaml

# Proprietary
$(call inherit-product, vendor/samsung/gtaxlwifi/gtaxlwifi-vendor.mk)

# Characteristics
PRODUCT_CHARACTERISTICS := tablet

# Audio: TinyHAL opens /vendor/etc/audio.<ro.product.vendor.device>.xml
PRODUCT_COPY_FILES += \
    $(DEVICE_PATH)/configs/audio/audio.gtaxlwifi.xml:$(TARGET_COPY_OUT_VENDOR)/etc/audio.gtaxlwifi.xml \
    $(DEVICE_PATH)/configs/audio/primary_audio_policy_configuration.xml:$(TARGET_COPY_OUT_VENDOR)/etc/primary_audio_policy_configuration.xml

# Wi-Fi PNO with the screen off
PRODUCT_PACKAGES += \
    GtaxlWifiOverlay

# ueventd: imported by ueventd.mainline.rc
PRODUCT_COPY_FILES += \
    $(DEVICE_PATH)/configs/init/ueventd.gtaxlwifi.rc:$(TARGET_COPY_OUT_VENDOR)/etc/ueventd.gtaxlwifi.rc

# Recovery: a larger /tmp for the addon backup, and an in-RAM misc (the
# factory partition table has none)
PRODUCT_COPY_FILES += \
    $(DEVICE_PATH)/rootdir/etc/init/gtaxl-recovery.rc:$(TARGET_COPY_OUT_RECOVERY)/root/system/etc/init/gtaxl-recovery.rc \
    $(DEVICE_PATH)/rootdir/etc/gtaxl-misc.sh:$(TARGET_COPY_OUT_RECOVERY)/root/system/etc/gtaxl-misc.sh

# Wi-Fi
PRODUCT_PACKAGES += \
    android.hardware.wifi-service \
    libwifi-hal-gtaxl

# Wi-Fi Direct: wpa_supplicant creates the nl80211 P2P_DEVICE itself
PRODUCT_VENDOR_PROPERTIES += \
    wifi.direct.interface=p2p-dev-wlan0

# Bluetooth: the QCA9377 answers APCF READ_EXTENDED_FEATURES with the bare
# OCF and the stack aborts. Basic APCF works.
PRODUCT_VENDOR_PROPERTIES += \
    bluetooth.le.disable_apcf_extended_features=1

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

# Fstab
PRODUCT_COPY_FILES += \
    $(DEVICE_PATH)/configs/fstab/fstab.gtaxlwifi:$(TARGET_COPY_OUT_RAMDISK)/fstab.gtaxlwifi \
    $(DEVICE_PATH)/configs/fstab/fstab.gtaxlwifi:$(TARGET_COPY_OUT_VENDOR)/etc/fstab.gtaxlwifi

# Init
PRODUCT_COPY_FILES += \
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

# Wi-Fi: cfg80211 loads the regulatory database from the ramdisk
PRODUCT_COPY_FILES += \
    external/linux-firmware-mainline/wireless-regdb/regulatory.db:$(TARGET_COPY_OUT_RAMDISK)/lib/firmware/regulatory.db \
    external/linux-firmware-mainline/wireless-regdb/regulatory.db.p7s:$(TARGET_COPY_OUT_RAMDISK)/lib/firmware/regulatory.db.p7s
PRODUCT_COPY_FILES += \
    external/wpa_supplicant_8/wpa_supplicant/wpa_supplicant_template.conf:$(TARGET_COPY_OUT_VENDOR)/etc/wifi/wpa_supplicant.conf

# OTA: extracted by the updater to /tmp/install
PRODUCT_COPY_FILES += \
    $(DEVICE_PATH)/ota/gtaxl-ota.sh:install/bin/gtaxl-ota.sh
