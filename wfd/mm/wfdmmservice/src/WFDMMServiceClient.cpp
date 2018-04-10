/*==============================================================================
*       WFDMMServiceClient.cpp
*
*  DESCRIPTION:
*       WFDMMServiceClient class implementation
*
*
*  Copyright (c) 2015 Qualcomm Technologies, Inc. All Rights Reserved.
*  Qualcomm Technologies Proprietary and Confidential.
*===============================================================================
*/
/*==============================================================================
                             Edit History
================================================================================
   When            Who           Why
-----------------  ------------  -----------------------------------------------
06/09/2014         SK            InitialDraft
================================================================================
*/

/*==============================================================================
**               Includes and Public Data Declarations
**==============================================================================
*/

/* =============================================================================

                     INCLUDE FILES FOR MODULE

================================================================================
*/

#include "IWFDMMService.h"
#include "WFDMMServiceClient.h"
#include "WFDMMLogs.h"
#include "MMMemory.h"
#include <binder/IServiceManager.h>
#ifndef WFD_ICS
#include "common_log.h"
#endif

static sp<IWFDMMService> gWFDMMServiceInstance;
sp<WFDMMServiceClient::WFDMMServiceDeathRecepient> pDeathNotifier;

extern "C" int isDSModeActive()
{
    return (WFDMMServiceClient::isSessionActive() &&
            WFDMMServiceClient::isStreamingFeatureEnabled() &&
            WFDMMServiceClient::isAppWhiteListed()) ? 1 : 0;
}


#define LOCK_MUTEX do {\
    if(pthread_mutex_lock(&sWFDMMSvcClientLock))\
    {\
        WFDMMLOGE1("Failed to acquire mutex due to %s", strerror(errno));\
    }\
}while(0);


#define UNLOCK_MUTEX do {\
    if(pthread_mutex_unlock(&sWFDMMSvcClientLock))\
    {\
        WFDMMLOGE1("Failed to acquire mutex due to %s", strerror(errno));\
    }\
}while(0);


/*------------------------------------------------------------------------------
 Critical Sections
--------------------------------------------------------------------------------
*/
#ifdef PTHREAD_RECURSIVE_MUTEX_INITIALIZER
static pthread_mutex_t sWFDMMSvcClientLock =
                                PTHREAD_RECURSIVE_MUTEX_INITIALIZER;
#else
static pthread_mutex_t sWFDMMSvcClientLock =
                                PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;
#endif

class WFDMMServiceClientLock
{
public:
    WFDMMServiceClientLock(){LOCK_MUTEX;};
    ~WFDMMServiceClientLock(){UNLOCK_MUTEX;};
};



/*==============================================================================

    FUNCTION:          WFDMMServiceClient

    DESCRIPTION:
*//**       @brief     Ctor for WFDMMServiceClient
*//**
@par     DEPENDENCIES:
                       None
*//*
    PARAMETERS:
*//**       @param     pUserdata
                       NotifyCbType pCb

*//*     RETURN VALUE:
*//**       @return
                       None
@par     SIDE EFFECTS:
                       None
*//*==========================================================================*/

WFDMMServiceClient::WFDMMServiceClient
(
    void* pUserdata,
    NotifyCbType pCb
)
{
    WFDMMLOGH2("WFDMMServiceClient ctor Context = %p, Cb = %p", pUserdata,pCb);
    if(pCb != NULL)
    {
        mpAppData = pUserdata;
        mpNotify = pCb;
    }
}

/*==============================================================================

         FUNCTION:          ~WFDMMServiceClient

         DESCRIPTION:
*//**       @brief
*//**
@par     DEPENDENCIES:
                            None
*//*
         PARAMETERS:
*//**       @param          None

*//*     RETURN VALUE:
*//**       @return
                            None
@par     SIFE EFFECTS:
                            None
*//*==========================================================================*/
WFDMMServiceClient::~WFDMMServiceClient()
{
    WFDMMLOGE("WFDMMServiceClient::~WFDMMServiceClient");
}

/*==============================================================================
         FUNCTION:          getWFDMMServiceInstance

         DESCRIPTION:
*//**       @brief          get an IWFDMMService Instance object
*//**
@par     DEPENDENCIES:
                            None
*//*
         PARAMETERS:
*//**       @param          None

*//*     RETURN VALUE:
*//**       @return
                            IWFDMMService - WFDMMService binder object
@par     SIDE EFFECTS:
                            None
*//*==========================================================================*/
IWFDMMService* WFDMMServiceClient::getWFDMMServiceInstance()
{
    if (gWFDMMServiceInstance.get() == NULL)
    {
        WFDMMLOGD("getWFDMMServiceInstance get new Instance");
        sp<IServiceManager> pSM = defaultServiceManager();

        sp<IBinder> pBinder;


        if(pSM->checkService(String16("wfd.native.mm.service"))
                                           == NULL)
        {
            WFDMMLOGE("Service not available yet");
            return NULL;
        }

        pBinder = pSM->getService(String16("wfd.native.mm.service"));

        if(pBinder == 0)
        {
            WFDMMLOGE("Service not available yet");
            return NULL;
        }

        if(pDeathNotifier.get() != NULL)
        {
            pDeathNotifier.clear();
            pDeathNotifier = NULL;
        }

        if (pDeathNotifier.get() == NULL)
        {
            pDeathNotifier = MM_New(WFDMMServiceDeathRecepient);
            if (pDeathNotifier != 0)
            {
                pBinder->linkToDeath(pDeathNotifier);
            }
        }

        gWFDMMServiceInstance = interface_cast<IWFDMMService>(pBinder);
    }

    return gWFDMMServiceInstance.get();
}

/*==============================================================================

        FUNCTION:          isSessionActive

        DESCRIPTION:
*//**       @brief
*//**
@par     DEPENDENCIES:
                           None
*//*
        PARAMETERS:
*//**       @param         None

*//*     RETURN VALUE:
*//**       @return
                           true for success else failure
@par     SIFE EFFECTS:
                           None
*//*==========================================================================*/
bool WFDMMServiceClient::isSessionActive()
{
    sp<IServiceManager> pSM = defaultServiceManager();

    if(pSM->checkService(String16("wfd.native.mm.service"))
                                       == NULL)
    {
        WFDMMLOGE("Service not available yet");
        return false;
    }

    return true;
}

/*==============================================================================

        FUNCTION:       isAppWhiteListed

        DESCRIPTION:
*//**       @brief      This function checks if application is present in
                        White List.
*//**
@par    DEPENDENCIES:
                        None
*//*
        PARAMETERS:
*//**       @param      None

*//*    RETURN VALUE:
*//**       @return
                        Bool
@par    SIDE EFFECTS:
                        None
*//*==========================================================================*/

bool WFDMMServiceClient::isAppWhiteListed()
{
    IWFDMMService *pWFDMMService  = getWFDMMServiceInstance();

    if (pWFDMMService)
    {
        return (pWFDMMService->isAppWhiteListed() == 1 ? true : false);
    }

    return false;
}

/*==============================================================================

        FUNCTION:isStreamingFeatureEnabled

        DESCRIPTION:
*//**       @brief         checks if streaming feature is enabled
*//**
@par     DEPENDENCIES:
                           None
*//*
        PARAMETERS:
*//**       @param         None

*//*     RETURN VALUE:
*//**       @return
                           true for success else failure
@par     SIFE EFFECTS:
                           None
*//*==========================================================================*/
bool WFDMMServiceClient::isStreamingFeatureEnabled()
{

    sp<IServiceManager> pSM = defaultServiceManager();

    sp<IBinder> pBinder;

    if(pSM->checkService(String16("wfd.native.mm.service"))
                                       == NULL)
    {
        WFDMMLOGE("Service not available yet");
        return false;
    }

    pBinder = pSM->getService(String16("wfd.native.mm.service"));

    if(pBinder == 0)
    {
        WFDMMLOGE("Service not available yet");
        return false;
    }

    sp<IWFDMMService> pWFDMMService =
                        interface_cast<IWFDMMService>(pBinder);

    if (pWFDMMService != NULL)
    {
        return (pWFDMMService->isStreamingFeatureEnabled() == 1) ? true : false;
    }
    return false;

}

/*==============================================================================

        FUNCTION:           startStreaming

         DESCRIPTION:
*//**       @brief          Tells wfdservice to start DS
*//**
@par     DEPENDENCIES:
                            None
*//*
         PARAMETERS:
*//**       @param          None

*//*     RETURN VALUE:
*//**       @return
                            0 for success else failure
@par     SIDE EFFECTS:
                            None
*//*==========================================================================*/
int WFDMMServiceClient::startStreaming()
{
    WFDMMServiceClientLock sLock;
    IWFDMMService *pWFDMMService = getWFDMMServiceInstance();

    if (pWFDMMService != NULL)
    {
        return pWFDMMService->startStreaming();
    }
    else
    {
        if(mpNotify)
        {
            mpNotify(mpAppData,
                             (StreamingEventType)STREAMING_SERVICE_DEAD,
                             NULL);
        }
    }

    return -1;
}

/*==============================================================================

      FUNCTION:           isStreamingSupported

       DESCRIPTION:
*//**       @brief
*//**
@par     DEPENDENCIES:
                          None
*//*
       PARAMETERS:
*//**       @param        codec - video codec
                          profile - video profile
                          level - video level
                          width - video width
                          height - video height
                          bSecure - drm video playback or not
                          bConstrainedProfile - if constrained profile

*//*     RETURN VALUE:
*//**       @return
                          0 for success else failure
@par     SIDE EFFECTS:
                          None
*//*==========================================================================*/
int WFDMMServiceClient::isStreamingSupported
(
    int codec,
    int profile,
    int level,
    int width,
    int height,
    bool bSecure,
    bool bConstrainedProfile
)
{
    WFDMMServiceClientLock sLock;
    /*--------------------------------------------------------------------------
     perform the action once received the buufer index and buffFd
    ----------------------------------------------------------------------------
    */
    IWFDMMService *pWFDMMService = getWFDMMServiceInstance();

    if(pWFDMMService != NULL)
    {
        return pWFDMMService->isStreamingSuccess(codec, profile,
                                               level, width, height, bSecure,
                                               bConstrainedProfile);
    }
    return -1;

}

/*==============================================================================

         FUNCTION:          writeFrame

         DESCRIPTION:
*//**       @brief          sends video frame to wfd service
*//**
@par     DEPENDENCIES:
                            None
*//*
         PARAMETERS:
*//**       @param          nLen    - Length of video frame
                            pData   - Pointer to buffer
                            time_us - timestamp of buffer

*//*     RETURN VALUE:
*//**       @return
                            0 for success else failure
@par     SIDE EFFECTS:
                            None
*//*==========================================================================*/
int WFDMMServiceClient::writeFrame
(
    unsigned long nLen,
    char* pData,
    unsigned long long time_us
)
{
    WFDMMServiceClientLock sLock;
    IWFDMMService *pWFDMMService = getWFDMMServiceInstance();

    if (pWFDMMService != NULL)
    {
        return pWFDMMService->writeFrame(nLen, pData, time_us);
    }
    else
    {
        if(mpNotify)
        {
            mpNotify(mpAppData,
                             (StreamingEventType)STREAMING_SERVICE_DEAD,
                             NULL);
        }

    }
    return -1;

}

/*==============================================================================

       FUNCTION:          writeFrameFd

       DESCRIPTION:
*//**       @brief        sends video frame to wfd service in an ion buffer
*//**
@par     DEPENDENCIES:
                          None
*//*
       PARAMETERS:
*//**       @param        nLen     - Length of frame
                          Fd       - ion buffer fd
                          time_us  - time_us
                          pData    - buffer header ptr
                          nFlags   - buffer flags
                          bSecure  - Secure buffer or not

*//*     RETURN VALUE:
*//**       @return
                          0 for success else failure
@par     SIDE EFFECTS:    None
*//*==========================================================================*/

int WFDMMServiceClient::writeFrameFd
(
    unsigned long nLen,
    int Fd,
    unsigned long long time_us,
    void* pData,
    int nFlags,
    bool bSecure
)
{
    WFDMMServiceClientLock sLock;
    WFDMMLOGH4("writeFrameFd in Client Fd = %d,nLen=%d,time_us = %lld,pData = %p ",
               Fd, nLen, time_us, pData);
    IWFDMMService *pWFDMMService = getWFDMMServiceInstance();
    if(pWFDMMService != NULL)
    {
        return pWFDMMService->writeFrameFd(nLen, Fd, time_us, pData, nFlags,
                                           bSecure);
    }
    else
    {
        if(mpNotify)
        {
            mpNotify(mpAppData,
                             (StreamingEventType)STREAMING_SERVICE_DEAD,
                             NULL);
        }
    }
    return -1;

}

/*==============================================================================

         FUNCTION:          renderFrame

         DESCRIPTION:
*//**       @brief          Tell wfdservice that frame with timestamp is
                            rendered
*//**
@par     DEPENDENCIES:
                            None
*//*
         PARAMETERS:
*//**       @param          Timestamp of frame rendered

*//*     RETURN VALUE:
*//**       @return
                            0 for success else failure
@par     SIDE EFFECTS:
                            None
*//*==========================================================================*/
int WFDMMServiceClient::renderFrame
(
    unsigned long long time_us
)
{
    WFDMMServiceClientLock sLock;
    IWFDMMService *pWFDMMService = getWFDMMServiceInstance();

    if (pWFDMMService != NULL)
    {
        return pWFDMMService->renderFrame(time_us);
    }
    else
    {
        if(mpNotify)
        {
            mpNotify(mpAppData,
                             (StreamingEventType)STREAMING_SERVICE_DEAD,
                             NULL);
        }
    }
    return -1;

}

/*==============================================================================

         FUNCTION:          pauseStreaming

         DESCRIPTION:
*//**       @brief          pause Streaming
*//**
@par     DEPENDENCIES:
                            None
*//*
         PARAMETERS:
*//**       @param          None

*//*     RETURN VALUE:
*//**       @return
                            0 for success else failure
@par     SIDE EFFECTS:
                            None
*//*==========================================================================*/
int WFDMMServiceClient::pauseStreaming()
{
    WFDMMServiceClientLock sLock;
    IWFDMMService *pWFDMMService = getWFDMMServiceInstance();

    if (pWFDMMService != NULL)
    {
        return pWFDMMService->pauseStreaming();
    }
    else
    {
        if(mpNotify)
        {
            mpNotify(mpAppData,
                             (StreamingEventType)STREAMING_SERVICE_DEAD,
                             NULL);
        }
    }
    return -1;

}

/*==============================================================================

        FUNCTION:           flush

        DESCRIPTION:
*//**       @brief         flush buffers held in WFDService. Used in seek
*//**
@par     DEPENDENCIES:
                           None
*//*
        PARAMETERS:
*//**       @param          None

*//*     RETURN VALUE:
*//**       @return
                           None
@par     SIDE EFFECTS:
                           None
*//*==========================================================================*/
int WFDMMServiceClient::flush()
{
    WFDMMServiceClientLock sLock;
    IWFDMMService *pWFDMMService = getWFDMMServiceInstance();

    if(pWFDMMService != NULL)
    {
        return pWFDMMService->flush();
    }
    else
    {
        if(mpNotify)
        {
            mpNotify(mpAppData,
                             (StreamingEventType)STREAMING_SERVICE_DEAD,
                             NULL);
        }
    }
    return -1;
}


/*==============================================================================

         FUNCTION:          endStreaming

         DESCRIPTION:
*//**       @brief
*//**
@par     DEPENDENCIES:
                            None
*//*
         PARAMETERS:
*//**       @param          None

*//*     RETURN VALUE:
*//**       @return
                            None
@par     SIDE EFFECTS:
                            None
*//*==========================================================================*/
int WFDMMServiceClient::endStreaming()
{
    WFDMMServiceClientLock sLock;
    IWFDMMService *pWFDMMService = getWFDMMServiceInstance();

    if (pWFDMMService != NULL)
    {
        return pWFDMMService->endStreaming();
    }
    return -1;
}

/*==============================================================================

        FUNCTION:          setWfdMMListener

        DESCRIPTION:       setting listner callback with WFD MM service
*//**       @brief
*//**
@par     DEPENDENCIES:
                           None
*//*
        PARAMETERS:
*//**       @param         None

*//*     RETURN VALUE:
*//**       @return
                           None
@par     SIDE EFFECTS:
                           None
*//*==========================================================================*/
WFDMMServiceClient* WFDMMServiceClient::setWfdMMListener
(
    bool flag
)
{
    WFDMMServiceClientLock sLock;
    IWFDMMService *pWFDMMService = getWFDMMServiceInstance();

    WFDMMLOGH2("setWfdMMListener =%p, flag = %d", pWFDMMService,flag);
    WFDMMServiceClient *pWFDMMServiceClient = NULL;
    if (pWFDMMService != NULL)
    {
        if (flag)
        {
            pWFDMMService->setWFDMMListener(true,this);
            WFDMMLOGH3("setWfdMMListener pWFDMMService=%p,Client= %p,flag =%d",
                       pWFDMMService, this,flag);
            pWFDMMServiceClient = this;
        }
        else
        {
            WFDMMLOGH3("setWfdMMListener pWFDMMService=%p,Client= %p,flag =%d",
                       pWFDMMService, this,flag);
            pWFDMMService->setWFDMMListener(false,this);
            mpNotify = 0;
        }
    }
    else
    {
        if(mpNotify && flag)
        {
            mpNotify(mpAppData,
                     (StreamingEventType)STREAMING_SERVICE_DEAD,
                     NULL);
        }
    }
    return pWFDMMServiceClient;
}

/*==============================================================================

     FUNCTION:          ~WFDMMServiceDeathRecepient

     DESCRIPTION:
*//**       @brief      DeathRecepient Dtor
*//**
@par     DEPENDENCIES:
                        None
*//*
     PARAMETERS:
*//**       @param      None

*//*     RETURN VALUE:
*//**       @return
                        None
@par     SIDE EFFECTS:
                        None
*//*==========================================================================*/

WFDMMServiceClient::WFDMMServiceDeathRecepient::~WFDMMServiceDeathRecepient()
{
    LOCK_MUTEX;
    IWFDMMService *pWFDMMService = getWFDMMServiceInstance();
    if (pWFDMMService != NULL)
    {
        pWFDMMService->asBinder(pWFDMMService)->unlinkToDeath(this);
    }
    UNLOCK_MUTEX;
}

/*==============================================================================

   FUNCTION:                binderDied

   DESCRIPTION:
*//**       @brief          callback from defaultServiceManger when
                            service is dead
*//**
@par     DEPENDENCIES:      None
*//*
   PARAMETERS:
*//**       @param          None

*//*     RETURN VALUE:
*//**       @return         None
@par     SIDE EFFECTS:      None
*//*==========================================================================*/

void WFDMMServiceClient::WFDMMServiceDeathRecepient::binderDied
(
    const wp<IBinder>& who
)
{
    LOCK_MUTEX;
    WFDMMLOGE("WFD.MM.NATIVE.SERVICE DEAD");
    (void)who;

    if(gWFDMMServiceInstance.get())
    {
        gWFDMMServiceInstance.clear();
    }

    WFDMMLOGE("WFD.MM.NATIVE.SERVICE DEAD Notify Clients");
    UNLOCK_MUTEX;

}

/*==============================================================================

 FUNCTION:               notify

 DESCRIPTION:
*//**       @brief       notification from WFDMMSource
*//**
@par     DEPENDENCIES:
                         None
*//*
 PARAMETERS:
*//**       @param       None

*//*     RETURN VALUE:
*//**       @return      None
@par     SIDE EFFECTS:   None
*//*==========================================================================*/

int WFDMMServiceClient::notify
(
    int event,
    void* PvtData
)
{
    LOCK_MUTEX;
    /*--------------------------------------------------------------------------
     perform the action once received the buufer index and buffFd
    ----------------------------------------------------------------------------
    */
    WFDMMLOGH2("WFDMMServiceClient notify event=%d, PvtData = %p",
               event, PvtData);
    if(mpNotify)
    {
        mpNotify(mpAppData, (StreamingEventType)event, PvtData);
    }
    UNLOCK_MUTEX;
    return 0;
}

/*==============================================================================

         FUNCTION:          setAppInfo

         DESCRIPTION:
*//**       @brief          This function receives name of application along with
                            operation performed on it. It further passes on this
                            information to Service to update it properly.
*//**
@par     DEPENDENCIES:
                            None
*//*
         PARAMETERS:
*//**       @param          [in]name
                            [in]operation

*//*     RETURN VALUE:
*//**       @return
                            None
@par     SIDE EFFECTS:
                            None
*//*==========================================================================*/
void WFDMMServiceClient::setAppInfo(const char* name,const char* operation)
{
    IWFDMMService *pWFDMMService  = getWFDMMServiceInstance();

    if (pWFDMMService)
    {
        pWFDMMService->setAppInfo(name,operation);
    }
}
