/* =======================================================================
                              WFDMMSourceVideoSource.h
DESCRIPTION

Header file for WFDMMSourceVideoSource.cpp file

Copyright (c) 2013-2016 Qualcomm Technologies, Inc.
All Rights Reserved.
Confidential and Proprietary - Qualcomm Technologies, Inc.
========================================================================== */

#ifndef __WFD_MM_VIDEO_SOURCE_H__
#define __WFD_MM_VIDEO_SOURCE_H__

/*========================================================================
 *                    INCLUDE FILES FOR MODULE
 *========================================================================*/
#include "MMThread.h"
#include "MMSignal.h"
#include <sys/mman.h>
#include <linux/videodev2.h>
#include "WFDMMSourceComDef.h"
#include "AEEStdDef.h"
#include "WFDMMIonMemory.h"
#include "MMCriticalSection.h"
#include "WFDMMSourceVideoEncode.h"
#include "WFDMMSourceVideoCapture.h"
#include <gui/IGraphicBufferProducer.h>
#include "WFDMMSourceStatistics.h"
#include "WFDMMService.h"

#define SCREEN_SOURCE_BUFFERS 5
#define ARRAY_SZ(a) (sizeof(a) / sizeof((a)[0]))
#define WFD_H264_MAX_QP 44
#define WFD_H264_MIN_QP 10
#define DEFAULT_I_FRAME_INTERVAL 4
#define DEFAULT_BITRATE 10 * 1000 * 1000
#define DEFAULT_BITRATE_PEAK 2 * DEFAULT_BITRATE
#define FILLER_ION_BUF_SIZE 4096
#define FILLER_NALU_SIZE    8
#define NUMBER_OF_BFRAME 0
#define MAX_STR_BUFFER_ASSUME 30
#define STREAMING_TEMP 1
#define QUART_MEGA_BYTES 256*1024

//#define ENABLE_STREAMING_DUMP

    /*--------------------------------------------------------------------------
            Signal definitions
    ----------------------------------------------------------------------------
    */
typedef enum {
    VIDEO_SOURCE_STREAMING_REQ,
    VIDEO_SOURCE_START,
    VIDEO_SOURCE_PAUSE,
    VIDEO_SOURCE_STREAMING_START,
    VIDEO_SOURCE_SET_START_STREAMING_SUCCESS,
    VIDEO_SOURCE_STREAMING_STOP,
    VIDEO_SOURCE_SET_STOP_STREAMING_SUCCESS,
    VIDEO_SOURCE_RENDER_FRAME,
    VIDEO_SOURCE_FLUSH,
    VIDEO_SOURCE_STOP,
    VIDEO_SOURCE_MAX_EVENT
}VideoSourceSignals;

/*========================================================================
 *                     CLASS F/W DECLARATIONS
 *========================================================================*/
class SignalQueue;
class CWFD_HdcpCp;
using namespace android;
/*!
 *@brief Input Event structure.
 */
struct input_event
{
    struct timeval time;
    __u16 type;
    __u16 code;
    __s32 value;
};

/*!************************************************************************
 *@brief memory information structure.
 **************************************************************************/
struct mem_info
{
    __u32 fd;
    __u32 offset;
    __u32 size;
};

typedef enum MirrorErrorEvent{
    MIRRORING_START_FAIL,
    MIRRORING_PAUSE_FAIL,
    MIRRORING_RESUME_FAIL,
    MIRRORING_FRAME_SENT_FAIL,
    MIRRORING_FINISH_FAIL,
    MIRRORING_FRAME_RCV_FAIL,
    MIRRORING_STOP_FAIL,
} MirrorErorEventType;

typedef enum {
    STREAMING_DEINIT,
    STREAMING_QUERY,
    STREAMING_SUCCESS,
    STREAMING_STOPPING,
    STREAMING_FAILURE
}StreamingStatus;

class WFDMMThreads;

/**
 * @brief Delivers YUV data in two different modes.
 *
 * In live mode, buffers are pre-populated with YUV data at the time
 * of configuration. The source will loop through and deliver the
 * pre-populated buffers throughout the life of the session. Frames will be
 * delivered at the configured frame rate. This mode is useful for gathering
 * performance statistics as no file reads are performed at run-time.
 *
 * In  non-live mode, buffers are populated with YUV data at run time.
 * Buffers are delivered downstream as they become available. Timestamps
 * are based on the configured frame rate, not on the system clock.
 *
 */

class VideoSource
{
public:

    /**
     * @brief Frame callback for YUV buffer deliver
     */
    typedef void (*FrameDeliveryFnType)(OMX_BUFFERHEADERTYPE* pBuffer, void*);

    /**
     * @brief Constructor
     */
    VideoSource();

    /**
     * @brief Destructor
     */
    ~VideoSource();


   /*!*************************************************************************
    * @brief     Configures the source
    *
    * @param[in] nFrames The number of frames to to play.
    * @param[in] nBuffers The number of buffers allocated for the session.
    * @param[in] pFrameDeliverFn Frame delivery callback.
    * @param[in] bFrameSkipEnabled frame skipping enabled
    * @param[in] nFrameSkipLimitInterval frame skipping time interval
    *
    * @return    OMX_ERRORTYPE
    *
    * @note
    **************************************************************************/
    OMX_ERRORTYPE Configure(
        VideoEncStaticConfigType* pConfig,
        OMX_S32 nBuffers,
        FrameDeliveryFnType pFrameDeliverFn,
        eventHandlerType pEventHandlerFn,
        OMX_BOOL bFrameSkipEnabled,
        OMX_U64 nFrameSkipLimitInterval,
        OMX_U32 nModuleId,
        void* appData);

   /*!*************************************************************************
    * @brief     Changes the frame rate
    *            The frame rate will take effect immediately.
    * @param[in] nFramerate New frame rate
    * @return    OMX_ERRORTYPE
    * @note
    **************************************************************************/
    OMX_ERRORTYPE ChangeFrameRate(OMX_S32 nFramerate);

   /*!*************************************************************************
    * @brief     Changes the bit rate
    *            The bit rate will take effect immediately.
    *
    * @param[in] nBitrate The new bit rate.
    *
    * @return    OMX_ERRORTYPE
    * @note      None
    **************************************************************************/
    OMX_ERRORTYPE ChangeBitrate(OMX_S32 nBitrate);


   /**************************************************************************
    * @brief        Request an I-Frame
    *
    * @param[in]    None
    *
    * @return       OMX_ERRORTYPE
    **************************************************************************/
    OMX_ERRORTYPE RequestIntraVOP();

   /**************************************************************************
    * @brief Starts the delivery of buffers.
    *
    *               All buffers should be registered through the SetFreeBuffer
    *               function prior to calling this function. Source will continue
    *               to deliver buffers at the specified rate until the specified
    *               number of frames have been delivered.
    *
    * @param[in]    None
    *
    * @return       OMX_ERRORTYPE
    *
    * @note         Note that Source will not deliver buffers unless it has ownership
    *               of atleast 1 buffer. If Source does not have ownership of buffer when
    *               timer expires, it will block until a buffer is given to the component.
    *               Free buffers should be given to the Source through SetFreeBuffer
    *               function.
    **************************************************************************/
    OMX_ERRORTYPE Start();

   /**************************************************************************
    * @brief        Wait for the thread to finish.
    *
    * @param[in]    None
    *
    * @return       OMX_ERRORTYPE
    *
    * @note         Function will block until the Source has delivered all frames.
    **************************************************************************/
    OMX_ERRORTYPE Finish();

   /**************************************************************************
    * @brief        Wait for the thread to Pause.
    *
    * @param[in]    None
    *
    * @return       OMX_ERRORTYPE
    *
    * @note         Function will block until the Source has delivered all frames.
    **************************************************************************/
    OMX_ERRORTYPE Pause();

   /**************************************************************************
    * @brief        Gives ownership of the buffer to the source.
    *               All buffers must be registered with the Source prior
    *               to invoking Start. This will give initial ownership to
    *               the source.
    *
    * @param[in]    pBuffer Pointer to the buffer
    *
    * @return       OMX_ERRORTYPE
    *
    * @note         After Start is called, buffers must be given ownership
    *               when YUV buffers are free.
    ***************************************************************************/
    OMX_ERRORTYPE SetFreeBuffer(OMX_BUFFERHEADERTYPE* pBuffer);

   /***************************************************************************
    * @brief        Get number of Output buffers required by the port.
    *
    * @param[in]    nPortIndex[in] - Port number
    * @param[in]    nBuffers[rout] - Number of buffers
    *
    * @return       OMX_ERRORTYPE
    ***************************************************************************/
    OMX_ERRORTYPE GetOutNumBuffers(OMX_U32 nPortIndex, OMX_U32* nBuffers);

    OMX_BUFFERHEADERTYPE** GetBuffers();

    IGraphicBufferProducer* getSurface() const
    {
        return m_pVencoder ? m_pVencoder->getSurface() : NULL;
    }

    OMX_ERRORTYPE HandleStandby();
    void UpdateWFDMMStreamingQueryResponse(bool bStatus);
    OMX_ERRORTYPE DirectStreamingSupport(bool enabled);
    CWFD_HdcpCp* m_pHdcpHandle;

    OMX_BOOL     m_bHdcpSessionValid;

private:

    OMX_ERRORTYPE CreateResources();

    OMX_ERRORTYPE ReleaseResources();

    OMX_ERRORTYPE GenerateBlankFrame(OMX_TICKS);

    static void Screen_TimerHandler(void*);
    static bool  m_bVideoSourceActive;
    static void VideoSourceFrameDelivery(
        OMX_BUFFERHEADERTYPE* pEncBufferHdr, void* pThis);

    static void VideoSourceEventHandlerCb(
        void* pThis,
        OMX_U32 nModuleId,
        WFDMMSourceEventType nEvent,
        OMX_ERRORTYPE nStatus, void* nData);

    void EncryptData
    (
        OMX_BUFFERHEADERTYPE *pBufferHdr
    );

    OMX_ERRORTYPE EncryptData
    (
        OMX_BUFFERHEADERTYPE *pBufferHdr,
        int                   fd,
        char                 *pData,
        unsigned long         nLen,
        bool                  bSecure
    );

    OMX_ERRORTYPE AllocateHDCPResources();

    OMX_ERRORTYPE DeallocateHDCPResources();

    OMX_ERRORTYPE  FillHdcpCpExtraData(
        OMX_BUFFERHEADERTYPE* pBufHdr,
        OMX_U8* pucPESPvtHeader,
        OMX_U32 nPortIndex);

    bool  FillFillerBytesExtraData(
        OMX_BUFFERHEADERTYPE* pBufHdr,
        OMX_U8* pucPESPvtHeader,
        OMX_U32 nPortIndex);

    /*--------------------------------------------------------------------------
     Streaming functionality extension
    --------------------------------------------------------------------------*/

    static int WFDBinderCallback(void*, int, void*);
    int ProcessWFDBinderCallback(int, void*);

    OMX_ERRORTYPE StartStreaming();
    OMX_ERRORTYPE StopStreaming();
    OMX_ERRORTYPE WriteData(void* pData);
    OMX_ERRORTYPE RenderFrame(void* pData);
    OMX_ERRORTYPE sendcallbackToBinder(StreamingEventType StrmEvent, void* pData);
    OMX_ERRORTYPE VideoSourceFreeBuffer(OMX_BUFFERHEADERTYPE* pBufferHdr);
    void FlushStreamingQueue();
    OMX_ERRORTYPE FreeStreamingBuffer(OMX_BUFFERHEADERTYPE* pBufferHdr);
    OMX_ERRORTYPE ExecuteStart();
    OMX_ERRORTYPE ExecutePause();
    OMX_ERRORTYPE ExecuteFinish();
    static void VideoSourceThreadEntry(void* pThreadData, unsigned int signal);
    OMX_ERRORTYPE VideoSourceThread(int nSignal);
    OMX_ERRORTYPE ExecuteRenderFrame(bool bEOS = false);
    OMX_ERRORTYPE ExecuteStopStreamingSuccess();
    int  setAppInfo(char *name,char *operation);
    bool isCurrentAppAllowed();
    void parseAndPopulateWhiteListApp();
    int checkStreamingValidityOnCodec(StreamParamsType streamParams);
    int isCfgSupportForStreaming();
    OMX_ERRORTYPE ExecuteFlush();

    OMX_BUFFERHEADERTYPE* ProcessInputBuffer
    (
        unsigned int nLen,
        int          fd,
        char        *pData,
        bool         bSecure
    );

    void DeleteStreamingBuffers();

    void DeleteAppSupported();

    bool  FillDTSExtraData
    (
        OMX_BUFFERHEADERTYPE* pBufHdr,
        OMX_TICKS nDTS,
        OMX_U32 nPortIndex
    );
    void AbortStreaming();

    typedef enum WFDVideoSourceStates
    {
        WFDMM_VideoSource_STATE_INIT = 0,
        WFDMM_VideoSource_STATE_IDLE,
        WFDMM_VideoSource_STATE_ERROR,
        WFDMM_VideoSource_STATE_PLAYING,
        WFDMM_VideoSource_STATE_PLAY,
        WFDMM_VideoSource_STATE_PAUSING,
        WFDMM_VideoSource_STATE_PAUSE,
        WFDMM_VideoSource_STATE_STOPPING,
        WFDMM_VideoSource_STATE_UNDEFINED
    }WFDVideoSourceStatesType;

    WFDVideoSourceStatesType m_eVideoSrcState;
    eventHandlerType m_pEventHandlerFn;
    OMX_U32 m_nModuleId;
    OMX_S32 m_nNumBuffers;
    struct buffer *m_pHdcpOutBuffers;
    OMX_BUFFERHEADERTYPE** m_pVideoSourceOutputBuffers;
    SignalQueue*           m_pHDCPOutputBufQ;
    OMX_BOOL m_bFrameSkipEnabled;
    OMX_U64  m_nFrameSkipLimitInterval;
    OMX_U32 m_nMuxBufferCount;
    OMX_BOOL m_bStarted;
    OMX_BOOL m_bPause;
    FrameDeliveryFnType m_pFrameDeliverFn;
    void *m_appData;
    int ionfd;
    OMX_BOOL m_secureSession;
    static OMX_U8 sFillerNALU[FILLER_NALU_SIZE];
    int m_nFillerInFd;
    int m_nFillerOutFd;
    ion_user_handle_t m_hFillerInHandle;
    ion_user_handle_t m_hFillerOutHandle;
    unsigned char *m_pFillerDataInPtr;
    unsigned char *m_pFillerDataOutPtr;
    OMX_BOOL     m_bFillerNaluEnabled;
    OMX_BOOL m_bVideoForbid;
    WFDMMSourceVideoEncode*     m_pVencoder;
    WFDMMSourceStatistics*      m_pWFDMMSrcStats;
    OMX_BOOL    m_bDropVideoFrame;
    OMX_BOOL m_bBlankFrameSupport;
    VideoEncStaticConfigType m_sConfig;
    OMX_U8* m_pBlankFrame;
    OMX_U32 m_nBlankFrameSize;
    SignalQueue* m_pBlankFrameQ;
    OMX_BOOL m_bHandleStandby;
    int m_nMaxBlankFrames;
    int m_nBlankFramesDelivered;
    int m_nBlankFrameInFd;
    ion_user_handle_t m_hBlankFrameInHandle;
    unsigned char* m_pBlankFrameDataInPtr;
    OMX_TICKS m_nLastFrameTS;

    OMX_BOOL                       m_bStreamingEnabled;
    OMX_BOOL                       m_bPauseSuccess;
    StreamingStatus                m_eStreamingStatus;
    WFDMMThreads*                  m_hVideoSourceThread;
    SignalQueue*                   m_pStreamingBufQ;
    SignalQueue*                   m_pStreamingPendBufQ;
    OMX_S32                        m_nStreamingBufCnt;
    OMX_BUFFERHEADERTYPE**         m_pStreamingBufArray;
    WFDVideoSourceStatesType       m_eVideoStreamingSrcState;
    WFDVideoSourceStatesType       m_ePrevVideoSrcState;
    MM_HANDLE                      m_hCritSect;
    WFDMMService*                  m_WFDMMService;
    codecInfoType                  m_CodecInfo;
    StreamParamsType               m_streamParams;
    bool                           m_StreamingClosingReqSent;
    OMX_BOOL                       mbVideoSourceStopped;
    bool                           mbStopWhileStreaming;
    bool                           m_bStreamingSupport;
    bool                           m_bBaseStreamTimeTaken;
    OMX_U64                        m_nBaseSystemTime;
    OMX_TICKS                      m_nRenderTS[256];
    OMX_TICKS                      sTicks[256];
    OMX_TICKS                      m_nBaseRenderTimeStamp;
    OMX_TICKS                      m_nPrevAdjTimeStamp;
    OMX_TICKS                      m_nDSTotalTimeDrift;
    OMX_TICKS                      m_nDSNumSamplesAvgDrift;
    OMX_U32                        m_nPrevPushedBufsOnRender;
    uint8                          m_nRenderTSReadIdx;
    uint8                          m_nRenderTSWriteIdx;
    uint8                          ntickIdx;
    uint64                         m_nBaseStreamTime;
    /* This stores package name and operation of activity
       that is being used*/
    WhiteListType                  m_AppWhiteList;
    /* This stores list of package names that are supported/
       allowed by WFD currently. It is populated after
       parsing WFD_APPLIST_CFG_FILE */
    char**                         m_pAppSupported;
    /* This holds count of Applications mentioned in WFD_APPLIST_CFG_FILE.
       If this is 0, then it means there is no restriction.
       Else check will be performed against total value of it */
    uint32                         m_nCountAppSupported;

#ifdef ENABLE_STREAMING_DUMP
    FILE*                          Str_dump_file;
    OMX_BOOL                       m_bEnableStrDump;
#endif /* ENABLE_DUMP */
  };
#endif // #ifndef __WFD_MM_VIDEO_SOURCE_H__
