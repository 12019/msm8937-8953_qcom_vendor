#ifndef __WFD_MM_SINK_VIDEO_RENDERER_H__
#define __WFD_MM_SINK_VIDEO_RENDERER_H__
/*==============================================================================
*       WFDMMSinkVideoRenderer.h
*
*  DESCRIPTION:
*       Class declaration for WFDMM Sink Video Renderer.Renders video samples
*
*  Copyright (c) 2015-2016 Qualcomm Technologies, Inc.
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
#ifdef WFD_ICS
#include "surfaceflinger/Surface.h"
#else
#include <Surface.h>
#include <android/native_window.h>
#endif

#include "WFDMMSinkCommon.h"
#include "OMX_Core.h"
#include "MMCriticalSection.h"
#include "binder/ProcessState.h"
#include "wdsm_mm_interface.h"
#include "WFDMMSinkRenderer.h"

class WFDMMThreads;
class SignalQueue;
class WFDMMSinkStatistics;

class WFDMMSinkVideoRenderer : public WFDMMSinkRenderer
{
public:

     WFDMMSinkVideoRenderer(int moduleId,
                            WFDMMSinkEBDType       pFnEBD,
                            WFDMMSinkHandlerFnType pFnHandler,
                            WFDMMSinkAudioLatFnType pAudioLatFnHandler,
                            void* clientData,
                            WFDMMSinkStatistics* pStatIns);

     ~WFDMMSinkVideoRenderer();

    OMX_ERRORTYPE  Configure(rendererConfigType *pCfg);

    OMX_ERRORTYPE  DeliverInput(int trackId,
                                OMX_BUFFERHEADERTYPE *pBuffer);

    OMX_ERRORTYPE  Stop();

    OMX_ERRORTYPE  Start();

    void SetMediaBaseTime(uint64 nTime);

    void pauseRendering(void);

    void restartRendering(bool flush);

    void setFlushTimeStamp(uint64 nTS);

    void setDecoderLatency(uint64 nLatencyInms);

private:

    OMX_ERRORTYPE  createResources();

    OMX_ERRORTYPE  AllocateBuffers();

    bool  createThreadsAndQueues();

    bool  Scheduler(int trackId, frameInfoType *pFrameInfo);

    bool  PushVideo4Render(frameInfoType *pFrameInfo);

    bool  RenderVideo(frameInfoType *pFrameInfo);

    bool  SearchBuffer(QMM_ListHandleType *pH,
                       frameInfoType **pFrameInfo,
                       long bufferId);

    static QMM_ListErrorType  ComparePayloadParam
    (
       void *pElement,
       void *pCmpValue
    );

    OMX_ERRORTYPE  deinitialize();

    static void  VideoThreadEntry(void* pThis,
                                  unsigned int nSignal);

    void  VideoThread(unsigned int nSignal);

    static void  GraphicThreadEntry(void* pThis,
                                    unsigned int nSignal);

    void  GraphicThread(unsigned int nSignal);

    int  state(int state, bool get_true_set_false);

    int  AccessPendingQ(frameInfoType **pPkt,
                        actionType bAction);

    int  AccessRenderQ(frameInfoType **pPkt, uint64 handle,
                                           actionType bAction);

    uint64  getCurrentTimeUs();

    void    InitData();

    bool    anyChangeInCrop(pictureInfoType *);//TBD

    bool    accessFrameInfoMeta(frameInfoType **pFrameInfo,
                             actionType action);

    void AddAudioTrackLatency();

    uint64             mnBaseMediaTime;
    frameInfoType      msFrameInfo[RENDERERMAXQSIZES];
    pictureInfoType    mPictInfo;
    SignalQueue       *mFrameInfoFreeQ;
    QMM_ListHandleType mVideoRenderQ;
    QMM_ListHandleType mVideoPendingQ;
    WFDMMThreads      *mpVideoThread;
    WFDMMThreads      *mpGraphicPollThread;
    MM_HANDLE          mhCritSect;
    MM_HANDLE          mhCritSectFrame;
    android::sp<ANativeWindow>  mpWindow;
    rendererConfigType mCfg;
    int                meState;
    void*              mClientData;
    int                mnModuleId;
    int                mnErr;
    uint64             mnNumVidQedBufs;
    uint64             mnNumVidDQedBufs;
    uint64             mnFlushTimeStamp;
    uint32             mnVideoDropFrames;
    uint32             mnVideoMaxConsecDrop;
    uint32             mnVideoDropCount;
    uint32             mnAudioLatencyCheckCount;
    uint32             mnAddAudioTrackLatency;
    uint32             mnAudioLatency;
    WFDMMSinkEBDType       mpFnEBD;
    WFDMMSinkHandlerFnType   mpFnHandler;
    WFDMMSinkAudioLatFnType  mpAudioLatFnHandler;
    bool               mbFirstFrame;
    bool               mbMediaTimeSet;
    bool               mbLookForIDR;
    bool               mbPaused;
    bool               mbFlushInProgress;
    bool               mbVideoTimeReset;
    WFDMMSinkStatistics *m_pStatInstance;
};

#endif /*__WFD_MM_SINK_VIDEO_RENDERER_H__*/

