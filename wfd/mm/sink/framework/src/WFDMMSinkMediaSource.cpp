/*==============================================================================
*       WFDMMSinkMediaSource.cpp
*
*  DESCRIPTION:
*       Connects RTP decoder and parser and provides Audio and Video samples to
*       the media framework.
*
*
*  Copyright (c) 2013-2017 Qualcomm Technologies, Inc.
*  All Rights Reserved.
*  Confidential and Proprietary - Qualcomm Technologies, Inc.
*===============================================================================
*/
/*==============================================================================
**               Includes and Public Data Declarations
**==============================================================================
*/

/* =============================================================================

                     INCLUDE FILES FOR MODULE

================================================================================
*/
#include "WFDMMThreads.h"
#include "WFDMMSinkCommon.h"
#include "WFDMMSourceSignalQueue.h"
#include "MMMalloc.h"
#include "MMMemory.h"
#include "MMTimer.h"
#include "WFDMMLogs.h"
#include "wfd_netutils.h"
#include "WFDMMSinkMediaSource.h"
#include "filesourcetypes.h"
#include "wfd_netutils.h"
#include <stdio.h>
#include <linux/msm_ion.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <stdlib.h>
#include "WFDMMSinkStatistics.h"
#include "WFDUtils.h"

#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "WFDMMMSINKMEDIASRC"

#define MEDIASRC_AUDIO_BUF_SIZE      32768
#define MEDIASRC_NUM_AUDIO_BUFFERS   16

#define MEDIASRC_GENERIC_BUF_SIZE    (1920*1080*1.5)
#define MEDIASRC_NUM_GENERIC_BUFFERS  6

#define MEDIASRC_FLUSH_IDR_THRESHOLD (2 * 33000)

#define MEDIASRC_TIMESCALE_CONVERSION 1000*1000
#define IDR_REQ_INTERVAL 500 //500ms


#define CRITICAL_SECT_ENTER if(mhCritSect)                                     \
                                  MM_CriticalSection_Enter(mhCritSect);        \
/*      END CRITICAL_SECT_ENTER    */

#define CRITICAL_SECT_LEAVE if(mhCritSect)                                     \
                                  MM_CriticalSection_Leave(mhCritSect);        \
/*      END CRITICAL_SECT_LEAVE    */

#define NOTIFYERROR  mpFnHandler((void*)(int64)mClientData,                    \
                                 mnModuleId,WFDMMSINK_ERROR,                   \
                                 OMX_ErrorUndefined,                           \
                                 0 );                                          \
/*NOTIFYERROR*/

#define NOTIFYBYTESLOST  mpFnHandler((void*)(int64)mClientData,                \
                                 mnModuleId,WFDMMSINK_PACKETLOSS,              \
                                 OMX_ErrorUndefined,                           \
                                 0 );                                          \
/*NOTIFYBYTESLOST*/

#define NOTIFYVIDEORECONFIGURE  mpFnHandler((void*)(int64)mClientData,         \
                                 mnModuleId,WFDMMSINK_VIDEO_RECONFIGURE,       \
                                 OMX_ErrorUndefined,                           \
                                 0 );                                          \
/*NOTIFYVIDEORECONFIGURE*/

#define NOTIFYAUDIORECONFIGURE  mpFnHandler((void*)(int64)mClientData,         \
                                 mnModuleId,WFDMMSINK_AUDIO_RECONFIGURE,       \
                                 OMX_ErrorUndefined,                           \
                                 0 );                                          \
/*NOTIFYAUDIORECONFIGURE*/

#define AAC_SUBSTREAM_HEADER_SIZE 4


/*------------------------------------------------------------------------------
 State definitions
--------------------------------------------------------------------------------
*/
#define DEINIT  0
#define INIT    1
#define OPENED  2
#define CLOSING 4
#define CLOSED  3

#define ISSTATEDEINIT  (state(0, true) == DEINIT )
#define ISSTATEINIT    (state(0, true) == INIT   )
#define ISSTATEOPENED  (state(0, true) == OPENED )
#define ISSTATECLOSED  (state(0, true) == CLOSED )
#define ISSTATECLOSING (state(0, true) == CLOSING)

#define SETSTATECLOSING (state(CLOSING, false))
#define SETSTATEINIT    (state(INIT   , false))
#define SETSTATEDEINIT  (state(DEINIT , false))
#define SETSTATEOPENED  (state(OPENED,  false))

/*==============================================================================

         FUNCTION:         WFDMMSinkMediaSource

         DESCRIPTION:
*//**       @brief         WFDMMSinkMediaSource contructor
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
WFDMMSinkMediaSource::WFDMMSinkMediaSource(int moduleId,
                                           WFDMMSinkHandlerFnType pFnHandler,
                                           WFDMMSinkFBDType       pFnFBD,
                                           avInfoCbType           pFnAVInfo,
                                           void* clientData,
                                           WFDMMSinkStatistics*   pStatInst)
{
    InitData();
    /*--------------------------------------------------------------------------

    ----------------------------------------------------------------------------
    */
    mpFnHandler = pFnHandler;
    mpFnFBD     = pFnFBD;
    mnModuleId  = moduleId;
    mClientData = clientData;
    mpFnAVInfoCb = pFnAVInfo;
    m_pStatInst = pStatInst;
    mhCritSect = NULL;
    int nRet = MM_CriticalSection_Create(&mhCritSect);
    if(nRet != 0)
    {
        mhCritSect = NULL;
        WFDMMLOGE("Error in Critical Section Create");
    }

    SETSTATEDEINIT;

}

/*==============================================================================

         FUNCTION:         WFDMMSinkMediaSource

         DESCRIPTION:
*//**       @brief         WFDMMSinkMediaSource contructor
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
WFDMMSinkMediaSource::~WFDMMSinkMediaSource()
{
    WFDMMLOGH("WFDMMSinkMediaSource destructor");

    deinitialize();

    if(mhCritSect)
    {
        MM_CriticalSection_Release(mhCritSect);
    }

    WFDMMLOGH("WFDMMSinkMediaSource destructor Ends...");

}


/*==============================================================================

         FUNCTION:          InitData

         DESCRIPTION:
*//**       @brief          Initializes class members
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
void WFDMMSinkMediaSource::InitData()
{
    mVideoTrackId = 0;
    mAudioTrackId = 0;
    mpVideoFmtBlk = NULL;
    mnVideoFmtBlkSize = 0;
    mpAudioFmtBlk = NULL;
    mnAudioFmtBlkSize = 0;
    mnAudioMaxBufferSize = 0;
    mnVideoMaxBufferSize = 0;
    mnVideoTimescale = 1;
    mnAudioTimescale = 1;
    mnGenericTimescale = 1;
    mnActualBaseTime = 0;
    mbMediaBaseTimeSet = false;
    mnMediaBaseTime = 0;
    mnNewBaseTime = 0;
    mnOldBaseTime = 0;
    mpAudioQ = NULL;
    mpVideoQ = NULL;
    mpGenericQ = NULL;
    mpVideoThread = NULL;
    mpAudioThread = NULL;
    mpGenericThread = NULL;
    mpRTPStreamPort = NULL;
    memset(&mCfg, 0, sizeof(mCfg));
    mhCritSect = NULL;
    mpFileSource = NULL;
    meState = DEINIT;
    mnTracks = 0;
    mIonFd = -1;
    mAudioBufFd = -1;
    mAudioBufSize = 0;
    mVideoBufSize = 0;
    mAudioIonHandle = 0;
    mVideoBufFd = -1;
    mVideoIonHandle = 0;
    mAudioBufPtr = NULL;
    mVideoBufPtr = NULL;
    mVideoHeapPtr = NULL;
    mpFnHandler = NULL;
    mpFnFBD = NULL;
    mpFnAVInfoCb = NULL;
    mnCurrVideoTime = 0;
    mbMediaTimeSet = false;
    mnVideoFrameDropMode = FRAME_DROP_DROP_NONE;
    mnFlushTimeStamp = 0;
    mbFlushInProgress = false;
    mbPaused = false;
    mbVideoPaused = false;
    mbAudioPaused = false;
    mbGenericPaused = false;
    mbSinkIDRRequest = true;
    mbAudioStarted = false;
    mbVideoStarted = false;
    mbGenericStarted = false;
    mbVideoTimeReset = false;
    mbAudioTimeReset = false;
    mbGenericTimeReset = false;
    meFrameTypeInfo = FRAME_INFO_UNAVAILABLE;
    mbCorruptedFrame = false;
    mnLastIDRReqTime = 0;
    mbVideoReconfigured = false;
    mbAudioReconfigured = false;
    return;
}

/*==============================================================================

         FUNCTION:         Configure

         DESCRIPTION:
*//**       @brief         Configures the media source
*//**
@par     DEPENDENCIES:
                           None
*//*
         PARAMETERS:
*//**       @param         pCfg        - Configuration Parameters

*//*     RETURN VALUE:
*//**       @return
                           OMX_ERRORTYPE
@par     SIDE EFFECTS:
                           None
*//*==========================================================================*/
OMX_ERRORTYPE WFDMMSinkMediaSource::Configure(mediaSourceConfigType *pCfg)
{
    CHECK_ARGS_RET_OMX_ERR_4(pCfg, pCfg->rtpPort, mpFnHandler, mpFnFBD);

    memcpy(&mCfg, pCfg, sizeof(mediaSourceConfigType));

    if(!pCfg->nLocalIP)
    {
        char sIP[24] = {0};
        getLocalIpAddress(sIP, 24);
        if(strlen(sIP) == 0)
        {
            WFDMMLOGE("Failed to get Local IP");
            return OMX_ErrorUndefined;
        }
    }
    SETSTATEINIT;
    if(createResources() != OMX_ErrorNone)
    {
        WFDMMLOGE("Failed to create MediaSrc resources");
        NOTIFYERROR;
        return OMX_ErrorInsufficientResources;
    }
    return OMX_ErrorNone;
}

/*==============================================================================

         FUNCTION:         createResources

         DESCRIPTION:
*//**       @brief         creates resources for WFDMMSinkMediaSource
*//**
@par     DEPENDENCIES:
                           None
*//*
         PARAMETERS:
*//**       @param         None

*//*     RETURN VALUE:
*//**       @return
                           OMX_ERRORTYPE
@par     SIDE EFFECTS:
                           None
*//*==========================================================================*/
OMX_ERRORTYPE WFDMMSinkMediaSource::createResources()
{
    if(!configureDataSource() ||
          !configureHDCPResources() ||
             !createThreadsAndQueues() ||
                !allocateAudioBuffers() ||
                   !configureParser())
    {
        WFDMMLOGE("Media Source Failed to create resources");
        return OMX_ErrorInsufficientResources;
    }
    if(!allocateGenericBuffers())
    {
        WFDMMLOGE("Media Source Failed to create Generic resources");
    }
    return OMX_ErrorNone;
}

/*==============================================================================

         FUNCTION:          allocateAudioBuffers

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
                            true or false
@par     SIDE EFFECTS:
                            None
*//*==========================================================================*/
bool WFDMMSinkMediaSource::allocateAudioBuffers()
{
    /*--------------------------------------------------------------------------
     If audio type is LPCM MediaSource allocates Buffers
    ----------------------------------------------------------------------------
    */
#ifdef USE_OMX_AAC_CODEC
    if(mCfg.eAudioFmt == WFD_AUDIO_LPCM)
#endif
    {
        /*----------------------------------------------------------------------
         Allocate buffers to carry PCM data to renderer here
        ------------------------------------------------------------------------
        */
        OMX_BUFFERHEADERTYPE *pBufferHdr = NULL;
        for(int i = 0; i < MEDIASRC_NUM_AUDIO_BUFFERS; i++)
        {
            pBufferHdr = (OMX_BUFFERHEADERTYPE*)
                       MM_Malloc(sizeof(OMX_BUFFERHEADERTYPE));
            if(!pBufferHdr)
            {
                WFDMMLOGE("Failed to allocate Audio BufferHdr");
                return false;
            }

            memset(pBufferHdr, 0, sizeof(OMX_BUFFERHEADERTYPE));

            pBufferHdr->pBuffer = (OMX_U8*)MM_Malloc(MEDIASRC_AUDIO_BUF_SIZE);

            if(!pBufferHdr->pBuffer)
            {
                WFDMMLOGE("Failed to allocate Audio Buffers");
                MM_Free(pBufferHdr);
                pBufferHdr = NULL;
                return false;
            }

            pBufferHdr->nAllocLen = MEDIASRC_AUDIO_BUF_SIZE;

            mpAudioQ->Push(&pBufferHdr, sizeof(&pBufferHdr));

        }
    }
    return true;
}


/*==============================================================================

         FUNCTION:          deallocateAudioBuffers

         DESCRIPTION:
*//**       @brief          deallocates Audio Buffers
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
bool WFDMMSinkMediaSource::deallocateAudioBuffers()
{
    /*--------------------------------------------------------------------------
     Only for LPCM MediaSource is the allocater of Buffers.
    ----------------------------------------------------------------------------
    */
#ifdef USE_OMX_AAC_CODEC
    if(mCfg.eAudioFmt == WFD_AUDIO_LPCM)
#endif
    {
        OMX_BUFFERHEADERTYPE *pBufferHdr;
        for(int i = 0; i < MEDIASRC_NUM_AUDIO_BUFFERS; i++)
        {
            pBufferHdr = NULL;

            mpAudioQ->Pop(&pBufferHdr, sizeof(&pBufferHdr), 0);

            if(pBufferHdr)
            {
                if(pBufferHdr->pBuffer)
                {
                    MM_Free(pBufferHdr->pBuffer);
                }
                MM_Free(pBufferHdr);
            }
        }
    }

    return true;
}

/*==============================================================================

         FUNCTION:          allocateGenericBuffers

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
                            true or false
@par     SIDE EFFECTS:
                            None
*//*==========================================================================*/
bool WFDMMSinkMediaSource::allocateGenericBuffers()
{
    {
        /*----------------------------------------------------------------------
         Allocate buffers to carry Generic data to renderer, here
        ------------------------------------------------------------------------
        */
        OMX_BUFFERHEADERTYPE *pBufferHdr = NULL;
        for(int i = 0; i < MEDIASRC_NUM_GENERIC_BUFFERS; i++)
        {
            pBufferHdr = (OMX_BUFFERHEADERTYPE*)
                       MM_Malloc(sizeof(OMX_BUFFERHEADERTYPE));
            if(!pBufferHdr)
            {
                WFDMMLOGE("Failed to allocate Generic BufferHdr");
                return false;
            }

            memset(pBufferHdr, 0, sizeof(OMX_BUFFERHEADERTYPE));

            pBufferHdr->pBuffer = (OMX_U8*)MM_Malloc(MEDIASRC_GENERIC_BUF_SIZE);

            if(!pBufferHdr->pBuffer)
            {
                WFDMMLOGE("Failed to allocate Generic Buffers");
                MM_Free(pBufferHdr);
                pBufferHdr = NULL;
                return false;
            }

            pBufferHdr->nAllocLen = MEDIASRC_GENERIC_BUF_SIZE;

            mpGenericQ->Push(&pBufferHdr, sizeof(&pBufferHdr));
        }
    }
    return true;
}

/*==============================================================================

         FUNCTION:          deallocateGenericBuffers

         DESCRIPTION:
*//**       @brief          deallocates Generic Buffers
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
bool WFDMMSinkMediaSource::deallocateGenericBuffers()
{
    {
        OMX_BUFFERHEADERTYPE *pBufferHdr = NULL;
        if(mpGenericQ)
        {
            for(int i = 0; i < MEDIASRC_NUM_GENERIC_BUFFERS; i++)
            {
                mpGenericQ->Pop(&pBufferHdr, sizeof(&pBufferHdr), 0);

                if(pBufferHdr)
                {
                    if(pBufferHdr->pBuffer)
                    {
                        MM_Free(pBufferHdr->pBuffer);
                        pBufferHdr->pBuffer = NULL;
                    }
                    MM_Free(pBufferHdr);
                    pBufferHdr = NULL;
                }
            }
        }
    }

    return true;
}

/*==============================================================================

         FUNCTION:          deallocateIonBufs

         DESCRIPTION:
*//**       @brief          Deallocates ION buffers
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
bool WFDMMSinkMediaSource::deallocateIonBufs()
{
    if(mIonFd > 0)
    {
        if(mAudioBufPtr)
        {
            munmap(mAudioBufPtr, mAudioBufSize);
        }

        if(mVideoBufPtr)
        {
            munmap(mVideoBufPtr, mVideoBufSize);
        }

        struct ion_fd_data sFdInfo;
        struct ion_handle_data sIonHandle;
        int32_t ret = 0;

        memset(&sFdInfo, 0, sizeof(sFdInfo));
        memset(&sIonHandle, 0, sizeof(sIonHandle));

        if(mAudioBufFd > 0)
        {
            sFdInfo.fd = mAudioBufFd;

            ret = ioctl(mIonFd, ION_IOC_IMPORT, &sFdInfo);

            if(sFdInfo.handle) {
                sIonHandle.handle = sFdInfo.handle;
                ret = ioctl(mIonFd, ION_IOC_FREE, &sIonHandle);
                if(ret) {
                    ALOGE("WFDSource Failed to free ION buf");
                }
            }

            close(mAudioBufFd);
        }

        if(mVideoBufFd > 0)
        {
            sFdInfo.fd = mVideoBufFd;
            sFdInfo.handle = 0;

            ret = ioctl(mIonFd, ION_IOC_IMPORT, &sFdInfo);

            if(sFdInfo.handle) {
                sIonHandle.handle = sFdInfo.handle;
                ret = ioctl(mIonFd, ION_IOC_FREE, &sIonHandle);
                if(ret) {
                    ALOGE("WFDSource Failed to free ION buf");
                }
            }
            close(mVideoBufFd);
        }
        close(mIonFd);
    }

    if(mVideoHeapPtr)
    {
        MM_Free(mVideoHeapPtr);
        mVideoHeapPtr = NULL;
    }

    return true;
}


/*==============================================================================

         FUNCTION:         createThreadsAndQueues

         DESCRIPTION:
*//**       @brief         creates threads and queues needed for the module
*//**
@par     DEPENDENCIES:
                           None
*//*
         PARAMETERS:
*//**       @param         None

*//*     RETURN VALUE:
*//**       @return
                           bool
@par     SIDE EFFECTS:
                           None
*//*==========================================================================*/
bool WFDMMSinkMediaSource::createThreadsAndQueues()
{
    /*--------------------------------------------------------------------------
      Create threads if session has audio and video
    ----------------------------------------------------------------------------
    */
    if(mCfg.bHasVideo)
    {
        mpVideoThread = new WFDMMThreads(1);

        if(!mpVideoThread)
        {
            WFDMMLOGE("Failed to create Video Thread");
            return false;
        }

        mpVideoThread->Start(VideoThreadEntry, -2,
                             32768,this, "WFDSinkVideoSrc");
    }

    if(mCfg.bHasAudio)
    {
        mpAudioThread = new WFDMMThreads(1);

        if(!mpAudioThread)
        {
            WFDMMLOGE("Failed to create Audio Thread");
            return false;
        }

        mpAudioThread->Start(AudioThreadEntry, -2,
                             32768,this, "WFDSinkAudioSrc");
    }

    if(mCfg.bHasGeneric)
    {
        mpGenericThread = new WFDMMThreads(1);

        if(!mpGenericThread)
        {
            WFDMMLOGE("Failed to create Genric Thread");
            return false;
        }

        mpGenericThread->Start(GenericThreadEntry, -2,
                             32768,this, "WFDSinkGenericSrc");

    }

    mpAudioQ   = MM_New_Args(SignalQueue, (100, sizeof(int*)));
    mpVideoQ   = MM_New_Args(SignalQueue, (100, sizeof(int*)));
    mpGenericQ = MM_New_Args(SignalQueue, (100, sizeof(int*)));

    if(!mpAudioQ || !mpVideoQ)
    {
        return false;
    }
    else if(!mpGenericQ)
    {
        WFDMMLOGE("Failed to create Genric Signal Queue");
    }

    return true;
}



/*==============================================================================

         FUNCTION:         deinitialize

         DESCRIPTION:
*//**       @brief         deallocated all resources
*//**
@par     DEPENDENCIES:
                           None
*//*
         PARAMETERS:
*//**       @param         None

*//*     RETURN VALUE:
*//**       @return
                           OMX_ERRORTYPE
@par     SIDE EFFECTS:
                           None
*//*==========================================================================*/
OMX_ERRORTYPE WFDMMSinkMediaSource::deinitialize()
{

    SETSTATECLOSING;

    WFDMMLOGH("Wait for Audio buffers to be back");

    while(
#ifdef USE_OMX_AAC_CODEC
          mCfg.eAudioFmt == WFD_AUDIO_LPCM &&
#endif
          mpAudioQ && mpAudioQ->GetSize() != MEDIASRC_NUM_AUDIO_BUFFERS)
    {
        MM_Timer_Sleep(2);
    }

    WFDMMLOGH("All Audio buffers are back");

    if(mpAudioQ)
    {
        deallocateAudioBuffers();
    }

    while(mpGenericQ && mpGenericQ->GetSize() != MEDIASRC_NUM_GENERIC_BUFFERS)
    {
        MM_Timer_Sleep(2);
    }
    WFDMMLOGH("All Generic buffers are back");

    if(mpGenericQ)
    {
        deallocateGenericBuffers();
    }

    /*--------------------------------------------------------------------------
      Close Threads and signal queues
    ----------------------------------------------------------------------------
    */
    if(mpAudioThread)
    {
        MM_Delete(mpAudioThread);
        mpAudioThread = NULL;
    }

    if(mpVideoThread)
    {
        MM_Delete(mpVideoThread);
        mpVideoThread = NULL;
    }

    if(mpGenericThread)
    {
        MM_Delete(mpGenericThread);
        mpGenericThread = NULL;
    }
    /*--------------------------------------------------------------------------
      Close filesource first
    ----------------------------------------------------------------------------
    */
    if(mpFileSource != NULL)
    {
        WFDMMLOGH("WFDMMSinkMediaSrc... Filesource close file");
        mpFileSource->CloseFile();
        MM_Delete(mpFileSource);
        mpFileSource = NULL;
    }

    if(mpAudioQ)
    {
        MM_Delete(mpAudioQ);
        mpAudioQ = NULL;
    }

    if(mpVideoQ)
    {
        MM_Delete(mpVideoQ);
        mpVideoQ = NULL;
    }

    if(mpGenericQ)
    {
        MM_Delete(mpGenericQ);
        mpGenericQ = NULL;
    }
    deallocateIonBufs();

    return OMX_ErrorNone;

}

/*==============================================================================

         FUNCTION:         VideoThreadEntry

         DESCRIPTION:
*//**       @brief         Static entry function called from WFDMMThread
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
void WFDMMSinkMediaSource::VideoThreadEntry(void *pThis,
                                            unsigned int nSignal)
{
    CHECK_ARGS_RET_VOID_1(pThis);

    WFDMMSinkMediaSource *pMe = (WFDMMSinkMediaSource*)pThis;

    pMe->VideoThread(nSignal);
}

/*==============================================================================

         FUNCTION:         VideoThread

         DESCRIPTION:
*//**       @brief         Video Thread for fetching samples from parser
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
void WFDMMSinkMediaSource::VideoThread(unsigned int nSignal)
{
    (void) nSignal;
    while(ISSTATEOPENED && !mbPaused && !mbVideoPaused)
    {
        if(mCfg.bSecure != true)
        {
            if(fetchVideoSample(mVideoTrackId) != FILE_SOURCE_DATA_OK)
            {
                MM_Timer_Sleep(2);
            }
        }
        else
        {
            if(fetchVideoSampleSecure(mVideoTrackId) != FILE_SOURCE_DATA_OK)
            {
                MM_Timer_Sleep(2);
            }
        }
    }
}

/*==============================================================================

         FUNCTION:         AudioThreadEntry

         DESCRIPTION:
*//**       @brief         Static entry function called from WFDMMThread
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
void WFDMMSinkMediaSource::AudioThreadEntry(void* pThis,
                                            unsigned int nSignal)
{
    CHECK_ARGS_RET_VOID_1(pThis);

    WFDMMSinkMediaSource *pMe = (WFDMMSinkMediaSource*)pThis;

    pMe->AudioThread(nSignal);
}

/*==============================================================================

         FUNCTION:         AudioThread

         DESCRIPTION:
*//**       @brief         Audio Thread for fetching samples from parser
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
void WFDMMSinkMediaSource::AudioThread(unsigned int nSignal)
{
    (void) nSignal;
    while(ISSTATEOPENED && !mbPaused && !mbAudioPaused)
    {
        if(!mCfg.bSecure)
        {
            if(fetchAudioSample(mAudioTrackId)!= FILE_SOURCE_DATA_OK)
            {
                MM_Timer_Sleep(2);
            }
        }
        else
        {
            if(fetchAudioSampleSecure(mAudioTrackId)!= FILE_SOURCE_DATA_OK)
            {
                MM_Timer_Sleep(2);
            }
        }
    }
}

/*==============================================================================

         FUNCTION:         GenericThreadEntry

         DESCRIPTION:
*//**       @brief         Static entry function called from WFDMMThread
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
void WFDMMSinkMediaSource::GenericThreadEntry(void* pThis,
                                            unsigned int nSignal)
{
    CHECK_ARGS_RET_VOID_1(pThis);

    WFDMMSinkMediaSource *pMe = (WFDMMSinkMediaSource*)pThis;

    pMe->GenericThread(nSignal);
}

/*==============================================================================

         FUNCTION:         GenericThread

         DESCRIPTION:
*//**       @brief         Generic Thread for fetching samples from parser
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
void WFDMMSinkMediaSource::GenericThread(unsigned int nSignal)
{
    (void) nSignal;
    /* Need to add thread sync */
    while(ISSTATEOPENED && !mbPaused && !mbGenericPaused)
    {
        if(!mCfg.bSecure)
        {
            if(fetchGenericSample(mGenericTrackId)!= FILE_SOURCE_DATA_OK)
            {
                MM_Timer_Sleep(2);
            }
        }
        /* Currently Secure overlay is not being discussed but keeping code
           same as audio case for future enhancement */
        else
        {
            if(fetchGenericSampleSecure(mGenericTrackId)!= FILE_SOURCE_DATA_OK)
            {
                MM_Timer_Sleep(10);
            }
        }
    }
}

/*==============================================================================

         FUNCTION:         fetchVideoSample

         DESCRIPTION:
*//**       @brief         checks if a video buffer is available and fills data
                           and sends to decoder
*//**
@par     DEPENDENCIES:
                           None
*//*
         PARAMETERS:
*//**       @param         nTrackId - helps in multiple audio case

*//*     RETURN VALUE:
*//**       @return
                           None
@par     SIDE EFFECTS:
                           None
*//*==========================================================================*/
int WFDMMSinkMediaSource::fetchVideoSample(uint32 nTrackId)
{
    OMX_BUFFERHEADERTYPE *pBuffer = NULL;

    OMX_ERRORTYPE result;
    uint64 startTime = 0;

     CheckAndReqIDR();

    /*--------------------------------------------------------------------------
      Check if there are any buffers returned back by the decoder
    ----------------------------------------------------------------------------
    */
    result = mpVideoQ->Pop(&pBuffer,
                          sizeof(pBuffer),
                          3);

    if(result != OMX_ErrorNone)
    {
        WFDMMLOGM("No Buffer available for Video. Continue");
        return FILE_SOURCE_DATA_INSUFFICIENT;
    }

    /*--------------------------------------------------------------------------
     Now we have an empty Buffer. Pull data from parser if available
    ----------------------------------------------------------------------------
    */
    FileSourceSampleInfo  sampleInfo;
    FileSourceMediaStatus mediaStatus = FILE_SOURCE_DATA_OK;
    uint32 nSize = pBuffer->nAllocLen;
    mediaStatus = mpFileSource->GetNextMediaSample
                                                 (nTrackId,
                                                  pBuffer->pBuffer,
                                                  &nSize,
                                                  sampleInfo);

#ifdef BLOCK_FILLER_NALU
    if(mediaStatus == FILE_SOURCE_DATA_OK && pBuffer->pBuffer[4] == 0xc)
    {
        uint32 nSize = pBuffer->nAllocLen;
        mediaStatus = mpFileSource->GetNextMediaSample
                                                     (nTrackId,
                                                      pBuffer->pBuffer,
                                                      &nSize,
                                                      sampleInfo);
    }
#endif

    switch(mediaStatus)
    {
        case FILE_SOURCE_DATA_OK:
            if(mbSinkIDRRequest)
            {
                /*--------------------------------------------------------------
                 * Handles the case where source itself sends IDR after every
                 * resumption of session. Also takes care of the IDR frame after
                 * the initial play of session.
                 ---------------------------------------------------------------
                 */
                 switch(meFrameTypeInfo)
                 {
                    case FRAME_INFO_UNAVAILABLE:
                        WFDMMLOGD("Frame info not yet available from decoder");
                        break;

                    case FRAME_INFO_I_FRAME:
                        WFDMMLOGD("First frame is IDR.");
                        mbSinkIDRRequest = false;
                        meFrameTypeInfo = FRAME_INFO_UNAVAILABLE;
                        break;

                    case FRAME_INFO_P_FRAME:
                        WFDMMLOGD("First frame is not IDR. Make delayed request");
                        mbCorruptedFrame = true;
                        mbSinkIDRRequest = false;
                        meFrameTypeInfo = FRAME_INFO_UNAVAILABLE;
                        break;

                    default:
                        WFDMMLOGD("Unknown frame info");
                        break;
                 }
            }

            /*------------------------------------------------------------------
              Check if we are recovering from frame loss or there are packet
              loss
            --------------------------------------------------------------------
            */
            pBuffer->nOffset = 0;
            pBuffer->nFlags  = 0;

            if(sampleInfo.nBytesLost)
            {
                mbCorruptedFrame = true;
                WFDMMLOGH1("Parser reports Packet Loss. TS[%llu]",
                    (uint64)(sampleInfo.startTime * mnVideoTimescale));
                if(mnVideoFrameDropMode == FRAME_DROP_DROP_CURRENT_FRAME)
                {
                     WFDMMLOGE("Bytes Lost: Drop current frame");
                     mpVideoQ->Push(&pBuffer, sizeof(OMX_BUFFERHEADERTYPE**));
                     break;
                }
                else
                {
                     WFDMMLOGE1("Bytes Lost: Indicate corrupt frame. TS[%llu]",
                        (uint64)(sampleInfo.startTime * mnVideoTimescale));
                     pBuffer->nFlags |= OMX_BUFFERFLAG_DATACORRUPT;
                }
            }

            startTime = (uint64)(sampleInfo.startTime * mnVideoTimescale);
            if(mbFlushInProgress)
            {
                if(startTime <= mnFlushTimeStamp)
                {
                    if(((mnFlushTimeStamp - startTime)
                        < MEDIASRC_FLUSH_IDR_THRESHOLD)&&
                                   sampleInfo.sync)
                    {
                        mbFlushInProgress = false;
                    }
                    else
                    {
                        mpVideoQ->Push(&pBuffer,sizeof(OMX_BUFFERHEADERTYPE**));
                        break;
                    }
                }
                else if(sampleInfo.sync)
                {
                    mbFlushInProgress = false;
                }
            }


            /*------------------------------------------------------------------
              Set the media base time once
            --------------------------------------------------------------------
            */
            pBuffer->nTimeStamp = (OMX_TICKS)(startTime);

            if(!mbMediaBaseTimeSet ||
              (!mbVideoStarted && (uint64)pBuffer->nTimeStamp < mnMediaBaseTime))
            {
                mbMediaBaseTimeSet = true;
                mnMediaBaseTime = pBuffer->nTimeStamp;
                mbMediaTimeSet = false;
            }
            if(!mbVideoStarted)
            {
                WFDMMLOGH1("Video Buffer set STARTTIME. TS[%llu]",
                    pBuffer->nTimeStamp);
                pBuffer->nFlags |= OMX_BUFFERFLAG_STARTTIME;

                if(mbVideoReconfigured)
                {
                    mbVideoReconfigured = false;
                    SetMediaBaseTime(0);
                }
            }

            if(mbVideoTimeReset &&
              ((mnOldBaseTime == mnNewBaseTime) ||
               ProximityCheck((uint64)sampleInfo.startTime,
                                                  mnOldBaseTime,
                                                  mnNewBaseTime)))
            {
                WFDMMLOGH1("Video Buffer set STARTTIME after VideoTimeReset. TS[%llu]",
                    pBuffer->nTimeStamp);
                pBuffer->nFlags |= OMX_BUFFERFLAG_STARTTIME;
                mbVideoTimeReset = false;
            }

            mbVideoStarted = true;

            if(WFD_UNLIKELY(m_pStatInst))
            {
                m_pStatInst->recordMediaSrcIP(pBuffer);
            }

            /* ALWAYS delay the very first IDR request from sink (initial/after
             * pause-play) by IDR_REQ_INTERVAL. This is to take care of 3rd
             * party sources which may crash upon receiving an IDR request at
             * the very start of playback */
            if(mnLastIDRReqTime == 0)
            {
                /* Update the mnLastIDRReqTime by Current Time to achieve
                 * the aforementioned task*/
                MM_Time_GetTimeEx(&mnLastIDRReqTime);
            }

            CheckBaseTimeChange(sampleInfo.startTime);

            if(!mbMediaTimeSet)
            {
                SetMediaBaseTime((uint64)pBuffer->nTimeStamp /*us*/);
                mbMediaTimeSet = true;
            }
            if(mnCurrVideoTime && (unsigned)pBuffer->nTimeStamp == mnCurrVideoTime)
            {
                pBuffer->nTimeStamp += 1;
            }
            mnCurrVideoTime = pBuffer->nTimeStamp;
            WFDMMLOGE1("Video: Timestamp = %lld", pBuffer->nTimeStamp);

            WFDMMLOGD2("H264 Sample %x          %x",
                       pBuffer->pBuffer[0], pBuffer->pBuffer[1]);
            WFDMMLOGM2("H264 Sample %x          %x",
                       pBuffer->pBuffer[2], pBuffer->pBuffer[3]);
            WFDMMLOGM2("H264 Sample %x          %x",
                       pBuffer->pBuffer[4], pBuffer->pBuffer[5]);
            WFDMMLOGM2("H264 Sample %x          %x",
                       pBuffer->pBuffer[6], pBuffer->pBuffer[7]);
            pBuffer->nFilledLen = nSize;

            if(sampleInfo.sync)
            {
                pBuffer->nFlags |= OMX_BUFFERFLAG_SYNCFRAME;
            }

            pBuffer->nFlags |= OMX_BUFFERFLAG_ENDOFFRAME;

            WFDMMLOGM1("Video ETB flags = %x", pBuffer->nFlags);

            if(mpFnFBD)
            {
                mpFnFBD(mClientData, mnModuleId, SINK_VIDEO_TRACK_ID, pBuffer);
            }
            break;

        case FILE_SOURCE_DATA_CODEC_INFO:
            {
                WFDMMLOGM("VIDEO_THREAD received FILE_SOURCE_DATA_CODEC_INFO");
                /*--------------------------------------------------------------
                1. Update track information
                2. Update flag to stop fetching further video samples
                3. Notify video decoder reconfiguration to WFDMMSink (ASYNC)
                --------------------------------------------------------------*/

                //1.
                pauseVideoThread();

                //2.
                updateTrackInfo(false);

                //3.
                WFDMMLOGH("Notify Video Decoder reconfiguration");
                NOTIFYVIDEORECONFIGURE;
            }
            break;

        case FILE_SOURCE_DATA_UNDERRUN:
        case FILE_SOURCE_DATA_INSUFFICIENT:
            WFDMMLOGM("FILE_SOURCE_DATA_UNDERRUN");
            mpVideoQ->Push(&pBuffer, sizeof(OMX_BUFFERHEADERTYPE**));
            return FILE_SOURCE_DATA_UNDERRUN;
        case FILE_SOURCE_DATA_REALLOCATE_BUFFER:
            WFDMMLOGE("FILE_SOURCE_DATA_REALLOCATE_BUFFER - Tearing down!!");
            NOTIFYERROR;
            return FILE_SOURCE_DATA_REALLOCATE_BUFFER;
        case FILE_SOURCE_DATA_END:
        case FILE_SOURCE_DATA_ERROR:
        default:
            mpVideoQ->Push(&pBuffer, sizeof(OMX_BUFFERHEADERTYPE**));
            return FILE_SOURCE_DATA_ERROR;
    }
    return FILE_SOURCE_DATA_OK;
}


/*==============================================================================

         FUNCTION:         fetchVideoSampleSecure

         DESCRIPTION:
*//**       @brief         checks if a video buffer is available and fills data
                           and sends to decoder
*//**
@par     DEPENDENCIES:
                           None
*//*
         PARAMETERS:
*//**       @param         nTrackId - helps in multiple audio case

*//*     RETURN VALUE:
*//**       @return
                           None
@par     SIDE EFFECTS:
                           None
*//*==========================================================================*/
int WFDMMSinkMediaSource::fetchVideoSampleSecure(uint32 nTrackId)
{
    (void) nTrackId;
    OMX_BUFFERHEADERTYPE *pBuffer = NULL;

    OMX_ERRORTYPE result;
    uint64 startTime = 0;



    if(mCfg.pFnDecrypt == NULL)
    {
        WFDMMLOGE("No function to decrypt");
        return -1;
    }

    if(!mVideoBufPtr)
    {
        WFDMMLOGE("No Local Buffer for decrypt");
        return -1;
    }

    CheckAndReqIDR();

    /*--------------------------------------------------------------------------
      Check if there are any buffers returned back by the decoder
    ----------------------------------------------------------------------------
    */
    result = mpVideoQ->Pop(&pBuffer,
                          sizeof(pBuffer),
                          3);

    if(result != OMX_ErrorNone)
    {
        WFDMMLOGM("No Buffer available for Video. Continue");
        return FILE_SOURCE_DATA_INSUFFICIENT;
    }

    /*--------------------------------------------------------------------------
     Now we have an empty Buffer. Pull data from parser if available
    ----------------------------------------------------------------------------
    */
    FileSourceSampleInfo  sampleInfo;
    FileSourceMediaStatus mediaStatus = FILE_SOURCE_DATA_OK;
    uint32 nSize = mVideoBufSize;
    uint8 *pDataPtr = mVideoHeapPtr ? mVideoHeapPtr : mVideoBufPtr;
    mediaStatus = mpFileSource->GetNextMediaSample
                                                 (nTrackId,
                                                  pDataPtr,
                                                  &nSize,
                                                  sampleInfo);


#ifdef BLOCK_FILLER_NALU

    /**Warning for use. When data is secure this may lead to actual
     * frames being dropped */
    if(mediaStatus == FILE_SOURCE_DATA_OK && nSize == 8)
    {
        uint32 nSize = pBuffer->nAllocLen;
        mediaStatus = mpFileSource->GetNextMediaSample
                                                     (nTrackId,
                                                      pDataPtr,
                                                      &nSize,
                                                      sampleInfo);
    }
#endif

    switch(mediaStatus)
    {
        case FILE_SOURCE_DATA_OK:

            if(mbSinkIDRRequest)
            {
                /*--------------------------------------------------------------
                 * Handles the case where source itself sends IDR after every
                 * resumption of session. Also takes care of the IDR frame after
                 * the initial play of session.
                 ---------------------------------------------------------------
                 */
                 switch(meFrameTypeInfo)
                 {
                    case FRAME_INFO_UNAVAILABLE:
                        WFDMMLOGD("Frame info not yet available from decoder");
                        break;

                    case FRAME_INFO_I_FRAME:
                        WFDMMLOGD("First frame is IDR.");
                        mbSinkIDRRequest = false;
                        meFrameTypeInfo = FRAME_INFO_UNAVAILABLE;
                        break;

                    case FRAME_INFO_P_FRAME:
                        WFDMMLOGD("First frame is not IDR. Make delayed request");
                        mbCorruptedFrame = true;
                        mbSinkIDRRequest = false;
                        meFrameTypeInfo = FRAME_INFO_UNAVAILABLE;
                        break;

                    default:
                        WFDMMLOGD("Unknown frame info");
                        break;
                 }
            }

            /*------------------------------------------------------------------
              Check if we are recovering from frame loss or there are packet
              loss
            --------------------------------------------------------------------
            */
            pBuffer->nOffset = 0;
            pBuffer->nFlags  = 0;

            if(sampleInfo.nBytesLost)
            {
                mbCorruptedFrame = true;
                WFDMMLOGH1("Parser reports Packet Loss.TS[%llu]",
                    (uint64)(sampleInfo.startTime * mnVideoTimescale));
                if(mnVideoFrameDropMode == FRAME_DROP_DROP_CURRENT_FRAME)
                {
                     WFDMMLOGE("Bytes Lost: Drop current frame");
                     mpVideoQ->Push(&pBuffer, sizeof(OMX_BUFFERHEADERTYPE**));
                     break;
                }
                else
                {
                     WFDMMLOGE1("Bytes Lost: Indicate corrupt frame.TS[%llu]",
                        (uint64)(sampleInfo.startTime * mnVideoTimescale));
                     pBuffer->nFlags |= OMX_BUFFERFLAG_DATACORRUPT;
                }
            }

            startTime = (uint64)(sampleInfo.startTime * mnVideoTimescale);
            if(mbFlushInProgress)
            {
                if(startTime <= mnFlushTimeStamp)
                {
                    if(((mnFlushTimeStamp - startTime)
                        < MEDIASRC_FLUSH_IDR_THRESHOLD) &&
                                   sampleInfo.sync)
                    {
                        mbFlushInProgress = false;
                    }
                    else
                    {
                        mpVideoQ->Push(&pBuffer,sizeof(OMX_BUFFERHEADERTYPE**));
                        break;
                    }
                }
                else if(sampleInfo.sync)
                {
                    mbFlushInProgress = false;
                }
            }
            /*------------------------------------------------------------------
              Set the media base time once
            --------------------------------------------------------------------
            */
            pBuffer->nTimeStamp = (OMX_TICKS)(startTime);

            if(!mbMediaBaseTimeSet ||
              (!mbVideoStarted && (uint64)pBuffer->nTimeStamp < mnMediaBaseTime))
            {
                mbMediaBaseTimeSet = true;
                mnMediaBaseTime = pBuffer->nTimeStamp;
                mbMediaTimeSet = false;
            }
            if(!mbVideoStarted)
            {
                WFDMMLOGH1("Video Buffer set STARTTIME. TS[%llu]",
                    pBuffer->nTimeStamp);
                pBuffer->nFlags |= OMX_BUFFERFLAG_STARTTIME;

                if(mbVideoReconfigured)
                {
                    mbVideoReconfigured = false;
                    SetMediaBaseTime(0);
                }
            }

            if(mbVideoTimeReset &&
              ((mnOldBaseTime == mnNewBaseTime) ||
               ProximityCheck((uint64)sampleInfo.startTime,
                                                  mnOldBaseTime,
                                                  mnNewBaseTime)))
            {
                WFDMMLOGH1("Video Buffer set STARTTIME after VideoTimeReset.TS[%llu]",
                    pBuffer->nTimeStamp);
                pBuffer->nFlags |= OMX_BUFFERFLAG_STARTTIME;
                mbVideoTimeReset = false;
            }

            mbVideoStarted = true;

            if(WFD_UNLIKELY(m_pStatInst))
            {
                m_pStatInst->recordMediaSrcIP(pBuffer);
            }

            /* ALWAYS delay the very first IDR request from sink (initial/after
             * pause-play) by IDR_REQ_INTERVAL. This is to take care of 3rd
             * party sources which may crash upon receiving an IDR request at
             * the very start of playback */
            if(mnLastIDRReqTime == 0)
            {
                /* Update the mnLastIDRReqTime by Current Time to achieve
                 * the aforementioned task*/
                MM_Time_GetTimeEx(&mnLastIDRReqTime);
            }

            CheckBaseTimeChange(sampleInfo.startTime);

            if(!mbMediaTimeSet)
            {
                SetMediaBaseTime((uint64)pBuffer->nTimeStamp /*us*/);
                mbMediaTimeSet = true;
            }

            if(mVideoHeapPtr)
            {
                memcpy(mVideoBufPtr, pDataPtr, nSize);
            }

            /*------------------------------------------------------------------
              Caching is enabled for QSECOM buffer. writeback and invalidate
            --------------------------------------------------------------------
            */
            struct ion_flush_data sFlushData;
            sFlushData.handle = (ion_user_handle_t)mVideoIonHandle;
            sFlushData.fd = 0;
            sFlushData.offset = 0;
            sFlushData.vaddr = mVideoBufPtr;
            sFlushData.length = nSize;

            if(ioctl (mIonFd, ION_IOC_CLEAN_INV_CACHES, &sFlushData) < 0)
            {
                WFDMMLOGE("Cache flush failed");
            }

            WFDMMLOGD2("Decrypt %d %p", (int)mVideoBufFd,
                                                    pBuffer->pBuffer);
            if(mCfg.pFnDecrypt(mClientData,SINK_VIDEO_TRACK_ID, mVideoBufFd,
                               (int64)pBuffer->pBuffer, nSize,
                        (char*)sampleInfo.sSubInfo.sCP.ucInitVector,
                        sampleInfo.sSubInfo.sCP.ucInitVectorSize)!= true)
            {
                WFDMMLOGE("Failed to decrypt Payload");
                mpFnHandler((void*)(int64)mClientData, mnModuleId, WFDMMSINK_DECRYPT_FAILURE,
                            OMX_ErrorHardware, 0);
                nSize = 0;
            }

            if(WFD_UNLIKELY(m_pStatInst))
            {
                m_pStatInst->recordDecryptStatistics(pBuffer);
            }

            WFDMMLOGE1("Video: Timestamp = %lld", pBuffer->nTimeStamp);

            pBuffer->nFilledLen = nSize;

            if(sampleInfo.sync)
            {
                pBuffer->nFlags |= OMX_BUFFERFLAG_SYNCFRAME;
            }

            pBuffer->nFlags |= OMX_BUFFERFLAG_ENDOFFRAME;

            WFDMMLOGM1("Video ETB flags = %x", pBuffer->nFlags);

            if(mpFnFBD)
            {
                mpFnFBD(mClientData, mnModuleId, SINK_VIDEO_TRACK_ID, pBuffer);
            }
            break;

        case FILE_SOURCE_DATA_CODEC_INFO:
            {
                WFDMMLOGM("VIDEO_THREAD_SECURE received FILE_SOURCE_DATA_CODEC_INFO");
                /*--------------------------------------------------------------
                1. Update track information
                2. Update flag to stop fetching further video samples
                3. Notify video decoder reconfiguration to WFDMMSink (ASYNC)
                --------------------------------------------------------------*/

                //1.
                pauseVideoThread();

                //2.
                updateTrackInfo(false);

                //3.
                WFDMMLOGH("Notify Video Decoder reconfiguration");
                NOTIFYVIDEORECONFIGURE;
            }
            break;

        case FILE_SOURCE_DATA_UNDERRUN:
        case FILE_SOURCE_DATA_INSUFFICIENT:
            WFDMMLOGM("FILE_SOURCE_DATA_UNDERRUN");
            mpVideoQ->Push(&pBuffer, sizeof(OMX_BUFFERHEADERTYPE**));
            return FILE_SOURCE_DATA_UNDERRUN;
        case FILE_SOURCE_DATA_REALLOCATE_BUFFER:
            WFDMMLOGE("FILE_SOURCE_DATA_REALLOCATE_BUFFER - Tearing down!!");
            NOTIFYERROR;
            return FILE_SOURCE_DATA_REALLOCATE_BUFFER;
        case FILE_SOURCE_DATA_END:
        case FILE_SOURCE_DATA_ERROR:

        default:
            mpVideoQ->Push(&pBuffer, sizeof(OMX_BUFFERHEADERTYPE**));
            return FILE_SOURCE_DATA_ERROR;
    }
    return FILE_SOURCE_DATA_OK;
}



/*==============================================================================

         FUNCTION:         fetchAudioSample

         DESCRIPTION:
*//**       @brief         checks if a audio buffer is available and fills data
                           and sends to decoder
*//**
@par     DEPENDENCIES:
                           None
*//*
         PARAMETERS:
*//**       @param         nTrackId - helps in multiple audio case

*//*     RETURN VALUE:
*//**       @return
                           None
@par     SIDE EFFECTS:
                           None
*//*==========================================================================*/
int WFDMMSinkMediaSource::fetchAudioSample(uint32 nTrackId)
{
    (void) nTrackId;
    OMX_BUFFERHEADERTYPE *pBuffer = NULL;

    OMX_ERRORTYPE result;
    uint64 startTime = 0;

    WFDMMLOGM("FetchAudioSampe");
    /*--------------------------------------------------------------------------
      Check if there are any buffers returned back by the decoder
    ----------------------------------------------------------------------------
    */
    result = mpAudioQ->Pop(&pBuffer,
                          sizeof(pBuffer),
                          3);

    if(result != OMX_ErrorNone || !pBuffer)
    {
        WFDMMLOGM("No Buffer available for Audio. Continue");
        return FILE_SOURCE_DATA_INSUFFICIENT;
    }

    /*--------------------------------------------------------------------------
     Now we have an empty Buffer. Pull data from parser if available
    ----------------------------------------------------------------------------
    */
    FileSourceSampleInfo  sampleInfo;
    FileSourceMediaStatus mediaStatus = FILE_SOURCE_DATA_OK;
    pBuffer->nFilledLen = pBuffer->nAllocLen;

    mediaStatus = mpFileSource->GetNextMediaSample
                                                 (nTrackId,
                                                  pBuffer->pBuffer,
                                                  (uint32*)&pBuffer->nFilledLen,
                                                  sampleInfo);

    switch(mediaStatus)
    {
        case FILE_SOURCE_DATA_OK:
            /*------------------------------------------------------------------
              Set the media base time once
            --------------------------------------------------------------------
            */
            startTime = (uint64)(sampleInfo.startTime * mnAudioTimescale);
            pBuffer->nTimeStamp = (OMX_TICKS)(startTime);
            pBuffer->nOffset = 0;
            pBuffer->nFlags  = 0;

            if(!mbMediaBaseTimeSet ||
              (!mbAudioStarted && (uint64)pBuffer->nTimeStamp < mnMediaBaseTime))
            {
                mbMediaBaseTimeSet = true;
                mnMediaBaseTime = pBuffer->nTimeStamp;
                mbMediaTimeSet = false;
            }

            CheckBaseTimeChange(sampleInfo.startTime);

            if(!mbMediaTimeSet)
            {
                SetMediaBaseTime((uint64)pBuffer->nTimeStamp/*us*/);
                mbMediaTimeSet = true;
            }
            if(!mbAudioStarted)
            {
                WFDMMLOGH("Audio Buffer set STARTTIME");
                pBuffer->nFlags |= OMX_BUFFERFLAG_STARTTIME;

                if(mbAudioReconfigured)
                {
                    mbAudioReconfigured = false;
                    SetMediaBaseTime(0);
                }
            }
            if(mbAudioTimeReset)
            {
              WFDMMLOGE("MediaSource Audio TIme reset");
              if((mnOldBaseTime == mnNewBaseTime) ||
                  ProximityCheck((uint64)sampleInfo.startTime,
                                                  mnOldBaseTime,
                                                  mnNewBaseTime))
              {
                WFDMMLOGH("Audio Buffer set STARTTIME after AudioTimeReset");
                pBuffer->nFlags |= OMX_BUFFERFLAG_STARTTIME;
                mbAudioTimeReset = false;
              }
            }

            mbAudioStarted = true;
#if 0
            if(mCfg.eAudioFmt == WFD_AUDIO_LPCM)
            {
                uint16 *pSrc = (uint16*)(pBuffer->pBuffer + 4);
                uint16 *pDst = (uint16*)(pBuffer->pBuffer);
                uint16 nVal;

                for(uint32 i = 0; i < (pBuffer->nFilledLen - 4)/2 ; i++)
                {
                    nVal = *pSrc++;

                    *pDst++ = (nVal >> 8) | (nVal << 8);

                }
                pBuffer->nFilledLen -= 4;
            }
#endif
            WFDMMLOGE1("Audio: Timestamp = %lld", pBuffer->nTimeStamp);
            if(mbFlushInProgress)
            {
                if(startTime <= mnFlushTimeStamp)
                {
                     mpAudioQ->Push(&pBuffer, sizeof(OMX_BUFFERHEADERTYPE**));
                     break;
                }
                else
                {
                     mbFlushInProgress = false;
                }
            }
            if(mpFnFBD)
            {
                mpFnFBD(mClientData, mnModuleId, SINK_AUDIO_TRACK_ID, pBuffer);
            }
            break;

        case FILE_SOURCE_DATA_CODEC_INFO:
            {
                WFDMMLOGH("MediaSource: Ignore Audio CODEC_CHANGE. PushBack");
                mpAudioQ->Push(&pBuffer, sizeof(OMX_BUFFERHEADERTYPE**));
#if 0
                WFDMMLOGM("AUDIO_THREAD received FILE_SOURCE_DATA_CODEC_INFO");
                /*--------------------------------------------------------------
                1. Update track information
                2. Update flag to stop fetching further video samples
                3. Notify video decoder reconfiguration to WFDMMSink (ASYNC)
                --------------------------------------------------------------*/

                //1.
                updateTrackInfo(false);

                //2.
                pauseAudioThread();

                //3.
                WFDMMLOGH("Notify Audio Decoder reconfiguration");
                NOTIFYAUDIORECONFIGURE;
#endif
            }
            break;

        case FILE_SOURCE_DATA_UNDERRUN:
        case FILE_SOURCE_DATA_INSUFFICIENT:
            WFDMMLOGM("FILE_SOURCE_DATA_UNDERRUN");
            mpAudioQ->Push(&pBuffer, sizeof(OMX_BUFFERHEADERTYPE**));
            return FILE_SOURCE_DATA_UNDERRUN;

        case FILE_SOURCE_DATA_END:
        case FILE_SOURCE_DATA_ERROR:

        default:
            mpAudioQ->Push(&pBuffer, sizeof(OMX_BUFFERHEADERTYPE**));
            return FILE_SOURCE_DATA_ERROR;
    }
    return FILE_SOURCE_DATA_OK;

}

/*==============================================================================

         FUNCTION:         fetchAudioSampleSecure

         DESCRIPTION:
*//**       @brief         checks if a audio buffer is available and fills data
                           decrypts it and sends to decoder
*//**
@par     DEPENDENCIES:
                           None
*//*
         PARAMETERS:
*//**       @param         nTrackId - helps in multiple audio case

*//*     RETURN VALUE:
*//**       @return
                           None
@par     SIDE EFFECTS:
                           None
*//*==========================================================================*/
int WFDMMSinkMediaSource::fetchAudioSampleSecure(uint32 nTrackId)
{
    (void) nTrackId;
    OMX_BUFFERHEADERTYPE *pBuffer = NULL;

    OMX_ERRORTYPE result;
    uint64 startTime = 0;

    WFDMMLOGH("FetchAudioSampe");
    /*--------------------------------------------------------------------------
      Check if there are any buffers returned back by the decoder
    ----------------------------------------------------------------------------
    */
    result = mpAudioQ->Pop(&pBuffer,
                          sizeof(pBuffer),
                          3);

    if(result != OMX_ErrorNone || !pBuffer)
    {
        WFDMMLOGM("No Buffer available for Audio. Continue");
        return FILE_SOURCE_DATA_ERROR;
    }

    if(!mAudioBufPtr)
    {
        WFDMMLOGE("No ION working Buf for audio");
        return FILE_SOURCE_DATA_ERROR;
    }
    /*--------------------------------------------------------------------------
     Now we have an empty Buffer. Pull data from parser if available
    ----------------------------------------------------------------------------
    */
    FileSourceSampleInfo  sampleInfo;
    FileSourceMediaStatus mediaStatus = FILE_SOURCE_DATA_OK;
    pBuffer->nFilledLen = pBuffer->nAllocLen;

    mediaStatus = mpFileSource->GetNextMediaSample(nTrackId,
                                                   mAudioBufPtr,
                                                   (uint32*)&pBuffer->nFilledLen,
                                                   sampleInfo);

    switch(mediaStatus)
    {
        case FILE_SOURCE_DATA_OK:
            /*------------------------------------------------------------------
              Set the media base time once
            --------------------------------------------------------------------
            */
            startTime = (uint64)(sampleInfo.startTime * mnAudioTimescale);
            pBuffer->nTimeStamp = (OMX_TICKS)(startTime);
            pBuffer->nOffset = 0;
            pBuffer->nFlags  = 0;

            if(!mbMediaBaseTimeSet ||
              (!mbAudioStarted && (uint64)pBuffer->nTimeStamp < mnMediaBaseTime))
            {
                mbMediaBaseTimeSet = true;
                mnMediaBaseTime = pBuffer->nTimeStamp;
                mbMediaTimeSet = false;
            }

            CheckBaseTimeChange(sampleInfo.startTime);

            if(!mbMediaTimeSet)
            {
                SetMediaBaseTime((uint64)pBuffer->nTimeStamp/*us*/);
                mbMediaTimeSet = true;
            }
            if(!mbAudioStarted)
            {
                WFDMMLOGH("Audio Buffer set STARTTIME");
                pBuffer->nFlags |= OMX_BUFFERFLAG_STARTTIME;

                if(mbAudioReconfigured)
                {
                    mbAudioReconfigured = false;
                    SetMediaBaseTime(0);
                }
            }
            if(mbAudioTimeReset &&
              ((mnOldBaseTime == mnNewBaseTime) ||
               ProximityCheck((uint64)sampleInfo.startTime,
                                                  mnOldBaseTime,
                                                  mnNewBaseTime)))
            {
                WFDMMLOGH("Audio Buffer set STARTTIME after AudioTimeReset");
                pBuffer->nFlags |= OMX_BUFFERFLAG_STARTTIME;
                mbAudioTimeReset = false;
            }

            mbAudioStarted = true;

            if(sampleInfo.sSubInfo.sCP.ucIsEncrypted && sampleInfo.sSubInfo.sCP.ucInitVectorSize)
            {
                /*--------------------------------------------------------------
                  Caching is enabled for QSECOM buffer. writeback and invalidate
                ----------------------------------------------------------------
                */
                struct ion_flush_data sFlushData;
                sFlushData.handle = (ion_user_handle_t)mAudioIonHandle;
                sFlushData.fd = 0;
                sFlushData.offset = 0;
                sFlushData.vaddr = mAudioBufPtr;
                sFlushData.length = pBuffer->nFilledLen;

                if(ioctl (mIonFd, ION_IOC_CLEAN_INV_CACHES, &sFlushData) < 0)
                {
                    WFDMMLOGE("Cache flush failed");
                }

                WFDMMLOGD2("Decrypt Audio %d %d", (int)mAudioBufFd,
                                                    (uint32)mAudioBufFd);

                if(mCfg.pFnDecrypt(mClientData, SINK_AUDIO_TRACK_ID,
                            mAudioBufFd,
                            mAudioBufFd,
                            pBuffer->nFilledLen,
                           (char*)sampleInfo.sSubInfo.sCP.ucInitVector,
                            sampleInfo.sSubInfo.sCP.ucInitVectorSize)!= true)
                {
                    WFDMMLOGE("Failed to decrypt Payload");
                    mpFnHandler((void*)(int64)mClientData, mnModuleId,
                                WFDMMSINK_DECRYPT_FAILURE,
                                OMX_ErrorHardware, 0);
                    mpAudioQ->Push(&pBuffer, sizeof(OMX_BUFFERHEADERTYPE**));
                    return -1;
                }
                if(mCfg.eAudioFmt == WFD_AUDIO_LPCM)
                {
                    uint16 *pSrc =
                           (uint16*)(mAudioBufPtr + AAC_SUBSTREAM_HEADER_SIZE);

                    uint16 *pDst = (uint16*)(pBuffer->pBuffer);
                    uint16 nVal;

                    /*----------------------------------------------------------
                     Remove substream ID and SWAP the bytes to make it
                     Signed LPCM 16bit LE
                    ------------------------------------------------------------
                    */
                    for(uint32 i = 0;
                        i < (pBuffer->nFilledLen - AAC_SUBSTREAM_HEADER_SIZE)/2;
                        i++)
                    {
                        nVal    = *pSrc++;
                        *pDst++ = (nVal >> 8) | (nVal << 8);
                    }
                    pBuffer->nFilledLen -= AAC_SUBSTREAM_HEADER_SIZE;
                }
                else
                {
                    memcpy(pBuffer->pBuffer, mAudioBufPtr, pBuffer->nFilledLen);
                }
            }
            else
            {
                if(mCfg.eAudioFmt == WFD_AUDIO_LPCM)
                {
                    memcpy(pBuffer->pBuffer,
                           mAudioBufPtr + AAC_SUBSTREAM_HEADER_SIZE,
                           pBuffer->nFilledLen - AAC_SUBSTREAM_HEADER_SIZE);

                    pBuffer->nFilledLen -= AAC_SUBSTREAM_HEADER_SIZE;
                }
                else
                {
                    memcpy(pBuffer->pBuffer, mAudioBufPtr, pBuffer->nFilledLen);
                }
            }
            if(mpFnFBD)
            {
                mpFnFBD(mClientData, mnModuleId, SINK_AUDIO_TRACK_ID, pBuffer);
            }
            break;

        case FILE_SOURCE_DATA_CODEC_INFO:
            {
                WFDMMLOGM("AUDIO_THREAD_SECURE ignore FILE_SOURCE_DATA_CODEC_INFO");
                mpAudioQ->Push(&pBuffer, sizeof(OMX_BUFFERHEADERTYPE**));
#if 0
                /*--------------------------------------------------------------
                1. Update track information
                2. Update flag to stop fetching further video samples
                3. Notify video decoder reconfiguration to WFDMMSink (ASYNC)
                --------------------------------------------------------------*/

                //1.
                updateTrackInfo(false);

                //2.
                pauseAudioThread();

                //3.
                WFDMMLOGH("Notify Audio Decoder reconfiguration");
                NOTIFYAUDIORECONFIGURE;
#endif
            }
            break;

        case FILE_SOURCE_DATA_UNDERRUN:
        case FILE_SOURCE_DATA_INSUFFICIENT:
            WFDMMLOGM("FILE_SOURCE_DATA_UNDERRUN");
            mpAudioQ->Push(&pBuffer, sizeof(OMX_BUFFERHEADERTYPE**));
            return FILE_SOURCE_DATA_UNDERRUN;

        case FILE_SOURCE_DATA_END:
        case FILE_SOURCE_DATA_ERROR:

        default:
            mpAudioQ->Push(&pBuffer, sizeof(OMX_BUFFERHEADERTYPE**));
            return FILE_SOURCE_DATA_ERROR;
    }
    return FILE_SOURCE_DATA_OK;

}

/*==============================================================================

         FUNCTION:         fetchGenericSample

         DESCRIPTION:
*//**       @brief         checks if a genric buffer is available and fills data
                           and sends to decoder
*//**
@par     DEPENDENCIES:
                           None
*//*
         PARAMETERS:
*//**       @param         nTrackId - helps in multiple generic data

*//*     RETURN VALUE:
*//**       @return
                           None
@par     SIDE EFFECTS:
                           None
*//*==========================================================================*/
int WFDMMSinkMediaSource::fetchGenericSample(uint32 nTrackId)
{
    (void) nTrackId;
    OMX_BUFFERHEADERTYPE *pBuffer = NULL;

    OMX_ERRORTYPE result;
    uint64 startTime = 0;

    WFDMMLOGM("FetchGenericSample");
    /*--------------------------------------------------------------------------
      Check if there are any buffers returned back by the decoder
    ----------------------------------------------------------------------------
    */
    result = mpGenericQ->Pop(&pBuffer,
                          sizeof(pBuffer),
                          3);

    if(result != OMX_ErrorNone || !pBuffer)
    {
        WFDMMLOGM("No Buffer available for Generic track. Continue");
        return FILE_SOURCE_DATA_INSUFFICIENT;
    }

    /*--------------------------------------------------------------------------
     Now we have an empty Buffer. Pull data from parser if available
    ----------------------------------------------------------------------------
    */
    FileSourceSampleInfo  sampleInfo;
    FileSourceMediaStatus mediaStatus = FILE_SOURCE_DATA_OK;
    pBuffer->nFilledLen = pBuffer->nAllocLen;

    mediaStatus = mpFileSource->GetNextMediaSample
                                                 (nTrackId,
                                                  pBuffer->pBuffer,
                                                  (uint32*)&pBuffer->nFilledLen,
                                                  sampleInfo);

    switch(mediaStatus)
    {
        case FILE_SOURCE_DATA_OK:
            startTime = (uint64)(sampleInfo.startTime * mnGenericTimescale);
            pBuffer->nTimeStamp = (OMX_TICKS)(startTime);
            pBuffer->nOffset = 0;
            pBuffer->nFlags  = 0;

            WFDMMLOGE1("Generic: GetNextMediaSample : Timestamp = %lld", pBuffer->nTimeStamp);

            if(!mbMediaBaseTimeSet ||
              (!mbGenericStarted && (uint64)pBuffer->nTimeStamp < mnMediaBaseTime))
            {
                mbMediaBaseTimeSet = true;
                mnMediaBaseTime = pBuffer->nTimeStamp;
                mbMediaTimeSet = false;
            }

            CheckBaseTimeChange(sampleInfo.startTime);

            if(!mbMediaTimeSet)
            {
                SetMediaBaseTime((uint64)pBuffer->nTimeStamp/*us*/);
                mbMediaTimeSet = true;
            }
            if(!mbGenericStarted)
            {
                WFDMMLOGH("Generic Buffer set STARTTIME");
                pBuffer->nFlags |= OMX_BUFFERFLAG_STARTTIME;
            }
            if(mbGenericTimeReset &&
              ((mnOldBaseTime == mnNewBaseTime) ||
               ProximityCheck((uint64)sampleInfo.startTime,
                                                  mnOldBaseTime,
                                                  mnNewBaseTime)))
            {
                WFDMMLOGH("Generic Buffer set STARTTIME after GenericTimeReset");
                pBuffer->nFlags |= OMX_BUFFERFLAG_STARTTIME;
                mbGenericTimeReset = false;
            }

            mbGenericStarted = true;

            if(mbFlushInProgress)
            {
                if(startTime <= mnFlushTimeStamp)
                {
                     mpGenericQ->Push(&pBuffer, sizeof(OMX_BUFFERHEADERTYPE**));
                     break;
                }
                else
                {
                     mbFlushInProgress = false;
                }
            }
            if(mpFnFBD)
            {
                mpFnFBD(mClientData, mnModuleId, SINK_GENERIC_TRACK_ID, pBuffer);
            }
            break;

        case FILE_SOURCE_DATA_CODEC_INFO:
            {
                /* This is not expected for generic track as of now
                   But we might need it if changing from Png to Jpg*/
                WFDMMLOGE("GENERIC_THREAD received FILE_SOURCE_DATA_CODEC_INFO : NOP");
                mpGenericQ->Push(&pBuffer, sizeof(OMX_BUFFERHEADERTYPE**));
            }
            break;
        case FILE_SOURCE_DATA_UNDERRUN:
                WFDMMLOGL("GENERIC_THREAD received FILE_SOURCE_DATA_UNDERRUN");
        case FILE_SOURCE_DATA_REALLOCATE_BUFFER:
        case FILE_SOURCE_DATA_INSUFFICIENT:
            //WFDMMLOGE1("FetchGenericSample: GetNextMediaSample : status = %d",mediaStatus);
            mpGenericQ->Push(&pBuffer, sizeof(OMX_BUFFERHEADERTYPE**));
            return FILE_SOURCE_DATA_UNDERRUN;

        case FILE_SOURCE_DATA_END:
        case FILE_SOURCE_DATA_ERROR:

        default:
            mpGenericQ->Push(&pBuffer, sizeof(OMX_BUFFERHEADERTYPE**));
            return FILE_SOURCE_DATA_ERROR;
    }
    return FILE_SOURCE_DATA_OK;

}

/*==============================================================================

         FUNCTION:         fetchGenericSampleSecure

         DESCRIPTION:
*//**       @brief         checks if a genric buffer is available and fills data
                           and sends to decoder
*//**
@par     DEPENDENCIES:
                           None
*//*
         PARAMETERS:
*//**       @param         nTrackId - helps in multiple generic data

*//*     RETURN VALUE:
*//**       @return
                           None
@par     SIDE EFFECTS:
                           None
*//*==========================================================================*/
int WFDMMSinkMediaSource::fetchGenericSampleSecure(uint32 nTrackId)
{
    return fetchGenericSample(nTrackId);
}

/*==============================================================================

         FUNCTION:          setFreeBuffer

         DESCRIPTION:
*//**       @brief          gives the empty buffer back to media source
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
OMX_ERRORTYPE WFDMMSinkMediaSource::setFreeBuffer
(
    int trackId,
    OMX_BUFFERHEADERTYPE *pBuffer
)
{
    if(pBuffer)
    {
        if((trackId == SINK_AUDIO_TRACK_ID) && mpAudioQ)
        {
            mpAudioQ->Push(&pBuffer, sizeof(&pBuffer));
        }
        else if((trackId == SINK_VIDEO_TRACK_ID) && mpVideoQ)
        {
            mpVideoQ->Push(&pBuffer, sizeof(&pBuffer));
        }
        else if((trackId == SINK_GENERIC_TRACK_ID)&& mpGenericQ)
        {
            mpGenericQ->Push(&pBuffer, sizeof(&pBuffer));
        }
    }

    return OMX_ErrorNone;
}

/*==============================================================================

         FUNCTION:         configureDataSource

         DESCRIPTION:
*//**       @brief         configures the RTP modue
*//**
@par     DEPENDENCIES:
                           None
*//*
         PARAMETERS:
*//**       @param         None

*//*     RETURN VALUE:
*//**       @return
                           bool - true for success
                                - false for failure
@par     SIDE EFFECTS:
                           None
*//*==========================================================================*/
bool WFDMMSinkMediaSource::configureDataSource()
{
    mpRTPStreamPort = mCfg.pRTPStreamPort;
    mpRTPStreamPort->Start();
    return true;

}

/*==============================================================================

         FUNCTION:         configureHDCPResources

         DESCRIPTION:
*//**       @brief         configures the HDCP related resources
*//**
@par     DEPENDENCIES:
                           None
*//*
         PARAMETERS:
*//**       @param         None

*//*     RETURN VALUE:
*//**       @return
                           bool - true for success
                                - false for failure
@par     SIDE EFFECTS:
                           None
*//*==========================================================================*/
bool WFDMMSinkMediaSource::configureHDCPResources()
{
    AllocateIonBufs();
    return true;
}

/*==============================================================================

         FUNCTION:         configureParser

         DESCRIPTION:
*//**       @brief         configures the Parser
*//**
@par     DEPENDENCIES:
                           None
*//*
         PARAMETERS:
*//**       @param         None

*//*     RETURN VALUE:
*//**       @return
                           bool - true for success
                                - false for failure
@par     SIDE EFFECTS:
                           None
*//*==========================================================================*/
bool WFDMMSinkMediaSource::configureParser()
{
    if(!mpRTPStreamPort)
    {
        WFDMMLOGE("RTPStreamPort Not ready");
        return false;
    }

    mpFileSource = MM_New_Args (FileSource, (fileSourceCallback, this));

    if(!mpFileSource)
    {
        WFDMMLOGE("Failed to create Parser Instance");
        return false;
    }

    WFDMMLOGH("Calling MMParser OPENFILE");

    if(mpFileSource->OpenFile((video::iStreamPort*)mpRTPStreamPort,
                              FILE_SOURCE_WFD_MP2TS,
                              false) != FILE_SOURCE_SUCCESS)
    {
        WFDMMLOGE("Failed to FileSource->OpenFile");
        return false;
    }

    WFDMMLOGH("Initialized Parser");
    return true;
}


/*==============================================================================

         FUNCTION:         fileSourceCallback

         DESCRIPTION:
*//**       @brief         static function that receives filesource callback
*//**
@par     DEPENDENCIES:
                           None
*//*
         PARAMETERS:
*//**       @param         status - Filesource status
                           pThis  - pointer to the current object

*//*     RETURN VALUE:
*//**       @return
                           None
@par     SIDE EFFECTS:
                           None
*//*==========================================================================*/
void WFDMMSinkMediaSource::fileSourceCallback
(
    FileSourceCallBackStatus status,
    void *pThis
)
{
    WFDMMSinkMediaSource *pMe = (WFDMMSinkMediaSource*)pThis;
    if(pMe)
    {
        pMe->fileSourceCallbackHandler(status,pThis);
    }
    else
    {
        /*----------------------------------------------------------------------
          Invalid this pointer.
        ------------------------------------------------------------------------
        */
    }
}

/*==============================================================================

         FUNCTION:         fileSourceCallbackHandler

         DESCRIPTION:
*//**       @brief         Handles filesource callback
*//**
@par     DEPENDENCIES:
                           None
*//*
         PARAMETERS:
*//**       @param         status: Filesource status

*//*     RETURN VALUE:
*//**       @return
                           None
@par     SIDE EFFECTS:
                           None
*//*==========================================================================*/
void WFDMMSinkMediaSource::fileSourceCallbackHandler
(
    FileSourceCallBackStatus status,
    void *pThis
)
{
    WFDMMLOGH1("Parser callback: status = %d", status);
    FileSourceConfigItem sItem;

    mnActualBaseTime = 0;
    WFDMMLOGH1("MediaSource Actual Base Time to parser %llu",mnActualBaseTime);

    switch(status)
    {
    case FILE_SOURCE_OPEN_COMPLETE:
        {
            WFDMMLOGH("OPEN_COMPLETE callback from parser. Update track info.");
            WFDMMSinkMediaSource *pMe = (WFDMMSinkMediaSource*)pThis;
            if(pMe)
            {
                pMe->updateTrackInfo(true);
            }
            else
            {
                /*----------------------------------------------------------------------
                  Invalid this pointer.
                ------------------------------------------------------------------------
                */
            }
        }
        break;
    case FILE_SOURCE_OPEN_FAIL:
            /*------------------------------------------------------------------
             Parser failed to find valid tracks in the stream. Cannot recover
             Give runtime Error.
            --------------------------------------------------------------------
            */
        WFDMMLOGE("MediaSource: Filesource failed to open file");
        NOTIFYERROR;
        break;
    case FILE_SOURCE_SEEK_COMPLETE:
            /*------------------------------------------------------------------
             Not of much intereset
            --------------------------------------------------------------------
            */
        break;
    case FILE_SOURCE_SEEK_UNDERRUN:
        break;
    case FILE_SOURCE_SEEK_FAIL:
        /*----------------------------------------------------------------------
          Who seeks in Wifi display
        ------------------------------------------------------------------------
        */
    case FILE_SOURCE_ERROR_ABORT:
    case FILE_SOURCE_SEEK_UNDERRUN_IN_FRAGMENT:
    default:
        /*----------------------------------------------------------------------
         Dont know what to do here :(. Give runtime error
        ------------------------------------------------------------------------
        */
        break;
    }
}


/*==============================================================================

         FUNCTION:         updateTrackInfo

         DESCRIPTION:
*//**       @brief         Fetches track based meta information
*//**
@par     DEPENDENCIES:
                           None
*//*
         PARAMETERS:
*//**       @param         status: Filesource status

*//*     RETURN VALUE:
*//**       @return
                           None
@par     SIDE EFFECTS:
                           None
*//*==========================================================================*/
void WFDMMSinkMediaSource::updateTrackInfo(bool bOnOpenComplete)
{
    /*--------------------------------------------------------------------------
     In case File source has succesfully opened the file -> Find the tracks,
     create any dynamic resources pending and trigger data fetch.

     In case DATA_CODEC_INFO is received from parser -> update track information
     [Currently updates information for all available tracks]
     FIXME: Update only relevant track information?
    ----------------------------------------------------------------------------
    */
    FileSourceTrackIdInfoType trackList[12];
    int32 numTracks = mpFileSource->GetWholeTracksIDList(trackList);

    WFDMMLOGH1("Number of detected tracks %d", numTracks);

    FileSourceConfigItem sItem;
    mnActualBaseTime = 0;
    WFDMMLOGH1("MediaSource Actual Base Time to parser %llu",mnActualBaseTime);

    for(int i = 0; i < numTracks; i++)
    {
        FileSourceMjMediaType majorType;
        FileSourceMnMediaType minorType;
        const char* pMimeType =NULL;
        FileSourceStatus status = mpFileSource->GetMimeType
                                           (trackList[i].id, majorType,
                                            minorType);

        MediaTrackInfo mediaTrackInfo;
        status = mpFileSource->GetMediaTrackInfo(trackList[i].id,
                                                 &mediaTrackInfo);

        if(status != FILE_SOURCE_SUCCESS)
        {
            WFDMMLOGE("Failed to get MediaTrackInfo");
            continue;
        }

        if(FILE_SOURCE_MJ_TYPE_VIDEO == trackList[i].majorType)
        {

            mVideoTrackId = trackList[i].id;
            if(mediaTrackInfo.videoTrackInfo.timeScale > 0)
            {
                mnVideoTimescale = (MEDIASRC_TIMESCALE_CONVERSION /
                                mediaTrackInfo.videoTrackInfo.timeScale);
            }
            WFDMMLOGH1("Video Time Scale = %lu",mnVideoTimescale);

            WFDMMLOGH2("Found Video Track height = %d width = %d",
                        mediaTrackInfo.videoTrackInfo.frameHeight,
                        mediaTrackInfo.videoTrackInfo.frameWidth);

            //Get the size of the format block.
            status = mpFileSource->GetFormatBlock(mVideoTrackId,
                                                  NULL,
                                                  &mnVideoFmtBlkSize);

            WFDMMLOGH1("VideoFormat Size = %d", mnVideoFmtBlkSize);

            if((status==FILE_SOURCE_SUCCESS) && mnVideoFmtBlkSize!=0)
            {
                mpVideoFmtBlk = (uint8 *)MM_Malloc(mnVideoFmtBlkSize);

                if(!mpVideoFmtBlk)
                {
                    WFDMMLOGE("Failed to allocate FormatBlock");
                    continue;
                }

                status = mpFileSource->GetFormatBlock
                                                (mVideoTrackId,
                                                 mpVideoFmtBlk,
                                                &mnVideoFmtBlkSize);

                if(status != FILE_SOURCE_SUCCESS)
                {
                    WFDMMLOGE("GetFormatBlock failed");
                }
            }

            if(bOnOpenComplete)
            {
                ++mnTracks;

                sItem.nresult = (unsigned long long)0;
                mpFileSource->SetConfiguration(
                               mVideoTrackId,
                               &sItem,
                               FILE_SOURCE_MEDIA_BASETIME);

                if(mpVideoThread && ISSTATEOPENED)
                {
                    mpVideoThread->SetSignal(0);
                }
            }
        }
        else if(FILE_SOURCE_MJ_TYPE_AUDIO == trackList[i].majorType)
        {
            WFDMMLOGH1("Found Audio Track %d", trackList[i].id);
            mAudioTrackId = trackList[i].id;
            if(mediaTrackInfo.audioTrackInfo.timeScale > 0)
            {
                mnAudioTimescale = (MEDIASRC_TIMESCALE_CONVERSION /
                                mediaTrackInfo.audioTrackInfo.timeScale);
            }
            WFDMMLOGH1("Audio Time Scale = %lu",mnAudioTimescale);

            mnAudioMaxBufferSize =
               mpFileSource->GetTrackMaxFrameBufferSize(
                                              trackList[i].id);

            WFDMMLOGH4("Channels-%d SampleRate-%d BitRate-%d MaxBufSiz-%d",
                   (int)mediaTrackInfo.audioTrackInfo.numChannels,
                   (int)mediaTrackInfo.audioTrackInfo.samplingRate,
                   (int)mediaTrackInfo.audioTrackInfo.bitRate,
                   (int)mnAudioMaxBufferSize);


            if(minorType == FILE_SOURCE_MN_TYPE_AAC)
            {
                FileSourceConfigItem sItem;
                mpFileSource->SetConfiguration(
                               mAudioTrackId,
                               &sItem,
                               FILE_SOURCE_MEDIA_OUTPUT_SINGLE_AUDIO_FRAME);

                status = mpFileSource->GetFormatBlock(mAudioTrackId,
                                                     NULL,
                                                   &mnAudioFmtBlkSize);
                if((status == FILE_SOURCE_SUCCESS)
                                && mnAudioFmtBlkSize!=0)
                {
                    mpAudioFmtBlk =
                               (uint8 *)MM_Malloc(mnAudioFmtBlkSize);
                    if(!mpAudioFmtBlk)
                    {
                        WFDMMLOGE("Failed to allocate FormatBlock");
                        continue;
                    }
                    status = mpFileSource->GetFormatBlock
                                                  (mAudioTrackId,
                                                   mpAudioFmtBlk,
                                                  &mnAudioFmtBlkSize);

                    if(status != FILE_SOURCE_SUCCESS)
                    {
                        WFDMMLOGE("GetFormatBlock failed");
                    }
                }
            }
            else if(minorType == FILE_SOURCE_MN_TYPE_PCM &&
              !mCfg.bSecure)
            {
                FileSourceConfigItem sItem;
                mpFileSource->SetConfiguration(
                               mAudioTrackId,
                               &sItem,
                               FILE_SOURCE_MEDIA_STRIP_AUDIO_HEADER);
            }
            else if(minorType == FILE_SOURCE_MN_TYPE_AC3)
            {

            }

            if(bOnOpenComplete)
            {
                ++mnTracks;

                sItem.nresult = (unsigned long long)0;
                mpFileSource->SetConfiguration(
                               mAudioTrackId,
                               &sItem,
                               FILE_SOURCE_MEDIA_BASETIME);

                if(mpAudioThread && ISSTATEOPENED)
                {
                    mpAudioThread->SetSignal(0);
                }
            }
        }
        else if(FILE_SOURCE_MJ_TYPE_IMAGE == trackList[i].majorType)
        {
            WFDMMLOGH1("Found Image Track %lu", trackList[i].id);
            mGenericTrackId = trackList[i].id;
            if(mediaTrackInfo.audioTrackInfo.timeScale > 0)
            {
                mnGenericTimescale = (MEDIASRC_TIMESCALE_CONVERSION /
                                mediaTrackInfo.sImageTrackInfo.ulTimeScale);
            }
            WFDMMLOGH1("Generic Time Scale = %lu",mnGenericTimescale);
            WFDMMLOGH2("Found Generic Track height = %lu width = %lu",
                        mediaTrackInfo.sImageTrackInfo.ulImageHeight,
                        mediaTrackInfo.sImageTrackInfo.ulImageWidth);
            if(bOnOpenComplete)
            {
                ++mnTracks;
                sItem.nresult = (unsigned long long)0;
                mpFileSource->SetConfiguration(
                               mGenericTrackId,
                               &sItem,
                               FILE_SOURCE_MEDIA_BASETIME);
                if(mpGenericThread && ISSTATEOPENED)
                {
                    WFDMMLOGH("Setting signal on Generic Thread");
                    mpGenericThread->SetSignal(0);
                }
            }
        }
    }
}

int WFDMMSinkMediaSource::state(int state, bool get_true_set_false)
{
    CRITICAL_SECT_ENTER;

    if(get_true_set_false == true)
    {
        /*----------------------------------------------------------------------
         Means get
        ------------------------------------------------------------------------
        */
    }
    else
    {
        meState = state;
    }

    CRITICAL_SECT_LEAVE;

    return meState;
}

/*==============================================================================

         FUNCTION:          Start

         DESCRIPTION:
*//**       @brief          Starts the media Source thread
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
OMX_ERRORTYPE WFDMMSinkMediaSource::Start()
{
    if(ISSTATEINIT)
    {
        meState = OPENED;

        if(mnTracks > 0)
        {
            if(mpVideoThread)
            {
                mpVideoThread->SetSignal(0);
            }
            if(mpAudioThread)
            {
                mpAudioThread->SetSignal(0);
            }
            if(mpGenericThread)
            {
                mpGenericThread->SetSignal(0);
            }
        }
    }
    else
    {
        WFDMMLOGE1("Start Called in Invalid state %d", meState);
        return OMX_ErrorInvalidState;
    }
    return OMX_ErrorNone;
}

/*==============================================================================

         FUNCTION:          Stop

         DESCRIPTION:
*//**       @brief          Stops the media source thread
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
OMX_ERRORTYPE WFDMMSinkMediaSource::Stop()
{
    if(ISSTATEOPENED)
    {
        SETSTATECLOSING;
    }
    else
    {
        WFDMMLOGE1("STOP Called in Invalid state %d", meState);
    }
    return OMX_ErrorNone;
}

/*==============================================================================

         FUNCTION:          AllocateIonBufs

         DESCRIPTION:
*//**       @brief          Allocates Ion Buffers for HDCP session
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
bool WFDMMSinkMediaSource::AllocateIonBufs()
{
    static struct ion_allocation_data sAllocInfo;
    static struct ion_fd_data sFdInfo;

    WFDMMLOGD("WFDMMSinkMediaSource: Allocate ION Buffers");

    mIonFd = open("/dev/ion",  O_RDONLY);

    if(mIonFd < 1)
    {
        WFDMMLOGE("WFDMMSinkMediaSource Failed to open ION device");
    }
    else
    {
        int32_t ret;

        memset(&sFdInfo, 0, sizeof(sFdInfo));
        memset(&sAllocInfo, 0, sizeof(sAllocInfo));

        /*----------------------------------------------------------------------
          ALlocate AUdio ION buffers
        ------------------------------------------------------------------------
        */
        sAllocInfo.len = 0;
        sAllocInfo.align = 4096;
        sAllocInfo.heap_id_mask = ION_HEAP(ION_QSECOM_HEAP_ID);
        sAllocInfo.handle = 0;
        sAllocInfo.flags |= ION_FLAG_CACHED;
        mAudioBufSize = 8192;
        mAudioBufSize = ((mAudioBufSize + (4096 -1)) >> 12) << 12;

        sAllocInfo.len = mAudioBufSize;
        ret = ioctl(mIonFd, ION_IOC_ALLOC, &sAllocInfo);

        if (ret != 0  || sAllocInfo.handle == 0)
        {
            WFDMMLOGE2("Failed to Alloc ION buf %d %d", ret, errno);
            return false;
        }



        sFdInfo.handle = sAllocInfo.handle;

        ret = ioctl(mIonFd, ION_IOC_SHARE, &sFdInfo);

        if (ret || sFdInfo.fd < 0)
        {
            WFDMMLOGE("WFDSource failed to alloc ION buffers");
            return false;
        }

        mAudioBufFd = sFdInfo.fd;
        mAudioIonHandle = sFdInfo.handle = sAllocInfo.handle;
        mAudioBufPtr = (uint8_t *)
                mmap(NULL, mAudioBufSize,
                     PROT_READ | PROT_WRITE, MAP_SHARED,
                     mAudioBufFd, 0);

        if(mAudioBufPtr == NULL)
        {
            WFDMMLOGE("WFDSource Failed to mmap ION buf");
            return false;
        }
        struct ion_flush_data sFlushData;
        sFlushData.handle = (ion_user_handle_t)mAudioIonHandle;
        sFlushData.fd = 0;
        sFlushData.offset = 0;
        sFlushData.vaddr = mAudioBufPtr;
        sFlushData.length = mAudioBufSize;

        if(ioctl (mIonFd, ION_IOC_CLEAN_INV_CACHES, &sFlushData) < 0)
            WFDMMLOGE("Cache flush failed");



        /*----------------------------------------------------------------------
         Allocate Video Ion buffers
        ------------------------------------------------------------------------
        */
        sAllocInfo.align = 4096;
        sAllocInfo.heap_id_mask = ION_HEAP(ION_QSECOM_HEAP_ID);
        sAllocInfo.handle = 0;
        sAllocInfo.flags |= ION_FLAG_CACHED;

        mVideoBufSize = 2048 * 1024 * 1;
       // mVideoBufSize = ((mVideoBufSize + 4096 - 1) >> 12) << 12;

        sAllocInfo.len = mVideoBufSize;

        sAllocInfo.handle = 0;
        ret = ioctl(mIonFd, ION_IOC_ALLOC, &sAllocInfo);

        if (ret != 0  || sAllocInfo.handle == 0)
        {
            WFDMMLOGE3("Failed to Alloc ION buf %d %d %d", (int)ret, (int)errno,
                       (int)mIonFd);
            return false;
        }

        mVideoIonHandle = sFdInfo.handle = sAllocInfo.handle;


        ret = ioctl(mIonFd, ION_IOC_SHARE, &sFdInfo);

        if (ret || sFdInfo.fd < 0)
        {
            WFDMMLOGE("WFDSource failed to alloc ION buffers");
            return false;
        }
        WFDMMLOGH2("ION Handle Input fd = %d; handle = %x",
                   sFdInfo.fd, (unsigned int)sAllocInfo.handle);
        mVideoBufFd = sFdInfo.fd;
        mVideoBufPtr = (uint8_t *)
                mmap(NULL, sAllocInfo.len,
                     PROT_READ | PROT_WRITE, MAP_SHARED, mVideoBufFd, 0);

        mVideoHeapPtr = (uint8*)MM_Malloc(sAllocInfo.len);

        if(!mVideoHeapPtr)
        {
            WFDMMLOGD("Failed to allocate temporary buffer");
        }

        if(mVideoBufPtr == NULL || (int64_t)mVideoBufPtr == -1) {
            WFDMMLOGE("WFDSource Failed to mmap ION buf");
            return false;
        }



        sFlushData.handle = (ion_user_handle_t)mVideoIonHandle;
        sFlushData.fd = 0;
        sFlushData.offset = 0;
        sFlushData.vaddr = mVideoBufPtr;
        sFlushData.length = mVideoBufSize;

        if(ioctl (mIonFd, ION_IOC_CLEAN_INV_CACHES, &sFlushData) < 0)
            WFDMMLOGE("Cache flush failed");
    }

    /*--------------------------------------------------------------------------
      Done allocation of buffers
    ----------------------------------------------------------------------------
    */

    return true;
}

/*==============================================================================

         FUNCTION:          SetMediaBaseTime

         DESCRIPTION:       Sets base time for AV Sync
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
void WFDMMSinkMediaSource::SetMediaBaseTime(uint64 nStartTime)
{
    (void) nStartTime;
    avInfoType sAVInfo;
    int64 nTime;
    uint64 nTimeTemp;

    sAVInfo.nBaseTime = -1;

    if(mpFnAVInfoCb)
    {

        struct timespec sTime;
        clock_gettime(CLOCK_MONOTONIC, &sTime);

        nTimeTemp = ((OMX_U64)sTime.tv_sec * 1000000/*us*/)
                                  + ((OMX_U64)sTime.tv_nsec / 1000);

        if(mpRTPStreamPort)
        {
            WFDMMLOGH("Reading BaseTIme from RTPSource");
            mpRTPStreamPort->GetRTPBaseTimeUs(&nTime);
            /*------------------------------------------------------------------
             If the first sample out already has a non zero timestamp, basetime
             has to be adjusted.
            --------------------------------------------------------------------
            */
            sAVInfo.nBaseTime = (uint64)nTime;

        }
        if(sAVInfo.nBaseTime == -1)
        {
            sAVInfo.nBaseTime = nTimeTemp;
        }
        sAVInfo.nBaseTimeStream = mnActualBaseTime;

        sAVInfo.nBaseTimeStream = mnActualBaseTime;
        sAVInfo.nBaseTime -= mnMediaBaseTime;
        WFDMMLOGE2("MediaSrc BaseTime = %lld, Curr Time = %llu",
                   sAVInfo.nBaseTime, nTimeTemp);
        mpFnAVInfoCb(mClientData, &sAVInfo);
        mbVideoTimeReset = true;
        mbAudioTimeReset = true;
        mbGenericTimeReset = true;
    }
}


/*==============================================================================

         FUNCTION:          setFlushTimeStamp

         DESCRIPTION:
*//**       @brief          sets a timestamp upto which data has to be flushed
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
void WFDMMSinkMediaSource::setFlushTimeStamp(uint64 nTS)
{
    mnFlushTimeStamp = nTS;
    mbFlushInProgress = true;
}

/*==============================================================================

         FUNCTION:          pauseVideoThread

         DESCRIPTION:
*//**       @brief          Pauses the video rendering and holds on to the data
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
void WFDMMSinkMediaSource::pauseVideoThread()
{
    if(ISSTATEOPENED)
    {
        WFDMMLOGH("Pausing Video Thread");
        mbVideoPaused = true;
    }
    return;
}

/*==============================================================================

         FUNCTION:          pauseAudioThread

         DESCRIPTION:
*//**       @brief          Pauses the audio rendering and holds on to the data
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
void WFDMMSinkMediaSource::pauseAudioThread()
{
    if(ISSTATEOPENED)
    {
        WFDMMLOGH("Pausing Audio Thread");
        mbAudioPaused = true;
    }
    return;
}


/*==============================================================================

         FUNCTION:          pauseGenericThread

         DESCRIPTION:
*//**       @brief          Pauses the Generic data rendering and holds on to the data
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
void WFDMMSinkMediaSource::pauseGenericThread()
{
    if(ISSTATEOPENED)
    {
        WFDMMLOGH("Pausing Generic Thread");
        mbGenericPaused = true;
    }
    return;
}

/*==============================================================================

         FUNCTION:          restartVideoThread

         DESCRIPTION:
*//**       @brief          Restarts Video Thread
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
void WFDMMSinkMediaSource::restartVideoThread()
{
    if(ISSTATEOPENED)
    {
        WFDMMLOGH("Restarting Video thread");
        mbVideoPaused = false;
        if(mpVideoThread)
        {
            mbVideoStarted = false;
            mbMediaBaseTimeSet = false;
            mbVideoReconfigured = true;
            mpVideoThread->SetSignal(0);
        }
    }
    return;
}

/*==============================================================================

         FUNCTION:          restartAudioThread

         DESCRIPTION:
*//**       @brief          Restarts audio Thread
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
void WFDMMSinkMediaSource::restartAudioThread()
{
    if(ISSTATEOPENED)
    {
        WFDMMLOGH("Restarting Audio thread");
        mbAudioPaused= false;
        if(mpAudioThread)
        {
            mbAudioStarted = false;
            mbMediaBaseTimeSet = false;
            mbAudioReconfigured = true;
            mpAudioThread->SetSignal(0);
        }
    }
    return;
}


/*==============================================================================

         FUNCTION:          restartGenericThread

         DESCRIPTION:
*//**       @brief          Restarts Generic Thread
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
void WFDMMSinkMediaSource::restartGenericThread()
{
    if(ISSTATEOPENED)
    {
        WFDMMLOGH("Restarting Generic thread");
        mbGenericPaused= false;
        if(mpGenericThread)
        {
            mpGenericThread->SetSignal(0);
        }
    }
    return;
}

/*==============================================================================

         FUNCTION:          streamPause

         DESCRIPTION:
*//**       @brief          Pauses the rendering and holds on to the data
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
void WFDMMSinkMediaSource::streamPause()
{
    if(ISSTATEOPENED)
    {
        mbPaused = true;
    }
    return;
}

/*==============================================================================

         FUNCTION:          streamPlay

         DESCRIPTION:
*//**       @brief          UnPauses the rendering
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
void WFDMMSinkMediaSource::streamPlay(bool flush)
{
    if(ISSTATEOPENED)
    {
        if(mbPaused && flush)
        {
            /*------------------------------------------------------------------
             Flush buffers
            --------------------------------------------------------------------
            */
            FileSourceSampleInfo  sampleInfo;
            FileSourceMediaStatus mediaStatus = FILE_SOURCE_DATA_OK;
            uint32 nSize = mVideoBufSize ? mVideoBufSize:1024*1024;
            uint8 *pTempBuf = (uint8*)MM_Malloc(nSize);
            uint32 nMaxCnt = 2000;
            if(pTempBuf)
            {
                while(mediaStatus == FILE_SOURCE_DATA_OK && nMaxCnt--)
                {
                    mediaStatus = mpFileSource->GetNextMediaSample
                                                         (mVideoTrackId,
                                                          pTempBuf,
                                                          &nSize,
                                                          sampleInfo);
                }
                nMaxCnt = 2000;
                while(mediaStatus == FILE_SOURCE_DATA_OK && nMaxCnt--)
                {
                    mediaStatus = mpFileSource->GetNextMediaSample
                                                         (mAudioTrackId,
                                                          pTempBuf,
                                                          &nSize,
                                                          sampleInfo);
                }
                MM_Free(pTempBuf);
            }
        }
        mbSinkIDRRequest = true;
        mbPaused = false;
        if(mpAudioThread)
        {
            mpAudioThread->SetSignal(0);
        }
        if(mpVideoThread)
        {
            mpVideoThread->SetSignal(0);
        }
    }
    return;
}

void WFDMMSinkMediaSource::videoFrameBufferInfo(videoFrameInfoType frameInfo)
{
    WFDMMLOGE1("Received frame info from Decoder as %d",frameInfo);
    meFrameTypeInfo = frameInfo;
    return;
}

#ifdef ABS64
#undef ABS64
#endif
#define ABS64(x) (((int64)(x)) < 0 ? -((int64)(x)) : x)
/*==============================================================================

         FUNCTION:          ProximityCheck

         DESCRIPTION:
*//**       @brief          Checks which number the test value is closer to
*//**
@par     DEPENDENCIES:
                            None
*//*
         PARAMETERS:
*//**       @param          None

*//*     RETURN VALUE:
*//**       @return
                            true if value is close to second num. else false
@par     SIFE EFFECTS:
                            None
*//*==========================================================================*/
bool WFDMMSinkMediaSource::ProximityCheck
(
    uint64 nTestVal,
    uint64 nValA,
    uint64 nValB
)
{
    WFDMMLOGE3("ProximityCheck Val = %llu, nValA = %llu, nValB = %llu",
                 nTestVal, nValA, nValB);
    if(ABS64(nTestVal - nValB) < ABS64(nTestVal - nValA))
    {
      //If closer to Value B return true.
      return true;
    }
    return false;
}

/*==============================================================================

         FUNCTION:          CheckBaseTimeChange

         DESCRIPTION:
*//**       @brief          Checks if base time has been updated
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
void WFDMMSinkMediaSource::CheckBaseTimeChange(uint32 nTime)
{
    /*--------------------------------------------------------------------------
      Check if there is an update in media time.
    ----------------------------------------------------------------------------
    */
    CRITICAL_SECT_ENTER;
    uint64 nPrevTS = 0;
    uint64 nNewBase = 0;
    bool   bDiscontinuity = false;
    if(mbMediaTimeSet && mpRTPStreamPort->IsBaseTimeAdjusted
              (&bDiscontinuity, &nPrevTS, &nNewBase))
    {
        if((!nPrevTS && !nNewBase) || ProximityCheck((uint64)nTime,
                                            nPrevTS,
                                            nNewBase) == true)
        {
            WFDMMLOGE("CheckBaseTime. Time to change BaseTime");
            SetMediaBaseTime((uint64)nTime * 1000);;
            mnNewBaseTime = nNewBase;
            mnOldBaseTime = nPrevTS;
        }
    }
    CRITICAL_SECT_LEAVE;
}

/*==============================================================================

         FUNCTION:          CheckAndReqIDR

         DESCRIPTION:
*//**       @brief          Checks if IDR request should be sent and sends if needed.
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
void WFDMMSinkMediaSource::CheckAndReqIDR()
{
    if(mbCorruptedFrame && mbVideoStarted)
    {
        uint64 nCurrTime;
        MM_Time_GetTimeEx(&nCurrTime);
        int delta = nCurrTime - mnLastIDRReqTime;
        if(delta > IDR_REQ_INTERVAL)
        {
            WFDMMLOGE1("CheckAndReqIDR : Requesting IDR delta = %d",delta);
            NOTIFYBYTESLOST;
            mnLastIDRReqTime = nCurrTime;
            mbCorruptedFrame = false;
        }
    }
}

OMX_ERRORTYPE WFDMMSinkMediaSource::flushVideoQ()
{
    WFDMMLOGH("Flushing VideoQ");

    OMX_BUFFERHEADERTYPE *pBuffer = NULL;
    OMX_ERRORTYPE result;
    OMX_S32 qSize = mpVideoQ? mpVideoQ->GetSize():0;

    if(mpVideoQ && qSize)
    {
        for(OMX_S32 i = 0; i < qSize; i++)
        {
            result = mpVideoQ->Pop(&pBuffer,sizeof(pBuffer),0);
            if(result != OMX_ErrorNone)
            {
                WFDMMLOGE("Unable to pop buffer from VideoQ");
                //:TODO: Should we return error here && allow calling module
                //:TODO: to take decision accordingly?
                /*return result;*/
            }
            pBuffer = NULL;
        }
    }
//    SETSTATEINIT;
    return OMX_ErrorNone;
}
