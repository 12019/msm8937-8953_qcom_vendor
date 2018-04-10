/*==============================================================================
*       WFDMMService.cpp
*
*  DESCRIPTION:
*       WFDMMService implementation
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

#include "WFDMMService.h"
#include "MMDebugMsg.h"
#include "WFDMMLogs.h"
#include "MMMemory.h"
#ifndef WFD_ICS
#include "common_log.h"
#endif
#include "IWFDMMListener.h"

android::sp<android::IServiceManager> WFDMMService::m_pMMServiceManager;
sp<WFDMMService> WFDMMService::m_pWFDMMService = NULL;
pFnWFDCallbackType gFnCb = NULL;
void *gUserdata = NULL;
bool m_bStreamingFeatureStatus = false;
bool m_bStreamingWhiteListApp = false;

/*==============================================================================

       FUNCTION:addCallback

       DESCRIPTION: passing Callback from WFDMMSourceVideoSource and Context
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

void WFDMMService::addCallback
(
    pFnWFDCallbackType pCb,
    void* handle
)
{
    gFnCb = pCb;
    gUserdata = handle;

}
/*==============================================================================

     FUNCTION:start()

     DESCRIPTION: Starting WFDMMService from WFDMMSourceVideoSource
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

WFDMMService* WFDMMService::start()
{
    WFDMMLOGH("WFDMMService :: Start");
    if( NULL == m_pWFDMMService.get())
    {
        m_pWFDMMService = MM_New(WFDMMService);

        if(NULL == m_pWFDMMService.get())
        {
            WFDMMLOGE("Failed to create WFDMMService");
            return NULL;
        }

        m_pMMServiceManager = defaultServiceManager();

        m_pMMServiceManager->addService(String16("wfd.native.mm.service"),
                                       m_pWFDMMService);

        if(m_pMMServiceManager->checkService(String16("wfd.native.mm.service"))
                                           == NULL)
        {
            WFDMMLOGH("Adding WFDMMService Failed!!");
            return NULL;
        }
        else
        {
            WFDMMLOGH1("Added WFDMMService m_pWFDMMService = %p",m_pWFDMMService.get());
        }
    }
    return (m_pWFDMMService.get());
}

/*==============================================================================

  FUNCTION:notifyClient

  DESCRIPTION: Informing the response from WFDMMService to client
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

int WFDMMService::notifyClient
(
    WFDMMService* pMine,
    int event,
    void* pData
)
{
    if(pMine!= NULL)
    {
        WFDMMLOGH1("WFDMMService::notifyClient %p",pMine);
        return pMine->notifyListener(event, pData);
    }
    WFDMMLOGH1("WFDMMService::notifyClient Failed %p", pMine);
    return -1;
}

/*==============================================================================
FUNCTION:notifyListener

DESCRIPTION: sending information to WFD Listener
*//**       @brief
*//**
@par     DEPENDENCIES:
                 None
*//*
PARAMETERS:
*//**       @param          event
                            pData

*//*     RETURN VALUE:
*//**       @return
                 None
@par     SIFE EFFECTS:
                 None
*//*==========================================================================*/

int WFDMMService::notifyListener
(
    int event,
    void* pData
)
{
    WFDMMLOGH1("WFDMMService::notifyListener %p",mMMServiceListener.get());

    if(mMMServiceListener.get() != NULL)
    {
        int result = mMMServiceListener->notify(event, pData);
        WFDMMLOGH2("WFDMMService::notifyListener Success = %p result = %d",
                   mMMServiceListener.get(),result);
        return 0;
    }
    else
    {
        WFDMMLOGE1("WFDMMService::notifyListener Failed = %p",mMMServiceListener.get());
        return -1;
    }
}

/*==============================================================================

FUNCTION:isStreamingSuccess

DESCRIPTION: checking whether Streaming can be supported with the clip data
             received  from player
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

int WFDMMService::isStreamingSuccess
(
    int codecType,
    int profile,
    int level,
    int width,
    int height,
    bool bSecure,
    bool bConstrainedProfile
)
{
    WFDMMLOGH2("isStreamingSuccess CodecType = %d, profile = %d",
               codecType, profile);
    WFDMMLOGH3("isStreamingSuccess level = %d, Width = %d, Height = %d",
               level, width, height);
    WFDMMLOGH2("isStreamingSuccess secure = %d,bConstrainedProfile = %d ",
               bSecure, bConstrainedProfile);

    streamDataType sData;
    sData.streamParams.codecType = codecType;
    sData.streamParams.profile = profile;
    sData.streamParams.level = level;
    sData.streamParams.width = width;
    sData.streamParams.height = height;
    sData.streamParams.bSecure = bSecure;
    sData.streamParams.bConstrainedProfile = bConstrainedProfile;
    return  gFnCb(gUserdata, WFDMMSERVICE_STREAMING_REQ, &sData);

}

/*==============================================================================

FUNCTION:isStreamingFeatureEnabled

DESCRIPTION: checks if Streaming is enabled by WFD stack
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
int WFDMMService::isStreamingFeatureEnabled()
{
    WFDMMLOGE1("WFDMMService::isStreamingFeatureEnabled - CurrentStatus [%d]",
                                            m_bStreamingFeatureStatus);
    return m_bStreamingFeatureStatus ? 1 : 0;
}

/*==============================================================================

FUNCTION:updateStreamingFeatureStatus

DESCRIPTION: udpates latest streaming support status from WFD framework
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
void WFDMMService::updateStreamingFeatureStatus(bool bStatus)
{
    m_bStreamingFeatureStatus = bStatus;

    WFDMMLOGH1("WFDMMService::updateStreamingFeatureStatus - Setting status[%d]",
        m_bStreamingFeatureStatus);

    return;
}

/*==============================================================================

FUNCTION:startStreaming

DESCRIPTION: request received from player to start streaming
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

int WFDMMService::startStreaming()
{
    WFDMMLOGE("StartStreaming");

    if(gFnCb)
    {
        gFnCb(gUserdata, WFDMMSERVICE_START_STREAMING, NULL);
    }
    return 0;
}

int WFDMMService::pauseStreaming()
{
    WFDMMLOGE("PauseStreaming");
    return 0;
}
/*==============================================================================

FUNCTION:endStreaming

DESCRIPTION: request received from player to stop streaming
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

int WFDMMService::endStreaming()
{
    WFDMMLOGE("end Streaming");
    if(gFnCb)
    {
        gFnCb(gUserdata, WFDMMSERVICE_STOP_STREAMING, NULL);
    }
    return 0;
}

/*==============================================================================

FUNCTION:writeFrame

DESCRIPTION: request received from player to process frame  for streaming
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

int WFDMMService::writeFrame
(
    unsigned long nLen,
    char* pData,
    unsigned long long time_us
)
{
    streamDataType sData;
    sData.WriteData.nLen = nLen;
    sData.WriteData.DataPtr.pData = pData;
    sData.WriteData.nTimeUs = time_us;
    sData.WriteData.bFd = false;

    if(gFnCb)
    {
        gFnCb(gUserdata, WFDMMSERVICE_WRITE_DATA, &sData);
    }
    WFDMMLOGE2("Write Frame %d %x", (int)nLen, pData );
    return 0;
}

/*==============================================================================

FUNCTION:writeFrameFd

DESCRIPTION: request received from player to process frame  with fd  instead of
             passing the buffer
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

int WFDMMService::writeFrameFd
(
    unsigned long nLen,
    int fd,
    unsigned long long time_us,
    void* pvtData,
    int nFlags,
    bool bSecure
)
{
    WFDMMLOGH4("writeFrameFd in Serv Fd = %d,nLen=%d,time_us=%lld,pData = %p \n",
               fd, nLen, time_us, pvtData);
    WFDMMLOGH1("writeFrameFd in Serv bSecure = %d \n ", bSecure);

    streamDataType sData;
    sData.WriteData.nLen = nLen;
    sData.WriteData.DataPtr.IonFdData.fd = fd;
    sData.WriteData.DataPtr.IonFdData.pvtData = pvtData;
    sData.WriteData.nTimeUs = time_us;
    sData.WriteData.bFd = true;
    sData.WriteData.nFlags = nFlags;
    sData.WriteData.bSecure = bSecure;

    if(gFnCb)
    {
        gFnCb(gUserdata, WFDMMSERVICE_WRITE_DATA, &sData);
    }
    WFDMMLOGE2("Write Frame len =%d with Fd = %d", (int)nLen, fd);
    return 0;
}


/*==============================================================================

FUNCTION:renderFrame

DESCRIPTION: request received from player to render frame received on writeFrame
            method
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
int WFDMMService::renderFrame(unsigned long long time_us)
{
    WFDMMLOGH1("WFDMMService::Render Frame TS= %lld", time_us);
    streamDataType sData;
    sData.RenderDataParam.nTimeUs = time_us;
    if(gFnCb)
    {
        gFnCb(gUserdata, WFDMMSERVICE_RENDER_DATA, &sData);
    }
    return 0;
}

/*==============================================================================

FUNCTION:setWFDMMListener

DESCRIPTION: request received from player to set the lister and de-register
             the listener.

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
int WFDMMService::setWFDMMListener
(
    bool bActive,
    const sp<IWFDMMListener>& WfdMMServiceListener
)
{

    if(gFnCb)
    {
        if (bActive)
        {
            if(NULL != WfdMMServiceListener.get())
            {
                mMMServiceListener = WfdMMServiceListener;
                WFDMMLOGE2("setWFDMMListener bActive = %d  Success = %p",
                           bActive,mMMServiceListener.get());
                gFnCb(gUserdata, WFDMMSERVICE_SET_WFD_LISTENER, NULL);
            }
        }
        else
        {
            WFDMMLOGE2("setWFDMMListener bActive %d %p",bActive,mMMServiceListener.get());
            if (mMMServiceListener.get() != NULL)
            {
                mMMServiceListener.clear();
            }
        }
    }
    return 0;
}

/*==============================================================================

FUNCTION:flush()

DESCRIPTION: request received from player to flush
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
*/ /*==========================================================================*/

int WFDMMService::flush()
{
    WFDMMLOGE("flush Streaming");
    if(gFnCb)
    {
        gFnCb(gUserdata, WFDMMSERVICE_FLUSH_STREAM, NULL);
    }
    return 0;
}

/*==============================================================================

   FUNCTION:       setAppInfo

   DESCRIPTION:
       @brief      This function sets name and operation of application.
   DEPENDENCIES:
                   None
   PARAMETERS:
       @param      None
   RETURN VALUE:
       @return     Integer
   SIDE EFFECTS:
                   None
*//*==========================================================================*/

int WFDMMService::setAppInfo
(
    const char *name,
    const char *operation
)
{
    WFDMMLOGL2("WFDMMService:setAppInfo = %s %s",name,operation);
    WhiteListType sWhiteListData;
    if(name != NULL && operation != NULL)
    {
        strlcpy(sWhiteListData.name,name,WHITELIST_APP_NAME_LENGTH);
        strlcpy(sWhiteListData.operation,operation,WHITELIST_OPERATION_LENGTH);
        if(gFnCb)
        {
            gFnCb(gUserdata, WFDMMSERVICE_APP_INFO, &sWhiteListData);
        }
        return 1;
    }
    return 0;
}

/*==============================================================================
   FUNCTION:       updateWhiteListStatus

   DESCRIPTION:
       @brief      This function updates value of m_bStreamingWhiteListApp with
                   the one received. m_bStreamingWhiteListApp is used in
                   checking whether application can do DS.
   DEPENDENCIES:
                   None
   PARAMETERS:
       @param      [in]Bool allowed
   RETURN VALUE:
       @return     None
   SIDE EFFECTS:
                   None
*//*==========================================================================*/

void WFDMMService::updateWhiteListStatus(bool allowed)
{
    WFDMMLOGL1("updateWhiteListStatus - Setting %d",allowed);
    m_bStreamingWhiteListApp = allowed;
}

/*==============================================================================

   FUNCTION:       isAppWhiteListed

   DESCRIPTION:
       @brief      This function is used to check whether application is allowed
                   to do DS.
   DEPENDENCIES:
                   None
   PARAMETERS:
       @param      None
   RETURN VALUE:
       @return     Integer
   SIDE EFFECTS:
                   None
*//*==========================================================================*/
int WFDMMService::isAppWhiteListed()
{
    WFDMMLOGH1("isAppWhiteListed - returning %d",m_bStreamingWhiteListApp);
    return m_bStreamingWhiteListApp ? 1 : 0;
}
