/*
 * SPDX-FileCopyrightText: 2026 The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <aidl/android/hardware/gnss/BnGnss.h>
#include <aidl/android/hardware/gnss/BnGnssPsds.h>
#include <hardware/gps.h>

#include <mutex>

namespace aidl::android::hardware::gnss {

/*
 * IGnssPsds on top of the legacy gps-xtra extension: libgps requests LTO
 * data, the framework downloads it from the config_gnssParameters servers.
 */
class GnssPsds : public BnGnssPsds {
  public:
    explicit GnssPsds(const GpsXtraInterface* xtra);

    ndk::ScopedAStatus setCallback(const std::shared_ptr<IGnssPsdsCallback>& callback) override;
    ndk::ScopedAStatus injectPsdsData(PsdsType psdsType,
                                      const std::vector<uint8_t>& psdsData) override;

    static void requestDownload();

  private:
    static std::mutex sMutex;
    static std::shared_ptr<IGnssPsdsCallback> sCallback;

    const GpsXtraInterface* mXtra;
};

/*
 * IGnss on top of Broadcom's legacy gps.default.so (hardware/gps.h).
 *
 * Unserved extensions return EX_UNSUPPORTED_OPERATION: the framework skips
 * those, while OK with a null pointer would make it wrap and call it.
 */
class Gnss : public BnGnss {
  public:
    explicit Gnss(const GpsInterface* gps);

    ndk::ScopedAStatus setCallback(const std::shared_ptr<IGnssCallback>& callback) override;
    ndk::ScopedAStatus close() override;
    ndk::ScopedAStatus start() override;
    ndk::ScopedAStatus stop() override;
    ndk::ScopedAStatus injectTime(int64_t timeMs, int64_t timeReferenceMs,
                                  int32_t uncertaintyMs) override;
    ndk::ScopedAStatus injectLocation(const GnssLocation& location) override;
    ndk::ScopedAStatus injectBestLocation(const GnssLocation& location) override;
    ndk::ScopedAStatus deleteAidingData(GnssAidingData aidingDataFlags) override;
    ndk::ScopedAStatus setPositionMode(const PositionModeOptions& options) override;
    ndk::ScopedAStatus startSvStatus() override;
    ndk::ScopedAStatus stopSvStatus() override;
    ndk::ScopedAStatus startNmea() override;
    ndk::ScopedAStatus stopNmea() override;

    ndk::ScopedAStatus getExtensionPsds(std::shared_ptr<IGnssPsds>* _aidl_return) override;
    ndk::ScopedAStatus getExtensionGnssConfiguration(
            std::shared_ptr<IGnssConfiguration>* _aidl_return) override;
    ndk::ScopedAStatus getExtensionGnssMeasurement(
            std::shared_ptr<IGnssMeasurementInterface>* _aidl_return) override;
    ndk::ScopedAStatus getExtensionGnssPowerIndication(
            std::shared_ptr<IGnssPowerIndication>* _aidl_return) override;
    ndk::ScopedAStatus getExtensionGnssBatching(
            std::shared_ptr<IGnssBatching>* _aidl_return) override;
    ndk::ScopedAStatus getExtensionGnssGeofence(
            std::shared_ptr<IGnssGeofence>* _aidl_return) override;
    ndk::ScopedAStatus getExtensionGnssNavigationMessage(
            std::shared_ptr<IGnssNavigationMessageInterface>* _aidl_return) override;
    ndk::ScopedAStatus getExtensionAGnss(std::shared_ptr<IAGnss>* _aidl_return) override;
    ndk::ScopedAStatus getExtensionAGnssRil(std::shared_ptr<IAGnssRil>* _aidl_return) override;
    ndk::ScopedAStatus getExtensionGnssDebug(std::shared_ptr<IGnssDebug>* _aidl_return) override;
    ndk::ScopedAStatus getExtensionGnssVisibilityControl(
            std::shared_ptr<visibility_control::IGnssVisibilityControl>* _aidl_return) override;
    ndk::ScopedAStatus getExtensionGnssAntennaInfo(
            std::shared_ptr<IGnssAntennaInfo>* _aidl_return) override;
    ndk::ScopedAStatus getExtensionMeasurementCorrections(
            std::shared_ptr<measurement_corrections::IMeasurementCorrectionsInterface>*
                    _aidl_return) override;
    ndk::ScopedAStatus getExtensionGnssAssistanceInterface(
            std::shared_ptr<gnss_assistance::IGnssAssistanceInterface>* _aidl_return) override;

  private:
    // gps.h callbacks carry no context, so their state is static
    static void locationCb(GpsLocation* location);
    static void statusCb(GpsStatus* status);
    static void svStatusCb(GpsSvStatus* svStatus);
    static void gnssSvStatusCb(GnssSvStatus* svStatus);
    static void nmeaCb(GpsUtcTime timestamp, const char* nmea, int length);
    static void setCapabilitiesCb(uint32_t capabilities);
    static void acquireWakelockCb();
    static void releaseWakelockCb();
    static pthread_t createThreadCb(const char* name, void (*start)(void*), void* arg);
    static void requestUtcTimeCb();
    static void setSystemInfoCb(const ::GnssSystemInfo* info);

    // libgps calls these extensions on its own: initialise them even if the
    // framework never asks, or it dereferences a null callback.
    void initLegacyExtensions();
    static void agpsRilRequestSetIdCb(uint32_t flags);
    static void agpsRilRequestRefLocCb(uint32_t flags);
    static void agpsStatusCb(AGpsStatus* status);
    static void niNotifyCb(GpsNiNotification* notification);
    static void xtraDownloadRequestCb();

    static std::shared_ptr<IGnssCallback> callback();
    static void reportCapabilitiesAndInfo(const std::shared_ptr<IGnssCallback>& cb);

    static GpsCallbacks sGpsCallbacks;
    static AGpsRilCallbacks sAGpsRilCallbacks;
    static AGpsCallbacks sAGpsCallbacks;
    static GpsNiCallbacks sGpsNiCallbacks;
    static GpsXtraCallbacks sGpsXtraCallbacks;
    static const AGpsRilInterface* sAGpsRil;
    static std::mutex sMutex;
    static std::shared_ptr<IGnssCallback> sCallback;
    static int32_t sCapabilities;
    static int32_t sYearOfHw;
    static bool sSvStatusEnabled;
    static bool sNmeaEnabled;

    const GpsInterface* mGps;
    std::shared_ptr<GnssPsds> mPsds;
    bool mInitialized = false;
    std::mutex mInitMutex;
};

}  // namespace aidl::android::hardware::gnss
