#ifndef __WFD_MM_SINK_GENERIC_RENDERER_H__
#define __WFD_MM_SINK_GENERIC_RENDERER_H__
/*==============================================================================
*                   WFDMMSinkGenericRenderer.h
*
*  DESCRIPTION:
*       Class declaration for WFDMM Sink Generic Renderer. Renders general samples
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

/*#include "OMX_Core.h"
#include "binder/ProcessState.h"
#include "wdsm_mm_interface.h"
#include "qmmList.h"*/

#include "WFDMMSinkCommon.h"
#include "WFDMMSinkRenderer.h"
#include "MMCriticalSection.h"
#include "MMFile.h"

#include "SkBitmap.h"
#include <Surface.h>
#include <android/native_window.h>
#include "SurfaceComposerClient.h"


using namespace android;

class  WFDMMThreads;
class  SignalQueue;

typedef struct {
        bool isActive;
        bool isData;
        int Id;
        int blendMode;
        int zLayer;
        int colorFormat;
        int presentMode;
        uint32 x;
        uint32 y;
        uint32 h;
        uint32 w;
        SkBitmap *pGenericBM;
        int nIndex;
        uint8* bits;
} overlayInfo;

typedef struct {
    bool isFree;
    int Id;
    android::sp<android::SurfaceControl> surfaceControl;
    android::sp<android::Surface> surface;
}overlayDispInfo;

typedef struct {
    bool isValid;
    int Id;
    uint64 nTime;
}overlayClearInfo;


#define MAX_OVERLAY_ID 10
#define MAX_OVERLAY_DISP 15

#define Z_ORDER_INIT 100000


class WFDMMSinkGenericRenderer
{
public:

     WFDMMSinkGenericRenderer(int moduleId,
                            WFDMMSinkEBDType       pFnEBD,
                            WFDMMSinkHandlerFnType pFnHandler,
                            void* clientData);
     ~WFDMMSinkGenericRenderer();


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

    android::sp<android::SurfaceComposerClient> mSurfaceComposerClient;

private:

    OMX_ERRORTYPE  createResources();
    OMX_ERRORTYPE  AllocateBuffers();
    bool  createGenericResources();
    bool  createThreadsAndQueues();

    bool  Scheduler(int trackId, frameInfoType *pFrameInfo);

    bool  PushGeneric4Render(frameInfoType *pFrameInfo);

    bool  RenderGeneric(int index);

    int  ClearGeneric(int id);

    bool  CreateSurface(overlayInfo *generic);

    OMX_ERRORTYPE  deinitialize();


    static void  GenericDecodeThreadEntry(void* pThis,
                                  unsigned int nSignal);
    static void  GenericRenderThreadEntry(void* pThis,
                                  unsigned int nSignal);
    void  GenericDecodeThread(unsigned int nSignal);
    void  GenericRenderThread(unsigned int nSignal);
    void  CheckAndClearGeneric();

    int  state(int state, bool get_true_set_false);

    uint64  getCurrentTimeUs();

    void    InitData();

    bool    accessFrameInfoMeta(frameInfoType **pFrameInfo,
                             actionType action);
    bool    accessFrameInfoData(frameInfoType **pFrameInfo,
                             actionType action);
    bool    accessFrameInfoClear(frameInfoType **pFrameInfo,
                             actionType action);

    static void  GenericTrackCb(int  nEvent,
                              void *pCookie,
                              void *pInfo);

    void prepareDumpFiles();

    bool createGenericTrack();

    void ProcessGenericTrackCb(int nEvent, void* pInfo);

    bool decodeGeneric(OMX_BUFFERHEADERTYPE *pBuf,frameInfoType *pFrameInfo);
    void decodeGenericHeader(OMX_BUFFERHEADERTYPE *pBuf,overlayInfo *ovInfo);

    uint64                  mnBaseMediaTime;

    frameInfoType           msFrameInfo[2*MAX_OVERLAY_ID];    //mFrameInfoFreeQ
    frameInfoType           msFrameInfoData[MAX_OVERLAY_ID];  //mFrameInfoDataFreeQ
    frameInfoType           msFrameInfoClear[3*MAX_OVERLAY_ID]; //mGenericDataClearQ

    /* GenericDecodeThread looks on data from this Queue
       to operate on. After data is brought from MediaSource,
       it is queued in this queue.*/
    SignalQueue             *mGenericQ;
    /* This is used to hold actual data from media source*/
    SignalQueue             *mFrameInfoFreeQ;
    /* GenericRenderThread will operate on it.This holds buffer
       which is carrying decoded frame to be presented on screen.*/
    SignalQueue             *mGenericPresentQ;
    /* This holds decoded png data which will be sent to
       screen for presentation.*/
    SignalQueue             *mFrameInfoDataFreeQ;
    /* This holds buffer which are required to be removed from screen.
       While presenting over screen we need to first clear these and then
       present.*/
    SignalQueue             *mGenericClearQ;
    /* This will hold information about png data which needs
       to be removed from screen.*/
    SignalQueue             *mFrameInfoDataClearQ;

    WFDMMThreads            *mpGenericDecodeThread;
    WFDMMThreads            *mpGenericRenderThread;

    overlayInfo             mOvInfo[MAX_OVERLAY_ID]; // Used for msFrameInfoData
    overlayDispInfo         mDispOvInfo[MAX_OVERLAY_DISP];
    overlayClearInfo        mDispOvClearInfo[3*MAX_OVERLAY_ID];// Used for msFrameInfoClear

    MM_HANDLE               mhCritSect;
    rendererConfigType      mCfg;
    int                     meState;
    void*                   mClientData;
    int                     mnModuleId;
    int                     mnErr;
    int                     mnGenericSessId;
    uint64                  mnFlushTimeStamp;
    //FILE                    *mpFpGeneric;
    MM_HANDLE               mpFpGeneric;
    WFDMMSinkEBDType        mpFnEBD;
    WFDMMSinkHandlerFnType  mpFnHandler;
    bool                    mbMediaTimeSet;
    bool                    mbDumpGeneric;
    bool                    mbPaused;
    bool                    mbFlushInProgress;
    bool                    mbGenericTimeReset;
    int                     countPNG;
    float                   mfXPosScaleFactor;
    float                   mfYPosScaleFactor;
    int                     mnDeltaXValue;
    int                     mnDeltaYValue;

    sp<Surface>             m_pGenericSurface;
    sp<ANativeWindow>       m_pGenericWindow;
};

#endif /*__WFD_MM_SINK_GENERIC_RENDERER_H__*/

