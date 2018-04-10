/*==============================================================================
*       WFDMMSourceOverlaySource.cpp
*
*  DESCRIPTION:
*       This module is for WFD source overlay implementation.
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

#include "WFDMMSourceOverlaySource.h"
#include "WFDSurfaceMediaSource.h"
#include <utils/String8.h>
#include <ui/GraphicBuffer.h>
#include "common_log.h"
#include "WFDMMLogs.h"
#include "WFDMMThreads.h"
#include <threads.h>
#include "WFDMMSourceMutex.h"
#include "WFDMMSourceSignalQueue.h"
#include "WFDUtils.h"

#include "MMTimer.h"
#include "MMMemory.h"
#include <cstdlib>
#include <errno.h>
#include <threads.h>

#include "Surface.h"
#include "ISurfaceComposer.h"
#include "SurfaceComposerClient.h"
#include <binder/IPCThreadState.h>
#include <media/stagefright/MetaData.h>
#include "gralloc_priv.h"
#include <cutils/properties.h>
#include "DisplayInfo.h"
#include "display_config.h"

#ifdef LOG_TAG
#undef LOG_TAG
#define LOG_TAG "WFDMMSRCOVERLAY"
#endif

using namespace android;

#define WFD_MM_SOURCE_THREAD_STACK_SIZE 16384

#define OVERLAY_CAPTURE_START_EVENT 0
#define OVERLAY_CAPTURE_STOP_EVENT  1
#define OVERLAY_CAPTURE_MAX_EVENT   2

#define OC_INVALID_OP               0
#define OC_READ_NEW_FRAME           1
#define OC_READ_NO_FRAME_UPDATE     2
#define OC_WRITE_SUCCESS            3

/*------------------------------------------------------------------------------
 State definitions
--------------------------------------------------------------------------------
*/
#define DEINIT  0
#define INIT    1
#define OPENED  2
#define PAUSED  3
#define CLOSING 5
#define CLOSED  4

/*------------------------------------------------------------------------------
 Macros to Update states
--------------------------------------------------------------------------------
*/
#define ISSTATEDEINIT  (state(0, true) == DEINIT)
#define ISSTATEINIT    (state(0, true) == INIT)
#define ISSTATEOPENED  (state(0, true) == OPENED)
#define ISSTATEPAUSED  (state(0, true) == PAUSED)
#define ISSTATECLOSED  (state(0, true) == CLOSED)
#define ISSTATECLOSING (state(0, true) == CLOSING)

#define SETSTATECLOSING (state(CLOSING, false))
#define SETSTATEINIT    (state(INIT   , false))
#define SETSTATEDEINIT  (state(DEINIT , false))
#define SETSTATEOPENED  (state(OPENED , false))
#define SETSTATEPAUSED  (state(PAUSED , false))
#define SETSTATECLOSED  (state(CLOSED , false))

/*------------------------------------------------------------------------------
 Critical Sections
--------------------------------------------------------------------------------
*/
#define WFD_OVERLAY_CRITICAL_SECT_ENTER(critSect) if(critSect)                 \
                                      MM_CriticalSection_Enter(critSect);
    /*      END CRITICAL_SECT_ENTER    */

#define WFD_OVERLAY_CRITICAL_SECT_LEAVE(critSect) if(critSect)                 \
                                      MM_CriticalSection_Leave(critSect);
    /*      END CRITICAL_SECT_LEAVE    */

/*------------------------------------------------------------------------------
//This macro provides a single point exit from function on
//encountering any error
--------------------------------------------------------------------------------
*/
#define WFD_OMX_ERROR_CHECK(result,error) ({                                   \
        if(result!= OMX_ErrorNone)                                             \
        {                                                                      \
            WFDMMLOGE(error);                                                  \
            goto EXIT;                                                         \
        }                                                                      \
    })

#define WFD_TIME_NSEC_IN_MSEC 1000000
#define WFD_TIME_USEC_IN_MSEC 1000
#define WFD_TIME_MSEC_IN_SEC 1000
#define WFD_TIME_NSEC_IN_USEC 1000
#define WFD_TIME_USEC_IN_SEC 1000000
#define WFD_TIME_NSEC_IN_USEC 1000
#define WFD_TIME_NSEC_IN_SEC 1000000000

#define OVERLAY_STATS_MAX_SLOTS 20

/*==============================================================================

         FUNCTION:          WFDMMSourceOverlaySource

         DESCRIPTION:
*//**       @brief          CTOR for WFDMMSourceOverlaySource
*//**
@par     DEPENDENCIES:
                            None
*//*
         PARAMETERS:
*//**       @param[in]

*//*     RETURN VALUE:
*//**       @return
                            None
@par     SIFE EFFECTS:
                            None
*//*==========================================================================*/
WFDMMSourceOverlaySource::WFDMMSourceOverlaySource()
{
    WFDMMLOGH("WFDMMSourceOverlaySource ctor()");
    initData();
}

/*==============================================================================

         FUNCTION:          ~WFDMMSourceOverlaySource

         DESCRIPTION:
*//**       @brief          DTOR for WFDMMSourceOverlaySource
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
WFDMMSourceOverlaySource::~WFDMMSourceOverlaySource()
{
    WFDMMLOGH("WFDMMSourceOverlaySource dtor()");
    releaseResources();
    WFDMMLOGH("Done with ~WFDMMSourceOverlaySource");
}

/*==============================================================================

         FUNCTION:          initData

         DESCRIPTION:
*//**       @brief          initializes data members of WFDMMSourceOverlaySource
*//**
@par     DEPENDENCIES:
                            None
*//*
         PARAMETERS:
*//**       @param          None

*//*     RETURN VALUE:
*//**       @return
                            OMX_ErrorNone or other Error
@par     SIDE EFFECTS:
                            None
*//*==========================================================================*/
OMX_ERRORTYPE WFDMMSourceOverlaySource::initData()
{
    OMX_ERRORTYPE result       = OMX_ErrorNone;

    m_hOverlayCaptureThread         = NULL;
    m_hInputDeliveryThread          = NULL;
    m_nModuleId                     = 0;
    m_pAppData                      = NULL;

    m_bPaused                       = OMX_FALSE;

    m_pFrameDeliveryFn              = NULL;
    m_pEventHandlerFn               = NULL;
    m_hStateCritSect                = NULL;

    m_bFirstFrameSent                = false;
    m_bPrevTransparent               = false;
    m_nEncodeTime                    = 0;

    m_mainDpy                       = NULL;
    m_dpy                           = NULL;
    m_pBuffProducer                 = NULL;
    m_pSurfaceMediaSrc              = NULL;

    m_bCaptureOverlay               = false;
    m_nCaptureHeight                = 0;
    m_nCaptureWidth                 = 0;

    m_pMuxInputQ                    = NULL;
    m_nMuxNumInputBufs              = OVERLAY_SINK_INPUT_Q_SIZE;
    m_pInputBuffers                 = NULL;
    m_nPNGBufferSize                = 0;

    m_nReadIdx                      = OVERLAY_CAPTURE_NUM_BUFF -1;
    m_nWriteIdx                     = 0;
    m_hQCritSect                    = NULL;
    m_pLastXmitBuffer               = NULL;

    m_pImgEncoder                   = NULL;

    m_nOverlayID                    = 1;
    m_sActiveIDInfo.pActiveIDList   = NULL;
    m_sActiveIDInfo.pInActiveIDList = NULL;
    m_nMaxOverlayIDSupport          = 0;

    m_nLastTimeStamp                = 0;
    m_nSendBuffTS                   = 0;

#ifdef OVERLAY_STATISTICS
    m_nAvgEncodeTime                = 0;
    m_nStatCounter                  = 0;
#endif

    for(int i = 0; i < OVERLAY_CAPTURE_NUM_BUFF ; i++)
    {
        m_sLatestFrameQ[i].pBuffer = NULL;
        m_sLatestFrameQ[i].isRead  = false;
        m_sLatestFrameQ[i].isValid = false;
        m_sLatestFrameQ[i].nCaptureTimeStamp = 0;
    }

    SETSTATEDEINIT;
    return result;
}

/*==============================================================================

         FUNCTION:          configure

         DESCRIPTION:
*//**       @brief          Configures overlay copmponent
*//**
@par     DEPENDENCIES:
                            None
*//*
         PARAMETERS:
*//**       @param[in]      pFrameDeliveryFn    Handle to provide back buffer to
                                                    controller module

            @param[in]      pEventHandlerFn     The callback for reporting
                                                    events to controller module

            @param[in]      pConfig             The input configuration
                                                    for WFD session

            @param[in]      nModuleId           Module Id assigned by client

            @param[in]      pAppData            The appdata for use in callbacks

*//*     RETURN VALUE:
*//**       @return
                            OMX_ERRORTYPE
@par     SIFE EFFECTS:
                            None
*//*==========================================================================*/
OMX_ERRORTYPE WFDMMSourceOverlaySource::configure
(
    FrameDeliveryFnType     pFrameDeliveryFn,
    eventHandlerType        pEventHandlerFn,
    OverlaySrcConfigType    *pConfig,
    OMX_U32                 nModuleId,
    OMX_PTR                 pAppData
)
{
    OMX_ERRORTYPE result = OMX_ErrorNone;

    if(!pFrameDeliveryFn || !pEventHandlerFn || !pAppData || !pConfig)
    {
        result = OMX_ErrorBadParameter;
        WFD_OMX_ERROR_CHECK(result,
            "Configure(): Invalid data to configure overlay module");
    }

    if(!ISSTATEDEINIT)
    {
        result = OMX_ErrorUndefined;
        WFD_OMX_ERROR_CHECK(result,
            "Configure(): Incorrect state for configure");
    }

    WFDMMLOGM3("Configure(): height[%d], width[%d], isHDCP[%d]",pConfig->frameHeight,
                                                              pConfig->frameWidth,
                                                              pConfig->IsHdcp);

    // Allocating Max sized buffers
    m_nPNGBufferSize    = ((pConfig->frameHeight * pConfig->frameWidth)* 4) +
                            OVERLAY_PVT_HEADER_SIZE;
    m_pFrameDeliveryFn  = pFrameDeliveryFn;
    m_pEventHandlerFn   = pEventHandlerFn;
    m_nModuleId         = nModuleId;
    m_pAppData          = pAppData;
    memcpy(&m_sConfig, pConfig, sizeof(m_sConfig));

    //Fill up WfdImgConfig
    m_sWfdImgConfig.nHeight = pConfig->frameHeight;
    m_sWfdImgConfig.nWidth = pConfig->frameWidth;
    m_sWfdImgConfig.eColor = WFD_IMG_COLOR_FMT_ARGB8888;

    //Fill up ActiveIDInfo
    m_nMaxOverlayIDSupport = 6; //:TODO: This would come as part of RTSP msg

    m_sActiveIDInfo.nListHead= -1;
    m_sActiveIDInfo.nInActiveListHead = -1;
    m_sActiveIDInfo.pActiveIDList = (uint8 *)calloc(m_nMaxOverlayIDSupport, sizeof(uint8));
    m_sActiveIDInfo.pInActiveIDList = (uint8 *)calloc(m_nMaxOverlayIDSupport*2, sizeof(uint8));

    result = createResources();
    WFD_OMX_ERROR_CHECK(result,"Configure(): createResources failed");

    SETSTATEINIT;

EXIT:
    return result;
}

/*==============================================================================

         FUNCTION:          createResources

         DESCRIPTION:
*//**       @brief          create threads & queues for WFDMMSourceOverlaySource
*//**
@par     DEPENDENCIES:
                            None
*//*
         PARAMETERS:
*//**       @param          None

*//*     RETURN VALUE:
*//**       @return
                            OMX_ErrorNone or other Error
@par     SIDE EFFECTS:
                            None
*//*==========================================================================*/
OMX_ERRORTYPE WFDMMSourceOverlaySource::createResources()
{
    OMX_ERRORTYPE result = OMX_ErrorNone;
    int nRet = 0;

    /*--------------------------------------------------------------------------
    Create a thread to capture frames from SurfaceMediaSource
    ----------------------------------------------------------------------------
    */
    m_hOverlayCaptureThread = new WFDMMThreads(OVERLAY_CAPTURE_MAX_EVENT);

    if(!m_hOverlayCaptureThread)
    {
        result = OMX_ErrorInsufficientResources;
        WFD_OMX_ERROR_CHECK(result,
              "createResources(): m_hOverlayCaptureThread create failed");
    }

    m_hOverlayCaptureThread->Start(captureThreadEntry,
                                  -2, 32768,this, "OverlayCaptureThread");

    /*--------------------------------------------------------------------------
    Create a thread to encode and deliver buffers to MUX
    ----------------------------------------------------------------------------
    */
    m_hInputDeliveryThread= new WFDMMThreads(OVERLAY_CAPTURE_MAX_EVENT);

    if(!m_hInputDeliveryThread)
    {
        result = OMX_ErrorInsufficientResources;
        WFD_OMX_ERROR_CHECK(result,
              "createResources(): m_hInputDeliveryThread create failed");
    }

    m_hInputDeliveryThread->Start(ipDeliveryThreadEntry,
                                  -2, 32768,this, "OverlayIpDelThread");

    /*--------------------------------------------------------------------------
    Create Critical Section for state transitions
    ----------------------------------------------------------------------------
    */
    if(MM_CriticalSection_Create(&m_hStateCritSect))
    {
        result = OMX_ErrorInsufficientResources;
        WFD_OMX_ERROR_CHECK(result,
              "createResources(): crit sect for state creation failed");
    }

    /*--------------------------------------------------------------------------
    Create Critical Section for Q Access
    ----------------------------------------------------------------------------
    */
    if(MM_CriticalSection_Create(&m_hQCritSect))
    {
        result = OMX_ErrorInsufficientResources;
        WFD_OMX_ERROR_CHECK(result,
              "createResources(): crit sect for Q Access creation failed");
    }

    /*--------------------------------------------------------------------------
    Create Q for holding input buffers for MUX and allocate the buffers
    ----------------------------------------------------------------------------
    */
    m_pMuxInputQ = MM_New_Args(SignalQueue, (100, sizeof(OMX_BUFFERHEADERTYPE*)));

    if(!m_pMuxInputQ)
    {
        result = OMX_ErrorInsufficientResources;
        WFD_OMX_ERROR_CHECK(result,
              "createResources(): failed to create MuxInputQ");
    }

    result = allocateBuffersForMuxQ();
    WFD_OMX_ERROR_CHECK(result,
        "createResources(): failed to allocate buffers for MuxInputQ");

    /*--------------------------------------------------------------------------
    Create the PNG Image Encoder
    ----------------------------------------------------------------------------
    */
    m_pImgEncoder = WFDMMSourceImageEncode::Create(WFD_IMG_CODING_PNG);

    if(!m_pImgEncoder)
    {
        result = OMX_ErrorInsufficientResources;
        WFD_OMX_ERROR_CHECK(result,
            "createResources(): Failed to create PNG Encoder");
    }

    int res;
    res = m_pImgEncoder->Configure(&m_sWfdImgConfig);
    if(res < 0)
    {
        result = OMX_ErrorUndefined;
        WFD_OMX_ERROR_CHECK(result,
            "createResources(): Failed to configure PNG Encoder");
    }

EXIT:
    return result;
}

/*==============================================================================

         FUNCTION:              allocateBuffersForMuxQ

         DESCRIPTION:
*//**       @brief              Allocates buffers for Q which interacts with MUX
                                    component
*//**
@par     DEPENDENCIES:
                                None
*//*
         PARAMETERS:
*//**       @param              None

*//*     RETURN VALUE:
*//**       @return
                                void
@par     SIDE EFFECTS:
                                None
*//*==========================================================================*/
OMX_ERRORTYPE WFDMMSourceOverlaySource::allocateBuffersForMuxQ()
{
    m_pInputBuffers = MM_New_Array(OMX_BUFFERHEADERTYPE* , m_nMuxNumInputBufs);

    if(!m_pMuxInputQ)
    {
        WFDMMLOGE("allocateBuffersForMuxQ(): MuxInputQ is NULL");
        return OMX_ErrorInsufficientResources;
    }

    for(OMX_S32 i = 0 ; i < m_nMuxNumInputBufs ; i++)
    {
        OMX_BUFFERHEADERTYPE* pBufferHdr = NULL;

        pBufferHdr = static_cast<OMX_BUFFERHEADERTYPE*>(
            calloc(1, sizeof(*pBufferHdr)));

        if(!pBufferHdr)
        {
            WFDMMLOGE("allocateBuffersForMuxQ(): Failed to allocate buffer");
            return OMX_ErrorInsufficientResources;
        }

        //Allocate Buffers
        pBufferHdr->pBuffer = (OMX_U8*)MM_Malloc(m_nPNGBufferSize);

        pBufferHdr->nAllocLen = m_nPNGBufferSize;

        WFDMMLOGL2("allocateBuffersForMuxQ(): [%d] Pushing %p to MuxInputQ",
                   i,pBufferHdr);
        m_pMuxInputQ->Push(&pBufferHdr, sizeof(pBufferHdr));

        //Point it to the allocated buffer
        m_pInputBuffers[i] = pBufferHdr;
    }
    return OMX_ErrorNone;
}

/*==============================================================================

         FUNCTION:              releaseResources

         DESCRIPTION:
*//**       @brief              release threads & queues for
                                    WFDMMSourceOverlaySource
*//**
@par     DEPENDENCIES:
                                None
*//*
         PARAMETERS:
*//**       @param              None

*//*     RETURN VALUE:
*//**       @return
                                void
@par     SIDE EFFECTS:
                                None
*//*==========================================================================*/
void WFDMMSourceOverlaySource::releaseResources()
{
    OMX_ERRORTYPE result = OMX_ErrorNone;
    WFDMMLOGH("releaseResources(): Releasing Overlay Resources");

    releaseMediaBuffers();

    if(m_hOverlayCaptureThread)
    {
        MM_Delete(m_hOverlayCaptureThread);
        m_hOverlayCaptureThread = NULL;
    }

    if(m_hInputDeliveryThread)
    {
        MM_Delete(m_hInputDeliveryThread);
        m_hInputDeliveryThread = NULL;
    }

    WFDMMLOGH("releaseResources(): Freeing up MuxInputQ");
    deallocateBuffersForMuxQ();

    if(m_pMuxInputQ)
    {
        MM_Delete(m_pMuxInputQ);
        m_pMuxInputQ= NULL;
    }

    WFDMMLOGH("releaseResources(): Clearing up Critical sections");
    if(m_hStateCritSect)
    {
        MM_CriticalSection_Release(m_hStateCritSect);
        m_hStateCritSect = NULL;
    }

    if(m_hQCritSect)
    {
        MM_CriticalSection_Release(m_hQCritSect);
        m_hQCritSect = NULL;
    }

    WFDMMLOGH("releaseResources(): Delete PNG encoder");
    //Delete Encoder
    if(m_pImgEncoder)
    {
        MM_Delete(m_pImgEncoder);
        m_pImgEncoder = NULL;
    }

    //Release VDS
    WFDMMLOGH("releaseResources(): Release VDS");
    releaseVDS();

#ifdef OVERLAY_STATISTICS
    if(m_nStatCounter > 0)
    {
        WFDMMLOGE2("OverlayStatistics: Avg encoding time after %d frames: %llu",
            m_nStatCounter, m_nAvgEncodeTime/m_nStatCounter);

        m_nStatCounter = 0;
        m_nAvgEncodeTime = 0;
    }
#endif
}

/*==============================================================================

         FUNCTION:              deallocateBuffersForMuxQ

         DESCRIPTION:
*//**       @brief              Deallocate buffers for Q which interact with MUX
                                    component
*//**
@par     DEPENDENCIES:
                                None
*//*
         PARAMETERS:
*//**       @param              None

*//*     RETURN VALUE:
*//**       @return
                                void
@par     SIDE EFFECTS:
                                None
*//*==========================================================================*/
OMX_ERRORTYPE WFDMMSourceOverlaySource::deallocateBuffersForMuxQ()
{
    WFDMMLOGM("deallocateBuffersForMuxQ()");
    if(m_pMuxInputQ)
    {
        /*----------------------------------------------------------------------
        Wait for all buffers to be returned from MUX, in any
        ------------------------------------------------------------------------
        */
        while(m_pMuxInputQ->GetSize() != m_nMuxNumInputBufs)
        {
            WFDMMLOGM1("deallocateBuffersForMuxQ(): Waiting for [%ld] frames from MUX",
                (m_nMuxNumInputBufs - m_pMuxInputQ->GetSize()));

            MM_Timer_Sleep(2);
        }

        //Deallocate
        while(m_pMuxInputQ->GetSize())
        {
            OMX_BUFFERHEADERTYPE* pBufferHdr = NULL;

            m_pMuxInputQ->Pop(&pBufferHdr,sizeof(pBufferHdr),100);

            if(pBufferHdr)
            {
                if(pBufferHdr->pBuffer != NULL)
                {
                    MM_Free(pBufferHdr->pBuffer);
                    pBufferHdr->pBuffer = NULL;
                }
                WFDMMLOGM1("deallocateBuffersForMuxQ(): Deleting [%p] from Q",pBufferHdr);
                MM_Free(pBufferHdr);
                pBufferHdr = NULL;
            }
        }
    }
    if(m_pInputBuffers != NULL)
    {
        MM_Delete_Array(m_pInputBuffers);
        m_pInputBuffers = NULL;
    }
    return OMX_ErrorNone;
}

/*==============================================================================

         FUNCTION:              createSurface

         DESCRIPTION:
*//**       @brief              Create surface for using SurfaceMediaSource APIs
*//**
@par     DEPENDENCIES:
                                None
*//*
         PARAMETERS:
*//*            @param[in]      width width of surface to be created

     *          @param[in]      height height of surface to be created

*//*     RETURN VALUE:
*//**       @return
                                OMX_ErrorNone or None
@par     SIDE EFFECTS:
                                None
*//*==========================================================================*/
OMX_ERRORTYPE WFDMMSourceOverlaySource::createSurface(int width, int height)
{
    WFDMMLOGH("createSurface(): Creating SMS for overlay capture");
    OMX_ERRORTYPE result = OMX_ErrorNone;

    m_pSurfaceMediaSrc   = MM_New_Args(WFDSurfaceMediaSource,
                                       (width, height, WFD_SMS_CONSUMER_SW));

    m_pBuffProducer = ((m_pSurfaceMediaSrc->getProducer()));

    if(m_pBuffProducer == NULL)
    {
        result = OMX_ErrorInsufficientResources;
        WFD_OMX_ERROR_CHECK(result,
                            "createSurface():Unable to get BufferQ");
    }

EXIT:
    return result;
}

/*==============================================================================

         FUNCTION:          AccessLatestBufQ

         DESCRIPTION:
*//**       @brief          Helper method to update read/write index in FrameQ
*//**
@par     DEPENDENCIES:
                            None
*//*
         PARAMETERS:
*//*         @param[in] read If a frame has been read of the FrameQ or not

*//*     RETURN VALUE:
*//**       @return
                            None
@par     SIDE EFFECTS:
                            None
*//*==========================================================================*/
int WFDMMSourceOverlaySource::AccessLatestBufQ(bool bRead,
                                           overlayFrameInfoType* pBufferInfo)
{

    int nRetVal = OC_INVALID_OP;
    WFD_OVERLAY_CRITICAL_SECT_ENTER(m_hQCritSect);

    /* -------------------------------------------------------------------------
    Currently the size of m_sLatestFrameQ is 1. The logic below is generic,
    so that if size of array is increased in future, the logic will still
    hold valid.
    ----------------------------------------------------------------------------
    */
    if(bRead)
    {
        OMX_U64 nReadIdx = (m_nReadIdx + 1) % OVERLAY_CAPTURE_NUM_BUFF;

        /*----------------------------------------------------------------------
        Check if the next buffer position has been updated with a new frame.
        Else keep sending the current frame
        ------------------------------------------------------------------------
        */
        if(m_sLatestFrameQ[nReadIdx].isValid)
        {
            if(!m_sLatestFrameQ[nReadIdx].isRead)
            {
                /*--------------------------------------------------------------
                There is an unread frame available. Read and update index
                ----------------------------------------------------------------
                */
                m_sLatestFrameQ[nReadIdx].isRead = true;

                if(pBufferInfo)
                {
                    memcpy(pBufferInfo, &m_sLatestFrameQ[nReadIdx],
                           sizeof(overlayFrameInfoType));
                }
                m_nReadIdx = nReadIdx;
                nRetVal =  OC_READ_NEW_FRAME;
            }
            else
            {
                /*--------------------------------------------------------------
                If there is no latest frame, simply return
                ----------------------------------------------------------------
                */
                 nRetVal = OC_READ_NO_FRAME_UPDATE;
             }
        }
    }
    else
    {
        if(m_sLatestFrameQ[m_nWriteIdx].isValid &&
           !m_sLatestFrameQ[m_nWriteIdx].isRead)
        {
            /*------------------------------------------------------------------
            Buffer is being overwritten without consumption. Release it
            --------------------------------------------------------------------
            */
            MediaBuffer *pMediaBuffer =
                          (MediaBuffer*)m_sLatestFrameQ[m_nWriteIdx].pBuffer;
            if(pMediaBuffer)
            {
                pMediaBuffer->release();
            }
            /*-----------------------------------------------------------------
            Latch on RdIdx to the current WriteIdx to avoid out of sequence
            frames
            -------------------------------------------------------------------
            */
            m_nReadIdx = m_nWriteIdx;
        }

        /*----------------------------------------------------------------------
        Update new MediaBuffer information in the queue
        ------------------------------------------------------------------------
        */

        if(pBufferInfo && pBufferInfo->pBuffer)
        {
            m_sLatestFrameQ[m_nWriteIdx].pBuffer = pBufferInfo->pBuffer;
            m_sLatestFrameQ[m_nWriteIdx].isRead  = false;
            m_sLatestFrameQ[m_nWriteIdx].isValid = true;
            m_sLatestFrameQ[m_nWriteIdx].nCaptureTimeStamp =
                pBufferInfo->nCaptureTimeStamp;

        }
        m_nWriteIdx= (m_nWriteIdx + 1) % OVERLAY_CAPTURE_NUM_BUFF;

        nRetVal = OC_WRITE_SUCCESS;
    }

    WFD_OVERLAY_CRITICAL_SECT_LEAVE(m_hQCritSect);
    return nRetVal;
}

/*==============================================================================

         FUNCTION:         GetOutNumBuffers

         DESCRIPTION:
*//**       @brief         This is the WFDMMOverlaySource function used for
                           getting the number of buffer.
*//**

@par     DEPENDENCIES:
                           None
*//*
         PARAMETERS:
*//**       @param         nPortIndex  -  Port index
                           nBuffers  - number of buffer


*//*     RETURN VALUE:
*//**       @return
                           OMX_ERRORTYPE

@par     SIDE EFFECTS:

*//*==========================================================================*/
OMX_ERRORTYPE WFDMMSourceOverlaySource::GetOutNumBuffers(
    OMX_U32 nPortIndex, OMX_U32* nBuffers)
{
    if(nPortIndex != 0)
    {
        return OMX_ErrorBadPortIndex;
    }

    WFDMMLOGM1("GetOutNumBuffers(): Populating Buffer count as [%d]",
        m_nMuxNumInputBufs);

    *nBuffers = m_nMuxNumInputBufs;
    return OMX_ErrorNone;
}

/*==============================================================================

         FUNCTION:              startCapture

         DESCRIPTION:
*//**       @brief              start capturing frames
*//**
@par     DEPENDENCIES:
                                None
*//*
         PARAMETERS:
*//*         @param
                                None

*//*     RETURN VALUE:
*//**       @return
                                OMX_ErrorNone or none
@par     SIDE EFFECTS:
                                None
*//*==========================================================================*/
OMX_ERRORTYPE WFDMMSourceOverlaySource::startCapture()
{
    OMX_ERRORTYPE result = OMX_ErrorNone;
    int nRet = 0;
    sp<MetaData> params;
    int numSMSBuff = 5;//default

    WFDMMLOGH("StartCapture(): Start capture received for overlay");

    if(ISSTATEPAUSED)
    {
        WFDMMLOGH("StartCapture(): Resume from earlier pause");
        return resumeCapture();
    }
    /*--------------------------------------------------------------------------
    Creating surface based on negotiated height and width which will be
    retained throughout the session
    ----------------------------------------------------------------------------
    */
    result = createSurface(m_sConfig.frameWidth,m_sConfig.frameHeight);

    WFD_OMX_ERROR_CHECK(result,
                        "StartCapture(): Failed to get surface");

    WFDMMLOGH("StartCapture(): Surface Created");

    SETSTATEOPENED;

    params = new MetaData();

    if(params == NULL  || m_pSurfaceMediaSrc == NULL)
    {
        result = OMX_ErrorInsufficientResources;
        WFD_OMX_ERROR_CHECK(result,"StartCapture(): SMS or MetaData not allocated!");
    }

    /*--------------------------------------------------------------------------
    Start SMS
    ----------------------------------------------------------------------------
    */
    params->setInt32(android::kKeyNumBuffers,numSMSBuff);
    WFDMMLOGE1("StartCapture(): Starting SMS with %d number of Buffers",
        numSMSBuff);

    m_pSurfaceMediaSrc->start(params.get());

    /*--------------------------------------------------------------------------
    Post signal to start Overlay capture. Delivery thread will function only
    when frame is available from display
    ----------------------------------------------------------------------------
    */
    m_hOverlayCaptureThread->SetSignal(OVERLAY_CAPTURE_START_EVENT);

    /*--------------------------------------------------------------------------
    Create native VDS
    ----------------------------------------------------------------------------
    */
    result = createVirtualDisplay();
    WFD_OMX_ERROR_CHECK(result,"StartCapture(): Failed to create VDS");

EXIT:
    return result;
}

/*==============================================================================

         FUNCTION:              stopCapture

         DESCRIPTION:
*//**       @brief              stop capturing frames
*//**
@par     DEPENDENCIES:
                                None
*//*
         PARAMETERS:
*//*         @param
                                None

*//*     RETURN VALUE:
*//**       @return
                                OMX_ErrorNone or none
@par     SIDE EFFECTS:
                                None
*//*==========================================================================*/
OMX_ERRORTYPE WFDMMSourceOverlaySource::stopCapture()
{
    WFDMMLOGH("stopCapture(): Stop capture received for Overlay Module");
    OMX_ERRORTYPE result = OMX_ErrorNone;

    m_bPaused = OMX_FALSE;

    SETSTATECLOSING;

    //Clear overlays
    WFDMMLOGH("stopCapture(): Notify clearing of all overlays");
    transferPrevOverlays();
    removePrevOverlays();

    WFDMMLOGD("stopCapture(): Waiting for Overlay StopCapture");
    while(m_pMuxInputQ && m_pMuxInputQ->GetSize() != m_nMuxNumInputBufs)
    {
        /*----------------------------------------------------------------------
        Wait until all buffers are returned from MUX, if any
        ------------------------------------------------------------------------
        */
        MM_Timer_Sleep(2);
    }

    releaseMediaBuffers();

    /*--------------------------------------------------------------------------
    Unblock SMS read, let it return EOS, we will exit the capture
    thread soon
    ----------------------------------------------------------------------------
    */

    WFDMMLOGH("stopCapture(): Post STOP on Capture and Delivery thread");
    if(m_pSurfaceMediaSrc.get() != NULL)
    {
        m_pSurfaceMediaSrc->stop();
    }

    if(m_hOverlayCaptureThread)
    {
        m_hOverlayCaptureThread->SetSignal(OVERLAY_CAPTURE_STOP_EVENT);
    }

    if(m_hInputDeliveryThread)
    {
        m_hInputDeliveryThread->SetSignal(OVERLAY_CAPTURE_STOP_EVENT);
    }

    m_bFirstFrameSent = false;
    m_bPrevTransparent = false;
    m_nEncodeTime = 0;

    SETSTATECLOSED;
    WFDMMLOGH("stopCapture(): Done stopping capture for overlay");
    return result;
}

/*==============================================================================

         FUNCTION:              pauseCapture

         DESCRIPTION:
*//**       @brief              pause capture
*//**
@par     DEPENDENCIES:
                                None
*//*
         PARAMETERS:
*//*         @param
                                None

*//*     RETURN VALUE:
*//**       @return
                                OMX_ERRORTYPE
@par     SIDE EFFECTS:
                                None
*//*==========================================================================*/
OMX_ERRORTYPE WFDMMSourceOverlaySource::pauseCapture()
{
    WFDMMLOGH("pauseCapture(): Pause capturing overlays");
    OMX_ERRORTYPE result = OMX_ErrorNone;

    if(!ISSTATEOPENED)
    {
        ALOGE("Incorrect state for PAUSE");
        return result;
    }

    m_bPaused = OMX_TRUE;

    SETSTATEPAUSED;

    //Clear Overlays
    transferPrevOverlays();
    removePrevOverlays();

#if 0
    /*Feature: Forced clearing of overlays on PAUSE/RESUME*/
    MM_Timer_Sleep(100);
    OMX_BUFFERHEADERTYPE *pBuf = NULL;

    m_pMuxInputQ->Pop(&pBuf, sizeof(&pBuf), 0);
    if(pBuf)
    {
        pvtHeaderInfoType pvtHdrInfo;

        pvtHdrInfo.bActivePresentation = true;
        pvtHdrInfo.overlayID = 0xFF;
        pvtHdrInfo.topLeftX = 1;
        pvtHdrInfo.topLeftY = 2;
        pvtHdrInfo.height = 3;
        pvtHdrInfo.width = 4;

        pBuf->nFilledLen = 18;

        GetCurTime(pBuf->nTimeStamp);
        //Attach Pvt Header
        attachOverlayPvtHeader(pBuf->pBuffer,&pvtHdrInfo);
        m_pFrameDeliveryFn(pBuf, m_pAppData);
    }
#endif

    WFDMMLOGM("pauseCapture(): Waiting for Overlay pauseCapture");
    while(m_pMuxInputQ && m_pMuxInputQ->GetSize() != m_nMuxNumInputBufs)
    {
        /*----------------------------------------------------------------------
        Wait for all the buffers to be returned from MUX, if any
        ------------------------------------------------------------------------
        */
        MM_Timer_Sleep(2);
    }
    WFDMMLOGH("pauseCapture(): All buffers recvd back into MuxInputQ");

    releaseMediaBuffers();

    WFDMMLOGH("pauseCapture(): Pause delivery thread");
    if(m_hInputDeliveryThread)
    {
        m_hInputDeliveryThread->SetSignal(OVERLAY_CAPTURE_STOP_EVENT);
    }

    /*--------------------------------------------------------------------------
    Overlay capture thread is not paused. Let it keep running and simply
    release the buffers in capture thread
    -----------------------------------------------------------------------------
    */

    m_bFirstFrameSent = false;
    m_bPrevTransparent = false;
    m_nEncodeTime = 0;

    return result;
}

/*==============================================================================

         FUNCTION:          resumeCapture

         DESCRIPTION:
*//**       @brief          resume capture
*//**
@par     DEPENDENCIES:
                            None
*//*
         PARAMETERS:
*//*         @param
                            None

*//*     RETURN VALUE:
*//**       @return
                            OMX_ERRORTYPE
@par     SIDE EFFECTS:
                            None
*//*==========================================================================*/
OMX_ERRORTYPE WFDMMSourceOverlaySource::resumeCapture()
{
    WFDMMLOGH("resumeCapture(): Calling resume for overlay capture");

    SETSTATEOPENED;

    m_bPaused = OMX_FALSE;
    m_nReadIdx              = OVERLAY_CAPTURE_NUM_BUFF -1;
    m_nWriteIdx             = 0;

#if 0
    /*Feature: Forced clearing of overlays on PAUSE/RESUME*/
    OMX_BUFFERHEADERTYPE *pBuf = NULL;

    m_pMuxInputQ->Pop(&pBuf, sizeof(&pBuf), 0);

    if(pBuf)
    {
        pvtHeaderInfoType pvtHdrInfo;

        pvtHdrInfo.bActivePresentation = true;
        pvtHdrInfo.overlayID = 0xFF;
        pvtHdrInfo.topLeftX = 4;
        pvtHdrInfo.topLeftY = 3;
        pvtHdrInfo.height = 2;
        pvtHdrInfo.width = 1;

        //Attach Pvt Header
        attachOverlayPvtHeader(pBuf->pBuffer,&pvtHdrInfo);
        pBuf->nFilledLen = 18;

        GetCurTime(pBuf->nTimeStamp);
        m_pFrameDeliveryFn(pBuf, m_pAppData);
    }
#endif
    return OMX_ErrorNone;
}

/*==============================================================================

         FUNCTION:              ipDeliveryThreadEntry

         DESCRIPTION:
*//**       @brief              entry point for input delivery thread
*//**
@par     DEPENDENCIES:
                                None
*//*
         PARAMETERS:
*//*         @param
                                None

*//*     RETURN VALUE:
*//**       @return
                                None

@par     SIDE EFFECTS:
                                None
*//*==========================================================================*/
void WFDMMSourceOverlaySource::ipDeliveryThreadEntry(void *pThreadData,
                                                        unsigned int signal)
{
    if (pThreadData)
    {
        WFDMMLOGH("ipDeliveryThreadEntry(): ipDeliveryThreadEntry");

        ((WFDMMSourceOverlaySource*)pThreadData)->ipDeliveryThread(signal);
    }
    else
    {
        WFDMMLOGE("ipDeliveryThreadEntry(): Null data passed in ipDeliveryThreadEntry");
    }
    return;
}

/*==============================================================================

         FUNCTION:              ipDeliveryThread

         DESCRIPTION:
*//**       @brief              main body for I/P Delivery Thread execution
*//**
@par     DEPENDENCIES:
                                None
*//*
         PARAMETERS:
*//*         @param
                                None

*//*     RETURN VALUE:
*//**       @return
                                OMX_ErrorNone  or error
@par     SIDE EFFECTS:
                                None
*//*==========================================================================*/
OMX_ERRORTYPE WFDMMSourceOverlaySource::ipDeliveryThread(int nSignal)
{
    OMX_ERRORTYPE result = OMX_ErrorNone;
    int ret              = 0;

    WFDMMLOGH("ipDeliveryThread() - Hola!");

    /*--------------------------------------------------------------------------
    If ever rechristening of this thread is implemented bear in mind that
    there is an upper limit to the name (as of now 16)
    ----------------------------------------------------------------------------
    */
    ret = pthread_setname_np(pthread_self(),"WFDOverlayDel");
    if(ret)
    {
        WFDMMLOGE2("ipDeliveryThread(): Failed to set thread name due to %d %s",
                    ret, strerror(errno));
        //No need to panic
    }

    if(nSignal == OVERLAY_CAPTURE_START_EVENT)
    {
        do
        {
            if(ISSTATEPAUSED || (!ISSTATEOPENED && !m_bPaused))
            {
                break;//out of while loop for encode --> send
            }

            //1: Get a mediabuffer from Q
            overlayFrameInfoType sFrameInfo;
            overlayFrameInfoType sPrevFrameInfo;
            memset(&sPrevFrameInfo, 0, sizeof(sPrevFrameInfo));
            memset(&sFrameInfo,0,sizeof(sFrameInfo));

            int nRet = AccessLatestBufQ(true,&sFrameInfo);

            if(nRet == OC_READ_NEW_FRAME)
            {

                //2: Encode and Send
                WFDMMLOGM("ipDeliveryThread(): New MediaBuffer found. Give for encoding");
                memcpy(&sPrevFrameInfo, &sFrameInfo, sizeof(sPrevFrameInfo));

                OMX_TICKS nStartTime; GetCurTime(nStartTime);
                OMX_TICKS nCurrTime;
                OMX_ERRORTYPE eErr = pngEncodeNSend(&sPrevFrameInfo);

                while(eErr == OMX_ErrorIncorrectStateOperation)
                {
                    MM_Timer_Sleep(5);

                    if(ISSTATEPAUSED || (!ISSTATEOPENED && !m_bPaused))
                    {
                        break;
                    }

                    int nRet = AccessLatestBufQ(true,&sFrameInfo);

                    if(nRet == OC_READ_NEW_FRAME)
                    {
                       ((MediaBuffer *)(sPrevFrameInfo.pBuffer))->release();
                        memcpy(&sPrevFrameInfo, &sFrameInfo, sizeof(sFrameInfo));
                        eErr = pngEncodeNSend(&sPrevFrameInfo);
                    }
                    else
                    {
                        GetCurTime(nCurrTime);
                        if(nCurrTime - nStartTime > 500000)
                        {
                            eErr = pngEncodeNSend(&sPrevFrameInfo);
                        }
                    }
                }

                if(eErr == OMX_ErrorNone)
                {
                    //2.5: safety removal
                    removePrevOverlays();
                }

                //3: Free the mediabuffer
                ((MediaBuffer *)(sPrevFrameInfo.pBuffer))->release();
            }
        }while(0);
    }
    else if (nSignal == OVERLAY_CAPTURE_STOP_EVENT)
    {
        WFDMMLOGE("OVERLAY_DELIVERY_STOP");
    }

EXIT:
    WFDMMLOGE("ipDeliveryThread() - Adios");
    return result;
}

/*==============================================================================

         FUNCTION:              CaptureThreadEntry

         DESCRIPTION:
*//**       @brief              entry point for capture thread
*//**
@par     DEPENDENCIES:
                                None
*//*
         PARAMETERS:
*//*         @param
                                None

*//*     RETURN VALUE:
*//**       @return
                                None

@par     SIDE EFFECTS:
                                None
*//*==========================================================================*/
void WFDMMSourceOverlaySource::captureThreadEntry(void *pThreadData,
                                                  unsigned int signal)
{
    if (pThreadData)
    {
        WFDMMLOGH("captureThreadEntry(): CaptureThreadEntry");

        ((WFDMMSourceOverlaySource*)pThreadData)->captureThread(signal);
    }
    else
    {
        WFDMMLOGE("captureThreadEntry(): Null data passed in CaptureThreadEntry");
    }
    return;
}

/*==============================================================================

         FUNCTION:              CaptureThread

         DESCRIPTION:
*//**       @brief              main body for Capture Thread execution
*//**
@par     DEPENDENCIES:
                                None
*//*
         PARAMETERS:
*//*         @param
                                None

*//*     RETURN VALUE:
*//**       @return
                                OMX_ErrorNone  or error
@par     SIDE EFFECTS:
                                None
*//*==========================================================================*/
OMX_ERRORTYPE WFDMMSourceOverlaySource::captureThread(int nSignal)
{
    OMX_ERRORTYPE result = OMX_ErrorNone;
    size_t     readBytes = 0;
    int         duration = 0;
    int              ret = 0;
    OMX_TICKS beforeRead = 0;
    OMX_TICKS afterRead  = 0;
    OMX_TICKS diffRead   = 0;

    WFDMMLOGE("WFDMMSourceOverlaySource::CaptureThread");

    /*--------------------------------------------------------------------------
    If ever rechristening of this thread is implemented bear in mind that
    there is an upper limit to the name (as of now 16)
    ----------------------------------------------------------------------------
    */
    ret = pthread_setname_np(pthread_self(),"WFD_OCAP_Thread");
    if(ret)
    {
        WFDMMLOGE2("captureThread(): Failed to set thread name due to %d %s",
                    ret, strerror(errno));
        //No need to panic
    }

    if(nSignal == OVERLAY_CAPTURE_START_EVENT)
    {
        WFDMMLOGE("OVERLAY_CAPTURE_START");
        MediaBuffer          *pMediaBuffer = NULL;
        overlayFrameInfoType sCaptureBuffer;

        sCaptureBuffer.pBuffer = (uint8*)pMediaBuffer;
        sCaptureBuffer.isRead  = false;
        sCaptureBuffer.isValid = true;

        while(1)
        {
            GetCurTime(beforeRead);
            int ret = m_pSurfaceMediaSrc->read(&pMediaBuffer);
            GetCurTime(afterRead);
            diffRead = afterRead - beforeRead;

            if(pMediaBuffer && ret == 0)
            {
                if(m_bPaused || !ISSTATEOPENED)
                {
                    pMediaBuffer->release();
                }
                else
                {
                    WFDMMLOGM1("captureThread(): refcount = %d",pMediaBuffer->refcount());
                    WFDMMLOGM1("captureThread(): size = %d",pMediaBuffer->size());
                    WFDMMLOGM2("captureThread(): rangeOffset = %d range_length = %d",
                    pMediaBuffer->range_offset(),pMediaBuffer->range_length());

                    /* Check if orientation is correct to send overlay */
                    if(!isLandscapeOrientation())
                    {
                        pMediaBuffer->release();
                        continue;
                    }

                    WFDMMLOGM3("captureThread():m_pSurfaceMediaSrc->read returned %d %p in %lld",
                        ret, pMediaBuffer, diffRead);

                    sCaptureBuffer.pBuffer = (uint8*)pMediaBuffer;

                    OMX_TICKS nTime = 0;
                    GetCurTime(nTime);
                    sCaptureBuffer.nCaptureTimeStamp = (OMX_U64)nTime;

                    /* Let's keep on pushing all buffers from SMS in a Q
                       and keep track of the time*/
                    ret = AccessLatestBufQ(false, &sCaptureBuffer);

                    /* Post signal on Delivery thread to send a frame */
                    if(m_hInputDeliveryThread)
                    {
                        m_hInputDeliveryThread->SetSignal
                                                (OVERLAY_CAPTURE_START_EVENT);
                    }
                }
            }
            else
            {
                /*--------------------------------------------------------------
                SurfaceMediaSource returning an error usually indicates that
                the producer has disconnected from its BufferQ (when surface
                has been removed), which will normally occur on teardown.
                SMS is returning EOS probably, so allow a small sleep for
                context switching
                ----------------------------------------------------------------
                */
                MM_Timer_Sleep(5);
            }
            if(!ISSTATEOPENED && !m_bPaused)
            {
                break;//out of while loop for SMS-> read
            }
        }
    }
    else if (nSignal == OVERLAY_CAPTURE_STOP_EVENT)
    {
        WFDMMLOGE("OVERLAY_CAPTURE_STOP_EVENT");
        if(m_pSurfaceMediaSrc.get() != NULL)
        {
            m_pSurfaceMediaSrc->stop();
        }
        WFDMMLOGH("captureThread(): StopOverlayCapture Done");
    }

EXIT:
    return result;
}

/*==============================================================================

         FUNCTION:          SetFreeBuffer

         DESCRIPTION:
*//**       @brief          Provides a free o/p buffer so that it can be
                                queue back to OMX Decoder
*//**
@par     DEPENDENCIES:
                            None
*//*
         PARAMETERS:
*//**       @param          None

*//*     RETURN VALUE:
*//**       @return
                            OMX_ErrorNone or other error
@par     SIFE EFFECTS:
                            None
*//*==========================================================================*/
OMX_ERRORTYPE WFDMMSourceOverlaySource::SetFreeBuffer(
                                                OMX_BUFFERHEADERTYPE *pBuffer,
                                                OMX_BOOL              bForce)
{
    OMX_ERRORTYPE eErr = OMX_ErrorUndefined;

    if (!pBuffer)
    {
        WFDMMLOGE("SetFreeBuffer(): Invalid Buffer Pointer in SetFreeBuffer");
        return OMX_ErrorBadParameter;
    }

    if(ISSTATEINIT || ISSTATEOPENED || bForce)
    {
        /*----------------------------------------------------------------------
        This module is the rightful owner of the buffer at death ot stop of
        the module. So hold the buffer in the collectorQ
        ------------------------------------------------------------------------
        */
        WFDMMLOGH("SetFreeBuffer(): Push buffer back to InputQ");
        m_pMuxInputQ->Push(&pBuffer, sizeof(&pBuffer));
        return OMX_ErrorNone;
    }
    else
    {
        /*----------------------------------------------------------------------
        Buffer provided in an invalid state return to caller
        ------------------------------------------------------------------------
        */
        WFDMMLOGE1("SetFreeBuffer(): Buffer provided in invalid state[%d]. Ignore",m_nState);
        m_pMuxInputQ->Push(&pBuffer, sizeof(&pBuffer));

        //We are the owners of all buffers. Do not report any error!
        //return OMX_ErrorInvalidState;
    }

    return OMX_ErrorNone;
}

/*==============================================================================

         FUNCTION:          releaseMediaBuffers

         DESCRIPTION:
*//**       @brief          Helper method to release all instances of
                                MediaBuffers held by this module outside
                                the Video InputQ

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
void WFDMMSourceOverlaySource::releaseMediaBuffers()
{
    /*--------------------------------------------------------------------------
     Release any mediabuffer references held in any queue
    ----------------------------------------------------------------------------
    */
    WFD_OVERLAY_CRITICAL_SECT_ENTER(m_hQCritSect);

    WFDMMLOGH("releaseMediaBuffers(): releasing Buffers");
    for(int i = 0; i < OVERLAY_CAPTURE_NUM_BUFF; i++)
    {
        if(m_sLatestFrameQ[i].isValid && !m_sLatestFrameQ[i].isRead)
        {
            ((MediaBuffer*)m_sLatestFrameQ[i].pBuffer)->release();
            m_sLatestFrameQ[i].isValid = false;
        }
    }

    WFD_OVERLAY_CRITICAL_SECT_LEAVE(m_hQCritSect);
}

/*==============================================================================

         FUNCTION:          state

         DESCRIPTION:
*//**       @brief          sets or gets the state. This makes state transitions
                            thread safe
*//**
@par     DEPENDENCIES:
                            None
*//*
         PARAMETERS:
*//**       @param          None

*//*     RETURN VALUE:
*//**       @return
                            int state when get else a Dont Care
@par     SIFE EFFECTS:
                            None
*//*==========================================================================*/
int WFDMMSourceOverlaySource::state(int state, bool get_true_set_false)
{
    WFD_OVERLAY_CRITICAL_SECT_ENTER(m_hStateCritSect);

    if(get_true_set_false == true)
    {
        /*----------------------------------------------------------------------
          Just return the current state
        ------------------------------------------------------------------------
        */
    }
    else
    {
        m_nState = state;
        WFDMMLOGE1("state(): WFDMMSourceOverlaySource Moved to state %d",state);
    }

    WFD_OVERLAY_CRITICAL_SECT_LEAVE(m_hStateCritSect);

    return (int)m_nState;
}

/*==============================================================================

         FUNCTION:          releaseVDS

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

@par     SIFE EFFECTS:
                            None
*//*==========================================================================*/
void WFDMMSourceOverlaySource::releaseVDS()
{
    WFDMMLOGH("releaseVDS(): Release VDS created for overlay module");
    if(m_dpy != NULL)
    {
        /*----------------------------------------------------------------------
        Stop the producer before destroying the consumer. Surface
        Media Source hijacks the display destroyed event realizing that
        prodcuer is disconnecting from the BufferQueue and that it should stop
        the recording. This will prevent calling stop on SMS from hanging.
        ------------------------------------------------------------------------
        */
        m_pBuffProducer= NULL;
        SurfaceComposerClient::destroyDisplay(m_dpy);
        WFDMMLOGH("releaseVDS(): Done with display destruction");
        m_dpy = NULL;
    }
}

/*==============================================================================

         FUNCTION:          isLandscapeOrientation

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

@par     SIFE EFFECTS:
                            None
*//*==========================================================================*/
bool WFDMMSourceOverlaySource::isLandscapeOrientation()
{
    int err = 0;

    if(m_mainDpy == NULL)
    {
        WFDMMLOGE("isLandscapeOrientation(): Main Display handle is NULL");
        return false;
    }

    DisplayInfo mainDpyInfo;
    err = SurfaceComposerClient::getDisplayInfo(m_mainDpy, &mainDpyInfo);

    if (err)
    {
        WFDMMLOGE1("isLandscapeOrientation(): Can't get display characteristics %d\n", err);
        return OMX_ErrorUndefined;
    }

    bool deviceRotated = mainDpyInfo.orientation == DISPLAY_ORIENTATION_90 ||
            mainDpyInfo.orientation == DISPLAY_ORIENTATION_270;

    if(!deviceRotated)
    {
        WFDMMLOGL("isLandscapeOrientation():Incorrect mode to capture overlay");

        //Clear overlays
        transferPrevOverlays();
        removePrevOverlays();

        return false;
    }
    else
    {
        //Check if reConfiguration of VDS is needed
        if(m_nCaptureHeight == 0 || m_nCaptureWidth == 0)
        {
            m_nCaptureHeight = mainDpyInfo.h;
            m_nCaptureWidth = mainDpyInfo.w;
            configureVirtualDisplay();
        }
    }
    return true;
}

/*==============================================================================

         FUNCTION:          configureVirtualDisplay

         DESCRIPTION:
*//**       @brief          Configures the virtual display
*//**
@par     DEPENDENCIES:
                            None
*//*
         PARAMETERS:
*//**       @param          None

*//*     RETURN VALUE:
*//**       @return         OMX_ERRORTYPE

@par     SIFE EFFECTS:
                            None
*//*==========================================================================*/
OMX_ERRORTYPE WFDMMSourceOverlaySource::configureVirtualDisplay()
{
    OMX_ERRORTYPE result = OMX_ErrorNone;
    int err = 0;

    if(m_mainDpy == NULL)
    {
        WFDMMLOGE("configureVDS(): Main Display handle is NULL");
        return OMX_ErrorInsufficientResources;
    }

    DisplayInfo mainDpyInfo;
    err = SurfaceComposerClient::getDisplayInfo(m_mainDpy, &mainDpyInfo);

    if (err)
    {
        WFDMMLOGE1("configureVDS(): Can't get display characteristics %d\n", err);
        return OMX_ErrorUndefined;
    }

    /*--------------------------------------------------------------------------
    Input rectangle settings:

    inputRect is the region of primary that needs to be captured,
    which for WFD is the entire screen. Set the correct value based
    on the orientation.
    ----------------------------------------------------------------------------
    */

    bool deviceRotated = mainDpyInfo.orientation == DISPLAY_ORIENTATION_90 ||
            mainDpyInfo.orientation == DISPLAY_ORIENTATION_270;

    if(!mainDpyInfo.h || !mainDpyInfo.w)
    {
        WFDMMLOGE("configureVDS(): Failed to get Display width/height");
        return OMX_ErrorUndefined;
    }

    uint32_t sourceWidth, sourceHeight;
    if(!deviceRotated)
    {
        sourceHeight = mainDpyInfo.h;
        sourceWidth = mainDpyInfo.w;
    }
    else
    {
        sourceHeight = mainDpyInfo.w;
        sourceWidth = mainDpyInfo.h;
    }

    Rect inputRect(sourceWidth, sourceHeight);
    WFDMMLOGH2("configureVDS(): Source Width = %d , Height = %d ", sourceWidth,sourceHeight);

    /*--------------------------------------------------------------------------
    Output rectangle calculations:

    outputRect is the output area for display.Set it equal to the negotiated
    width and height for overlay
    ----------------------------------------------------------------------------
    */
    Rect outputRect(0, 0, m_sConfig.frameWidth, m_sConfig.frameHeight);

    if(m_dpy == NULL)
    {
        m_dpy= SurfaceComposerClient::createDisplay(
            String8("wfd-overlay"), static_cast<bool>(m_sConfig.IsHdcp));

        if(m_dpy.get() == NULL)
        {
            WFDMMLOGE("configureVDS(): Unable to create Virtual Display");
            return OMX_ErrorInsufficientResources;
        }
    }

    SurfaceComposerClient::openGlobalTransaction();
    SurfaceComposerClient::setDisplaySurface(m_dpy, m_pBuffProducer);
    SurfaceComposerClient::setDisplayProjection(m_dpy,
                                                DISPLAY_ORIENTATION_0,
                                                inputRect, outputRect);
    SurfaceComposerClient::setDisplayLayerStack(m_dpy, 0);
    SurfaceComposerClient::closeGlobalTransaction();

    WFDMMLOGH("configureVDS(): Done with display configuration");

    return OMX_ErrorNone;
}

/*==============================================================================

         FUNCTION:          createVirtualDisplay

         DESCRIPTION:
*//**       @brief          Creates native virtual display
*//**
@par     DEPENDENCIES:
                            None
*//*
         PARAMETERS:
*//**       @param          None

*//*     RETURN VALUE:
*//**       @return         OMX_ERRORTYPE

@par     SIFE EFFECTS:
                            None
*//*==========================================================================*/
OMX_ERRORTYPE WFDMMSourceOverlaySource::createVirtualDisplay()
{
    OMX_ERRORTYPE result = OMX_ErrorNone;
    int err = 0;

    WFDMMLOGH("createVirtualDisplay(): Creating native VD for overlay capture");
    if(m_pBuffProducer != NULL)
    {
        sp<ProcessState> self = ProcessState::self();
        self->startThreadPool();

        m_mainDpy =
            SurfaceComposerClient::getBuiltInDisplay(ISurfaceComposer::eDisplayIdMain);

        if(m_mainDpy != NULL)
        {
            result = configureVirtualDisplay();
            if(result != OMX_ErrorNone)
            {
                WFDMMLOGE("createVDS(): Failed to configure VDS");
                return result;
            }
        }
        else
        {
            WFDMMLOGE("createVDS(): Failed to get display characteristics");
            return OMX_ErrorInsufficientResources;
        }
    }
    else
    {
        WFDMMLOGE("createVDS(): m_pBufferProducer is NULL!");
        return OMX_ErrorUndefined;
    }

    return OMX_ErrorNone;
}

/*==============================================================================

         FUNCTION:          pngEncodeNSend

         DESCRIPTION:
*//**       @brief          Encodes the Media Buffer to png and delivers to MUX
*//**
@par     DEPENDENCIES:
                            None
*//*
         PARAMETERS:
*//**       @param          None

*//*     RETURN VALUE:
*//**       @return

@par     SIFE EFFECTS:
                            None
*//*==========================================================================*/
OMX_ERRORTYPE WFDMMSourceOverlaySource::pngEncodeNSend(overlayFrameInfoType* pBufferInfo)
{
    OMX_ERRORTYPE result = OMX_ErrorNone;

    OMX_TICKS beforeRead = 0;
    OMX_TICKS afterRead  = 0;
    OMX_TICKS diffRead   = 0;

    bool isFmtCorrect    = false;

    unsigned int nNumRegions = 0;
    imageRegionInfoNodeType *pRegions = NULL;
    imageRegionInfoNodeType *pCurrRegion = NULL;

    // Extract the pvtHandle from the gralloc buffer
    private_handle_t *pPvtHandle = (private_handle_t *)
        extractPvtHndlFrmMediaBfr((void *)pBufferInfo->pBuffer);

    //Check if the raw buffer received is in correct format
    switch (pPvtHandle->format)
    {
        case HAL_PIXEL_FORMAT_RGBA_8888:
        case HAL_PIXEL_FORMAT_RGBX_8888:
        case HAL_PIXEL_FORMAT_BGRA_8888:
            isFmtCorrect = true;
            WFDMMLOGM1("pngEncodeNSend(): Raw Buffer format is [%d] supported",
                pPvtHandle->format);
        break;

        default:
            isFmtCorrect = false;
            WFDMMLOGM1("pngEncodeNSend(): Raw Buffer format is [%d] unsupported",
                pPvtHandle->format);
        break;
    }

    if(!isFmtCorrect)
    {
        WFDMMLOGE("pngEncodeNSend(): Unable to encode due to incorrect format");
        return OMX_ErrorUndefined;
    }

#if 0
        /*FEATURE: Dumping RAW data in file*/
        static int index = 0;
        char sOutName[512];
        snprintf(sOutName, 512, "%s%d.raw", "/data/media/pngRAW", index++);
        FILE *fPNGTempFile = fopen(sOutName,"wb");

        if(fPNGTempFile == NULL)
        {
            WFDMMLOGE("pngEncodeNSend(): File open failed for fPNGTempFile");
            //return OMX_ErrorInsufficientResources;
        }
        else
        {
            fwrite((unsigned char *)pPvtHandle->base,
                    sizeof(char),
                    m_sConfig.frameHeight * m_sConfig.frameWidth * 4,
                    fPNGTempFile);
            fclose(fPNGTempFile);
         }
#endif

    //Get the list of all non-transperent regions in the buffer received
    bool bTransparent = false;
    unsigned int nPercentageTransparency = 0;

    if(m_pImgEncoder)
    {
        int res;

        res = m_pImgEncoder->GetNonTransparentRegions(6,//:TODO: This would come as part of RTSP negotiation
                                                (unsigned char *)pPvtHandle->base,
                                                &nNumRegions,
                                                &pRegions,
                                                &bTransparent,
                                                &nPercentageTransparency);
        if(res < 0)
        {
            WFDMMLOGE("pngEncodeNSend(): Unable to perform GetNonTransparentRegions");
            //:TODO: Should we bail out or encode the complete buffer
            //return OMX_ErrorUndefined;
        }

        OMX_TICKS nTime;
        GetCurTime(nTime);

        if(bTransparent)
        {
             WFDMMLOGE("pngEncodeNSend(): Transparent");
        }

        WFDMMLOGM1("pngEncodeNSend(): Percentage Transparency %ld", nPercentageTransparency);

        if((!m_bFirstFrameSent && bTransparent) ||
           (m_bPrevTransparent && bTransparent) ||
           (!m_bPrevTransparent && !bTransparent && ((nTime - m_nLastTimeStamp) < 200000)) ||
           (nPercentageTransparency < 50))
        {

            WFDMMLOGE("pngEncodeNSend(): Not transmitting this overlay");

            //Clear up
            pCurrRegion = pRegions;
            imageRegionInfoNodeType *pTmpNodePtr = NULL;

            while(pCurrRegion != NULL)
            {
                pTmpNodePtr = pCurrRegion;
                pCurrRegion = pCurrRegion->pNext;
                MM_Free(pTmpNodePtr);
            }

            if((nPercentageTransparency < 50) ||
               (!m_bFirstFrameSent && bTransparent) ||
               (m_bPrevTransparent && bTransparent))
            {
                return OMX_ErrorUndefined;
            }

            return OMX_ErrorIncorrectStateOperation;
        }
        else
        {
            transferPrevOverlays();
        }
    }
    else
    {
        WFDMMLOGE("pngEncodeNSend(): Encoder not found!");
        return OMX_ErrorInsufficientResources;
    }

    m_bFirstFrameSent = true;
    GetCurTime(m_nLastTimeStamp);

    //We now have the complete LinkedList of images
    pCurrRegion = pRegions;

    WFDMMLOGH1("pngEncodeNSend(): Total regions for encode in buffer = %d",nNumRegions);

    if(nNumRegions == 0)
    {
        //This is a complete transperent buffer. This will not be encoded and sent
        return OMX_ErrorNone;
    }

    imageRegionInfoNodeType *pTmpNodePtr = NULL;

    //Pick up each non-transperent region -> Encode -> Send
    if(bTransparent)
    {
        pBufferInfo->nCaptureTimeStamp += m_nEncodeTime;
        m_bPrevTransparent = true;
        removePrevOverlays();
    }

    while(pCurrRegion != NULL)
    {
        if(!bTransparent && (!(pCurrRegion->nHeight == 36 && pCurrRegion->nWidth == 1280)))
        {
            m_bPrevTransparent = false;
            pngEncodeNSendHelper(pCurrRegion, pBufferInfo->nCaptureTimeStamp);
        }
        pTmpNodePtr = pCurrRegion;
        pCurrRegion = pCurrRegion->pNext;
        MM_Free(pTmpNodePtr);
    }

#ifdef OVERLAY_STATISTICS
    m_nStatCounter++;

    if(m_nStatCounter == OVERLAY_STATS_MAX_SLOTS)
    {
        WFDMMLOGE2("OverlayStatistics: Avg encoding time after %d frames: %llu",
            m_nStatCounter, m_nAvgEncodeTime/m_nStatCounter);

        m_nAvgEncodeTime = 0;
        m_nStatCounter = 0;
    }
#endif

    if(!bTransparent)
    {
        GetCurTime(m_nEncodeTime);
        m_nEncodeTime -= m_nLastTimeStamp; //m_nEncodeTime is not thread safe.
    }

    WFDMMLOGH("pngEncodeNSend(): exit");
    return result;
}

/*==============================================================================

         FUNCTION:          pngEncodeNSendHelper

         DESCRIPTION:
*//**       @brief          Encodes the region and sends to MUX
*//**
@par     DEPENDENCIES:
                            None
*//*
         PARAMETERS:
*//**       @param

*//*     RETURN VALUE:
*//**       @return

@par     SIFE EFFECTS:
                            None
*//*==========================================================================*/
OMX_ERRORTYPE WFDMMSourceOverlaySource::pngEncodeNSendHelper(
                                              imageRegionInfoNodeType* pRegion,
                                              OMX_U64 nTimeStamp)
{
    //Local variables
    OMX_ERRORTYPE result = OMX_ErrorNone;
    OMX_TICKS beforeRead = 0;
    OMX_TICKS afterRead  = 0;
    OMX_TICKS diffRead   = 0;
    pvtHeaderInfoType pvtHdrInfo;

    unsigned int nLen = pRegion->nHeight * pRegion->nWidth * 4;

    pvtHdrInfo.bActivePresentation = true;
    pvtHdrInfo.overlayID = m_nOverlayID;
    pvtHdrInfo.topLeftX = pRegion->x0;
    pvtHdrInfo.topLeftY = pRegion->y0;
    pvtHdrInfo.height = pRegion->nHeight;
    pvtHdrInfo.width = pRegion->nWidth;

    //Pull one buffer out of MuxQ for holding encoded data
    OMX_BUFFERHEADERTYPE* pOPBufferHeader = NULL;
    result = m_pMuxInputQ->Pop(&pOPBufferHeader, sizeof(pOPBufferHeader),100);

    if(result != OMX_ErrorNone)
    {
        WFDMMLOGE1("pngEncodeHelper(): Failed to pop from MuxInputQ [%x]",result);
        return result;
    }

    //Attach Pvt Header
    attachOverlayPvtHeader(pOPBufferHeader->pBuffer,&pvtHdrInfo);

    //Encode
    if(m_pImgEncoder)
    {
        GetCurTime(beforeRead);
        m_pImgEncoder->Encode(pRegion, 100,
                              (unsigned char *)(pOPBufferHeader->pBuffer + OVERLAY_PVT_HEADER_SIZE),
                              &nLen);
        GetCurTime(afterRead);

        diffRead = afterRead - beforeRead;

        WFDMMLOGM2("pngEncodeHelper(): Time for encode [%lld]. ID[%d]",
                                        diffRead, (uint8)(m_nOverlayID - 1));

#ifdef OVERLAY_STATISTICS
        m_nAvgEncodeTime += diffRead;
#endif

        //Fill up required fields
        pOPBufferHeader->nTimeStamp     = nTimeStamp;
        pOPBufferHeader->nOffset        = 0;
        pOPBufferHeader->nFilledLen     = nLen + OVERLAY_PVT_HEADER_SIZE;

        //Remove Previous overlays if present
        WFDMMLOGM("pngEncodeHelper(): Remove previous overlays after 1st encoding");
        m_nSendBuffTS = nTimeStamp;
        removePrevOverlays();

        //Send Buffer to Mux
        if(m_pFrameDeliveryFn)
        {
            //Maintain this overlayID info
            updateActiveIDList(m_nOverlayID);

            //Increement the ID
            m_nOverlayID += 1;

            //Taking care of wrap around
            if(m_nOverlayID == 0)
            {
                WFDMMLOGH("pngEncodeHelper(): Taking care of overlay wrap-around");
                m_nOverlayID = 1;
            }

            WFDMMLOGM2("pngEncodeHelper(): Sending overlay data to MUXER @ nTimeStamp[%lld] nFilledLen[%ld]",
                pOPBufferHeader->nTimeStamp, nLen + OVERLAY_PVT_HEADER_SIZE);
            m_pFrameDeliveryFn(pOPBufferHeader, m_pAppData);
        }
    }
    else
    {
        WFDMMLOGE("pngEncodeHelper(): Failed to find encoder");
        return OMX_ErrorInsufficientResources;
    }

    return result;
}

/*==============================================================================

         FUNCTION:          extractPvtHndlFrmMediaBfr

         DESCRIPTION:
*//**       @brief          Extracts the pvt Handle from MediaBuffer
*//**
@par     DEPENDENCIES:
                            None
*//*
         PARAMETERS:
*//**       @param          None

*//*     RETURN VALUE:
*//**       @return

@par     SIFE EFFECTS:
                            None
*//*==========================================================================*/
void* WFDMMSourceOverlaySource::extractPvtHndlFrmMediaBfr(void* pMediaBuffer)
{
    buffer_handle_t *pBufferHandle;
    uint8 *pData = (uint8*)(((MediaBuffer*)pMediaBuffer)->data());

    unsigned long nBufferHandle = 0;

    for(int i = 0 ; i < sizeof(pBufferHandle) ; i++)
    {
        nBufferHandle |= (unsigned long) pData[i + 4] << (i * 8);
    }

    pBufferHandle = (buffer_handle_t *)nBufferHandle;
    private_handle_t *pPvtHandle = (private_handle_t*)pBufferHandle;

    return pPvtHandle;
}

/*==============================================================================

         FUNCTION:          attachOverlayPvtHeader

         DESCRIPTION:
*//**       @brief          Fills the private PNG header on top of every buffer
*//**
@par     DEPENDENCIES:
                            None
*//*
         PARAMETERS:
*//**       @param          None

*//*     RETURN VALUE:
*//**       @return

@par     SIFE EFFECTS:
                            None
*//*==========================================================================*/
bool WFDMMSourceOverlaySource::attachOverlayPvtHeader(OMX_U8* pBuffer,
                                                      pvtHeaderInfoType *pvtHdrInfo)
{
    if(pvtHdrInfo == NULL)
    {
        WFDMMLOGE("attachOverlayPvtHeader(): pvtHdrInfo is NULL");
        return false;
    }

    uint8* PVT_Header;
    int index = 0;

    PVT_Header = (uint8*)MM_Malloc(OVERLAY_PVT_HEADER_SIZE);
    if(PVT_Header == NULL)
    {
        WFDMMLOGE("attachOverlayPvtHeader(): Failed to allocate memory");
        return false;
    }

    memset(PVT_Header, 0, OVERLAY_PVT_HEADER_SIZE);

    //PVT_Header

    // 1 byte
    // 8 bits: Overlay ID
    PVT_Header[index] |= (uint8)pvtHdrInfo->overlayID;
    index += 1;

    // 1 byte
    // 2 bits: presentation_mode, 1bit: data_present_flag, 5 bits: Reserved
    if(pvtHdrInfo->bActivePresentation == true)
    {
        PVT_Header[index] |= 0x40;  /*Active Content[01000000]*/
        PVT_Header[index] |= 0x20;  /*data_present = true*/
    }
    else
    {
        PVT_Header[index] |= 0x80; /*Deactivate content[10000000]*/
    }
    index += 1;

    // 2 bytes
    // 16 Bits: overlay_pos_top_left_x
    int x0 = pvtHdrInfo->topLeftX;
    PVT_Header[index] |= (uint8)((x0 >> 8) & 0xff);
    index += 1;
    PVT_Header[index] |= (uint8)(x0 & 0xff);
    index += 1;

    // 2 bytes
    // 16 bits: overlay_pos_top_left_y
    int y0 = pvtHdrInfo->topLeftY;
    PVT_Header[index] |= (uint8)((y0 >> 8) & 0xff);
    index += 1;
    PVT_Header[index] |= (uint8)(y0 & 0xff);
    index += 1;

    // 2 bytes
    // 16 bits: overlay_width
    int width = pvtHdrInfo->width;
    PVT_Header[index] |= (uint8)((width >> 8) & 0xff);
    index += 1;
    PVT_Header[index] |= (uint8)(width & 0xff);
    index += 1;

    // 2 bytes
    // 16 bits: overlay_height
    int height = pvtHdrInfo->height;
    PVT_Header[index] |= (uint8)((height >> 8) & 0xff);
    index += 1;
    PVT_Header[index] |= (uint8)(height & 0xff);
    index += 1;

    // 1 byte
    // 8 bits: overlay_z_layer
    PVT_Header[index] |= 0x00;
    index += 1;

    // 2 bytes
    // 4 bits: blend_alpha_selection, 4 bits: blend_mode_selection
    // 1 bit: const_alpha_flag, 2 bits: per_pixel_alpha_info, 5 bits: color_bitwidth
    PVT_Header[index] |= 0x10; /*00010000: per_pixel source alpha*/
    PVT_Header[index] |= 0x01; /*00000001: normal alpha blending*/
    index += 1;
    PVT_Header[index] |= 0x80; /*10000000: constant alpha is included*/
    PVT_Header[index] |= 0x20; /*00100000: straight per pixel alpha is embedded*/
    PVT_Header[index] |= 0x00; //:TODO: Fixme! What to write as 32?
    index += 1;

    // 1 byte
    // 8 bits: constant_alpha_value
    PVT_Header[index] |= 0x00;
    index += 1;

    // 2 bytes
    // 16 bits: perpixel_alpha_pointer
    PVT_Header[index] |= 0x00;
    index += 1;
    PVT_Header[index] |= 0x00;
    index += 1;

    // 2 bytes
    // 16 bits: perpixel_alpha_length
    PVT_Header[index] |= 0x00;
    index += 1;
    PVT_Header[index] |= 0x00;
    index += 1;


    //Attach the header to pBuffer
    memcpy(pBuffer,PVT_Header,OVERLAY_PVT_HEADER_SIZE);

    //Free the temporary allocations
    MM_Free(PVT_Header);

    return true;
}

/*==============================================================================

         FUNCTION:         GetBuffers

         DESCRIPTION:
*//**       @brief         This is the WFDMMOverlaySource Function used for
                           getting the input buffer.
*//**

@par     DEPENDENCIES:
                           None
*//*
         PARAMETERS:
*//**       @param         None


*//*     RETURN VALUE:
*//**       @return
                           OMX_BUFFERHEADERTYPE

@par     SIDE EFFECTS:

*//*==========================================================================*/
OMX_BUFFERHEADERTYPE** WFDMMSourceOverlaySource::GetBuffers()
{
    return m_pInputBuffers;
}

/*==============================================================================

         FUNCTION:         updateActiveIDList

         DESCRIPTION:
*//**       @brief         Appends the activeIDList with incoming overlayID.
*//**

@par     DEPENDENCIES:
                           None
*//*
         PARAMETERS:
*//**       @param         nOverlayID


*//*     RETURN VALUE:
*//**       @return
                           bool

@par     SIDE EFFECTS:

*//*==========================================================================*/
bool WFDMMSourceOverlaySource::updateActiveIDList(uint8 nOverlayID)
{
    //Note: nListHead always points to the next free slot available
    if(m_sActiveIDInfo.pActiveIDList == NULL)
    {
        WFDMMLOGE("updateActiveIDList(): List is NULL");
        return false;
    }

    if(m_sActiveIDInfo.nListHead == m_nMaxOverlayIDSupport)
    {
        WFDMMLOGE("updateActiveIDList(): No room to Q overlayID");
        return false;
    }

    if(m_sActiveIDInfo.nListHead == -1)
    {
        m_sActiveIDInfo.nListHead++;
    }

    WFDMMLOGM2("updateActiveIDList(): Queueing overlayID[%d] at pos[%d]",
        nOverlayID,m_sActiveIDInfo.nListHead);

    m_sActiveIDInfo.pActiveIDList[m_sActiveIDInfo.nListHead] = nOverlayID;
    m_sActiveIDInfo.nListHead += 1;

    return true;
}


/*==============================================================================

         FUNCTION:         transferPrevOverlays

         DESCRIPTION:
*//**       @brief         Transfers active IDs to Inactive List
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

*//*==========================================================================*/
void WFDMMSourceOverlaySource::transferPrevOverlays()
{
    if(m_sActiveIDInfo.nListHead == -1)
    {
        return;
    }

    int index = 0;
    int overlayID = 0;

    if(m_sActiveIDInfo.pActiveIDList == NULL)
    {
        WFDMMLOGE("transferPrevOverlays(): List is NULL");
        return;
    }

    //Loop through active overlayIDs
    for(index = 0; index <= m_sActiveIDInfo.nListHead -1; index++)
    {
        overlayID = m_sActiveIDInfo.pActiveIDList[index];
        WFDMMLOGM2("transferPrevOverlays(): index[%d] overlayID[%d]", index, overlayID);
        //Move this overlay ID to inActiveList
        if(overlayID > 0)
        {
            if(m_sActiveIDInfo.nInActiveListHead == -1)
                m_sActiveIDInfo.nInActiveListHead++;

            m_sActiveIDInfo.pInActiveIDList[m_sActiveIDInfo.nInActiveListHead] = overlayID;
            m_sActiveIDInfo.nInActiveListHead++;

            WFDMMLOGM2("transferPrevOverlays():MOVING index[%d] overlayID[%d]", index, overlayID);
        }
    }

    m_sActiveIDInfo.nListHead = -1;

    return;
}

/*==============================================================================

         FUNCTION:         removePrevOverlays

         DESCRIPTION:
*//**       @brief         Informs sink to flush out previous overlays
*//**

@par     DEPENDENCIES:
                           None
*//*
         PARAMETERS:
*//**       @param


*//*     RETURN VALUE:
*//**       @return         bool

@par     SIDE EFFECTS:

*//*==========================================================================*/
bool WFDMMSourceOverlaySource::removePrevOverlays()
{

    //Return if no overlays are present
    if(m_sActiveIDInfo.nInActiveListHead == -1)
    {
        return true;
    }

    int index = 0;
    int overlayID = 0;
    pvtHeaderInfoType pvtHdrInfo;
    OMX_ERRORTYPE result = OMX_ErrorNone;
    OMX_TICKS nTimeStamp;

    if(m_nSendBuffTS == 0)
    {
        //Get the TimeStamp to be attached to all these buffers
        GetCurTime(nTimeStamp);
        m_nSendBuffTS = (OMX_U64)nTimeStamp + m_nEncodeTime;
    }

    if(m_sActiveIDInfo.pInActiveIDList == NULL)
    {
        WFDMMLOGE("removePrevOverlays(): List is NULL");
        return false;
    }

    //Loop through active overlayIDs
    for(index = 0; index <= m_sActiveIDInfo.nInActiveListHead - 1; index++)
    {
        overlayID = m_sActiveIDInfo.pInActiveIDList[index];
        WFDMMLOGM2("removePrevOverlays(): index[%d] overlayID[%d]", index, overlayID);

        //Notify to shut down this overlayID
        if(overlayID > 0)
        {
            //Pull a buffer out of MuxQ
            OMX_BUFFERHEADERTYPE* pOPBufferHeader = NULL;
            result = m_pMuxInputQ->Pop(&pOPBufferHeader, sizeof(pOPBufferHeader),100);

            if(result != OMX_ErrorNone)
            {
                WFDMMLOGE1("removePrevOverlays(): Failed to pop from MuxInputQ [%x]",result);
                return false;
            }

            //Attach Pvt Header
            pvtHdrInfo.overlayID = overlayID;
            pvtHdrInfo.bActivePresentation = false;
            pvtHdrInfo.height = 0;
            pvtHdrInfo.width = 0;
            pvtHdrInfo.topLeftX = 0;
            pvtHdrInfo.topLeftY = 0;
            attachOverlayPvtHeader(pOPBufferHeader->pBuffer,&pvtHdrInfo);

            //There will not be any PNG data associated with this buffer

            //Fill up required fields
            pOPBufferHeader->nTimeStamp     = m_nSendBuffTS/* + m_nEncodeTime*/;
            pOPBufferHeader->nOffset        = 0;
            pOPBufferHeader->nFilledLen     = OVERLAY_PVT_HEADER_SIZE;

            //Send Buffer to Mux
            if(m_pFrameDeliveryFn)
            {
                m_pFrameDeliveryFn(pOPBufferHeader, m_pAppData);
            }
        }
    }

    //All Overlays are removed. Reset
    m_sActiveIDInfo.nInActiveListHead = -1;

    return true;
}
