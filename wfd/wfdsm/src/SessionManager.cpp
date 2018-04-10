/*==============================================================================
*        @file SessionManager.cpp
*
*  @par DESCRIPTION:
*        SessionManager class.
*
*
*  Copyright (c) 2012-2016 Qualcomm Technologies, Inc.
*  All Rights Reserved.
*  Confidential and Proprietary - Qualcomm Technologies, Inc.
==============================================================================*/

#ifdef LOG_TAG
#undef LOG_TAG
#endif

#define LOG_NDEBUG 0
#define LOG_NDDEBUG 0

#include <utils/Log.h>
#ifndef WFD_ICS
#include <common_log.h>
#endif
#include "SessionManager.h"
#include "wfd_netutils.h"
#include "wifidisplay.h"
#include "Device.h"
#include "WFDSession.h"
#include "RTSPSession.h"
#include "MMAdaptor.h"
#include "UIBCAdaptor.h"
#include "MMCapability.h"


SessionManager* SessionManager::uniqueInstance = NULL;

SessionManager* SessionManager::Instance() {
    if (uniqueInstance == NULL) {
        uniqueInstance = new SessionManager();
    }
    return uniqueInstance;
}

void SessionManager::DeleteInstance() {
  if (uniqueInstance) {
     delete uniqueInstance;
     uniqueInstance = NULL;
  }
}

SessionManager::SessionManager() :
    pMyDevice(NULL) {
    rtspSessionId = -1;
    serviceState = DISABLED;
}

SessionManager::~SessionManager() {

    if (pMyDevice) {
      delete pMyDevice;
    }
    pMyDevice = NULL;
    vector<WFDSession*>::iterator it;
    for (it=vecWFDSessions.begin(); it!=vecWFDSessions.end(); it++) {
        if (*it != NULL)
          delete(*it);
    }
}


/**
 * Function to enable WFD service.  It queries the
 * local wifi info from WLAN module, then configures myDevice.
 * @param devType
 */
void SessionManager::enableWfd(Device *thisDevice) {
    if (pMyDevice) {
        delete pMyDevice;
        pMyDevice = NULL;
    }

    pMyDevice = thisDevice;

    //Setting the UIBC Device Type
    UIBCAdaptor::setDeviceType(thisDevice->getDeviceType());
    UIBCAdaptor::createUibcInterface();

    if(thisDevice->getDeviceType() != SOURCE)
    {
        // configure local device info
        MMAdaptor::updateLocalMMCapability(pMyDevice->pMMCapability, thisDevice->getDeviceType());
        if(pMyDevice->pMMCapability->m_nAVFormatCount <= 0)
        {
            if(pMyDevice->pMMCapability->pExtendedCapability)
            {
                MM_MSG_PRIO(MM_GENERAL,MM_PRIO_ERROR,"enableWfd : Freeing Ext Capability - Local");
                if(pMyDevice->pMMCapability->pExtendedCapability->extended_video_config.extended_video_config.codec != NULL)
                {
                    MM_Free(pMyDevice->pMMCapability->pExtendedCapability->extended_video_config.extended_video_config.codec);
                    pMyDevice->pMMCapability->pExtendedCapability->extended_video_config.extended_video_config.codec = NULL;
                    pMyDevice->pMMCapability->pExtendedCapability->extended_video_config.extended_video_config.num_codec = 0;
                }
                MM_Free(pMyDevice->pMMCapability->pExtendedCapability);
                pMyDevice->pMMCapability->pExtendedCapability = NULL;
            }
        }
        if(pMyDevice->pMMCapability->plocalExCapability)
        {
            MM_MSG_PRIO(MM_GENERAL,MM_PRIO_ERROR,"enableWfd : dumping Source Ext Capability - Local");
            int count = pMyDevice->pMMCapability->plocalExCapability->extended_video_config.extended_video_config.num_codec;
            MM_MSG_PRIO1(MM_GENERAL,MM_PRIO_ERROR,"enableWfd : count = %d ", count);
            for(int i = 0; i < count ; i++)
            {
                if((pMyDevice->pMMCapability->plocalExCapability->extended_video_config.extended_video_config.codec + i) != NULL)
                {
                    MM_MSG_PRIO2(MM_GENERAL,MM_PRIO_ERROR,"enableWfd : Name = %s , Profile = %d",
                    pMyDevice->pMMCapability->plocalExCapability->extended_video_config.extended_video_config.codec[i].name,
                    pMyDevice->pMMCapability->plocalExCapability->extended_video_config.extended_video_config.codec[i].profile);
                    MM_MSG_PRIO3(MM_GENERAL,MM_PRIO_ERROR,"enableWfd : CEA = %x , Vesa = %x , hh = %x",
                    pMyDevice->pMMCapability->plocalExCapability->extended_video_config.extended_video_config.codec[i].supported_cea_mode,
                    pMyDevice->pMMCapability->plocalExCapability->extended_video_config.extended_video_config.codec[i].supported_vesa_mode,
                    pMyDevice->pMMCapability->plocalExCapability->extended_video_config.extended_video_config.codec[i].supported_hh_mode);
                }
            }
        }

        UIBCAdaptor::updateLocalUIBCCapability(pMyDevice->pMMCapability->pUibcCapability);
        MM_MSG_PRIO(MM_GENERAL,MM_PRIO_HIGH,"Local MM capability dump:");
        pMyDevice->pMMCapability->dump();
    }

    serviceStateTransition(ENABLED);
}



/**
 * Function to disable WFD service
 */
void SessionManager::disableWfd() {

    UIBCAdaptor::destroyUibcInterface();
    delete pMyDevice;
    pMyDevice = NULL;
    serviceStateTransition(DISABLED);
}

/**
 * Function to transition the serviceState between ENABLED and
 * DISABLED
 * @param state
 */
void SessionManager::serviceStateTransition(ServiceState newState) {
    if (serviceState != newState) {
        serviceState = newState;

        if (serviceState == ENABLED) {
            eventServiceEnabled();
        } else {
            eventServiceDisabled();
        }
    }
}

/**
 * Function to start a WFD session
 * @param peer Device info
 */
bool SessionManager::startWfdSession(WfdDevice *pPeerDevice) {

    /* store the peer mac address */
    if(pPeerDevice && pMyDevice) {
        strlcpy((char*)pMyDevice->pMMCapability->pCapability->peer_ip_addrs.device_addr1,
                        pPeerDevice->macaddress,
                        sizeof(pMyDevice->pMMCapability->pCapability->peer_ip_addrs.device_addr1));

        /* If it is SOURCE, query the network properties to decide whether to enable 4K or not */
#if 0
         if(SOURCE == pMyDevice->getDeviceType())
        {
            getNetworkProperties(pPeerDevice->macaddress,
                &pMyDevice->pMMCapability->pCapability->link_properties.nss,
                &pMyDevice->pMMCapability->pCapability->link_properties.rateflags,
                &pMyDevice->pMMCapability->pCapability->link_properties.freq,
                &pMyDevice->pMMCapability->pCapability->link_properties.enable_4k);

            /* Read configuration file again, with link properties updated */
            MMAdaptor::updateLocalMMCapability(pMyDevice->pMMCapability, pMyDevice->getDeviceType());
        }
#endif
    }
    else {
        MM_MSG_PRIO(MM_GENERAL, MM_PRIO_ERROR,
            "startWfdSession: No peer or MyDevice is NULL.");
        return false;
    }

    /* If it is SINK, Store the mac address
     * For source we can query the MAC address later */
    if(SOURCE != pMyDevice->getDeviceType())
    {
        // update local IP if it's not updated yet
        if (pPeerDevice == NULL)
        {
            MM_MSG_PRIO(MM_GENERAL, MM_PRIO_ERROR,
                  "startWfdSession: No peer to connect to");
            return false;
        }
        if (pMyDevice->ipAddr.empty())
        {
            char ip[20];
            int ret = getLocalIpAddress(ip, sizeof(ip));
            if (ret)
            {
                MM_MSG_PRIO(MM_GENERAL,MM_PRIO_ERROR,"Failed to get local IP address in startWfdSession");
                //If IP is not available for some reason here then don't bail out
                //Local IP can be retrieved at a later stage and updated
            }
            else
            {
                pMyDevice->ipAddr = ip;
                MM_MSG_PRIO1(MM_GENERAL,MM_PRIO_HIGH,"Local IP updated: %s", pMyDevice->ipAddr.c_str());
            }
        }
    }
    //string peerMacStr = string(pPeerMacAddr);
    MM_MSG_PRIO1(MM_GENERAL,MM_PRIO_HIGH,
        "Peer Device type %d =", pPeerDevice->deviceType);
    if (pPeerDevice->deviceType == SOURCE_PRIMARY_SINK){
        if ( pMyDevice->getDeviceType() == SOURCE)
            pPeerDevice->deviceType = PRIMARY_SINK;
        else if (pMyDevice->getDeviceType() == PRIMARY_SINK)
            pPeerDevice->deviceType = SOURCE;
    }
    WFDSession* pSession = new WFDSession(pMyDevice->getDeviceType(), pPeerDevice, STREAMING);
    vecWFDSessions.insert(vecWFDSessions.end(), pSession);

    // start the WFD session state machine
    if(false == pSession->start()) {
      MM_MSG_PRIO(MM_GENERAL,MM_PRIO_ERROR,"Start WFD failed");
      return false;
    }
    return true;
}

void SessionManager::stopWfdSession(int rtspId) {

  vector<WFDSession*>::iterator it;
  int cnt = 0;
  MM_MSG_PRIO(MM_GENERAL,MM_PRIO_HIGH,"StopWfdSession");
  for (it= vecWFDSessions.begin(); it!= vecWFDSessions.end(); it++,cnt++) {
      WFDSession* wfdSession = *it;
      if (wfdSession != NULL ){
         RTSPSession* rtspSession = wfdSession->getRtspSession();
        if (rtspSession && rtspSession->rtspSessionId == rtspId) {
          //Destroying session based on RTSP session ID
            delete wfdSession;
            vecWFDSessions.erase(vecWFDSessions.begin() + cnt);
            break;
        }
      }
  }
}

void SessionManager::setUserCapability(int capType, void* value) {
  MM_MSG_PRIO(MM_GENERAL,MM_PRIO_HIGH,"Setting user capability");
  if (value == NULL)
  {
    MM_MSG_PRIO(MM_GENERAL,MM_PRIO_ERROR,"Input value is NULL ");
    return;
  }
  switch (capType)
  {
  case    UIBC_Generic:
    if (strcasecmp((const char*)value,"enable") != 0) {
      userCapabilities[UIBC_Generic] = ENABLE;
    }
    else if (strcasecmp((const char*)value,"disable") !=0 )
      userCapabilities[UIBC_Generic] = DISABLE;
    break;
  case    UIBC_INPUT:
    if (strcasecmp((const char*)value,"Keyboard") != 0)
      userCapabilities[UIBC_INPUT] = ENABLE;
    else if (strcasecmp((const char*)value,"Mouse") !=0 )
      userCapabilities[UIBC_INPUT] = DISABLE;
    break;
  case    HDCP:
    if (strcasecmp((const char*)value,"enable") != 0)
      userCapabilities[HDCP] = ENABLE;
    else if (strcasecmp((const char*)value,"disable") !=0 )
      userCapabilities[HDCP] = DISABLE;
    break;
  case    FRAME_SKIPPING:
    if (strcasecmp((const char*)value,"enable") != 0)
      userCapabilities[FRAME_SKIPPING] = ENABLE;
    else if (strcasecmp((const char*)value,"disable") !=0 )
      userCapabilities[FRAME_SKIPPING] = DISABLE;
    break;
  case    STANDBY_CAP:
    if (strcasecmp((const char*)value,"enable") != 0)
      userCapabilities[STANDBY_CAP] = ENABLE;
    else if (strcasecmp((const char*)value,"disable") !=0 )
      userCapabilities[STANDBY_CAP] = DISABLE;
    break;
  case    INPUT_VIDEO:
    if (strcasecmp((const char*)value,"Protected") != 0)
      userCapabilities[INPUT_VIDEO] = ENABLE;
    else if (strcasecmp((const char*)value,"Unprotected") !=0 )
      userCapabilities[INPUT_VIDEO] = DISABLE;
    break;
  case    VIDEO_RECOVERY:
    if (strcasecmp((const char*)value,"enable") != 0)
      userCapabilities[VIDEO_RECOVERY] = ENABLE;
    else if (strcasecmp((const char*)value,"disable") !=0 )
      userCapabilities[VIDEO_RECOVERY] = DISABLE;
    break;
  case    PREFFERED_DISPLAY:
    if (strcasecmp((const char*)value,"enable") != 0)
      userCapabilities[PREFFERED_DISPLAY] = ENABLE;
    else if (strcasecmp((const char*)value,"disable") !=0 )
      userCapabilities[PREFFERED_DISPLAY] = DISABLE;
    break;
  default :
    MM_MSG_PRIO(MM_GENERAL,MM_PRIO_ERROR,"Unknown/Unsupported capability");
    break;
  }


}

void SessionManager::modifyUserCapability(int capType, MMCapability* localCap, CapabilityStatus status) {
  MM_MSG_PRIO(MM_GENERAL,MM_PRIO_HIGH,"Modifying user capability");

  switch (capType)
  {
  case    UIBC_Generic:
    if (status  == ENABLE) {
      localCap->pUibcCapability->config.category |=GENERIC ;
    }
    else if (status == DISABLE)
      localCap->pUibcCapability->config.category &= ~GENERIC ;
    break;
  case    UIBC_INPUT:
    if (status  == ENABLE)
      localCap->pUibcCapability->config.generic_input_type |= MOUSE;
    else if (status == DISABLE)
      localCap->pUibcCapability->config.generic_input_type |= KEYBOARD;
    break;
  case    HDCP:
    // TODO
    break;
  case    FRAME_SKIPPING:
    // TODO
    break;
  case    STANDBY_CAP:
    if (status == ENABLE)
      localCap->pCapability->standby_resume_support = true;
    else if (status == DISABLE )
      localCap->pCapability->standby_resume_support = false;
    break;
  case    INPUT_VIDEO:
    // TODO
    break;
  case    VIDEO_RECOVERY:
    // TODO
    break;
  case    PREFFERED_DISPLAY:
    //TODO
    break;
  default :
    MM_MSG_PRIO(MM_GENERAL,MM_PRIO_ERROR,"Unknown/Unsupported capability");
    break;
  }

}

