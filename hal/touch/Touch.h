/*
 * SPDX-FileCopyrightText: 2026 The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <aidl/vendor/lineage/touch/BnKeyDisabler.h>
#include <aidl/vendor/lineage/touch/BnKeySwapper.h>

namespace aidl::vendor::lineage::touch {

// Swaps the keycodes of the two capacitive keys
class KeySwapper : public BnKeySwapper {
  public:
    ::ndk::ScopedAStatus getEnabled(bool* _aidl_return) override;
    ::ndk::ScopedAStatus setEnabled(bool enabled) override;
};

// Stops the controller scanning the capacitive keys; home is on gpio-keys
class KeyDisabler : public BnKeyDisabler {
  public:
    ::ndk::ScopedAStatus getEnabled(bool* _aidl_return) override;
    ::ndk::ScopedAStatus setEnabled(bool enabled) override;
};

}  // namespace aidl::vendor::lineage::touch
