/*
 * SPDX-FileCopyrightText: 2026 The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <health-impl/ChargerUtils.h>
#include <health-impl/Health.h>

#include <chrono>
#include <optional>

namespace aidl::android::hardware::health {

/*
 * The AOSP default health HAL, plus a debounce of spurious cable unplugs in
 * charger mode: "power present" flaps for up to ~2.5 s early in charger
 * mode, which made the charger wake the screen and arm the power-off.
 * Only the charger's view (ChargerIsOnline) is filtered; HealthInfo is not.
 */
class GtaxlHealth : public Health {
  public:
    using Health::Health;

    // An unplug counts only after kUnpluggedDebounce without power.
    bool DebouncedChargerOnline(bool online);

    // No event arrives when the debounce expires: wake up for it.
    int OnPrepareToWait() override;

  protected:
    // sm5703-charger has no current_max; input_current_limit (set from the
    // port type) is the equivalent. SystemUI uses it for "charging slowly".
    void UpdateHealthInfo(HealthInfo* health_info) override;

  private:
    using Clock = std::chrono::steady_clock;

    bool seen_online_ = false;
    std::optional<Clock::time_point> unplugged_since_;
};

class GtaxlChargerCallback : public charger::ChargerCallback {
  public:
    explicit GtaxlChargerCallback(const std::shared_ptr<GtaxlHealth>& service)
        : ChargerCallback(service), service_(service) {}

    bool ChargerIsOnline() override;

  private:
    std::shared_ptr<GtaxlHealth> service_;
};

}  // namespace aidl::android::hardware::health
