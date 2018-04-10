/*==============================================================================
*       WFDMMSinkAudioRenderer.cpp
*
*  DESCRIPTION:
*       Does AV Sync and audio render.
*
*  Copyright (c) 2013-16 Qualcomm Technologies, Inc.
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

#include <cutils/properties.h>
#include "WFDMMLogs.h"
#include "WFDMMSinkAudioRenderer.h"
#include "WFDMMThreads.h"
#include "WFDMMSourceMutex.h"
#include "WFDMMSourceSignalQueue.h"
#include "MMMalloc.h"
#include "MMMemory.h"
#include "qmmList.h"
#include "MMTimer.h"
#include "AudioTrack.h"
#include "gralloc_priv.h"
#include "qdMetaData.h"
#ifndef USE_OMX_AAC_CODEC
#include "WFDMMSinkAudioDecode.h"
#endif
#include "WFDMMSinkStatistics.h"
#include "WFDUtils.h"

#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "WFDMMAUDIORENDERER"

#define MAX_CONSEQ_AUDIO_DROP 5
#define AUDIO_RENDER_TOLERANCE 2000 //us
#define INITIAL_AUDIO_DROP_WINDOW 700000 //700ms
#define AUDIO_AVSYNC_DROP_WINDOW_AUDIO_ONLY -400000

using namespace android;

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

#define WFD_DUMP_PCM
#define WFD_DUMP_AAC

/*------------------------------------------------------------------------------
 State definitions
--------------------------------------------------------------------------------
*/
#define DEINIT  0
#define INIT    1
#define OPENED  2
#define CLOSING 4
#define CLOSED  3

#define ISSTATEDEINIT  (state(0, true) == DEINIT)
#define ISSTATEINIT    (state(0, true) == INIT)
#define ISSTATEOPENED  (state(0, true) == OPENED)
#define ISSTATECLOSED  (state(0, true) == CLOSED)
#define ISSTATECLOSING (state(0, true) == CLOSING)

#define ISERROR         (mnErr == -1)

#define SETSTATECLOSING (state(CLOSING, false))
#define SETSTATEINIT    (state(INIT   , false))
#define SETSTATEDEINIT  (state(DEINIT , false))
#define SETSTATEOPENED  (state(OPENED , false))
#define SETSTATECLOSED  (state(CLOSED , false))


/*==============================================================================

         FUNCTION:         WFDMMSinkAudioRenderer

         DESCRIPTION:
*//**       @brief         WFDMMSinkAudioRenderer contructor
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
WFDMMSinkAudioRenderer::WFDMMSinkAudioRenderer(int moduleId,
                                     WFDMMSinkEBDType       pFnEBD,
                                     WFDMMSinkHandlerFnType   pFnHandler,
                                     void* clientData,
                                     WFDMMSinkStatistics* pStatInst):
m_pStatInstance(pStatInst)
{

    InitData();
    /*--------------------------------------------------------------------------

    ----------------------------------------------------------------------------
    */
    mpFnHandler = pFnHandler;
    mpFnEBD     = pFnEBD;
    mnModuleId  = moduleId;
    mClientData = clientData;

    mhCritSect = NULL;
#ifndef USE_OMX_AAC_CODEC
    mpAudioDecoder = NULL;
#endif
    int nRet = MM_CriticalSection_Create(&mhCritSect);
    if(nRet != 0)
    {
        mhCritSect = NULL;
        WFDMMLOGE("Error in Critical Section Create");
    }

    SETSTATEDEINIT;
}

/*==============================================================================

         FUNCTION:          ~WFDMMSinkAudioRenderer

         DESCRIPTION:
*//**       @brief          Destructor for Audio renderer class
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
WFDMMSinkAudioRenderer::~WFDMMSinkAudioRenderer()
{
    WFDMMLOGH("WFDMMSinkAudioRenderer Destructor");
    deinitialize();
}

/*==============================================================================

         FUNCTION:          InitData

         DESCRIPTION:
*//**       @brief          Initializes class variables
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
void WFDMMSinkAudioRenderer::InitData()
{
    mAudioQ  = NULL;
    mFrameInfoFreeQ = NULL;
    mpAudioThread = NULL;
    mpFpAAC = NULL;
    mbDumpAAC = false;
    mpFpWAV = NULL;
    mbDumpWav = false;
    mhCritSect = NULL;
    mpAudioTrack = NULL;
    mnAudioSessId = NULL;
    mbMediaTimeSet = false;
    mbAudioTimeReset = true;
    mbAudioCodecHdrSet = false;
    memset(&mCfg, 0, sizeof(mCfg));
    meState = DEINIT;
    mClientData = 0;
    mnModuleId = 0;
    mpFnEBD = NULL;
    mpFnHandler = NULL;
    mnBaseMediaTime = 0;
    mbMediaTimeSet = false;
    mnErr = 0;
    mbFlushInProgress = false;
    mnFlushTimeStamp = 0;
    mbPaused = false;
    mnAudioOffset = -1;
    mnFirstAudioFrameTimeStamp= 0;
    return;
}

/*==============================================================================

         FUNCTION:         Configure

         DESCRIPTION:
*//**       @brief         Configures the renderer
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
OMX_ERRORTYPE WFDMMSinkAudioRenderer::Configure(rendererConfigType *pCfg)
{
    CHECK_ARGS_RET_OMX_ERR_3(pCfg, mpFnHandler, mpFnEBD);

    memcpy(&mCfg, pCfg, sizeof(rendererConfigType));

    if(createResources() != OMX_ErrorNone)
    {
        WFDMMLOGE("Renderer Create Resources Failed");
        return OMX_ErrorInsufficientResources;
    }
    SETSTATEINIT;
    return OMX_ErrorNone;
}

/*==============================================================================

         FUNCTION:          AllocateBuffers

         DESCRIPTION:
*//**       @brief          Allocates Required Buffers
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
OMX_ERRORTYPE WFDMMSinkAudioRenderer::AllocateBuffers()
{
    OMX_ERRORTYPE eErr = OMX_ErrorNone;

    /*--------------------------------------------------------------------------
     WFDMMSinkRenderer does not allocates Buffers. Revisit
    ----------------------------------------------------------------------------
    */
    return eErr;
}

/*==============================================================================

         FUNCTION:          DeliverInput

         DESCRIPTION:
*//**       @brief          Audio Buffers are received through this
*//**
@par     DEPENDENCIES:
                            None
*//*
         PARAMETERS:
*//**       @param          trackId - Audio/Video track identifier
                            pBuffer - Buffer Header

*//*     RETURN VALUE:
*//**       @return
                            OMX_ErrorType
@par     SIDE EFFECTS:
                            None
*//*==========================================================================*/
OMX_ERRORTYPE WFDMMSinkAudioRenderer::DeliverInput(int trackId,
                                             OMX_BUFFERHEADERTYPE *pBuffer)
{

    if (ISSTATECLOSING || ISSTATEDEINIT || ISSTATECLOSED)
    {
        /*----------------------------------------------------------------------
         We can allow buffer flow in INIT and OPENED
        ------------------------------------------------------------------------
        */
        WFDMMLOGE("Output Delivered when state not in play");
        return OMX_ErrorInvalidState;
    }
    WFDMMLOGM("WFDMMSinkAudioRenderer DeliverInput");

    if (!pBuffer)
    {
        WFDMMLOGE("Invalid Arguments");
        return OMX_ErrorBadParameter;
    }

    /*--------------------------------------------------------------------------
     Check if Video or Audio is advertized during start up. Reject any samples
     from unsupported track
    ----------------------------------------------------------------------------
    */
    if(trackId == SINK_AUDIO_TRACK_ID && !mCfg.bHasAudio)
    {
        WFDMMLOGE("Sample for track when track not advertized");
        return OMX_ErrorBadParameter;
    }

    /*--------------------------------------------------------------------------
        Check if timebase has changed and needs reset
    ----------------------------------------------------------------------------
    */
    if(trackId == SINK_AUDIO_TRACK_ID &&
      mbAudioTimeReset && pBuffer->nFilledLen)
    {
        WFDMMLOGH("Audio Time Reset. Waiting");
        if((pBuffer->nFlags & OMX_BUFFERFLAG_STARTTIME) == 0)
        {
            WFDMMLOGE("Renderer waiting for Audio STARTTIME");
            return OMX_ErrorInvalidState;
        }
        else
        {
            mbAudioTimeReset = false;
        }
    }

#ifndef USE_AUDIO_TUNNEL_MODE
#ifndef USE_OMX_AAC_CODEC
    /*--------------------------------------------------------------------------
     Decode frame it is AAC. In place decoding is done. The time taken for
     decoding will be considered in AV Sync
    ----------------------------------------------------------------------------
    */
    if(trackId == SINK_AUDIO_TRACK_ID &&
       mCfg.sAudioInfo.eAudioFmt == WFD_AUDIO_AAC)
    {
#ifdef WFD_DUMP_AAC
        if(mpFpAAC)
        {
            fwrite(pBuffer->pBuffer, 1, pBuffer->nFilledLen, mpFpAAC);
        }
#endif
        OMX_ERRORTYPE eErr = mpAudioDecoder->Decode(pBuffer);
        if(eErr != OMX_ErrorNone)
        {
            WFDMMLOGE("Failed to decode AAC frame");
          //  NOTIFYERROR;
            return OMX_ErrorDynamicResourcesUnavailable;
        }
    }
#endif
#endif

    frameInfoType *pFrameInfo = NULL;

    accessFrameInfoMeta(&pFrameInfo, popfront);

    if (!pFrameInfo)
    {
        WFDMMLOGE("Failed to get frameInfo");
        NOTIFYERROR;
        return OMX_ErrorInsufficientResources;
    }

    pFrameInfo->pBuffer = pBuffer;


    if(!Scheduler(trackId, pFrameInfo))
    {
        accessFrameInfoMeta(&pFrameInfo, pushrear);
        NOTIFYERROR;
        WFDMMLOGE1("Failed to Schedule track = %d", trackId);
        return OMX_ErrorUndefined;
    }
    return OMX_ErrorNone;
}
/*==============================================================================

         FUNCTION:         createResources

         DESCRIPTION:
*//**       @brief         creates resources for WFDMMSinkRenderer
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
OMX_ERRORTYPE WFDMMSinkAudioRenderer::createResources()
{

    if(!createThreadsAndQueues())
    {
        WFDMMLOGE("Renderer Failed to create Threads and Queues");
        return OMX_ErrorInsufficientResources;
    }
    if(mCfg.bHasAudio)
    {
        if(!createAudioResources())
        {
            WFDMMLOGE("Renderer failed to create audio resources");
            NOTIFYERROR;
            return OMX_ErrorInsufficientResources;
        }
#ifdef USE_OMX_AAC_CODEC
        mnAudioDropBytes = mCfg.sAudioInfo.nSampleRate *
                           mCfg.sAudioInfo.nChannels * 2;
#endif
    }

    prepareDumpFiles();

    return OMX_ErrorNone;
}

/*==============================================================================

         FUNCTION:          createAudioTrack

         DESCRIPTION:
*//**       @brief          creates Audio Track
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

bool WFDMMSinkAudioRenderer::createAudioTrack()
{
    if(mpAudioTrack != NULL)
    {
        WFDMMLOGD("Destory Previous Audio Track");
        mpAudioTrack->stop();
    }
    int AudioFmt = AUDIO_FORMAT_PCM_16_BIT;
    int nFlags   = AUDIO_OUTPUT_FLAG_NONE;

    if(mCfg.sAudioInfo.eAudioFmt == WFD_AUDIO_LPCM)
    {
        AudioFmt = AUDIO_FORMAT_PCM_16_BIT;
    }
    else if(mCfg.sAudioInfo.eAudioFmt == WFD_AUDIO_AAC)
    {
#ifdef USE_AUDIO_TUNNEL_MODE
        AudioFmt = AUDIO_FORMAT_AAC;
        nFlags = AUDIO_OUTPUT_FLAG_TUNNEL|AUDIO_OUTPUT_FLAG_DIRECT;
#else //USE_AUDIO_TUNNEL_MODE
        AudioFmt = AUDIO_FORMAT_PCM_16_BIT;
#ifdef USE_OMX_AAC_CODEC
        WFDMMLOGE("Can't support non LPCM in Renderer");
        return false;
#endif//USE_OMX_AAC_CODEC
#endif//USE_AUDIO_TUNNEL_MODE
    }
    else if(mCfg.sAudioInfo.eAudioFmt == WFD_AUDIO_DOLBY_DIGITAL)
    {
        AudioFmt = AUDIO_FORMAT_AC3;
#ifdef USE_AUDIO_TUNNEL_MODE
        nFlags = AUDIO_OUTPUT_FLAG_TUNNEL|AUDIO_OUTPUT_FLAG_DIRECT;
#endif
    }

#ifdef USE_AUDIO_TUNNEL_MODE
    if(nFlags & AUDIO_OUTPUT_FLAG_TUNNEL)
    {
        WFDMMLOGH("Tunnel mode audio Configure audiosession");
        mnAudioSessId = AudioSystem::newAudioSessionId();
        AudioSystem::acquireAudioSessionId(mnAudioSessId,getpid());
    }
#endif
    nFlags |= AUDIO_OUTPUT_FLAG_FAST;

    WFDMMLOGD2("SinkRenderer Audio SampleRate %d Channels %d",
               mCfg.sAudioInfo.nSampleRate, mCfg.sAudioInfo.nChannels);

    mpAudioTrack = MM_New_Args(android::AudioTrack, (
                          AUDIO_STREAM_MUSIC,
                          mCfg.sAudioInfo.nSampleRate,
                          (audio_format_t)AudioFmt,
                          audio_channel_out_mask_from_count
                                     (mCfg.sAudioInfo.nChannels),
                          (int)0,
                          (audio_output_flags_t) nFlags,
                          AudioTrackCb,
                          this,
                          0,
                          audio_session_t(mnAudioSessId),
                          android::AudioTrack::TRANSFER_SYNC));

    if(mpAudioTrack == NULL)
    {
        WFDMMLOGE("Failed to create Audio Track");
        return false;
    }

    status_t nErr = mpAudioTrack->initCheck();

    if(nErr != 0)
    {
        WFDMMLOGE("Failed to Init Audio track");
        return false;
    }

    mpAudioTrack->start();
    mnAudioLatency = mpAudioTrack->latency() * 1000;
    WFDMMLOGH1("AudioTrack STarted with latency %ld",mnAudioLatency);

    return true;
}

/*==============================================================================

         FUNCTION:          createAudioResources

         DESCRIPTION:
*//**       @brief          creates Audio Resources. Hmm
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
bool WFDMMSinkAudioRenderer::createAudioResources()
{

    if(createAudioTrack() == false)
    {
        WFDMMLOGE("Failed to create Audio Track");
        return false;
    }
#ifndef USE_AUDIO_TUNNEL_MODE
#ifndef USE_OMX_AAC_CODEC
    /*--------------------------------------------------------------------------
      Create A decoder if the audio codec needs One. Currently only AAC needs
      a decoder. Make sure to update this comment if you are adding AC3
    ----------------------------------------------------------------------------
    */

    if(mCfg.sAudioInfo.eAudioFmt == WFD_AUDIO_AAC)
    {
        mpAudioDecoder = WFDMMSinkAudioDecode::create(WFD_AUDIO_AAC);
        if(!mpAudioDecoder)
        {
            WFDMMLOGE("Failed to create Audio Decoder");
            return false;
        }
        audioConfigType sAudioCfg;
        sAudioCfg.eAudioType     = mCfg.sAudioInfo.eAudioFmt;
        sAudioCfg.nBitsPerSample = 16;
        sAudioCfg.nChannels      = mCfg.sAudioInfo.nChannels;
        sAudioCfg.nSampleRate    = mCfg.sAudioInfo.nSampleRate;

        OMX_ERRORTYPE eRet = mpAudioDecoder->Configure(&sAudioCfg);

        if(eRet != OMX_ErrorNone)
        {
            WFDMMLOGE("Failed to configure Audio decoder");
            return false;
        }
        WFDMMLOGH("AudioDecoder Created");
    }
#endif
#endif
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
bool WFDMMSinkAudioRenderer::createThreadsAndQueues()
{
    /*--------------------------------------------------------------------------
      Create threads if session has audio and video
    ----------------------------------------------------------------------------
    */
    if(mCfg.bHasAudio)
    {
        mpAudioThread = MM_New_Args(WFDMMThreads, (1));

        if(!mpAudioThread)
        {
            WFDMMLOGE("Failed to create Audio Thread");
            return false;
        }

        mpAudioThread->Start(AudioThreadEntry, -2,
                             65536*2, this, "WFDSinkAudioRenderer");
    }

    if(mCfg.bHasAudio)
    {
        mAudioQ = MM_New_Args(SignalQueue, (100, sizeof(int*)));
        if(!mAudioQ)
        {
            return false;
        }
    }

    mFrameInfoFreeQ = MM_New_Args(SignalQueue,
                                  (RENDERERMAXQSIZES, sizeof(int*)));
    /*--------------------------------------------------------------------------
     Frame Info Q is used to keep a list of frameInfo structures. These are
     meta data associated with each buffer that is going to be scheduled for
     rendering
    ----------------------------------------------------------------------------
    */
    if(!mFrameInfoFreeQ)
    {
        WFDMMLOGE("Failed to allocate the FrameInfoQ");
        return false;
    }

    /*--------------------------------------------------------------------------
     Populate the FreeQ with all the frameInfo structure objects that we have
    ----------------------------------------------------------------------------
    */

    for(int i = 0; i < RENDERERMAXQSIZES; i++)
    {
        uint8 *pFrameInfoPtr = (uint8*)(msFrameInfo + i);
        mFrameInfoFreeQ->Push(&pFrameInfoPtr, sizeof(int*));
    }
    return true;
}

/*==============================================================================

         FUNCTION:          accessFrameInfoMeta

         DESCRIPTION:
*//**       @brief          Objects of frameInfo are used to store Audio and
                            video buffers in queues before being rendered.
                            This function is used to either get an empty
                            frameinfo object or to push a frame info object back
                            to queue once its use is complete.
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
bool WFDMMSinkAudioRenderer::accessFrameInfoMeta(frameInfoType **pFrameInfo,
                                            actionType action)
{
    CRITICAL_SECT_ENTER
    /*--------------------------------------------------------------------------
     Note that this Q supports only push and pop. Therefore poprear and popfront
     maps to pop and pushrear and pushfront maps to push
    ----------------------------------------------------------------------------
    */
    if(mFrameInfoFreeQ && pFrameInfo)
    {
        if(action == poprear || action == popfront)
        {
            *pFrameInfo = NULL;

            mFrameInfoFreeQ->Pop(pFrameInfo, sizeof(pFrameInfo), 0);

            if(*pFrameInfo)
            {
                /*--------------------------------------------------------------
                 Found a free frameinfo object
                ----------------------------------------------------------------
                */
                CRITICAL_SECT_LEAVE
                return true;
            }
            WFDMMLOGE("FrameInfo Popped is NULL");
        }
        else if(action == pushrear || action == pushfront)
        {
            if(!(*pFrameInfo))
            {
                WFDMMLOGE("Invalid FrameInfo Pushed");
                CRITICAL_SECT_LEAVE;
                return false;
            }
            OMX_ERRORTYPE eErr =
                mFrameInfoFreeQ->Push(pFrameInfo, sizeof(pFrameInfo));

            if(eErr == OMX_ErrorNone)
            {
                /*--------------------------------------------------------------
                 Push free frameinfo object
                ----------------------------------------------------------------
                */
                CRITICAL_SECT_LEAVE
                return true;
            }
        }
    }
    WFDMMLOGE("Failed to operate on FrameInfo Q");
    CRITICAL_SECT_LEAVE
    return false;
}


/*==============================================================================

         FUNCTION:          Scheduler

         DESCRIPTION:
*//**       @brief          Schedules Audio and Video Frames for rendering
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
bool WFDMMSinkAudioRenderer::Scheduler(int trackId, frameInfoType *pFrameInfo)
{

    /*--------------------------------------------------------------------------
     TODO - do any adjustments based on different propagation delays here.
     For this delays of audio and video paths has to be measured.
    ----------------------------------------------------------------------------
    */
    pFrameInfo->nTSArrival = getCurrentTimeUs();;
    pFrameInfo->nTSSched   = pFrameInfo->pBuffer->nTimeStamp;

    if(trackId == SINK_AUDIO_TRACK_ID)
    {
        if(!mbMediaTimeSet)
        {
            mnBaseMediaTime = 0;
            mnBaseMediaTime = getCurrentTimeUs();
            mbMediaTimeSet  = true;
        }
        WFDMMLOGH("Push Audio to Pending Q");
        if(mAudioQ)
        {
            mAudioQ->Push(&pFrameInfo, sizeof(&pFrameInfo));
        }
        return true;
    }

    return false;

}

/*==============================================================================

         FUNCTION:          PushAudio4Render

         DESCRIPTION:
*//**       @brief          Render the latest audio frame
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
bool WFDMMSinkAudioRenderer::PushAudio4Render(frameInfoType *pFrameInfo)
{
    /*--------------------------------------------------------------------------
      See if there are video or audio frames pending for rendering
    ----------------------------------------------------------------------------
    */
    WFDMMLOGD("Push Audio For Rendering");

    if(mbRestartAudioTrack)
    {
        WFDMMLOGH("Restart Audio Track");
        mbRestartAudioTrack = false;
        if(createAudioTrack() == false)
        {

            mpFnEBD(mClientData, mnModuleId, SINK_AUDIO_TRACK_ID,
                 pFrameInfo->pBuffer);
            accessFrameInfoMeta(&pFrameInfo, pushfront);
            NOTIFYERROR;
        }
    }

#ifdef USE_AUDIO_TUNNEL_MODE
    if(!mbAudioCodecHdrSet && mCfg.sAudioInfo.eAudioFmt == WFD_AUDIO_AAC)
    {
        uint8 sAACHeader[2];
        uint32 nSize = 2;

        bool bRet = GenerateAACHeaderFromADTS((uint8*)pFrameInfo->pBuffer->pBuffer,
                                  (uint32)pFrameInfo->pBuffer->nFilledLen,
                                  (uint8*)sAACHeader,
                                  &nSize);

        if(nSize >= 2 && bRet == true)
        {
            mpAudioTrack->write(sAACHeader,nSize);
        }
        mbAudioCodecHdrSet = true;
    }
#endif

    if(pFrameInfo != NULL)
    {
        WFDMMLOGD("Push Audio For Rendering to audio Track");
        int nBytesWritten = 0;

        uint8* pData = pFrameInfo->pBuffer->pBuffer +
                         pFrameInfo->pBuffer->nOffset;
        uint32 nDataSize = pFrameInfo->pBuffer->nFilledLen;

#ifdef USE_AUDIO_TUNNEL_MODE
        if(mCfg.sAudioInfo.eAudioFmt == WFD_AUDIO_AAC)
        {
            if(nDataSize <= 7)
            {
                pFrameInfo->pBuffer->nFilledLen = 0;
                mpFnEBD(mClientData, mnModuleId, SINK_AUDIO_TRACK_ID,
                         pFrameInfo->pBuffer);
                accessFrameInfoMeta(&pFrameInfo, pushfront);
                return true;
            }
            pData += 7;
            nDataSize -= 7;
        }
#endif
        if(pFrameInfo->pBuffer->nFilledLen)
        {
#ifdef USE_OMX_AAC_CODEC
            /*------------------------------------------------------------------
             OMX AAC cocec intriduces an initial latency. We need to drop some
             amount of data to catchup.
            --------------------------------------------------------------------
            */
            if(mnAudioDropBytes)
            {
                if(pFrameInfo->pBuffer->nFilledLen < mnAudioDropBytes)
                {
                    mnAudioDropBytes -= pFrameInfo->pBuffer->nFilledLen;
                }
                else
                {
                    mnAudioDropBytes = 0;
                }
                WFDMMLOGH("Renderer: Drop Initial Audio");
            }
            else
            {
#endif

#ifdef WFD_DUMP_PCM
                if(mpFpWAV)
                {
                    fwrite(pData, 1, nDataSize, mpFpWAV);
                }

#endif
                nBytesWritten = mpAudioTrack->write(pData,nDataSize);
#ifdef USE_OMX_AAC_CODEC
            }
#endif
            WFDMMLOGD1("PCM Write NumBytes Written = %d", nBytesWritten);
            pFrameInfo->pBuffer->nFilledLen = 0;
            /* Debug Code , can be enabled if needed */
            //mnAudioLatency = mpAudioTrack->latency();
            //WFDMMLOGM1("mpAudioTrack->latency() = %ld ms",mnAudioLatency);
        }

        mpFnEBD(mClientData, mnModuleId, SINK_AUDIO_TRACK_ID,
                 pFrameInfo->pBuffer);
        accessFrameInfoMeta(&pFrameInfo, pushfront);
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
OMX_ERRORTYPE WFDMMSinkAudioRenderer::deinitialize()
{

    if(!ISSTATECLOSED || !ISSTATEINIT)
    {
        SETSTATECLOSING;
    }

    WFDMMLOGH("Wait for all buffers to be returned");

    int nAudioQLen;
    while (1)
    {
        nAudioQLen   = mAudioQ ? mAudioQ->GetSize() : 0;

        WFDMMLOGH1("QueueLength at exit Audio- %d", nAudioQLen);

        if(!nAudioQLen)
        {
            WFDMMLOGH("All Buffers returned or freed");
            break;
        }
        MM_Timer_Sleep(5);
    }

    WFDMMLOGH("Done waiting for all buffers to be returned");

    if(ISSTATECLOSING)
    {
        SETSTATECLOSED;
    }
#ifndef USE_OMX_AAC_CODEC
    if(mpAudioDecoder)
    {
        MM_Delete(mpAudioDecoder);
        mpAudioDecoder = NULL;
    }
#endif

    /*--------------------------------------------------------------------------
      Close Threads and signal queues
    ----------------------------------------------------------------------------
    */
    if(mpAudioThread)
    {
        MM_Delete(mpAudioThread);
        mpAudioThread = NULL;
    }

    if(mpAudioTrack != NULL)
    {
        mpAudioTrack->stop();
    }

    if(mAudioQ)
    {
        MM_Delete(mAudioQ);
        mAudioQ = NULL;
    }

    if(mFrameInfoFreeQ)
    {
        MM_Delete(mFrameInfoFreeQ);
        mFrameInfoFreeQ = NULL;
    }

    if (mpFpAAC)
    {
        fclose(mpFpAAC);
        mpFpAAC = NULL;
    }

    if(mpFpWAV)
    {
        fclose(mpFpWAV);
        mpFpWAV = NULL;
    }

    if (mhCritSect)
    {
        MM_CriticalSection_Release(mhCritSect);
        mhCritSect = NULL;
    }
    return OMX_ErrorNone;
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
void WFDMMSinkAudioRenderer::AudioThreadEntry(void* pThis,
                                         unsigned int nSignal)
{
    CHECK_ARGS_RET_VOID_1(pThis);

    WFDMMSinkAudioRenderer *pMe = (WFDMMSinkAudioRenderer*)pThis;

    if(pMe)
    {
        pMe->AudioThread(nSignal);
    }
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
void WFDMMSinkAudioRenderer::AudioThread(unsigned int nSignal)
{
    (void) nSignal;
    WFDMMLOGH("WFDMMSinkRenderer AudioThread");
    int nDropCount = 0;
    uint64 timedecider;

    if(!mCfg.bHasVideo)
    {
        WFDMMLOGD("No Video Relax AV Sync window");
        mCfg.naudioAVSyncDropWindow = AUDIO_AVSYNC_DROP_WINDOW_AUDIO_ONLY;

    }

    while(ISSTATEOPENED || ISSTATECLOSING)
    {
        if(mbPaused && !ISSTATECLOSING)
        {
            /*------------------------------------------------------------------
             Rendering has been paused. Hold on to the buffers and continue.
            --------------------------------------------------------------------
            */
            MM_Timer_Sleep(10);
            continue;
        }
        frameInfoType *pFrameInfo = NULL;
        mAudioQ->Peek(&pFrameInfo,
                     sizeof(&pFrameInfo));

        if(pFrameInfo)
        {
           WFDMMLOGD("Got a Pending Audio Frame");
           if(mCfg.nDecoderLatency <= mnAudioLatency)
           {
              timedecider = pFrameInfo->pBuffer->nTimeStamp -
                                     getCurrentTimeUs() +
                                     (mnAudioOffset != -1 ? mnAudioOffset : 0);
           }
           else
           {
              timedecider = pFrameInfo->pBuffer->nTimeStamp -
                                     getCurrentTimeUs() + mCfg.nDecoderLatency +
                                     (mnAudioOffset != -1 ? mnAudioOffset : 0) -
                                     mnAudioLatency;
            }
            bool bAVSyncDrop = false;

            /*With PCR change timestamp value will be incremental rather than getting reset
              to 0*/
            if(mnFirstAudioFrameTimeStamp == 0)
                mnFirstAudioFrameTimeStamp = pFrameInfo->pBuffer->nTimeStamp;

            int deltaAudioTimeStamp = pFrameInfo->pBuffer->nTimeStamp - mnFirstAudioFrameTimeStamp;
            if(deltaAudioTimeStamp >= 0 &&
               deltaAudioTimeStamp <= INITIAL_AUDIO_DROP_WINDOW)
            {
                bAVSyncDrop = true;
                WFDMMLOGE1("deltaAudioTimeStamp = %d",deltaAudioTimeStamp);
            }
            else if (nDropCount)
            {
                nDropCount--;
                bAVSyncDrop = true;
            }
            if((( (int64)timedecider <= (int64)mCfg.naudioAVSyncDropWindow)&&
                  mCfg.bAVSyncMode) || ISSTATECLOSING ||
                 (mbFlushInProgress
              && (uint64)pFrameInfo->pBuffer->nTimeStamp <= mnFlushTimeStamp)||
                 bAVSyncDrop)
            {
                /*-------------------------------------------------------------
                  In this case the frame is late. Therefor we need to drop
                  it if it exceeds the time window
                ----------------------------------------------------------------
                */

                if(!bAVSyncDrop && !mbFlushInProgress
                    && !ISSTATECLOSING)
                    nDropCount = MAX_CONSEQ_AUDIO_DROP;

                if(WFD_LIKELY(m_pStatInstance))
                {
                    rendererStatsType nRendStatObj;

                    nRendStatObj.nIsVideoFrame = 0;
                    nRendStatObj.nIsLate = 1;
                    nRendStatObj.nTimeDecider = timedecider;
                    nRendStatObj.nSessionTime =
                        getCurrentTimeUs() - mnBaseMediaTime;
                    nRendStatObj.nArrivalTime = pFrameInfo->nTSArrival;

                    m_pStatInstance->UpdateDropsOrLateby(nRendStatObj);
                }

                WFDMMLOGH2(
                   "Renderer drop Audioframe TS = %llu, lateby = %lld",
                   pFrameInfo->pBuffer->nTimeStamp,
                   (int64)timedecider);
                pFrameInfo->pBuffer->nFilledLen = 0;
                mAudioQ->Pop(&pFrameInfo,
                     sizeof(&pFrameInfo),
                     0);
                mpFnEBD(mClientData, mnModuleId, SINK_AUDIO_TRACK_ID,
                        pFrameInfo->pBuffer);
                accessFrameInfoMeta(&pFrameInfo, pushfront);

                if(mAudioQ->GetSize() == 0)
                {
                    MM_Timer_Sleep(2);
                }
            }
            else if((int64)timedecider <= AUDIO_RENDER_TOLERANCE ||
              !mCfg.bAVSyncMode)
            {
                if(mbFlushInProgress &&
                   ((uint64)pFrameInfo->pBuffer->nTimeStamp >= mnFlushTimeStamp))
                {
                    WFDMMLOGH("Renderer Flush complete");
                    mbFlushInProgress = false;
                }
                if(WFD_LIKELY(m_pStatInstance))
                {
                    rendererStatsType nRendStatObj;

                    nRendStatObj.nIsVideoFrame = 0;
                    nRendStatObj.nIsLate = 0;
                    nRendStatObj.nTimeDecider = timedecider;
                    nRendStatObj.nSessionTime =
                        getCurrentTimeUs() - mnBaseMediaTime;
                    nRendStatObj.nArrivalTime = pFrameInfo->nTSArrival;

                    m_pStatInstance->UpdateDropsOrLateby(nRendStatObj);
                }

                mAudioQ->Pop(&pFrameInfo,
                     sizeof(&pFrameInfo),
                     0);
                if(mnAudioOffset == -1)
                {
                    mnAudioOffset = getCurrentTimeUs() - (mCfg.nDecoderLatency +
                                      pFrameInfo->pBuffer->nTimeStamp); //in us
                    if(mnAudioOffset < 0)
                    {
                        mnAudioOffset = 0;
                    }
                }
                if(!PushAudio4Render(pFrameInfo))
                {
                    WFDMMLOGE("Failed to render a Audio frame");
                }
                if(mAudioQ->GetSize() == 0)
                {
                    MM_Timer_Sleep(2);
                }
            }
            else
            {
                WFDMMLOGD("Pending AudioFrame not ready for render");
                MM_Timer_Sleep(2);
            }
        }
        else
        {
            MM_Timer_Sleep(2);
        }
    }
    WFDMMLOGD("Renderer Audio Thread ended");
}

/*==============================================================================
         FUNCTION:         state
         DESCRIPTION:
*//**       @brief         Updates or looks at current state
*//**
@par     DEPENDENCIES:
                           None
*//*
         PARAMETERS:
*//**       @param         state   - target State if set
                           get_true_set_false - get or set state
*//*     RETURN VALUE:
*//**       @return
                           current state
@par     SIDE EFFECTS:
                           None
*//*==========================================================================*/

int WFDMMSinkAudioRenderer::state(int state, bool get_true_set_false)
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

         FUNCTION:         getCurrentTimeUs

         DESCRIPTION:
*//**       @brief         returns the current system clock in microsecond
                           precision
*//**
@par     DEPENDENCIES:
                            None
*//*
         PARAMETERS:
*//**       @param          None

*//*     RETURN VALUE:
*//**       @return
                            Current time in Us
@par     SIDE EFFECTS:
                            None
*//*==========================================================================*/
uint64 WFDMMSinkAudioRenderer::getCurrentTimeUs()
{
    struct timespec sTime;
    clock_gettime(CLOCK_MONOTONIC, &sTime);

    uint64 currTS = ((OMX_U64)sTime.tv_sec * 1000000/*us*/)
                          + ((OMX_U64)sTime.tv_nsec / 1000);
    return currTS - mnBaseMediaTime;

}

/*==============================================================================

         FUNCTION:          Stop

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
OMX_ERRORTYPE WFDMMSinkAudioRenderer::Stop()
{

    SETSTATECLOSING;

    uint32 nAudPendingSize = mAudioQ ? mAudioQ->GetSize() : 0;

    /*--------------------------------------------------------------------------
      Wait for all buffers to be returned
    ----------------------------------------------------------------------------
    */
    WFDMMLOGH("Waiting for Renderer buffers to be returned");

    unsigned long nStartMs = 0;
    unsigned long nCurrMs = 0;
    bool bIsStartTimeSet = false;

    while(nAudPendingSize)
    {
        MM_Timer_Sleep(2);
        nAudPendingSize = mAudioQ ? mAudioQ->GetSize() : 0;

        WFDMMLOGH1("Aud Pending = %d",nAudPendingSize);
        if(nAudPendingSize == 0)
        {
            break;
        }
    }

    /*--------------------------------------------------------------------------
     Now we have returned all buffers. We can move to INIT.
    ----------------------------------------------------------------------------
    */
    SETSTATECLOSED;

    return OMX_ErrorNone;
}

/*==============================================================================

         FUNCTION:          Start

         DESCRIPTION:
*//**       @brief          Make Renderer ready for buffer flow
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
OMX_ERRORTYPE WFDMMSinkAudioRenderer::Start()
{
    if(!ISSTATEINIT)
    {
        WFDMMLOGE("Renderer not in Init when started");
        return OMX_ErrorInvalidState;
    }
    WFDMMLOGH("WFDMMSinkRenderer Start");

    SETSTATEOPENED;

    if(mpAudioThread)
    {
        mpAudioThread->SetSignal(0);
    }
    return OMX_ErrorNone;
}
/*==============================================================================

         FUNCTION:

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
void WFDMMSinkAudioRenderer::SetMediaBaseTime(uint64 nTime)
{
    WFDMMLOGH("Audio Renderer Setting Media Base Time");
    /*-------------------------------------------------------------------------
      Renderer shall not receive any new frames when Media Base Time is being
      set. Renderer will flush out its existing data and only then will honor
      the new base time
    ---------------------------------------------------------------------------
    */
    mbAudioTimeReset = true;
    mnFlushTimeStamp = (uint64)-1;
    mbFlushInProgress = true;

    unsigned long nStartMs = 0;
    unsigned long nCurrMs = 0;
    MM_Time_GetTime(&nStartMs);

    while(mAudioQ && mAudioQ->GetSize())
    {
        MM_Time_GetTime(&nCurrMs);
        if((nCurrMs - nStartMs) > 1000)//wait for a second)
        {
            break;
        }
        MM_Timer_Sleep(5);
    }

    mnFlushTimeStamp = 0;
    mbFlushInProgress = false;

    mbMediaTimeSet = true;
    mnBaseMediaTime = nTime;
}

/*==============================================================================

         FUNCTION:          GenerateAACHeaderFromADTS

         DESCRIPTION:
*//**       @brief          Generate ADIF AAC header from ADTS header
                            information
*//**
@par     DEPENDENCIES:
                            None
*//*
         PARAMETERS:
*//**       @param          None

*//*     RETURN VALUE:
*//**       @return
                            true or false
@par     SIFE EFFECTS:
                            None
*//*==========================================================================*/
bool WFDMMSinkAudioRenderer::GenerateAACHeaderFromADTS(uint8* pAdts, uint32 nAdtsLen,
                                          uint8 *pAacHeader, uint32 *nAACHdrLen)
{
    if(!pAdts || nAdtsLen < 7 || !pAacHeader || (*nAACHdrLen) < 2)
    {
        WFDMMLOGE("GenerateAACHeader Invalid Args");
        return false;
    }

    uint8 nAudioObjectType = ((pAdts [2] >> 6) & 0x03) + 1;
    uint8 nSampFreqIndex   = ((pAdts [2] >> 2) & 0x0F);
    uint8 nChannelConfig   = ((pAdts [2] << 2) & 0x04)
                           | ((pAdts [3] >> 6) & 0x03);


    pAacHeader [0]
            = (OMX_U8)((nAudioObjectType << 3)
               | ((nSampFreqIndex & 0x0E) >> 1));
    pAacHeader [1]
            = (OMX_U8)(((nSampFreqIndex & 0x01) << 7)
               | (nChannelConfig << 3));

    *nAACHdrLen = 2;

    return true;
}

/*==============================================================================

         FUNCTION:          AudioTrackCb

         DESCRIPTION:
*//**       @brief          Any event for Audio track is handled here
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
void  WFDMMSinkAudioRenderer::AudioTrackCb(int  nEvent,
                                      void *pCookie,
                                      void *pInfo)
{
    (void) pCookie;
    (void) pInfo;
    WFDMMLOGD1("AudioTrack CallBack event = %d", nEvent);

    WFDMMSinkAudioRenderer  *pMe = (WFDMMSinkAudioRenderer*)pCookie;

    if(pMe)
    {
        pMe->ProcessAudioTrackCb(nEvent,pInfo);
    }

    return;
}

/*==============================================================================

         FUNCTION:          ProcessAudioTrackCb

         DESCRIPTION:
*//**       @brief          Any event for Audio track is processed here
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
void  WFDMMSinkAudioRenderer::ProcessAudioTrackCb(int  nEvent,
                                      void *pInfo)
{
    UNUSED(pInfo);
    WFDMMLOGD1("Process AudioTrack CallBack event = %d", nEvent);

    if(nEvent == android::AudioTrack::EVENT_NEW_IAUDIOTRACK)
    {
        mbRestartAudioTrack = true;
    }

    return;
}

/*==============================================================================

         FUNCTION:          prepareDumpFiles

         DESCRIPTION:
*//**       @brief          prepare the Dump files
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
void WFDMMSinkAudioRenderer::prepareDumpFiles()
{
    int nRet = 0;
    char szTemp[PROPERTY_VALUE_MAX] = {(char)0};
#ifdef WFD_DUMP_PCM
    nRet = property_get("persist.debug.wfd.dumpwav",szTemp,NULL);
    if (nRet <= 0 )
    {
        WFDMMLOGE2("Failed to read prop %d %s value PCM", nRet, szTemp);
    }

    if(strncmp(szTemp,"1", sizeof("1"))==0)
    {
        WFDMMLOGH("Renderer Dump enabled for PCM");
        mbDumpWav = true;
    }


    if(mbDumpWav)
    {
        mpFpWAV= fopen("/data/media/wfdsinkaudiodump.wav","wb");
        if(!mpFpWAV)
        {
            WFDMMLOGE("Renderer::audiodump fopen failed");
            mbDumpWav = OMX_FALSE;
        }
        else
        {
           #define ID_RIFF 0x46464952
           #define ID_WAVE 0x45564157
           #define ID_FMT  0x20746d66
           #define ID_DATA 0x61746164
            struct WavHeader {
                uint32_t riff_id;
                uint32_t riff_sz;
                uint32_t riff_fmt;
                uint32_t fmt_id;
                uint32_t fmt_sz;
                uint16_t audio_format;
                uint16_t num_channels;
                uint32_t sample_rate;
                uint32_t byte_rate;
                uint16_t block_align;
                uint16_t bits_per_sample;
                uint32_t data_id;
                uint32_t data_sz;
            }sHdr;

            sHdr.riff_id = ID_RIFF;
            sHdr.riff_fmt = ID_WAVE;
            sHdr.fmt_id = ID_FMT;
            sHdr.fmt_sz = 16;
            sHdr.audio_format = 1;
            sHdr.num_channels = mCfg.sAudioInfo.nChannels;
            sHdr.sample_rate = 48000;
            sHdr.bits_per_sample = 16;
            sHdr.byte_rate = (48000 * mCfg.sAudioInfo.nChannels
                                      * mCfg.sAudioInfo.nSampleRate) / 8;
            sHdr.block_align = (sHdr.bits_per_sample *
                                 mCfg.sAudioInfo.nSampleRate) / 8;
            sHdr.data_id = ID_DATA;
            sHdr.data_sz = 2147483648LL;
            sHdr.riff_sz = sHdr.data_sz + 44 - 8;
            fwrite(&sHdr,1, sizeof(sHdr),mpFpWAV);
            WFDMMLOGE("Renderer::Writing  wav header");
        }
    }
#endif
#ifdef WFD_DUMP_AAC
    memset(szTemp, 0, sizeof(szTemp));
    nRet = property_get("persist.debug.wfd.dumpaac",szTemp,NULL);
    if (nRet <= 0 )
    {
        WFDMMLOGE2("Failed to read prop %d %s value AAC", nRet, szTemp);
    }

    if(strncmp(szTemp,"1", sizeof("1"))==0)
    {
        WFDMMLOGH("Renderer Dump Enabled for AAC");
        mbDumpAAC = true;
    }

    if(mbDumpAAC)
    {
        mpFpAAC = fopen("/data/media/wfdsinkaacdump.aac","wb");
        if(!mpFpAAC)
        {
            WFDMMLOGE("Renderer:aacdump fopen failed");
            mbDumpAAC = OMX_FALSE;
        }
    }
#endif
}

/*==============================================================================

         FUNCTION:          pauseRendering

         DESCRIPTION:
*//**       @brief          pauses rendering and holds on to the buffers
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
void WFDMMSinkAudioRenderer::pauseRendering(void)
{
    mbPaused = true;
}

/*==============================================================================

         FUNCTION:          pauseRendering

         DESCRIPTION:
*//**       @brief          pauses rendering and holds on to the buffers
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
void WFDMMSinkAudioRenderer::restartRendering(bool bFlush)
{
    if(mbPaused && bFlush)
    {
        mnFlushTimeStamp = (uint64)-1;
        mbFlushInProgress = true;

        unsigned long nStartMs = 0;
        unsigned long nCurrMs = 0;
        MM_Time_GetTime(&nStartMs);

        while(mAudioQ && mAudioQ->GetSize())
        {
            MM_Time_GetTime(&nCurrMs);
            if((nCurrMs - nStartMs) > 1000)//wait for a second)
            {
                break;
            }
            MM_Timer_Sleep(5);
        }

        mnFlushTimeStamp = 0;
        mbFlushInProgress = false;

    }
    mbPaused = false;
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
void WFDMMSinkAudioRenderer::setFlushTimeStamp(uint64 nTS)
{
    mnFlushTimeStamp = nTS;
    mbFlushInProgress = true;
}

/*==============================================================================

         FUNCTION:          setDecoderLatency

         DESCRIPTION:
*//**       @brief          This simulates decoder latency by delaying the
                            rendering of frames to control smoothness of
                            playback
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
void WFDMMSinkAudioRenderer::setDecoderLatency(uint64 nLatencyInms)
{
    WFDMMLOGH("Renderer Setting decode latency");
    CRITICAL_SECT_ENTER;
    mCfg.nDecoderLatency = nLatencyInms;
    CRITICAL_SECT_LEAVE;
}

/*==============================================================================

         FUNCTION:          getLatency

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
uint32 WFDMMSinkAudioRenderer::getLatency()
{
    uint32 trackLatency = 0;
    if(mpAudioTrack != NULL)
    {
        trackLatency = mpAudioTrack->latency();
        WFDMMLOGM1("AudioRenderer:: Audio Track Latency = %u",trackLatency);
    }
    else
    {
        WFDMMLOGE("AudioRenderer:: Audio Track Handle is NULL");
    }

    return trackLatency;
}

