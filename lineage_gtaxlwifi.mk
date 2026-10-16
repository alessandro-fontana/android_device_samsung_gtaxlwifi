#
# SPDX-FileCopyrightText: 2026 The LineageOS Project
# SPDX-License-Identifier: Apache-2.0
#

# Inherit from those products. Most specific first.
$(call inherit-product, $(SRC_TARGET_DIR)/product/core_64_bit_only.mk)
$(call inherit-product, $(SRC_TARGET_DIR)/product/full_base.mk)
$(call inherit-product, $(SRC_TARGET_DIR)/product/non_ab_device.mk)

# Inherit some common Lineage stuff.
$(call inherit-product, vendor/lineage/config/common_full_tablet_wifionly.mk)

# Inherit from device
$(call inherit-product, device/samsung/gtaxlwifi/device.mk)

PRODUCT_NAME := lineage_gtaxlwifi
PRODUCT_DEVICE := gtaxlwifi
PRODUCT_BRAND := samsung
PRODUCT_MANUFACTURER := Samsung
# The model code, as on stock: Google's device catalogue is keyed on it
PRODUCT_MODEL := SM-T580
