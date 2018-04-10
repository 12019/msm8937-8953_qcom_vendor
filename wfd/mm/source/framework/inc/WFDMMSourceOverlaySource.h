#ifndef _WFD_MM_OVERLAY_SOURCE_H_
#define _WFD_MM_OVERLAY_SOURCE_H_

/*==============================================================================
*       WFDMMSourceOverlaySource.h
*
*  DESCRIPTION:
*       Class declaration WFDMMSourceOverlaySource
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

#include "OMX_Core.h"
#include "WFDMMSource.h"
#include <media/stagefright/MediaBuffer.h>
#include "WFDMMSourceImageEncode.h"

#define OVERLAY_CAPTURE_NUM_BUFF        1
#define OVERLAY_SINK_INPUT_Q_SIZE       3
#define OVERLAY_PVT_HEADER_SIZE         18

//Forward Declaration
namespace android {
    class WFDSurfaceMediaSource;
    class IGraphicBufferProducer;
    class IBinder;
}

/**
* @brief Frame callback for PNG buffer deliver
*/
typedef void (*FrameDeliveryFnType) (OMX_BUFFERHEADERTYPE* pBuffer, void *);
#define OVERLAY_STATISTICS

typedef struct overlayFrameInfo
{
    uint8  *pBuffer;
    bool    isRead;
    bool    isValid;
    OMX_U64 nCaptureTimeStamp;
}overlayFrameInfoType;

typedef struct activeIDInfo
{
    int     nListHead;
    uint8   *pActiveIDList;
    int     nInActiveListHead;
    uint8   *pInActiveIDList;
}activeIDInfoType;

typedef struct pvtHeaderInfo
{
    uint8   overlayID;
    bool    bActivePresentation;
    int     topLeftX;
    int     topLeftY;
    int     height;
    int     width;
}pvtHeaderInfoType;

class WFDMMThreads;

class WFDMMSourceOverlaySource
{
public:

    /**
    * @brief Constructor
    */
    WFDMMSourceOverlaySource();

    /**
    * @brief Destructor
    */
    ~WFDMMSourceOverlaySource();

    OMX_ERRORTYPE configure(FrameDeliveryFnType pFrameDeliveryFn,
                            eventHandlerType pEventHandlerFn,
                            OverlaySrcConfigType *pConfig,
                            OMX_U32 nModuleID,
                            OMX_PTR pAppData);

    OMX_ERRORTYPE startCapture();

    OMX_ERRORTYPE stopCapture();

    OMX_ERRORTYPE pauseCapture();

    OMX_ERRORTYPE resumeCapture();

    OMX_ERRORTYPE SetFreeBuffer(OMX_BUFFERHEADERTYPE *pBuffer,
                                OMX_BOOL              bForce = OMX_FALSE);

    OMX_ERRORTYPE GetOutNumBuffers(OMX_U32 nPortIndex,
                                   OMX_U32 *nBuffers);

    OMX_BUFFERHEADERTYPE** GetBuffers();

private:

    WFDMMThreads                                    *m_hOverlayCaptureThread;
    WFDMMThreads                                    *m_hInputDeliveryThread;
    MM_HANDLE                                       m_hStateCritSect;
    MM_HANDLE                                       m_hQCritSect;

    OMX_U32                                         m_nModuleId;
    OMX_PTR                                         m_pAppData;
    eventHandlerType                                m_pEventHandlerFn;
    FrameDeliveryFnType                             m_pFrameDeliveryFn;
    OverlaySrcConfigType                            m_sConfig;

    android::sp<android::WFDSurfaceMediaSource>     m_pSurfaceMediaSrc;
    android::sp<android::IGraphicBufferProducer>    m_pBuffProducer;
    android::sp<android::IBinder>                   m_dpy;

    OMX_BOOL                                        m_bPaused;
    OMX_S32                                         m_nState;

    android::sp<android::IBinder>                   m_mainDpy;

    bool                                            m_bCaptureOverlay;
    uint32_t                                        m_nCaptureHeight;
    uint32_t                                        m_nCaptureWidth;

    SignalQueue                                     *m_pMuxInputQ;
    OMX_S32                                         m_nMuxNumInputBufs;
    OMX_BUFFERHEADERTYPE**                          m_pInputBuffers;
    int                                             m_nPNGBufferSize;

    overlayFrameInfoType                            m_sLatestFrameQ[OVERLAY_CAPTURE_NUM_BUFF];
    OMX_U64                                         m_nReadIdx;
    OMX_U64                                         m_nWriteIdx;
    android::MediaBuffer                            *m_pLastXmitBuffer;

    WFDMMSourceImageEncode                          *m_pImgEncoder;
    wfdImgConfigParamsType                          m_sWfdImgConfig;

    uint8                                           m_nOverlayID;
    activeIDInfoType                                m_sActiveIDInfo;
    int                                             m_nMaxOverlayIDSupport;

    OMX_TICKS                                       m_nLastTimeStamp;
    OMX_TICKS                                       m_nEncodeTime;
    OMX_U64                                         m_nSendBuffTS;
    bool                                            m_bFirstFrameSent;
    bool                                            m_bPrevTransparent;

#ifdef OVERLAY_STATISTICS
    OMX_TICKS                                       m_nAvgEncodeTime;
    int                                             m_nStatCounter;
#endif

    OMX_ERRORTYPE initData();
    OMX_ERRORTYPE createResources();

    void releaseResources();
    void releaseMediaBuffers();

    OMX_ERRORTYPE createSurface(int width, int height);
    OMX_ERRORTYPE createVirtualDisplay();
    OMX_ERRORTYPE configureVirtualDisplay();
    bool isLandscapeOrientation();

    void releaseVDS();

    static void captureThreadEntry(void* pThreadData, unsigned int signal);
    OMX_ERRORTYPE captureThread(int nSignal);

    static void ipDeliveryThreadEntry(void* pThreadData, unsigned int signal);
    OMX_ERRORTYPE ipDeliveryThread(int nSignal);

    OMX_ERRORTYPE pngEncodeNSend(overlayFrameInfoType* pBufferInfo);
    OMX_ERRORTYPE pngEncodeNSendHelper(imageRegionInfoNodeType *pRegion, OMX_U64 nTimeStamp);
    void* extractPvtHndlFrmMediaBfr(void* pMediaBuffer);
    bool attachOverlayPvtHeader(OMX_U8* pBuffer, pvtHeaderInfoType *pvtHdrInfo);

    int state(int state, bool get_true_set_false);

    void inputDeliveryHandler();

    int AccessLatestBufQ(bool bRead, overlayFrameInfoType* pBufferInfo);

    OMX_ERRORTYPE allocateBuffersForMuxQ();
    OMX_ERRORTYPE deallocateBuffersForMuxQ();

    bool updateActiveIDList(uint8 nOverlayID);
    bool removePrevOverlays();
    void transferPrevOverlays();
};
#endif //_WFD_MM_OVERLAY_SOURCE_H_
