/*==============================================================================
*       WFDMMSinkVideoRenderer.cpp
*
*  DESCRIPTION:
*       Does AV SYnc and video render.
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
#include <cutils/properties.h>
#include "WFDMMLogs.h"
#include "WFDMMSinkVideoRenderer.h"
#include "WFDMMThreads.h"
#include "WFDMMSourceMutex.h"
#include "WFDMMSourceSignalQueue.h"
#include "MMMalloc.h"
#include "MMMemory.h"
#include "qmmList.h"
#include "MMTimer.h"
#include "gralloc_priv.h"
#include "qdMetaData.h"
#include "WFDMMSinkStatistics.h"

#include "WFDUtils.h"
#define MAX_CONSEQ_VIDEO_DROP 2
#define MAX_CONSEQ_VIDEO_FRAMES_COUNT 30
#define DEFAULT_AUDIO_TRACK_LATENCY 90000

#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "WFDMMVIDEORENDERER"

#define VIDEO_RENDER_TOLERANCE 2000 //us

using namespace android;

#define CRITICAL_SECT_ENTER if(mhCritSect)                                     \
                                  MM_CriticalSection_Enter(mhCritSect);        \
/*      END CRITICAL_SECT_ENTER    */

#define CRITICAL_SECT_LEAVE if(mhCritSect)                                     \
                                  MM_CriticalSection_Leave(mhCritSect);        \
/*      END CRITICAL_SECT_LEAVE    */

#define CRITICAL_SECT_ENTER_FRAME if(mhCritSectFrame)                          \
                                  MM_CriticalSection_Enter(mhCritSectFrame);   \
/*      END CRITICAL_SECT_ENTER_FRAME    */

#define CRITICAL_SECT_LEAVE_FRAME if(mhCritSectFrame)                          \
                                  MM_CriticalSection_Leave(mhCritSectFrame);   \
/*      END CRITICAL_SECT_LEAVE_FRAME    */

#define NOTIFYERROR  mpFnHandler((void*)(int64)mClientData,                    \
                                 mnModuleId,WFDMMSINK_ERROR,                   \
                                 OMX_ErrorUndefined,                           \
                                 0 );                                          \
/*NOTIFYERROR*/

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

         FUNCTION:         WFDMMSinkRenderer

         DESCRIPTION:
*//**       @brief         WFDMMSinkRenderer contructor
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
WFDMMSinkVideoRenderer::WFDMMSinkVideoRenderer(int moduleId,
                                     WFDMMSinkEBDType       pFnEBD,
                                     WFDMMSinkHandlerFnType   pFnHandler,
                                     WFDMMSinkAudioLatFnType pAudioLatFnHandler,
                                     void* clientData,
                                     WFDMMSinkStatistics* pStatInst)
{

    InitData();
    /*--------------------------------------------------------------------------

    ----------------------------------------------------------------------------
    */
    mpFnHandler = pFnHandler;
    mpFnEBD     = pFnEBD;
    mpAudioLatFnHandler = pAudioLatFnHandler;
    mnModuleId  = moduleId;
    mClientData = clientData;
    m_pStatInstance = pStatInst;

    mhCritSect = NULL;
    int nRet = MM_CriticalSection_Create(&mhCritSect);
    if(nRet != 0)
    {
        mhCritSect = NULL;
        WFDMMLOGE("Error in Critical Section Create");
    }

    mhCritSectFrame = NULL;
    nRet = MM_CriticalSection_Create(&mhCritSectFrame);
    if(nRet != 0)
    {
        mhCritSectFrame = NULL;
        WFDMMLOGE("Error in Critical Section Frame Create");
    }

    SETSTATEDEINIT;

}

/*==============================================================================

         FUNCTION:          ~WFDMMSinkRenderer

         DESCRIPTION:
*//**       @brief          Destructor for renderer class
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
WFDMMSinkVideoRenderer::~WFDMMSinkVideoRenderer()
{
    WFDMMLOGH("WFDMMSinkRenderer Destructor");
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
void WFDMMSinkVideoRenderer::InitData()
{
    mbFirstFrame = true;
    mFrameInfoFreeQ = NULL;
    mpVideoThread = NULL;
    mpGraphicPollThread = NULL;
    mhCritSect = NULL;
    mhCritSectFrame = NULL;
    mpWindow = NULL;
    mbMediaTimeSet = false;
    mbVideoTimeReset = true;
    mnNumVidQedBufs = 0;
    mnNumVidDQedBufs = 0;
    memset(&mCfg, 0, sizeof(mCfg));
    memset(&mPictInfo, 0, sizeof(mPictInfo));
    memset(&msFrameInfo, 0, sizeof(msFrameInfo));
    meState = DEINIT;
    mClientData = 0;
    mnModuleId = 0;
    mpFnEBD = NULL;
    mpFnHandler = NULL;
    mnBaseMediaTime = 0;
    mbMediaTimeSet = false;
    mnErr = 0;
    mnVideoMaxConsecDrop = MAX_CONSEQ_VIDEO_DROP;
    mnVideoDropCount = 0;
    mbLookForIDR = true;
    mbFlushInProgress = false;
    mnFlushTimeStamp = 0;
    mbPaused = false;
    mnAudioLatencyCheckCount = 0;
    mnAddAudioTrackLatency = 0;
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
OMX_ERRORTYPE WFDMMSinkVideoRenderer::Configure(rendererConfigType *pCfg)
{
    WFDMMLOGH("videorenderer Configure");
    CHECK_ARGS_RET_OMX_ERR_3(pCfg, mpFnHandler, mpFnEBD);

    memcpy(&mCfg, pCfg, sizeof(rendererConfigType));

    if(pCfg->bHasVideo)
    {
        mpWindow = pCfg->pWindow;

        if(mpWindow == NULL)
        {
            return OMX_ErrorBadParameter;
        }
    }

    if(createResources() != OMX_ErrorNone)
    {
        WFDMMLOGE("VideoRenderer Create Resources Failed");
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
OMX_ERRORTYPE WFDMMSinkVideoRenderer::AllocateBuffers()
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
*//**       @brief          Audio and Video Buffers are received through this
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
OMX_ERRORTYPE WFDMMSinkVideoRenderer::DeliverInput(int trackId,
                                             OMX_BUFFERHEADERTYPE *pBuffer)
{
    /*--------------------------------------------------------------------------
        Once a frame is received to renderer as part of DeliverInput, drop it
        here provided any one of the following conditions is met:
        1. If pBuffer is invalid
        2. If buffer is received in incorrect state
        3. If buffer is received when video track is not advertised

        4. If renderer is waiting for OMX_BUFFERFLAG_STARTTIME
        5. If zero length buffers are received in the middle of session
        6. If session starts with a P-Frame, drop till an I-Frame is received
        7. If corrupt buffer is received in DROP_TILL_IDR mode
    ----------------------------------------------------------------------------
    */
    CRITICAL_SECT_ENTER_FRAME;
    if (!pBuffer)
    {
        WFDMMLOGE("Invalid Arguments");
        CRITICAL_SECT_LEAVE_FRAME;
        return OMX_ErrorBadParameter;
    }

    WFDMMLOGM5("DeliverInput: TS[%llu] nFilledLen[%d] SYNC/CRPT/STIM[%d/%d/%d]",
        pBuffer->nTimeStamp, pBuffer->nFilledLen,
        pBuffer->nFlags & OMX_BUFFERFLAG_SYNCFRAME,
        pBuffer->nFlags & OMX_BUFFERFLAG_DATACORRUPT,
        pBuffer->nFlags & OMX_BUFFERFLAG_STARTTIME);

    if (ISSTATECLOSING || ISSTATEDEINIT || ISSTATECLOSED)
    {
        /*----------------------------------------------------------------------
         We can allow buffer flow in INIT and OPENED
        ------------------------------------------------------------------------
        */
        WFDMMLOGE1("Output Delivered when state not in play. TS[%llu]",
            pBuffer->nTimeStamp);
        CRITICAL_SECT_LEAVE_FRAME;
        return OMX_ErrorInvalidState;
    }

    /*--------------------------------------------------------------------------
     Check if Video or Audio is advertized during start up. Reject any samples
     from unsupported track
    ----------------------------------------------------------------------------
    */
    if(trackId == SINK_VIDEO_TRACK_ID && !mCfg.bHasVideo)
    {
        WFDMMLOGE1("Sample for track when track not advertized. TS[%llu]",
            pBuffer->nTimeStamp);
        CRITICAL_SECT_LEAVE_FRAME;
        return OMX_ErrorBadParameter;
    }

    if(trackId == SINK_VIDEO_TRACK_ID)
    {
        bool returnFrame = false;
        /*----------------------------------------------------------------------
            Check if timebase has changed and needs reset
        ------------------------------------------------------------------------
        */
        if(mbVideoTimeReset)
        {
            if((pBuffer->nFlags & OMX_BUFFERFLAG_STARTTIME) == 0)
            {
                WFDMMLOGE1("Renderer Waiting for Video STARTTIME. TS[%llu]",
                    pBuffer->nTimeStamp);
                returnFrame = true;
            }
            else
            {
                mbVideoTimeReset = false;
            }
        }

        /*----------------------------------------------------------------------
            Do not queue zero sized frame in middle of session
        ------------------------------------------------------------------------
        */
        if(!mbFirstFrame && !pBuffer->nFilledLen)
        {
            WFDMMLOGE1("Not queing 0 size frames in middle of session. TS[%llu]",
                pBuffer->nTimeStamp);
            returnFrame = true;
        }

        /*----------------------------------------------------------------------
            If a session starts off with a P-Frame, drop all the frames till an
            I-Frame is received
        ------------------------------------------------------------------------
        */
        if(mbLookForIDR)
        {
            if(!(pBuffer->nFlags & OMX_BUFFERFLAG_SYNCFRAME))
            {
                WFDMMLOGE1("Not queuing P-frames, looking for I-Frame. TS[%llu]",
                    pBuffer->nTimeStamp);
                returnFrame = true;
            }
            else
            {
                mbLookForIDR = false;
            }
        }

        /*----------------------------------------------------------------------
            Drop the frame in case it is corrupted and DROP_TILL_IDR mode is set
        ------------------------------------------------------------------------
        */
        if(mCfg.sVideoInfo.nFrameDropMode == FRAME_DROP_DROP_TILL_IDR)
        {
            if(pBuffer->nFlags & OMX_BUFFERFLAG_SYNCFRAME)
            {
                mbLookForIDR = false;
                WFDMMLOGE1("Renderer Found IDR, stop frame drop. TS[%llu]",
                    pBuffer->nTimeStamp);
            }
            if(pBuffer->nFlags & OMX_BUFFERFLAG_DATACORRUPT)
            {
                mbLookForIDR = true;
                WFDMMLOGE1("Renderer Drop Frame, Corrupt Data. TS[%llu]",
                    pBuffer->nTimeStamp);
            }
            if(mbLookForIDR)
            {
                WFDMMLOGE1("Renderer Drop Corrupt Frame. Wait for IDR. TS[%llu]",
                    pBuffer->nTimeStamp);
                returnFrame = true;
            }
        }

        if(returnFrame)
        {
            CRITICAL_SECT_LEAVE_FRAME;
            return OMX_ErrorInvalidState;
        }
    }

    frameInfoType *pFrameInfo = NULL;
    accessFrameInfoMeta(&pFrameInfo, popfront);
    if (!pFrameInfo)
    {
        WFDMMLOGE("Failed to get frameInfo");
        NOTIFYERROR;
        CRITICAL_SECT_LEAVE_FRAME;
        return OMX_ErrorInsufficientResources;
    }

    pFrameInfo->pBuffer = pBuffer;
    if(!Scheduler(trackId, pFrameInfo))
    {
        accessFrameInfoMeta(&pFrameInfo, pushrear);
        CRITICAL_SECT_LEAVE_FRAME;
        NOTIFYERROR;
        WFDMMLOGE1("Failed to Schedule track = %d", trackId);
        return OMX_ErrorUndefined;
    }

    CRITICAL_SECT_LEAVE_FRAME;
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
OMX_ERRORTYPE WFDMMSinkVideoRenderer::createResources()
{
    if(!createThreadsAndQueues())
    {
        WFDMMLOGE("Renderer Failed to create Threads and Queues");
        return OMX_ErrorInsufficientResources;
    }

    if(mCfg.bHasVideo)
    {
        if(mpWindow == NULL)
        {
            WFDMMLOGE("No valid native window to render");
            return OMX_ErrorInsufficientResources;
        }
        mnVideoDropFrames = mCfg.sVideoInfo.nFrameRate;
    }
    return OMX_ErrorNone;
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
bool WFDMMSinkVideoRenderer::createThreadsAndQueues()
{
    /*--------------------------------------------------------------------------
      Create threads if session has audio and video
    ----------------------------------------------------------------------------
    */
    if(mCfg.bHasVideo)
    {
        mpVideoThread = MM_New_Args(WFDMMThreads, (1));

        if(!mpVideoThread)
        {
            WFDMMLOGE("Failed to create Video Thread");
            return false;
        }

        mpVideoThread->Start(VideoThreadEntry, -2,
                             32768,this, "WFDSinkVideorenderer");

        mpGraphicPollThread = MM_New_Args(WFDMMThreads, (1));

        if(!mpGraphicPollThread)
        {
            WFDMMLOGE("Failed to create Graphic Thread");
            return false;
        }

        mpGraphicPollThread->Start(GraphicThreadEntry, -2,
                             32768,this, "WFDSinkGraphicrenderer");

    }

    if(mCfg.bHasVideo)
    {
        (void)qmm_ListInit(&mVideoPendingQ);
        (void)qmm_ListInit(&mVideoRenderQ);
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
bool WFDMMSinkVideoRenderer::accessFrameInfoMeta(frameInfoType **pFrameInfo,
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
bool WFDMMSinkVideoRenderer::Scheduler(int trackId, frameInfoType *pFrameInfo)
{

    /*--------------------------------------------------------------------------
     TODO - do any adjustments based on different propagation delays here.
     For this delays of audio and video paths has to be measured.
    ----------------------------------------------------------------------------
    */
    pFrameInfo->nTSArrival = getCurrentTimeUs();;
    pFrameInfo->nTSSched   = pFrameInfo->pBuffer->nTimeStamp;

    if(trackId == SINK_VIDEO_TRACK_ID)
    {
        if(mbFirstFrame && !pFrameInfo->pBuffer->nFilledLen)
        {
            WFDMMLOGH1("Cancel Initial frames to Native Window.TS[%llu]",
                pFrameInfo->pBuffer->nTimeStamp);
            /*------------------------------------------------------------------
              Cancel Initial Buffers. Don't know why, but native window
              doesnt work without this and no documentation provided.
            --------------------------------------------------------------------
            */
            pictureInfoType *pPictInfo =
                           (pictureInfoType *)pFrameInfo->pBuffer->pAppPrivate;

            GraphicBuffer *pGraphicBuffer = (GraphicBuffer *)
                                        pPictInfo->pGraphicBuffer;

            int nErr = mpWindow->cancelBuffer(
                          mpWindow.get(),
                          pGraphicBuffer->getNativeBuffer(),
                         -1);
            if (nErr != 0)
            {
                WFDMMLOGE("Failed to cancel graphic buffer");
                return false;
            }
            mnNumVidQedBufs++;
            AccessRenderQ(&pFrameInfo, (uint32)0, pushrear);
            return true;
        }
        /*----------------------------------------------------------------------
          At this time we should have received Base time, if not take it here.
        ------------------------------------------------------------------------
        */
        if(!mbMediaTimeSet)
        {
            mnBaseMediaTime = 0;
            mnBaseMediaTime = getCurrentTimeUs();
            mbMediaTimeSet  = true;
        }
        AccessPendingQ(&pFrameInfo, pushrear);
        return true;
    }

    return false;

}

/*==============================================================================

         FUNCTION:          PushVideo4Render

         DESCRIPTION:
*//**       @brief          Pushes video frames for rendering
*//**
@par     DEPENDENCIES:
                            None
*//*
         PARAMETERS:
*//**       @param          pFrameInfo - Current frame to be rendered

*//*     RETURN VALUE:
*//**       @return
                            true or false
@par     SIDE EFFECTS:
                            None
*//*==========================================================================*/
bool WFDMMSinkVideoRenderer::PushVideo4Render(frameInfoType *pFrameInfo)
{

    if(pFrameInfo != NULL)
    {
#ifdef DROP_INITIAL_VIDEO_FRAMES
        if(mnVideoDropFrames)
        {
            mnVideoDropFrames--;

            if(mnVideoDropFrames % 3)
            {
                pFrameInfo->pBuffer->nFilledLen = 0;
                mpFnEBD(mClientData, mnModuleId, SINK_VIDEO_TRACK_ID,
                        pFrameInfo->pBuffer);
                accessFrameInfoMeta(&pFrameInfo, pushfront);
                return true;
            }
        }
#endif

        /*------------------------------------------------------------------
          First push to render Q to avoid any race condition. Then send to
          actualRenders
        --------------------------------------------------------------------
        */
        AccessRenderQ(&pFrameInfo, (uint32)0, pushrear);

        bool bRet = false;

        bRet = RenderVideo(pFrameInfo);
        if(!bRet)
        {
            WFDMMLOGE("Failed to Render video track");
            pFrameInfo = NULL;
            AccessRenderQ(&pFrameInfo, (uint32)0, poprear);
            if(pFrameInfo)
            {
                mpFnEBD(mClientData, mnModuleId, SINK_VIDEO_TRACK_ID,
                        pFrameInfo->pBuffer);
                accessFrameInfoMeta(&pFrameInfo, pushfront);
            }
            return false;
        }
    }
    return true;
}

/*==============================================================================

         FUNCTION:          RenderVideo

         DESCRIPTION:
*//**       @brief          Render the video. Push to Native window
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
bool WFDMMSinkVideoRenderer::RenderVideo(frameInfoType *pFrameInfo)
{
    if(!pFrameInfo || !pFrameInfo->pBuffer ||
                 !pFrameInfo->pBuffer->pAppPrivate)
    {
        WFDMMLOGE("Invalid FrameBuffer passed");
        return false;
    }

    pictureInfoType *pPictInfo = (pictureInfoType *)
                                  pFrameInfo->pBuffer->pAppPrivate;

    GraphicBuffer *pGraphicBuffer = (GraphicBuffer *)pPictInfo->pGraphicBuffer;
    ANativeWindowBuffer * pNativeBuffer = pGraphicBuffer->getNativeBuffer();
    BufferDim_t bufferDim = {(int32_t)pPictInfo->rect.nWidth,
                             (int32_t)pPictInfo->rect.nHeight};
    int nRets = setMetaData((private_handle_t*)pNativeBuffer->handle,
                            UPDATE_BUFFER_GEOMETRY, &bufferDim);
    if(nRets != 0)
    {
         WFDMMLOGE("Failed to set Buffer geometry");
         return false;
    }

    WFDMMLOGM3("QueuBuffer to Native window: TS[%llu] nFilledLen[%d] FirstFrame[%d]",
        pFrameInfo->pBuffer->nTimeStamp, pFrameInfo->pBuffer->nFilledLen,
        mbFirstFrame);

    if(mbFirstFrame || anyChangeInCrop(pPictInfo))
    {
        /*----------------------------------------------------------------------
         Set the crop and reset dimensions if video resolution changes in
         runtime - read smoothstreaming.
        ------------------------------------------------------------------------
        */
        memcpy(&mPictInfo, pPictInfo, sizeof(mPictInfo));
        android_native_rect_t sCrop;

        sCrop.left     = pPictInfo->rect.nLeft;
        sCrop.top      = pPictInfo->rect.nTop;
        sCrop.right    = pPictInfo->rect.nLeft + pPictInfo->rect.nWidth;
        sCrop.bottom   = pPictInfo->rect.nTop + pPictInfo->rect.nHeight;

        WFDMMLOGH4("WFDMMRenderer SetCrop %d %d %d %d",
                   sCrop.left, sCrop.top, sCrop.right, sCrop.bottom);

        int nRet = 0;
/*
        nRet = mpWindow.get()->perform(mpWindow.get(),
               NATIVE_WINDOW_UPDATE_BUFFERS_GEOMETRY,
               pPictInfo->nWidth,
               pPictInfo->nHeight,
               pPictInfo->eColorFmt);
*/

        nRet = native_window_set_crop(mpWindow.get(),
                                          &sCrop);
        if(nRet != 0)
        {
            WFDMMLOGE("Failed to set crop on native window");
        }
        mbFirstFrame = false;
    }

    if(WFD_LIKELY(m_pStatInstance))
    {
        m_pStatInstance->recordSchedDelay(pFrameInfo->pBuffer);
    }

    int nRet = mpWindow->queueBuffer(mpWindow.get(),
                                 pGraphicBuffer->getNativeBuffer(), -1);

    if(nRet != 0)
    {
        WFDMMLOGE("Failed to queueBuffer to NativeWindow");
        return false;
    }

    if(WFD_LIKELY(m_pStatInstance))
    {
        m_pStatInstance->recordDisplayIP(pFrameInfo->pBuffer);
    }

    mnNumVidQedBufs++;

    return true;
}

/*==============================================================================
         FUNCTION:         SearchBuffer
         DESCRIPTION:
*//**       @brief         Search for a node in the packet queue
*//**
@par     DEPENDENCIES:
                           None
*//*
         PARAMETERS:
*//**       @param         pH - pointer the the list handle
                           pFrameInfo - pointer to hold the packet once found
                           seqNum - sequence number of the packet being searched
*//*     RETURN VALUE:
*//**       @return
                           0 -success
                           -1 failure
@par     SIDE EFFECTS:
                           None
*//*==========================================================================*/
bool WFDMMSinkVideoRenderer::SearchBuffer(QMM_ListHandleType *pH,
                                    frameInfoType **pFrameInfo,
                                    long bufferId)
{
    if(!pH || !pFrameInfo)
    {
        WFDMMLOGE("Invalid Arguments");
        return false;
    }

    QMM_ListErrorType eError;

    WFDMMLOGH1("Searching for Buffer Id = %ld", bufferId);

    //Look for a valid Packet in History with matching seqNo
    eError = qmm_ListSearch(pH, ComparePayloadParam, &bufferId,
                            (QMM_ListLinkType **)pFrameInfo);

    if(eError == QMM_LIST_ERROR_PRESENT && *pFrameInfo)
    {
        (void)qmm_ListPopElement(pH, (QMM_ListLinkType *)(*pFrameInfo));
    }
    else
    {
        return false;
    }

    return true;
}

/*==============================================================================
         FUNCTION:         ComparePayloadParam
         DESCRIPTION:
*//**       @brief         Compare a mamber in a node against a specified value
*//**
@par     DEPENDENCIES:
                           None
*//*
         PARAMETERS:
*//**       @param         pElement - pointer the the list Element
                           pCmpValue - pointer to value to be compared
*//*     RETURN VALUE:
*//**       @return
                           QMM_ListErrorType
@par     SIDE EFFECTS:
                           None
*//*==========================================================================*/
QMM_ListErrorType WFDMMSinkVideoRenderer::ComparePayloadParam
(
   void *pElement,
   void *pCmpValue
)
{
    if(!pElement || !pCmpValue)
    {
        return QMM_LIST_ERROR_BAD_PARM;
    }

    frameInfoType *pPkt = (frameInfoType *)pElement;

    pictureInfoType *pPictInfo = (pictureInfoType *)
                                 (pPkt->pBuffer->pAppPrivate);

    if(!pPictInfo || !pPictInfo->pGraphicBuffer)
    {
        return QMM_LIST_ERROR_NOT_PRESENT;
    }
    GraphicBuffer *pGraphicBuffer = (GraphicBuffer *)pPictInfo->pGraphicBuffer;

    if(*(reinterpret_cast<long*>(pCmpValue)) ==
       reinterpret_cast<long>(pGraphicBuffer->handle))
    {
        //Found what we are looking for
        WFDMMLOGH1("Buffer Found %lu", (long)pCmpValue);
        return QMM_LIST_ERROR_PRESENT;
    }

    return QMM_LIST_ERROR_NOT_PRESENT;
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
OMX_ERRORTYPE WFDMMSinkVideoRenderer::deinitialize()
{

    if(!ISSTATECLOSED || !ISSTATEINIT)
    {
        SETSTATECLOSING;
    }

    WFDMMLOGH("Wait for all buffers to be returned");

    int nPendingQLen;
    int nRenderQLen;
    while (1)
    {
        nPendingQLen = AccessPendingQ(NULL, size);
        nRenderQLen  = AccessRenderQ(NULL, (uint32)0, size);

        WFDMMLOGH1("QueueLength at exit VideoRender- %d", nRenderQLen);
        WFDMMLOGH1("QueueLength at exit VIdeoPending- %d", nPendingQLen);

        if((!nPendingQLen || nPendingQLen == -1) &&
           (!nRenderQLen  || nRenderQLen  == -1))
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

    /*--------------------------------------------------------------------------
      Close Threads and signal queues
    ----------------------------------------------------------------------------
    */

    if(mpVideoThread)
    {
        MM_Delete(mpVideoThread);
        mpVideoThread = NULL;
    }

    if (mpGraphicPollThread)
    {
        MM_Delete(mpGraphicPollThread);
        mpGraphicPollThread = NULL;
    }

    if(mFrameInfoFreeQ)
    {
        MM_Delete(mFrameInfoFreeQ);
        mFrameInfoFreeQ = NULL;
    }

    if(mhCritSect)
    {
        MM_CriticalSection_Release(mhCritSect);
        mhCritSect = NULL;
    }

    if(mhCritSectFrame)
    {
        MM_CriticalSection_Release(mhCritSectFrame);
        mhCritSectFrame = NULL;
    }

    qmm_ListDeInit(&mVideoRenderQ);
    qmm_ListDeInit(&mVideoPendingQ);

    if (mhCritSect)
    {
        MM_CriticalSection_Release(mhCritSect);
        mhCritSect = NULL;
    }

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
void WFDMMSinkVideoRenderer::VideoThreadEntry(void *pThis,
                                         unsigned int nSignal)
{
    CHECK_ARGS_RET_VOID_1(pThis);

    WFDMMSinkVideoRenderer *pMe = (WFDMMSinkVideoRenderer*)pThis;

    if(pMe)
    {
        pMe->VideoThread(nSignal);
    }
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
void WFDMMSinkVideoRenderer::VideoThread(unsigned int nSignal)
{
    (void) nSignal;
    WFDMMLOGH("WFDMMSinkRenderer::VideoThread");
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
        uint32 pendingQsize = AccessPendingQ(NULL, size);

        if(pendingQsize)
        {
            if(AccessPendingQ(&pFrameInfo, peekfront) == QMM_LIST_ERROR_NONE &&
               pFrameInfo != NULL)
            {
                if(mCfg.bHasAudio)
                {
                    AddAudioTrackLatency();
                }

                uint64 timedecider = pFrameInfo->pBuffer->nTimeStamp -
                                   getCurrentTimeUs() + mCfg.nDecoderLatency +
                                   mnAddAudioTrackLatency;

                if(((int64)timedecider <= (int64)mCfg.nvideoAVSyncDropWindow &&
                  mnVideoDropCount < mnVideoMaxConsecDrop &&
                  mCfg.bAVSyncMode) ||
                  ISSTATECLOSING    ||
                 (mbFlushInProgress
                && (uint64)pFrameInfo->pBuffer->nTimeStamp < mnFlushTimeStamp))
                {
                    /*----------------------------------------------------------
                      In this case the frame is late. Therefor we need to drop
                      it if it exceeds the time window and take the statistics.
                    ------------------------------------------------------------
                    */
                    mnVideoDropCount++;

                    if(!ISSTATECLOSING && !mbFlushInProgress)
                    {
                        if(WFD_LIKELY(m_pStatInstance))
                        {
                            rendererStatsType nRendStatObj;

                            nRendStatObj.nIsVideoFrame = 1;
                            nRendStatObj.nIsLate = 1;
                            nRendStatObj.nTimeDecider = (int64)timedecider;
                            nRendStatObj.nSessionTime =
                                getCurrentTimeUs() - mnBaseMediaTime;
                            nRendStatObj.nArrivalTime = pFrameInfo->nTSArrival;
                            nRendStatObj.nAudioTrackLatency = mnAudioLatency;
                            nRendStatObj.nAddedAudioTrackLatency =
                                mnAddAudioTrackLatency;
                            m_pStatInstance->UpdateDropsOrLateby(nRendStatObj);
                        }
                     }
                    if(WFD_LIKELY(m_pStatInstance))
                    {
                        m_pStatInstance->recordVideoFrameDrop(pFrameInfo->\
                            pBuffer,true);
                    }

                    WFDMMLOGE4(
                       "Renderer drop Videoframe TS = %llu, lateby = %lld flushing  %d %lld",
                       pFrameInfo->pBuffer->nTimeStamp,
                       (int64)timedecider, mbFlushInProgress,
                               mnFlushTimeStamp);

                    AccessPendingQ(&pFrameInfo, popfront);

                    if(pFrameInfo)
                    {
                        pFrameInfo->pBuffer->nFilledLen = 0;
                        mpFnEBD(mClientData, mnModuleId, SINK_VIDEO_TRACK_ID,
                            pFrameInfo->pBuffer);
                        accessFrameInfoMeta(&pFrameInfo, pushfront);
                    }
                    if(AccessPendingQ(NULL, size) == 0)
                    {
                        MM_Timer_Sleep(2);
                    }
                }
                else if((int64)timedecider <= (int64)VIDEO_RENDER_TOLERANCE ||
                  !mCfg.bAVSyncMode)
                {
                    if(mbFlushInProgress &&
                       ((uint64)pFrameInfo->pBuffer->nTimeStamp
                                                           >= mnFlushTimeStamp))
                    {
                        WFDMMLOGH("Renderer Flush complete");
                        mbFlushInProgress = false;
                    }
                    mnVideoDropCount = 0;
                    /*----------------------------------------------------------
                      Update the frame lateby statistics here
                    ------------------------------------------------------------
                    */

                    if(WFD_LIKELY(m_pStatInstance))
                    {
                        rendererStatsType nRendStatObj;

                        nRendStatObj.nIsVideoFrame = 1;
                        nRendStatObj.nIsLate = 0;
                        nRendStatObj.nTimeDecider = (int64)timedecider;
                        nRendStatObj.nSessionTime =
                            getCurrentTimeUs() - mnBaseMediaTime;
                        nRendStatObj.nArrivalTime = pFrameInfo->nTSArrival;
                        nRendStatObj.nAudioTrackLatency = mnAudioLatency;
                        nRendStatObj.nAddedAudioTrackLatency =
                            mnAddAudioTrackLatency;
                        m_pStatInstance->UpdateDropsOrLateby(nRendStatObj);
                    }

                    WFDMMLOGM1("PushVideoFrame to render. TS[%llu]",
                        pFrameInfo->pBuffer->nTimeStamp);
                    (void)AccessPendingQ(&pFrameInfo, popfront);

                    if(!PushVideo4Render(pFrameInfo))
                    {
                        WFDMMLOGE("Failed to render a video frame");
                    }
                    if(AccessPendingQ(NULL, size) == 0)
                    {
                        MM_Timer_Sleep(2);
                    }
                }
                else
                {
                    WFDMMLOGH("Push back video frame not time yet");
                    MM_Timer_Sleep(2);
                }
            }
            else
            {
                MM_Timer_Sleep(2);
            }
        }
        else
        {
            MM_Timer_Sleep(2);
        }
    }
    WFDMMLOGD("Renderer Video Thread Ended");
}

/*==============================================================================

         FUNCTION:         GraphicThreadEntry

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
void WFDMMSinkVideoRenderer::GraphicThreadEntry(void* pThis,
                                           unsigned int nSignal)
{
    CHECK_ARGS_RET_VOID_1(pThis);

    WFDMMSinkVideoRenderer *pMe = (WFDMMSinkVideoRenderer*)pThis;

    pMe->GraphicThread(nSignal);
}

/*==============================================================================

         FUNCTION:         GraphicThread

         DESCRIPTION:
*//**       @brief         Thread for getting frames back from native window
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
void WFDMMSinkVideoRenderer::GraphicThread(unsigned int nSignal)
{
    (void)nSignal;
    ANativeWindowBuffer *pBuf;
    frameInfoType       *pFrameInfo;

    WFDMMLOGD("graphic Thread Started");

    while((ISSTATEOPENED || ISSTATECLOSING) && !ISERROR)
    {
        int nRet = 0;

        pFrameInfo = NULL;

        if(mnNumVidQedBufs - mnNumVidDQedBufs > 2)
        {
            nRet = native_window_dequeue_buffer_and_wait(mpWindow.get(), &pBuf);
            if (nRet != 0)
            {
                WFDMMLOGH("failed to dequeue Buffer");
                MM_Timer_Sleep(2);
                continue;
            }
            else
            {
                AccessRenderQ(&pFrameInfo, (uint64)pBuf->handle, search);
                mnNumVidDQedBufs++;

                if (pFrameInfo)
                {
                    WFDMMLOGH("Found a match for dequeued buffer");

                    if(WFD_LIKELY(m_pStatInstance))
                    {
                        m_pStatInstance->recordDisplayOP(pFrameInfo->pBuffer);
                        m_pStatInstance->PrintStatistics();
                    }

                    pFrameInfo->pBuffer->nFilledLen = 0;

                    mpFnEBD(mClientData, mnModuleId, SINK_VIDEO_TRACK_ID,
                            pFrameInfo->pBuffer);
                    accessFrameInfoMeta(&pFrameInfo, pushrear);
                    pFrameInfo = NULL;

                }
                else
                {
                    WFDMMLOGE("Failed to find a buffer match after dequeue");
                }
            }
        }
        else
        {
            MM_Timer_Sleep(2);
        }
    }
    /*--------------------------------------------------------------------------
      If we encounter an error in dequeue we are not going to try again
      Lets flush all pending buffers in Render Q
    ----------------------------------------------------------------------------
    */
    if(ISERROR)
    {
        uint32 nSize = AccessRenderQ(NULL, 0, size);
        frameInfoType *pFrameInfo = NULL;

        for(uint32 i = 0; i < nSize; i++)
        {
            AccessRenderQ(&pFrameInfo, 0, popfront);
            if(pFrameInfo)
            {
                pFrameInfo->pBuffer->nFilledLen = 0;
                mpFnEBD(mClientData, mnModuleId, SINK_VIDEO_TRACK_ID,
                        pFrameInfo->pBuffer);
                accessFrameInfoMeta(&pFrameInfo, pushrear);
                pFrameInfo = NULL;
            }
        }
    }
    WFDMMLOGD("graphic Thread Ended");
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

int WFDMMSinkVideoRenderer::state(int state, bool get_true_set_false)
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
         FUNCTION:         AccessPendingQ
         DESCRIPTION:
*//**       @brief         Access pending Queue
*//**
@par     DEPENDENCIES:
                           None
*//*
         PARAMETERS:
*//**       @param         pPkt    - pointer to hold the packet
                           bAction - action to be performed on the Queue
*//*     RETURN VALUE:
*//**       @return
                           0 - success
                           -1 - failure
@par     SIDE EFFECTS:
                           None
*//*==========================================================================*/
int WFDMMSinkVideoRenderer::AccessPendingQ(frameInfoType **pPkt,
                                       actionType bAction)
{
    if(pPkt == NULL && bAction != size)
    {
        return -1;
    }

    if(!mCfg.bHasVideo)
    {
        if(pPkt)
        {
            *pPkt = NULL;
        }
        return 0;
    }

    CRITICAL_SECT_ENTER;
    QMM_ListErrorType eListError = QMM_LIST_ERROR_NONE;
    QMM_ListSizeType nQlen =0;

    eListError = qmm_ListSize(&mVideoPendingQ, &nQlen);

    if(bAction == size)
    {
        if(eListError == QMM_LIST_ERROR_NONE)
        {
            CRITICAL_SECT_LEAVE
            return nQlen;
        }
        else
        {
            CRITICAL_SECT_LEAVE
            return -1;
        }
    }

    if(bAction == pushrear || bAction == pushfront)
    {
        WFDMMLOGL("Push to pending");
        if(*pPkt)
        {

            if(bAction == pushrear)
            {
                eListError =
                     qmm_ListPushRear(&mVideoPendingQ, &((*pPkt)->pLink));
            }
            else
            {
                eListError =
                     qmm_ListPushFront(&mVideoPendingQ, &((*pPkt)->pLink));
            }

            if(eListError != QMM_LIST_ERROR_NONE)
            {
                WFDMMLOGE("Failed to enqueue");
            }
        }
    }
    else if(bAction == popfront || bAction == poprear || bAction == peekfront
            || bAction == peekrear)
    {
        WFDMMLOGL1("POP from pending %d", nQlen);

        if(!nQlen)
        {
            *pPkt = NULL;
            CRITICAL_SECT_LEAVE;
            return 0;
        }

        if(bAction == popfront)
        {
            eListError = qmm_ListPopFront(&mVideoPendingQ,
                                          (QMM_ListLinkType **)pPkt);
        }
        else if(bAction == peekfront)
        {
            eListError = qmm_ListPeekFront(&mVideoPendingQ,
                                          (QMM_ListLinkType **)pPkt);
        }
        else if(bAction == peekrear)
        {
            eListError = qmm_ListPeekRear(&mVideoPendingQ,
                                          (QMM_ListLinkType **)pPkt);
        }
        else
        {
            eListError = qmm_ListPopRear(&mVideoPendingQ,
                                          (QMM_ListLinkType **)pPkt);
        }

        if(eListError != QMM_LIST_ERROR_NONE ||  !(*pPkt))
        {
            WFDMMLOGE("Failed to Pop");
        }
    }
    CRITICAL_SECT_LEAVE;
    return 0;
}

/*==============================================================================
         FUNCTION:         AccessRenderQ
         DESCRIPTION:
*//**       @brief         Access buffers Queued for rendering
*//**
@par     DEPENDENCIES:
                           None
*//*
         PARAMETERS:
*//**       @param         pPkt    - pointer to hold the packet
                           bAction - action to be performed on the Queue
*//*     RETURN VALUE:
*//**       @return
                           0 - success
                           -1 - failure
@par     SIDE EFFECTS:
                           None
*//*==========================================================================*/
int WFDMMSinkVideoRenderer::AccessRenderQ(frameInfoType **pPkt, uint64 handle,
                                       actionType bAction)
{
    if(pPkt == NULL && bAction != size)
    {
        return -1;
    }

    if(!mCfg.bHasVideo)
    {
        if(pPkt)
        {
            *pPkt = NULL;
        }
        return 0;
    }

    CRITICAL_SECT_ENTER;
    QMM_ListErrorType eListError = QMM_LIST_ERROR_NONE;
    QMM_ListSizeType nQlen =0;

    eListError = qmm_ListSize(&mVideoRenderQ, &nQlen);

    if(bAction == size)
    {
        if(eListError == QMM_LIST_ERROR_NONE)
        {
            CRITICAL_SECT_LEAVE
            return nQlen;
        }
        else
        {
            CRITICAL_SECT_LEAVE
            return -1;
        }
    }

    if(bAction == pushrear || bAction == pushfront)
    {
        WFDMMLOGL("Push to FreeQ");
        if(*pPkt)
        {
            if(bAction == pushrear)
            {
                eListError =
                     qmm_ListPushRear(&mVideoRenderQ, &((*pPkt)->pLink));
            }
            else
            {
                eListError =
                      qmm_ListPushFront(&mVideoRenderQ, &((*pPkt)->pLink));
            }

        }
    }
    else if(bAction == popfront || bAction == poprear)
    {
        WFDMMLOGL1("POP from pending %d", nQlen);

        if(!nQlen)
        {
            *pPkt = NULL;
            CRITICAL_SECT_LEAVE;
            return 0;
        }

        if(bAction == popfront)
        {
            eListError = qmm_ListPopFront(&mVideoRenderQ,
                                          (QMM_ListLinkType **)pPkt);
        }
        else
        {
            eListError = qmm_ListPopRear(&mVideoRenderQ,
                                          (QMM_ListLinkType **)pPkt);
        }

    }
    else if(bAction == search)
    {
        SearchBuffer(&mVideoRenderQ, pPkt,handle);
    }
    CRITICAL_SECT_LEAVE;
    return 0;
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
uint64 WFDMMSinkVideoRenderer::getCurrentTimeUs()
{
    struct timespec sTime;
    clock_gettime(CLOCK_MONOTONIC, &sTime);

    uint64 currTS = ((OMX_U64)sTime.tv_sec * 1000000/*us*/)
                          + ((OMX_U64)sTime.tv_nsec / 1000);
    return currTS - mnBaseMediaTime;

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
OMX_ERRORTYPE WFDMMSinkVideoRenderer::Stop()
{
    CRITICAL_SECT_ENTER_FRAME;

    SETSTATECLOSING;

    uint32 nVidPendingSize = AccessPendingQ(NULL, size);
    uint32 nVidRenderSize = AccessRenderQ(NULL, (uint32)0, size);

    /*--------------------------------------------------------------------------
      Wait for all buffers to be returned
    ----------------------------------------------------------------------------
    */
    WFDMMLOGH("Waiting for Renderer buffers to be returned");

    unsigned long nStartMs = 0;
    unsigned long nCurrMs = 0;
    bool bIsStartTimeSet = false;

    #define DRAW_CYCLE_MS 17
    #define WAIT_MARGIN 3

    uint32 nWaitMs = (nVidRenderSize + nVidPendingSize + WAIT_MARGIN)
                                        * DRAW_CYCLE_MS;

    while(nVidPendingSize || nVidRenderSize > NUM_BUFS_HELD_IN_NATIVE_WINDOW)
    {
        MM_Timer_Sleep(2);
        nVidPendingSize = AccessPendingQ(NULL, size);
        nVidRenderSize = AccessRenderQ(NULL, (uint32)0, size);
        WFDMMLOGH2("Vid Pending = %d, Vid Render = %d",
                   nVidPendingSize, nVidRenderSize);
        if(nVidPendingSize== 0)
        {
            if(!bIsStartTimeSet)
            {
                MM_Time_GetTime(&nStartMs);
                bIsStartTimeSet = true;
            }
            MM_Time_GetTime(&nCurrMs);
            if((unsigned)(nCurrMs - nStartMs) > nWaitMs)
            {
                break;
            }
        }
    }

    frameInfoType *pPkt;

    /*--------------------------------------------------------------------------
     Remove all buffers that are stuck in Render Q
    ----------------------------------------------------------------------------
    */
    for(uint32 i = 0; i < nVidRenderSize; i++)
    {
        WFDMMLOGH("Free Buffer stuck in Render Q");
        AccessRenderQ(&pPkt, 0, popfront);
        if(pPkt)
        {
            if((nVidRenderSize - i) > 0)
            {
                pPkt->pBuffer->nFilledLen = 0;
                mpFnEBD(mClientData, mnModuleId, SINK_VIDEO_TRACK_ID,
                   pPkt->pBuffer);
            }
            accessFrameInfoMeta(&pPkt, pushrear);
        }
    }

    mnNumVidDQedBufs = mnNumVidQedBufs; /* Reset this, required for handling port reconfiguration */

    WFDMMLOGH("Done waiting for all Renderer Buffers to be returned");

    /*--------------------------------------------------------------------------
     Now we have returned all buffers. We can move to CLOSED.
    ----------------------------------------------------------------------------
    */
    SETSTATECLOSED;

    CRITICAL_SECT_LEAVE_FRAME;

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
OMX_ERRORTYPE WFDMMSinkVideoRenderer::Start()
{
    if(!(ISSTATEINIT || ISSTATECLOSED))
    {
        WFDMMLOGE("Renderer not in Init when started");
        return OMX_ErrorInvalidState;
    }
    WFDMMLOGH("WFDMMSinkRenderer Start");

    mbFirstFrame = true;
    mbLookForIDR = true;

    SETSTATEOPENED;

    if(mpVideoThread)
    {
        mpVideoThread->SetSignal(0);
    }
    if(mpGraphicPollThread)
    {
        mpGraphicPollThread->SetSignal(0);
    }

    WFDMMLOGH("WFDMMSinkRenderer Start complete");
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
void WFDMMSinkVideoRenderer::SetMediaBaseTime(uint64 nTime)
{
    WFDMMLOGH("Renderer Setting Media Base Time");
    /*-------------------------------------------------------------------------
      Renderer shall not receive any new frames when Media Base Time is being
      set. Renderer will flush out its existing data and only then will honor
      the new base time
    ---------------------------------------------------------------------------
    */
    mbVideoTimeReset = true;
    mnFlushTimeStamp = (uint64)-1;
    mbFlushInProgress = true;

    unsigned long nStartMs = 0;
    unsigned long nCurrMs = 0;
    MM_Time_GetTime(&nStartMs);

    while(AccessPendingQ(NULL, size))
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

         FUNCTION:          anyChangeInCrop

         DESCRIPTION:
*//**       @brief          Looks if any picture crop settings changed. Usually
                            happens when video dimension changes
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
bool WFDMMSinkVideoRenderer::anyChangeInCrop(pictureInfoType *pPictInfo)
{
    if(pPictInfo)
    {
        if(mPictInfo.rect.nHeight != pPictInfo->rect.nHeight ||
           mPictInfo.rect.nLeft   != pPictInfo->rect.nLeft   ||
           mPictInfo.rect.nTop    != pPictInfo->rect.nTop    ||
           mPictInfo.rect.nWidth  != pPictInfo->rect.nWidth  ||
           mPictInfo.nHeight      != pPictInfo->nHeight      ||
           mPictInfo.nWidth       != pPictInfo->nWidth)
        {
            WFDMMLOGH("Crop Info Changed");
            return true;
        }
    }
    return false;
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
void WFDMMSinkVideoRenderer::pauseRendering(void)
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
void WFDMMSinkVideoRenderer::restartRendering(bool bFlush)
{
    mbPaused = false;

    if(bFlush)
    {
        mnFlushTimeStamp = (uint64)-1;
        mbFlushInProgress = true;

        unsigned long nStartMs = 0;
        unsigned long nCurrMs = 0;
        MM_Time_GetTime(&nStartMs);

        while(AccessPendingQ(NULL, size))
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
void WFDMMSinkVideoRenderer::setFlushTimeStamp(uint64 nTS)
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
void WFDMMSinkVideoRenderer::setDecoderLatency(uint64 nLatencyInms)
{
    WFDMMLOGH("Renderer Setting decode latency");
    CRITICAL_SECT_ENTER;
    mCfg.nDecoderLatency = nLatencyInms;
    CRITICAL_SECT_LEAVE;
}

/*==============================================================================

         FUNCTION:          AddAudioTrackLatency

         DESCRIPTION:
*//**       @brief          add the audiotrack latency to video thhread
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
void WFDMMSinkVideoRenderer::AddAudioTrackLatency()
{
    uint32 mnAudioLatencyNew = 0;
    mnAudioLatencyCheckCount++;
    if(mCfg.bAudioTrackLatencyMode == 0)
    {
        if(mnAudioLatency > DEFAULT_AUDIO_TRACK_LATENCY)
        {
            /*------------------------------------------------------------------
              case when BT is connected and wfd started.
              ------------------------------------------------------------------
            */
             mnAddAudioTrackLatency = mnAudioLatency;
        }
        if(mnAudioLatencyCheckCount == MAX_CONSEQ_VIDEO_FRAMES_COUNT)
        {
             if(mClientData)
             {
                WFDMMLOGM("VideoRenderer:: Fetching Audio Track Latency");
                mnAudioLatencyNew = 1000 * mpAudioLatFnHandler(mClientData);
                    //((WFDMMSink *)mClientData)->getAudioTrackLatency();
             }
             else
             {
                WFDMMLOGE("VideoRenderer:: Unable to get Renderer instance!");
             }

             if(mnAudioLatencyNew > DEFAULT_AUDIO_TRACK_LATENCY)
             {
                 /*-------------------------------------------------------------
                   BT is connected after the wfd started
                   -------------------------------------------------------------
                 */
                 mnAddAudioTrackLatency = mnAudioLatencyNew;
             }
             else
             {
                 /*-------------------------------------------------------------
                   Don't consider the audio track latency
                   -------------------------------------------------------------
                 */
                 mnAddAudioTrackLatency = 0;
             }
             mnAudioLatencyCheckCount = 0;
             mnAudioLatency = mnAudioLatencyNew;
        }
    }
    else if(mCfg.bAudioTrackLatencyMode == 1)
    {
        mnAddAudioTrackLatency = mnAudioLatency;
        /*----------------------------------------------------------------------
          If flag is set we add audio track latency to renderer and in case if
          BT is  connected ,we add the new latency to renderer to improve the AV
          sync
         -----------------------------------------------------------------------
        */
        if(mnAudioLatencyCheckCount == MAX_CONSEQ_VIDEO_FRAMES_COUNT)
        {
            if(mClientData)
            {
                WFDMMLOGM("VideoRenderer:: Fetching Audio Track Latency");
                mnAudioLatencyNew = 1000 * mpAudioLatFnHandler(mClientData);
                    //((WFDMMSink *)mClientData)->getAudioTrackLatency();
            }
            else
            {
                WFDMMLOGE("VideoRenderer:: Unable to get Renderer instance!");
            }

            if(mnAudioLatencyNew > DEFAULT_AUDIO_TRACK_LATENCY)
            {
                mnAddAudioTrackLatency = mnAudioLatencyNew;
            }
            else
            {
                mnAddAudioTrackLatency = mnAudioLatency;
            }
            mnAudioLatencyCheckCount = 0;
            mnAudioLatency = mnAudioLatencyNew;
       }
    }
}

