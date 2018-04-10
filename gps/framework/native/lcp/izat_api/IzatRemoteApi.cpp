/*====*====*====*====*====*====*====*====*====*====*====*====*====*====*====*
  Copyright (c) 2016 Qualcomm Technologies, Inc.
  All Rights Reserved.
  Confidential and Proprietary - Qualcomm Technologies, Inc.
=============================================================================*/

#include <stdint.h>
#include <IzatRemoteApi.h>
#include <mq_client/IPCMessagingProxy.h>
#include <gps_extended_c.h>
#include <IzatTypes.h>
#include <algorithm>
#include <vector>

using namespace std;
using namespace qc_loc_fw;
using namespace izat_manager;

#define POS_LOC_FLAGS                    "GPS-LOC-FLAGS"
#define POS_LATITUDE                     "LATITUDE"
#define POS_LONGITUDE                    "LONGITUDE"
#define POS_HORIZONTAL_ACCURACY          "HORIZONTAL-ACCURACY"
#define POS_ALTITUDE                     "ALTITUDE"
#define POS_UTC_TIME                     "UTC-TIME-STAMP"
#define POS_VERT_UNCERT                  "VERT-UNCERT"
#define POS_BEARING                      "BEARING"
#define POS_SPEED                        "SPEED"
#define POS_SOURCE                       "POSITION-SOURCE"

#define POS_LOC_EXT_FLAGS                "GPS-LOC-EXT-FLAGS"
#define POS_ALT_MEAN_SEA_LEVEL           "ALT-MEAN-SEA-LEVEL"
#define POS_PDOP                         "PDOP"
#define POS_HDOP                         "HDOP"
#define POS_VDOP                         "VDOP"
#define POS_MAGNETIC_DEVIATION           "MAGNETIC-DEVIATION"
#define POS_VERT_UNCERT                  "VERT-UNCERT"
#define POS_SPEED_UNCERT                 "SPEED-UNCERT"
#define POS_BEARING_UNCERT               "BEARING-UNCERT"
#define POS_HOR_RELIABILITY              "HOR-RELIABILITY"
#define POS_VERT_RELIABILITY             "VERT-RELIABILITY"
#define POS_HOR_ELIP_UNC_MAJOR           "HOR-ELIP-UNC-MAJOR"
#define POS_HOR_ELIP_UNC_MINOR           "HOR-ELIP-UNC-MINOR"
#define POS_HOR_ELIP_UNC_AZIMUTH         "HOR-ELIP-UNC-AZIMUTH"


namespace izat_remote_api {

struct OutCard {
    const char* mTo;
    OutPostcard* const mCard;
    inline OutCard(const char* to) :
        mTo(to), mCard(OutPostcard::createInstance()) {}
    inline ~OutCard() { delete mCard; }
};

class IzatNotifierProxy : public IIPCMessagingResponse {
private:
    const char* const mName;
    IPCMessagingProxy* const mIPCProxy;
    vector<IzatNotifier*> mNotifiers;
    inline IzatNotifierProxy(const char* const name, IPCMessagingProxy* ipcProxy) :
        mName(name), mIPCProxy(ipcProxy), mNotifiers() {
    }
    inline ~IzatNotifierProxy() {}
public:
    static IzatNotifierProxy* get(const char* const name) {
        IPCMessagingProxy* ipcProxy = IPCMessagingProxy::getInstance();
        IzatNotifierProxy* notifierProxy = (IzatNotifierProxy*)
                                           ipcProxy->getResponseObj(name);
        if (notifierProxy == nullptr) {
            notifierProxy = new IzatNotifierProxy(name, ipcProxy);
            ipcProxy->registerResponseObj(name, notifierProxy);
        }
        return notifierProxy;
    }
    static void drop(IzatNotifierProxy* notifierProxy) {
        if (notifierProxy->mNotifiers.size() == 0) {
            notifierProxy->mIPCProxy->unregisterResponseObj(notifierProxy->mName);
            delete notifierProxy;
        }
    }
    inline void addNotifier(IzatNotifier* notifier, const OutCard* subCard) {
        mNotifiers.push_back(notifier);
        if (subCard && mNotifiers.size() == 1) {
            mIPCProxy->sendMsg(subCard->mCard, subCard->mTo);
        }
    }
    inline void removeNotifier(IzatNotifier* notifier) {
        vector<IzatNotifier*>::iterator it =
            find(mNotifiers.begin(), mNotifiers.end(), notifier);
        if (it != mNotifiers.end()) {
            mNotifiers.erase(it);
        }
    }
    inline virtual void handleMsg(InPostcard * const in_card) final {
        for (auto const& notifier : mNotifiers) {
            notifier->handleMsg(in_card);
        }
    }
    inline void sendCard(OutPostcard* card, bool deleteCard=true) {
        if (mIPCProxy && card) {
            mIPCProxy->sendIpcMessage(card->getEncodedBuffer());
            if (deleteCard) {
                delete card;
            }
        }
    }
};


IzatNotifier::IzatNotifier(const char* const name, const OutCard* subCard) :
    mNotifierProxy(IzatNotifierProxy::get(name)) {
    mNotifierProxy->addNotifier(this, subCard);
}

IzatNotifier::~IzatNotifier() {
    mNotifierProxy->removeNotifier(this);
    IzatNotifierProxy::drop(mNotifierProxy);
}

// a static method outside LocationUpdater class.
static const char* sToTag = "TO";
static const char* sFromTag = "FROM";
static const char* sReqTag = "REQ";
static const char* sInfoTag = "INFO";

static OutCard* getLocationUpdaterSubscriptionCard() {
    const char* TO = "IZAT-MANAGER";
    OutCard* card = new OutCard(TO);
    if (nullptr != card) {
        card->mCard->init();
        card->mCard->addString(sToTag, TO);
        card->mCard->addString(sFromTag, LocationUpdater::sName);
        card->mCard->addString(sReqTag, "PASSIVE-LOCATION");
        card->mCard->addUInt16("LISTENS-TO", IZAT_STREAM_ALL);
        card->mCard->finalize();
    }
    return card;
}

const char LocationUpdater::sName[] = "LOCATION-UPDATE";
const char* const LocationUpdater::sLatTag = "LATITUDE";
const char* const LocationUpdater::sLonTag = "LONGITUDE";
const char* const LocationUpdater::sAccuracyTag = "HORIZONTAL-ACCURACY";
const OutCard* LocationUpdater::sSubscriptionCard =
    getLocationUpdaterSubscriptionCard();

void LocationUpdater::handleMsg(InPostcard * const in_msg) {
    const char* info = nullptr;
    if (in_msg != nullptr &&
        0 == in_msg->getString(sInfoTag, &info) &&
        0 == strncmp(sName, info, sizeof(sName)-1)) {
        UlpLocation ulpLoc;
        GpsLocationExtended locExtended;
        memset(&ulpLoc, 0, sizeof(ulpLoc));
        memset(&locExtended, 0, sizeof(locExtended));

        in_msg->getUInt16(POS_LOC_FLAGS, ulpLoc.gpsLocation.flags);
        in_msg->getDouble(POS_LATITUDE, ulpLoc.gpsLocation.latitude);
        in_msg->getDouble(POS_LONGITUDE, ulpLoc.gpsLocation.longitude);
        in_msg->getFloat(POS_HORIZONTAL_ACCURACY, ulpLoc.gpsLocation.accuracy);
        in_msg->getDouble(POS_ALTITUDE, ulpLoc.gpsLocation.altitude);
        in_msg->getInt64(POS_UTC_TIME, (long long&)ulpLoc.gpsLocation.timestamp);

        in_msg->getFloat(POS_BEARING, ulpLoc.gpsLocation.bearing);
        in_msg->getFloat(POS_SPEED, ulpLoc.gpsLocation.speed);
        in_msg->getUInt16(POS_SOURCE, ulpLoc.position_source);

        in_msg->getUInt16(POS_LOC_EXT_FLAGS, locExtended.flags);
        in_msg->getFloat(POS_ALT_MEAN_SEA_LEVEL, locExtended.altitudeMeanSeaLevel);
        in_msg->getFloat(POS_PDOP, locExtended.pdop);
        in_msg->getFloat(POS_HDOP, locExtended.hdop);
        in_msg->getFloat(POS_VDOP, locExtended.vdop);
        in_msg->getFloat(POS_MAGNETIC_DEVIATION, locExtended.magneticDeviation);
        in_msg->getFloat(POS_VERT_UNCERT, locExtended.vert_unc);
        in_msg->getFloat(POS_SPEED_UNCERT, locExtended.speed_unc);
        in_msg->getFloat(POS_BEARING_UNCERT, locExtended.bearing_unc);
        in_msg->getUInt8(POS_HOR_RELIABILITY, (uint8_t&)locExtended.horizontal_reliability);
        in_msg->getUInt8(POS_VERT_RELIABILITY, (uint8_t&)locExtended.vertical_reliability);
        in_msg->getFloat(POS_HOR_ELIP_UNC_MAJOR, locExtended.horUncEllipseSemiMajor);
        in_msg->getFloat(POS_HOR_ELIP_UNC_MINOR, locExtended.horUncEllipseSemiMinor);
        in_msg->getFloat(POS_HOR_ELIP_UNC_AZIMUTH, locExtended.horUncEllipseOrientAzimuth);


        locationUpdate(ulpLoc, locExtended);
    }
}

const char SstpUpdater::sName[] = "WAA-LISTENER";
const char* const SstpUpdater::sLatTag = "LATITUDE";
const char* const SstpUpdater::sLonTag = "LONGITUDE";
const char* const SstpUpdater::sUncTag = "HOR-UNC-M";
const char* const SstpUpdater::sUncConfTag = "HOR-UNC-CONFIDENCE";

void SstpUpdater::stop() {
    if (mNotifierProxy) {
        OutPostcard* card = OutPostcard::createInstance();
        if (nullptr != card) {
            card->init();
            card->addString(sToTag, "GTP-WAA");
            card->addString(sFromTag, SstpUpdater::sName );
            card->addString(sReqTag, "STOP");
            card->finalize();

            mNotifierProxy->sendCard(card);
        }
    }
}

void SstpUpdater::handleMsg(InPostcard * const in_msg) {
    const char* info = nullptr;
    const char* status = nullptr;
    const char SUCCESS[] = "MCC-DETECTION-SUCCESS";
    const char INFO[] = "MCC-AND-SITE-INFO";

    if (nullptr != in_msg &&
        0 == in_msg->getString(sInfoTag, &info) &&
        0 == in_msg->getString("STATUS", &status) &&
        0 == strncmp(INFO, info, sizeof(INFO)-1)) {
        if (0 == strncmp(status, SUCCESS, sizeof(SUCCESS)-1)) {
            uint32_t mcc = 0;
            const char* confidence = nullptr;
            in_msg->getUInt32("MCC", mcc);
            in_msg->getString("MCC-CONFIDENCE", &confidence);

            mccUpdate(mcc, confidence);

            const char sstp[] = "SSTP";
            const char* source = nullptr;
            if (0 == in_msg->getString("SOURCE", &source) &&
                0 == strncmp(source, sstp, sizeof(sstp)-1)) {
                const char* siteName = nullptr;
                double lat = 360.0, lon = 360.0;
                float unc = 0;
                int32_t uncConf = 0;
                in_msg->getString("SITE-NAME", &siteName);
                in_msg->getDouble("LATITUDE-DEG", lat);
                in_msg->getDouble("LONGITUDE-DEG", lon);
                in_msg->getFloat("HOR-UNC-M", unc);
                in_msg->getInt32("HOR-UNC-CONFIDENCE", uncConf);

                siteUpdate(siteName, lat, lon, unc, uncConf);
            }
        } else {
            errReport(status);
        }
    }
}

}
