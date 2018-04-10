/*=============================================================================
  Copyright (c) 2016 Qualcomm Technologies, Inc.
  All Rights Reserved.
  Confidential and Proprietary - Qualcomm Technologies, Inc.
  =============================================================================*/

#include <izat_remote_api.h>
#include <IzatRemoteApi.h>

using namespace izat_remote_api;

// ======== LocationUpdater ========
class LocationUpdaterWrapper : public LocationUpdater {
    const locationUpdateCb mLocationCb;
    const void* mClientData;
public:
    inline LocationUpdaterWrapper(locationUpdateCb locCb, void* clientData) :
        LocationUpdater(), mLocationCb(locCb), mClientData(clientData) {
    }
    inline virtual void locationUpdate(UlpLocation& location, GpsLocationExtended& locExtended) override {
        mLocationCb(&location, &locExtended, (void*)mClientData);
    }
};


void* registerLocationUpdater(locationUpdateCb locationCb, void* clientData) {
    return (locationCb && clientData) ?
        new LocationUpdaterWrapper(locationCb, clientData) :
        NULL;
}

void unregisterLocationUpdater(void* locationUpdaterHandle) {
    if (locationUpdaterHandle) {
        delete (LocationUpdaterWrapper*)locationUpdaterHandle;
    }
}


// ======== SstpUpdater ========
class SstpUpdaterWrapper : public SstpUpdater {
    const sstpSiteUpdateCb mSiteCb;
    const sstpMccUpdateCb mMccCb;
    const errReportCb mErrCb;
    const void* mClientData;
public:
    inline SstpUpdaterWrapper(sstpSiteUpdateCb siteCb, sstpMccUpdateCb mccCb,
                              errReportCb errCb, void* clientData) :
        SstpUpdater(), mSiteCb(siteCb), mMccCb(mccCb), mErrCb(errCb),
        mClientData(clientData) {
    }
    inline virtual void errReport(const char* errStr) override {
        if (mErrCb != nullptr) mErrCb(errStr, (void*)mClientData);
    }
    inline virtual void siteUpdate(const char* name, double lat, double lon,
                                   float unc, int32_t uncConfidence) override {
        mSiteCb(name, lat, lon, unc, uncConfidence, (void*)mClientData);
    }
    inline virtual void mccUpdate(uint32_t mcc, const char* confidence) override {
        mMccCb(mcc, confidence, (void*)mClientData);
    }
};

void* registerSstpUpdater(sstpSiteUpdateCb siteCb, sstpMccUpdateCb mccCb,
                          errReportCb errCb, void* clientData) {
    return (siteCb && mccCb) ?
        new SstpUpdaterWrapper(siteCb, mccCb, errCb, clientData) :
        NULL;
}

void unregisterSstpUpdater(void* sstpUpdaterHandle) {
    if (sstpUpdaterHandle) {
        delete (SstpUpdaterWrapper*)sstpUpdaterHandle;
    }
}

void stopSstpUpdate(void* sstpUpdaterHandle) {
    if (sstpUpdaterHandle) {
        ((SstpUpdaterWrapper*)sstpUpdaterHandle)->stop();
    }
}
