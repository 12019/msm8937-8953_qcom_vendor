#ifndef __WFD_MM_SINK_AUDIO_RENDERER_H__
#define __WFD_MM_SINK_AUDIO_RENDERER_H__
/*==============================================================================
*       WFDMMSinkAudioRenderer.h
*
*  DESCRIPTION:
*       Class declaration for WFDMM Sink Audio Renderer. Renders audio samples
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

#include "WFDMMSinkCommon.h"
#include "OMX_Core.h"
#include "MMCriticalSection.h"
#include "binder/ProcessState.h"
#include "wdsm_mm_interface.h"
#include "WFDMMSinkRenderer.h"
#include "qmmList.h"

class  WFDMMThreads;
class  SignalQueue;
namespace android {
    class  AudioTrack;
}
#ifndef USE_OMX_AAC_CODEC
class WFDMMSinkAudioDecode;
#endif

class WFDMMSinkStatistics;

class WFDMMSinkAudioRenderer
{
public:

     WFDMMSinkAudioRenderer(int moduleId,
                            WFDMMSinkEBDType       pFnEBD,
                            WFDMMSinkHandlerFnType pFnHandler,
                            void* clientData,
                            WFDMMSinkStatistics*);
     ~WFDMMSinkAudioRenderer();


    OMX_ERRORTYPE  Configure(rendererConfigType *pCfg);
    OMX_ERRORTYPE  DeliverInput(int trackId,
                                OMX_BUFFERHEADERTYPE *pBuffer);

    OMX_ERRORTYPE  Stop();
    OMX_ERRORTYPE  Start();

    void SetMediaBaseTime(uint64 nTime);
    void setFlushTimeStamp(uint64 nTS);
    void setDecoderLatency(uint64 nLatencyInms);

    void pauseRendering(void);
    void restartRendering(bool flush);


    uint32 getLatency();

private:

    OMX_ERRORTYPE  createResources();
    OMX_ERRORTYPE  AllocateBuffers();
    bool  createAudioResources();
    bool  createThreadsAndQueues();

    bool  Scheduler(int trackId, frameInfoType *pFrameInfo);

    bool  PushAudio4Render(frameInfoType *pFrameInfo);

    OMX_ERRORTYPE  deinitialize();


    static void  AudioThreadEntry(void* pThis,
                                  unsigned int nSignal);
    void  AudioThread(unsigned int nSignal);

    int  state(int state, bool get_true_set_false);

    uint64  getCurrentTimeUs();

    void    InitData();

    bool    accessFrameInfoMeta(frameInfoType **pFrameInfo,
                             actionType action);

    bool    GenerateAACHeaderFromADTS(uint8 *pAdts,      uint32 nAdtsLen,
                                   uint8 *pAacHeader, uint32 *nAACHdrLen);

    static void  AudioTrackCb(int  nEvent,
                                      void *pCookie,
                                      void *pInfo);
    void prepareDumpFiles();

    bool createAudioTrack();

    void ProcessAudioTrackCb(int nEvent, void* pInfo);

    uint64             mnBaseMediaTime;
    frameInfoType msFrameInfo[RENDERERMAXQSIZES];
    SignalQueue       *mFrameInfoFreeQ;
    SignalQueue       *mAudioQ;
    WFDMMThreads      *mpAudioThread;
    MM_HANDLE          mhCritSect;
#ifndef USE_OMX_AAC_CODEC
    WFDMMSinkAudioDecode *mpAudioDecoder;
#endif
    android::sp<android::AudioTrack>  mpAudioTrack;
    rendererConfigType mCfg;
    int                meState;
    void*              mClientData;
    int                mnModuleId;
    int                mnErr;
    int                mnAudioSessId;
    uint64             mnFlushTimeStamp;
    uint32             mnAudioDropBytes;
    FILE              *mpFpAAC;
    FILE              *mpFpWAV;
    uint32             mnAudioLatency;
    WFDMMSinkEBDType       mpFnEBD;
    WFDMMSinkHandlerFnType   mpFnHandler;
    bool               mbMediaTimeSet;
    bool               mbAudioCodecHdrSet;
    bool               mbDumpWav;
    bool               mbDumpAAC;
    bool               mbPaused;
    bool               mbFlushInProgress;
    bool               mbRestartAudioTrack;
    int32              mnAudioOffset;
    bool               mbAudioTimeReset;
    uint64             mnFirstAudioFrameTimeStamp;
    WFDMMSinkStatistics* m_pStatInstance;
};

#endif /*__WFD_MM_SINK_AUDIO_RENDERER_H__*/

