/*
 * SPDX-FileCopyrightText: 2026 The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <android/binder_auto_utils.h>

namespace gtaxlwifi::touch {

// Boolean stmfts sysfs attributes backing the vendor.lineage.touch interfaces
bool exists(const char* path);
::ndk::ScopedAStatus readFlag(const char* path, bool* out);
::ndk::ScopedAStatus writeFlag(const char* path, bool value);

// The touchscreen is always i2c-2 0x49; fixed paths also match genfs_contexts
inline constexpr const char* kKeySwapPath =
        "/sys/devices/platform/soc@0/13840000.i2c/i2c-2/2-0049/key_swap";
inline constexpr const char* kKeyDisablePath =
        "/sys/devices/platform/soc@0/13840000.i2c/i2c-2/2-0049/key_disable";

}  // namespace gtaxlwifi::touch
