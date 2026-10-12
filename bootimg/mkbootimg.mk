#
# SPDX-FileCopyrightText: 2026 The LineageOS Project
# SPDX-License-Identifier: Apache-2.0
#
# S-BOOT ignores the boot image command line and rejects ramdisks above
# 14 MiB, so the image "kernel" is bootloader/loader, followed by the LZ4
# kernel, the DTB, the ramdisk and the command line (BoardConfig.mk).

GTAXL_BOOTIMG := $(DEVICE_PATH)/bootimg/gtaxl_bootimg.py
GTAXL_LOADER_DIR := $(DEVICE_PATH)/bootloader/loader
GTAXL_LOADER_SRCS := $(wildcard $(GTAXL_LOADER_DIR)/*.c $(GTAXL_LOADER_DIR)/*.S \
	$(GTAXL_LOADER_DIR)/*.h $(GTAXL_LOADER_DIR)/include/*.h) \
	$(GTAXL_LOADER_DIR)/linker.lds $(GTAXL_LOADER_DIR)/build.sh
GTAXL_LOADER := $(call intermediates-dir-for,PACKAGING,gtaxl_loader)/loader.bin
GTAXL_DT_DIR := $(call intermediates-dir-for,PACKAGING,gtaxl_dt)
GTAXL_SAMSUNG_DT := $(GTAXL_DT_DIR)/stub-dt.img
GTAXL_CMDLINE := $(GTAXL_DT_DIR)/cmdline.txt
GTAXL_RECOVERY_CMDLINE_FILE := $(GTAXL_DT_DIR)/recovery-cmdline.txt
GTAXL_DTC := $(HOST_OUT_EXECUTABLES)/dtc
# KERNEL_OUT is not defined yet at this point
GTAXL_DTB := $(TARGET_OUT_INTERMEDIATES)/KERNEL_OBJ/arch/arm64/boot/dts/exynos/exynos7870-gtaxlwifi-decon.dtb
GTAXL_LZ4 := $(HOST_OUT_EXECUTABLES)/lz4
# The kernel build writes the DTB along with the Image: declare it, so that
# a device tree change alone rebuilds the images
GTAXL_KERNEL_IMAGE := $(TARGET_OUT_INTERMEDIATES)/KERNEL_OBJ/arch/arm64/boot/$(BOARD_KERNEL_IMAGE_NAME)
$(GTAXL_DTB): $(GTAXL_KERNEL_IMAGE)
	@true
# INSTALLED_RAMDISK_TARGET is defined after this file is included and would
# expand to nothing as a prerequisite.
GTAXL_RAMDISK := $(PRODUCT_OUT)/ramdisk.img

# The DT table S-BOOT requires next to the kernel, from bootloader/stub.dts
$(GTAXL_SAMSUNG_DT): $(DEVICE_PATH)/bootloader/stub.dts $(GTAXL_BOOTIMG) $(GTAXL_DTC)
	$(hide) mkdir -p $(dir $@)
	$(hide) $(GTAXL_DTC) -I dts -O dtb -o $(GTAXL_DT_DIR)/stub.dtb $<
	$(hide) python3 $(GTAXL_BOOTIMG) dtbh --dtb $(GTAXL_DT_DIR)/stub.dtb --output $@

# ninja runs these again whenever the command, and so the line, changes
$(GTAXL_CMDLINE): PRIVATE_CMDLINE := $(strip $(BOARD_KERNEL_CMDLINE))
$(GTAXL_RECOVERY_CMDLINE_FILE): PRIVATE_CMDLINE := $(strip $(GTAXL_RECOVERY_CMDLINE))
$(GTAXL_CMDLINE) $(GTAXL_RECOVERY_CMDLINE_FILE):
	$(hide) mkdir -p $(dir $@)
	$(hide) echo '$(PRIVATE_CMDLINE)' > $@

$(GTAXL_LOADER): $(GTAXL_LOADER_SRCS)
	$(call pretty,"Target gtaxlwifi loader: $@")
	$(hide) CLANG_DIR=$(TARGET_KERNEL_CLANG_PATH) $(GTAXL_LOADER_DIR)/build.sh $@ $(BUILD_TOP)

# boot: vbmeta includes the descriptor of the AVB footer, unsigned; S-BOOT
# reads only the part the header describes.
$(INSTALLED_BOOTIMAGE_TARGET): $(GTAXL_BOOTIMG) $(GTAXL_LOADER) $(INSTALLED_KERNEL_TARGET) \
		$(GTAXL_RAMDISK) $(GTAXL_DTB) $(GTAXL_CMDLINE) \
		$(GTAXL_SAMSUNG_DT) $(GTAXL_LZ4) $(AVBTOOL)
	$(call pretty,"Target boot image: $@")
	$(hide) python3 $(GTAXL_BOOTIMG) loader --loader $(GTAXL_LOADER) \
		--kernel $(INSTALLED_KERNEL_TARGET) --dtb $(GTAXL_DTB) \
		--ramdisk $(GTAXL_RAMDISK) \
		--cmdline $(GTAXL_CMDLINE) --dt $(GTAXL_SAMSUNG_DT) \
		--lz4 $(GTAXL_LZ4) --output $@
	$(hide) $(AVBTOOL) add_hash_footer --image $@ \
		--partition_size $(BOARD_BOOTIMAGE_PARTITION_SIZE) --partition_name boot

# recovery: the script checks the real 38 MiB partition limit
$(INSTALLED_RECOVERYIMAGE_TARGET): $(GTAXL_BOOTIMG) $(GTAXL_LOADER) $(INSTALLED_KERNEL_TARGET) \
		$(recovery_ramdisk) $(GTAXL_DTB) $(GTAXL_RECOVERY_CMDLINE_FILE) \
		$(GTAXL_SAMSUNG_DT) $(GTAXL_LZ4)
	$(call pretty,"Target recovery image: $@")
	$(hide) python3 $(GTAXL_BOOTIMG) loader --recovery --loader $(GTAXL_LOADER) \
		--kernel $(INSTALLED_KERNEL_TARGET) --dtb $(GTAXL_DTB) \
		--ramdisk $(recovery_ramdisk) \
		--cmdline $(GTAXL_RECOVERY_CMDLINE_FILE) --dt $(GTAXL_SAMSUNG_DT) \
		--lz4 $(GTAXL_LZ4) --output $@
