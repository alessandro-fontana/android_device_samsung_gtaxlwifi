/*
 * SPDX-FileCopyrightText: 2026 The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#include <android-base/logging.h>
#include <android/binder_interface_utils.h>
#include <health-impl/ChargerUtils.h>
#include <health/utils.h>

#include "GtaxlHealth.h"

using aidl::android::hardware::health::GtaxlChargerCallback;
using aidl::android::hardware::health::GtaxlHealth;
using aidl::android::hardware::health::HalHealthLoop;
using aidl::android::hardware::health::charger::ChargerModeMain;

static constexpr const char* gInstanceName = "default";
static constexpr std::string_view gChargerArg{"--charger"};

int main(int argc, char** argv) {
    const bool charger = argc >= 2 && argv[1] == gChargerArg;

    // logd is not running in charger mode
    if (charger) android::base::InitLogging(argv, android::base::KernelLogger);

    auto config = std::make_unique<healthd_config>();
    ::android::hardware::health::InitHealthdConfig(config.get());
    auto binder = ndk::SharedRefBase::make<GtaxlHealth>(gInstanceName, std::move(config));

    if (charger) {
        return ChargerModeMain(binder, std::make_shared<GtaxlChargerCallback>(binder));
    }

    LOG(INFO) << "Starting health HAL.";
    auto hal_health_loop = std::make_shared<HalHealthLoop>(binder, binder);
    return hal_health_loop->StartLoop();
}
