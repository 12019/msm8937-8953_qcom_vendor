#ifndef __WFD_MM_SINK_RENDERER_H__
#define __WFD_MM_SINK_RENDERER_H__
/*==============================================================================
*       WFDMMSinkRenderer.h
*
*  DESCRIPTION:
*       Class declaration for WFDMM Sink MediaSource. Connects RTP decoder and
*       parser and provides Audio and Video samples to
*       the media framework.
*
*
*  Copyright (c) 2013-2015 Qualcomm Technologies, Inc. All Rights Reserved.
*  Qualcomm Technologies Proprietary and Confidential.
*===============================================================================
*/
/*==============================================================================
                             Edit History
================================================================================
   When            Who           Why
-----------------  ------------  -----------------------------------------------
03/28/2013         SK            InitialDraft
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
#ifdef WFD_ICS
#include "surfaceflinger/Surface.h"
#else
#include <Surface.h>
#include <android/native_window.h>
#endif
//#include "media/mediaplayer.h"
//#include "AEEStd.h"
#include "WFDMMSinkCommon.h"
#include "OMX_Core.h"
#include "MMCriticalSection.h"
#include "binder/ProcessState.h"
#include "wdsm_mm_interface.h"

#include "qmmList.h"

//using namespace android;
#define RENDERERMAXQSIZES 200
#define AUDIO_AV_SYNC_DROP_WINDOW   -200000
#define VIDEO_AV_SYNC_DROP_WINDOW   -200000
#define GENERIC_AV_SYNC_DROP_WINDOW -200000

/*Common structures needed for all renderers*/
typedef struct rendererConfig
{
    bool   bHasVideo;
    bool   bHasAudio;
    bool   bHasGeneric;
    bool   bAVSyncMode;
    bool   bAudioTrackLatencyMode;

    struct audioInfo
    {
        WFD_audio_type eAudioFmt;
        uint32 nSampleRate;
        uint32 nChannels;
        uint32 nFrameLength;
    }sAudioInfo;

    struct videoInfo
    {
        uint32 nFrameWidth;
        uint32 nFrameHeight;
        uint32 nFrameRate;
        int32  nFrameDropMode;
    }sVideoInfo;

    struct genericInfo
    {
        uint32 surfaceHeight;
        uint32 surfaceWidth;
        uint32 presentationHeight;
        uint32 presentationWidth;
        uint32 sessionHeight;
        uint32 sessionWidth;
    }sGenericInfo;

    uint64          nDecoderLatency;
    int32 naudioAVSyncDropWindow;
    int32 nvideoAVSyncDropWindow;
    ANativeWindow  *pWindow;
}rendererConfigType;

typedef struct frameInfo
{
    QMM_ListLinkType      pLink;
    OMX_BUFFERHEADERTYPE *pBuffer;  //Pointer of the buffer
    uint64                nTSSched;  //Time at which the frame to be scheduled
    uint64                nTSArrival; // Arrival time of the buffer
}frameInfoType;


class WFDMMSinkRenderer
{
public:

     WFDMMSinkRenderer(){;};
     virtual ~WFDMMSinkRenderer(){;};


    virtual OMX_ERRORTYPE  Configure(rendererConfigType *pCfg) = 0;


    virtual OMX_ERRORTYPE  DeliverInput(int trackId,
                                OMX_BUFFERHEADERTYPE *pBuffer) = 0;


    virtual OMX_ERRORTYPE  Stop() = 0;
    virtual OMX_ERRORTYPE  Start() = 0;


    virtual void SetMediaBaseTime(uint64 nTime) = 0;


    virtual void pauseRendering() = 0;
    virtual void restartRendering(bool flush) = 0;


    virtual void setFlushTimeStamp(uint64 nTS) = 0;
    virtual void setDecoderLatency(uint64 nLatencyInms) = 0;
};

#endif /*__WFD_MM_SINK_RENDERER_H__*/
