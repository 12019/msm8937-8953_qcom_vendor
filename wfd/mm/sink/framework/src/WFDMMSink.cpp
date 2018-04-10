/*==============================================================================
*       WFDMMSink.cpp
*
*  DESCRIPTION:
*       WFDMMSink framework for multimedia in WFD Sink
*
*
*  Copyright (c) 2012 - 2016 Qualcomm Technologies, Inc.
*  All Rights Reserved.
*  Confidential and Proprietary - Qualcomm Technologies, Inc.
*===============================================================================
*/
/*==============================================================================
                             Edit History
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
#include "MMDebugMsg.h"
#include "MMThread.h"
#include "MMTimer.h"
#include "MMMalloc.h"
#include "MMCriticalSection.h"
#include "MMDebugMsg.h"
#include "MMSignal.h"
#include "WFDMMSink.h"
#include "WFDMMSinkCommon.h"
#include "MMMemory.h"
#include "WFDMMSinkMediaSource.h"
#include "WFDMMSinkVideoDecode.h"
#include "RTPStreamPort.h"
#ifdef USE_OMX_AAC_CODEC
#include "WFDMMSinkAudioDecode.h"
#endif
#include "WFDMMSinkAudioRenderer.h"
#include "WFDMMSinkVideoRenderer.h"
#include "WFDMMSinkGenericRenderer.h"
#include "WFDMMSinkStatistics.h"
#include "OMX_Core.h"
#include "WFDMMLogs.h"
#include "WFDMMThreads.h"
#include "HDCPManager.h"
#include "wfd_cfg_parser.h"

#include <stdlib.h>

#include <arpa/inet.h>
#include <sys/socket.h>
#include <linux/if.h>
#include "MMFile.h"

#include <binder/ProcessState.h>
#include "ISurfaceComposer.h"
#include "DisplayInfo.h"

#ifdef WFD_ICS
#include <surfaceflinger/Surface.h>
#else
#include <Surface.h>
#endif

#include "display_config.h"
#include "WFDUtils.h"

#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "WFDMMSINK"

#define SINK_MODULE_MEDIASOURCE       1001
#define SINK_MODULE_VIDEO_DECODER     2002
#define SINK_MODULE_AUDIO_DECODER     3003
#define SINK_MODULE_RENDERER          4004
#define SINK_MODULE_HDCP              5005
#define SINK_MODULE_SELF              6006

#define IDR_MIN_INTERVAL              500

#define DEINIT 0
#define INIT   1
#define PLAY   2
#define STOP   3
#define STREAM_PLAY 4
#define STREAM_PAUSE 5
#define STREAM_FLUSH 6
#define STREAM_SET_VOLUME 7
#define STREAM_SET_DECODER_LATENCY 8
#define RECONFIGURE_VIDEO_DECODER 9
#define RECONFIGURE_AUDIO_DECODER 10
#define RECONFIGURE_VIDEO_RENDERER 11
#define STATE_CONTROL_MAX 12

extern int MM_Memory_InitializeCheckPoint( void );
extern int MM_Memory_ReleaseCheckPoint( void );

#define FEATURE_CONFIG_FROM_FILE
/* =============================================================================

                              DATA DECLARATIONS

================================================================================
*/
/* -----------------------------------------------------------------------------
** Constant / Define Declarations
** -------------------------------------------------------------------------- */

/* -----------------------------------------------------------------------------
** Type Declarations
** -------------------------------------------------------------------------- */
//This threshhold should be tuned\r
//it depends on Video frame size as well, bit rate,packetization scheme\r
#define DEFINE_THRESHHOLD_NBYTES_DROP 7*188*15 // 7Ts packets in 15 frames
#define DEFINE_TIMER_INTERVAL 1000
/* -----------------------------------------------------------------------------
** Global Constant Data Declarations
** -------------------------------------------------------------------------- */

/* =============================================================================
*                       Local Function Definitions
* =========================================================================== */

/* -----------------------------------------------------------------------------
** Local Data Declarations
** -------------------------------------------------------------------------- */
WFDMMSink        *WFDMMSink::m_pMe = NULL;
unsigned          WFDMMSink::m_nRefCnt = 0;
/* =============================================================================
**                          Macro Definitions
** ========================================================================== */

// MM related error checks
#define SET_MM_ERROR_FAIL        (m_nMMStatus = WFD_STATUS_FAIL)
#define SET_MM_ERROR_BADPARAM    (m_nMMStatus = WFD_STATUS_BADPARAM)
#define SET_MM_ERROR_UNSUPPORTED (m_nMMStatus = WFD_STATUS_NOTSUPPORTED)
#define SET_MM_ERROR_RESOURCE    (m_nMMStatus = WFD_STATUS_MEMORYFAIL)
#define RESET_MM_ERROR           (m_nMMStatus = WFD_STATUS_SUCCESS)

// DRM Related Error Checks
#define SET_DRM_ERROR_FAIL        (m_nDrmStatus = WFD_STATUS_FAIL)
#define SET_DRM_ERROR_BADPARAM    (m_nDrmStatus = WFD_STATUS_BADPARAM)
#define SET_DRM_ERROR_UNSUPPORTED (m_nDrmStatus = WFD_STATUS_NOTSUPPORTED)
#define SET_DRM_ERROR_RESOURCE    (m_nDrmStatus = WFD_STATUS_MEMORYFAIL)
#define RESET_DRM_ERROR           (m_nDrmStatus = WFD_STATUS_SUCCESS)

// MM State management
#define RESET_MM_STATE            (m_eMMState = WFD_SINK_MM_STATE_ZERO)
#define SET_MM_STATE_DEINIT       (m_eMMState = WFD_SINK_MM_STATE_DEINIT)
#define SET_MM_STATE_PLAY         (m_eMMState = WFD_SINK_MM_STATE_PLAY)
#define SET_MM_STATE_PAUSE        (m_eMMState = WFD_SINK_MM_STATE_PAUSE)
#define SET_MM_STATE_INIT         (m_eMMState = WFD_SINK_MM_STATE_INIT)
#define SET_MM_STATE_RECONFIGURE  (m_eMMState = WFD_SINK_MM_STATE_RECONFIGURE)

#define IS_MM_STATE_PLAY          (m_eMMState == WFD_SINK_MM_STATE_PLAY)
#define IS_MM_STATE_INIT          (m_eMMState == WFD_SINK_MM_STATE_INIT)
#define IS_MM_STATE_PAUSE         (m_eMMState == WFD_SINK_MM_STATE_PAUSE)
#define IS_MM_STATE_DEINIT        (m_eMMState == WFD_SINK_MM_STATE_DEINIT)

#define ISMMTEARDOWNNEEDED        (m_eState == MMWFDSINK_STATE_INIT ||         \
                                   m_eState == MMWFDSINK_STATE_PLAY ||         \
                                   m_eState == MMWFDSINK_STATE_PAUSE)          \
/*      END ISMMTEARDOWNNEEDED     */

// Drm State management
#define RESET_DRM_STATE           (m_eDrmState = WFD_SINK_DRM_STATE_ZERO)
#define SET_DRM_STATE_INIT        (m_eDrmState = WFD_SINK_DRM_STATE_INIT)
#define SET_DRM_STATE_ACQUIRING   (m_eDrmState = WFD_SINK_DRM_STATE_ACQUIRING)
#define SET_DRM_STATE_ACQUIRED    (m_eDrmState = WFD_SINK_DRM_STATE_ACQUIRED)

#define IS_DRM_STATE_INIT         (m_eDrmState == WFD_SINK_DRM_STATE_INIT)
#define IS_DRM_STATE_ACQUIRING    (m_eDrmState == WFD_SINK_DRM_STATE_ACQUIRING)
#define IS_DRM_STATE_ACQUIRED     (m_eDrmState == WFD_SINK_DRM_STATE_ACQUIRED)
#define IS_DRM_STATE_ZERO         (m_eDrmState == WFD_SINK_DRM_STATE_ZERO)
#define IS_DRM_TEARDOWN_NEEDED    (m_eDrmState != WFD_SINK_DRM_STATE_ZERO)

#define CRITICAL_SECT_ENTER if(m_hCritSect)                                    \
                                  MM_CriticalSection_Enter(m_hCritSect);       \
/*      END CRITICAL_SECT_ENTER    */

#define CRITICAL_SECT_LEAVE if(m_hCritSect)                                    \
                                  MM_CriticalSection_Leave(m_hCritSect);       \
/*      END CRITICAL_SECT_LEAVE    */


#define CRITICAL_SECT_ENTER_FBD if(m_hCritSectFBD)                             \
                                  MM_CriticalSection_Enter(m_hCritSectFBD);    \
/*      END CRITICAL_SECT_ENTER    */

#define CRITICAL_SECT_LEAVE_FBD if(m_hCritSectFBD)                             \
                                  MM_CriticalSection_Leave(m_hCritSectFBD);    \
/*      END CRITICAL_SECT_LEAVE    */


#define CRITICAL_SECT_ENTER_EBD if(m_hCritSectEBD)                             \
                                  MM_CriticalSection_Enter(m_hCritSectEBD);    \
/*      END CRITICAL_SECT_ENTER    */

#define CRITICAL_SECT_LEAVE_EBD if(m_hCritSectEBD)                             \
                                  MM_CriticalSection_Leave(m_hCritSectEBD);    \
/*      END CRITICAL_SECT_LEAVE    */

#define CRITICAL_SECT_ENTER_EVT if(m_hCritSectEvtHdlr)                         \
                                  MM_CriticalSection_Enter(m_hCritSectEvtHdlr);\
/*      END CRITICAL_SECT_ENTER_EVT   */

#define CRITICAL_SECT_LEAVE_EVT if(m_hCritSectEvtHdlr)                         \
                                  MM_CriticalSection_Leave(m_hCritSectEvtHdlr);\
/*      END CRITICAL_SECT_LEAVE_EVT    */


#define RETURNUNSUPPORTED  {return WFD_STATUS_NOTSUPPORTED; }
#define RETURNNORESOURCES  {return WFD_STATUS_MEMORYFAIL;   }
#define RETURNSUCCESS      {return WFD_STATUS_SUCCESS;      }
#define RETURNFAIL         {return WFD_STATUS_FAIL;         }
#define RETURNBADPARAM     {return WFD_STATUS_BADPARAM;     }
#define RETURNSUCCESS      {return WFD_STATUS_SUCCESS;      }
#define RETURNBADSTATE     {return WFD_STATUS_RUNTIME_ERROR;}


// Memory Alloc Free Routines with Null checks
#define SINK_FREEIF(x)      if(x){MM_Free(x); x = NULL;}
#define SINK_MALLOC(x)      MM_Malloc(x)

#define CHECK_ERROR_STATUS if(m_nMMStatus != WFD_STATUS_SUCCESS){              \
                               WFDMMLOGE("Failed. Status Fail");               \
                               return m_nMMStatus;                             \
                           }                                                   \
/* END  CHECK_ERROR_STATUS  */

#define CHECK_ERROR_STATUS_CS if(m_nMMStatus != WFD_STATUS_SUCCESS){           \
                               WFDMMLOGE("Failed. Status Fail");               \
                               CRITICAL_SECT_LEAVE;                            \
                               return m_nMMStatus;                             \
                           }                                                   \
/* END  CHECK_ERROR_STATUS  */

#define CHECK_NULL_ERROR(x, y) if(x == NULL) {                                 \
        WFDMMLOGE(y);                                                          \
        return WFD_STATUS_FAIL;                                                \
}                                                                              \
/* END  CHECK_NULL_ERROR   */

#define DRM_HDCP_MIME_TYPE  "video/hdcp"

/* =============================================================================
**                            Function Definitions
** ========================================================================== */

/*==============================================================================

         FUNCTION:         WFDMMSink::CreateInstance

         DESCRIPTION:
*//**       @brief         This is the public API to create the single WFD Sink
                           instance.
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

*//*==========================================================================*/
WFDMMSink* WFDMMSink::CreateInstance()
{
    if(!m_pMe)
    {
        WFDMMLOGE("WFDMMSink::CreateInstance");
        m_pMe = MM_New(WFDMMSink);
        ++m_nRefCnt;

    }
    WFDMMLOGH1("WFDMMSink: Ref count = %u",m_nRefCnt);
    return m_pMe;
}

/*==============================================================================

         FUNCTION:         WFDMMSink::DeleteInstance

         DESCRIPTION:
*//**       @brief         This is the public API to delete the WFD Sink
                           instance.
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

*//*==========================================================================*/
void WFDMMSink::DeleteInstance()
{
    WFDMMLOGH1("Delete WFDMMSink, Instance count %d", (int)m_nRefCnt);

    if(m_pMe)
    {
        MM_Delete(m_pMe);
        m_pMe = NULL;
        --m_nRefCnt;
    }

    return;
}

/*==============================================================================

         FUNCTION:         WFDMMSink

         DESCRIPTION:
*//**       @brief         This is the WFDMMSink class constructor-initializes
                           the class members.
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

*//*==========================================================================*/
WFDMMSink::WFDMMSink()
{
    WFDMMLOGH("WFDMMSink constructor");
    MM_Memory_InitializeCheckPoint( );

    int nRet = 0;

    FILE *pDumpFile = fopen("/data/media/rtpdump.ts", "rb");
    if(pDumpFile != NULL)
    {
        fclose(pDumpFile);
        pDumpFile = NULL;
        if(MM_File_Delete("/data/media/rtpdump.ts") != 0)
            WFDMMLOGE("Caution!!Dump file not deleted.Previous dumps present");
    }

    RESET_MM_ERROR;
    RESET_DRM_ERROR;

    RESET_MM_STATE;
    RESET_DRM_STATE;

    nRet = MM_CriticalSection_Create(&m_hCritSect);
    if(nRet != 0)
    {
        m_hCritSect = NULL;
        WFDMMLOGE("Error in Critical Section Create");
    }

    nRet = MM_CriticalSection_Create(&m_hCritSectEBD);
    if(nRet != 0)
    {
        m_hCritSectEBD = NULL;
        WFDMMLOGE("Error in Critical Section Create for EBD");
    }


    nRet = MM_CriticalSection_Create(&m_hCritSectFBD);
    if(nRet != 0)
    {
        m_hCritSectFBD = NULL;
        WFDMMLOGE("Error in Critical Section Create for FBD");
    }

    nRet = MM_CriticalSection_Create(&m_hCritSectEvtHdlr);
    if(nRet != 0)
    {
        m_hCritSectEvtHdlr = NULL;
        WFDMMLOGE("Error in Critical Section Create for EvtHdlr");
    }

    m_nAudioSampleRate = 0;
    m_nAudioChannels = 0;
    m_pVideoSurface = NULL;
    m_pWindow = NULL;
    m_pHDCPManager = NULL;
    m_pAudioDecode = NULL;
    m_pVideoDecode = NULL;
    m_pMediaSrc    = NULL;
    m_pAudioRenderer = NULL;
    m_pVideoRenderer = NULL;
    m_pGenericRenderer = NULL;
    m_pSinkStatistics = NULL;
    m_bMMTearingDown = OMX_FALSE;
    m_nTimeLastIdrReq = 0;
    m_hIDRDelayTimer = NULL;
    m_bPendingIDRReq = false;
    m_bIDRTimerPending = false;
    m_nDecoderLatency = 0;
    m_nFlushTimestamp = 0;
    m_nActualBaseTime = 0;
    m_pRTPStreamPort = NULL;
    m_bUpdatedSurface = false;
    m_bStreamPaused = false;
    m_pNegotiatedCap = NULL;
    m_pExtNegotiatedCap = NULL;
    m_pAuxNegotiatedCap = NULL;
    m_nFrameCntDownStartTime = 0;
    m_bNotifyFirstFrameInfo = true;
    m_pCmdThread = MM_New_Args(WFDMMThreads,
                               ((unsigned int)STATE_CONTROL_MAX));
    m_bVideoReConfNeeded = false;

    if(!m_pCmdThread)
    {
        WFDMMLOGE("Failed to create Command Thread");
    }

    if(m_pCmdThread && m_pCmdThread->Start(CmdThreadEntry, -2, 32768 * 4,
                           this, "SinkCmdThread"))
    {
        WFDMMLOGE("Failed to start command thread");
    }


    m_pSinkStatistics = MM_New(WFDMMSinkStatistics);
    if(!m_pSinkStatistics)
    {
        WFDMMLOGE("Failed to create WFDMMSinkStatistics");//non-fatal
    }
    else
    {
        if(WFD_UNLIKELY(m_pSinkStatistics->setup()))
        {
            WFDMMLOGE("Failed to setup WFDMMSinkStatistics!!!");
            MM_Delete(m_pSinkStatistics);
            m_pSinkStatistics = NULL;
        }
    }

    SET_MM_STATE_DEINIT;
}

/*==============================================================================

         FUNCTION:          CmdThreadEntry

         DESCRIPTION:
*//**       @brief          Entry function for command thread
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
void WFDMMSink::CmdThreadEntry(void* pHandle,unsigned int nCmd)
{
    WFDMMSink *pMe = (WFDMMSink*)pHandle;

    if(!pMe)
    {
        WFDMMLOGE("Invalid Handle received");
        return;
    }

    return pMe->CmdThread(nCmd);
}

/*==============================================================================

         FUNCTION:          CmdThread

         DESCRIPTION:
*//**       @brief          Cmd thread which process commands
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
void WFDMMSink::CmdThread(int nCmd)
{
    OMX_ERRORTYPE eErr = OMX_ErrorNone;

    switch(nCmd)
    {
    case INIT:
        /*----------------------------------------------------------------------
          This is expected to come after SETUP. At this point we will allocate
          all resources needed by different modules and make them ready to start
          the session.
        ------------------------------------------------------------------------
        */
        if(IS_MM_STATE_INIT)
        {
            WFDMMLOGE("INIT when alreay in INIT");
            break;
        }
        if(IS_MM_STATE_PLAY)
        {
            WFDMMLOGE("INIT request in PLAY State");
            break;
        }

        /*----------------------------------------------------------------------
         Create the Surfacetextureclient here for this part of the session.
        ------------------------------------------------------------------------
        */
        if( m_pNegotiatedCap->pSurface != NULL )
        {
            WFDMMLOGH1("WFDMMSink: received surface from upper layer%p",
                       m_pNegotiatedCap->pSurface);
            m_pVideoSurface = (Surface*)m_pNegotiatedCap->pSurface;

            /*------------------------------------------------------------------
                 Having a native window is a prerequisite for calling ::play
            --------------------------------------------------------------------
            */
            if(m_pVideoSurface != NULL)
            {
#ifdef MR2
                m_pWindow = MM_New_Args(Surface,
                            (m_pVideoSurface->getIGraphicBufferProducer()));
#else
                m_pWindow = MM_New_Args(SurfaceTextureClient,
                                    (m_pVideoSurface->getSurfaceTexture()));
#endif
                if(m_pWindow == NULL)
                {
                    WFDMMLOGE("WFDMMSink Failed to create Native Window");
                    SET_MM_ERROR_FAIL;
                    WFDMMLOGE("MediaSource failed to start");
                    break;
                }
            }
        }
        else
        {
            WFDMMLOGE("WFDMMSink Surface is Null");
            SET_MM_ERROR_FAIL;
            return;
        }

        WFDMMLOGH("Notify Display HAL to Disable Dynamic refresh rate");
        qdutils::configureDynRefreshRate(
                           qdutils::DISABLE_METADATA_DYN_REFRESH_RATE, 0);
        /*----------------------------------------------------------------------
         Note: If the native window is created proceed with creating resources
         for each module
        ------------------------------------------------------------------------
        */
        if(WFD_LIKELY(m_pSinkStatistics))
        {
            m_pSinkStatistics->resetStats();
        }

        if(!createDataSource  () ||
           !createVideoDecoder() ||
           !createAudioDecoder() ||
           !createAudioRenderer() ||
           !createVideoRenderer() ||
           !createMediaSource ())
        {
            WFDMMLOGE("Failed to create MM components");
            if(!destroyMMComponents())
            {
                WFDMMLOGE("Failed to destroy MM components");
            }
            SET_MM_ERROR_FAIL;
            break;
        }
        if(!createGenericRenderer())
        {
            WFDMMLOGE("Failed to create Generic Renderer.. IGNORING");
        }

        WFDMMLOGH("WFDMMSINK Moves to INIT");
        SET_MM_STATE_INIT;

        break;


    case DEINIT:
        if(IS_MM_STATE_DEINIT)
        {
            WFDMMLOGE("DEINIT when already in DEINIT");
            if(!destroyMMComponents())
            {
                WFDMMLOGE("Failed to destroy MM components");
            }
            else
            {
                WFDMMLOGE("Success to destroy MM components");
            }
            break;
        }

        if(IS_MM_STATE_PLAY)
        {
            SET_MM_STATE_INIT;
            WFDMMLOGE("State In Play, Go to Init first");

            /*-----------------------------------------------------------------
                CHanged state to INIT, which means buffer flow will stop and
                buffers will be returned to final resting places of buffers.
                Calling renderer stop will make sure renderer doesnt hold any
                buffers.
                Call EBD and FDB once to make sure no buffers are midway
                while we are changing the state.
            -------------------------------------------------------------------
            */
            processEBD(SINK_MODULE_SELF, 0, NULL);
            processFBD(SINK_MODULE_SELF, 0, NULL);

            if(m_pMediaSrc != NULL)
            {
                m_pMediaSrc->Stop();
            }

            if(m_pVideoDecode != NULL)
            {
                m_pVideoDecode->Stop();
            }
#ifdef USE_OMX_AAC_CODEC
            if(m_pAudioDecode != NULL)
            {
                m_pAudioDecode->Stop();
            }
#endif
            if(m_pAudioRenderer != NULL)
            {
                m_pAudioRenderer->Stop();
            }
            if(m_pVideoRenderer != NULL)
            {
                m_pVideoRenderer->Stop();
            }
            if(m_pGenericRenderer!= NULL)
            {
                m_pGenericRenderer->Stop();
            }

        }

        if(!destroyMMComponents())
        {
            WFDMMLOGE("Failed to destroy MM components");
        }

        if(m_pVideoSurface != NULL)
        {
            if(m_pWindow != NULL)
            {
                m_pWindow.clear();
                m_pWindow = NULL;
            }
            m_pVideoSurface.clear();
            m_pVideoSurface = NULL;
        }

        WFDMMLOGH("Notify Display HAL to Enable Dynamic refresh rate");
        qdutils::configureDynRefreshRate(
                  qdutils::ENABLE_METADATA_DYN_REFRESH_RATE, 0);

        WFDMMLOGH("WFD Sink moves to DEINIT");
        SET_MM_STATE_DEINIT;
        break;

    case PLAY:
        WFDMMLOGH("Play Command Received");

        /*----------------------------------------------------------------------
         We receive this command after RTSP PLAY. By this time INIT must
         have completed
        ------------------------------------------------------------------------
        */

        /*----------------------------------------------------------------------
         There are a few things to do here.
         1. Make sure the state is in INIT
        ------------------------------------------------------------------------
        */
        if(IS_MM_STATE_PLAY)
        {
            WFDMMLOGE("PLAY when already in PLAY state");
            break;
        }
        if(!IS_MM_STATE_INIT)
        {
            WFDMMLOGE("Called play when not in INIT");
            return;
        }

        /*----------------------------------------------------------------------
        2. Resources are already created in INIT. Now start the modules to
        indicate the session is about to start. In this step, the buffers are
        to the rightful owners at the start of session,
         - All Video O/P buffers must be with decoder.
         - All Audio O/P buffers must be with audio decoder
         - All Video and Audio i/p buffers must be with MediaSource.
         - Renderer holds no buffers
        ------------------------------------------------------------------------
        */
        m_bNotifyFirstFrameInfo = true;

        if(m_bStreamPaused)
        {
            m_bStreamPaused = false;
        }

        if(m_pVideoDecode != NULL)
        {
            eErr = m_pVideoDecode->Start();
            if(eErr != OMX_ErrorNone)
            {
                SET_MM_ERROR_FAIL;
                WFDMMLOGE("Video Decode failed to start");
                break;
            }
        }

        if(m_pAudioRenderer != NULL)
        {
            eErr = m_pAudioRenderer->Start();
            if(eErr != OMX_ErrorNone)
            {
                SET_MM_ERROR_FAIL;
                WFDMMLOGE("Audio Renderer failed to start");
                break;
            }
        }

        if(m_pVideoRenderer != NULL)
        {
            eErr = m_pVideoRenderer->Start();
            if(eErr != OMX_ErrorNone)
            {
                SET_MM_ERROR_FAIL;
                WFDMMLOGE("Video Renderer failed to start");
                break;
            }
        }

        if(m_pGenericRenderer != NULL)
        {
            eErr = m_pGenericRenderer->Start();
            if(eErr != OMX_ErrorNone)
            {
                SET_MM_ERROR_FAIL;
                WFDMMLOGE("Generic Renderer failed to start");
                break;
            }
        }

#ifdef USE_OMX_AAC_CODEC
        if(m_pAudioDecode != NULL)
        {
            eErr = m_pAudioDecode->Start();
            if(eErr != OMX_ErrorNone)
            {
                SET_MM_ERROR_FAIL;
                WFDMMLOGE("Audio Decode failed to start");
                break;
            }
        }
#endif

        WFDMMLOGH("WFD Sink MM moves to PLAY");
        SET_MM_STATE_PLAY;

        /*----------------------------------------------------------------------
         Ensure WFD MM Sink moves to PLAY state before mediasource starts
         emitting track samples to correctly process all the frames
         -----------------------------------------------------------------------
         */
        if(m_pMediaSrc != NULL)
        {
            eErr = m_pMediaSrc->Start();
            if(eErr != OMX_ErrorNone)
            {
                SET_MM_ERROR_FAIL;
                WFDMMLOGE("MediaSource failed to start");
                break;
            }
        }

        break;

    case STOP:
        WFDMMLOGH("Stop Command Received");
        /*----------------------------------------------------------------------
         We receive this command after RTSP TEARDOWN or Pause.
        ------------------------------------------------------------------------
        */

        /*----------------------------------------------------------------------
         There are a few things to do here.
         1. Make sure the state is in PLAY.
         2. Each component has its role to play to ensure the buffer flow has
         been stopped properly.
            VideoDecode module must let go off all input buffers and must
            be returned to the MediaSource
            VideoDecode must collect all output buffers back from Renderer
            Renderer must let go of all audio and video buffers.
            Audio Decoder wherever applicable must hold on to all audio output
            buffers.
            MediaSource must collect all VideoDecoder input buffers and audio
            input Buffers.
        3. Once this is done we can move the state to INIT. Resources will
           remain allocated.
        ------------------------------------------------------------------------
        */
        if(!IS_MM_STATE_PLAY)
        {
            WFDMMLOGE("Stop when not in PLAY. Nothing to do");
            break;
        }


        if(m_pMediaSrc != NULL)
        {
            eErr = m_pMediaSrc->Stop();
            if(eErr != OMX_ErrorNone)
            {
                SET_MM_ERROR_FAIL;
                WFDMMLOGE("MediaSource failed to stop");
            }
        }

        if(m_pVideoDecode != NULL)
        {
            eErr = m_pVideoDecode->Stop();
            if(eErr != OMX_ErrorNone)
            {
                SET_MM_ERROR_FAIL;
                WFDMMLOGE("Video Decode failed to Stop");
            }
        }

        if(m_pAudioRenderer != NULL)
        {
            eErr = m_pAudioRenderer->Stop();
            if(eErr != OMX_ErrorNone)
            {
                SET_MM_ERROR_FAIL;
                WFDMMLOGE("Audio Renderer failed to Stop.. ");
            }
        }

        if(m_pVideoRenderer != NULL)
        {
            eErr = m_pVideoRenderer->Stop();
            if(eErr != OMX_ErrorNone)
            {
                SET_MM_ERROR_FAIL;
                WFDMMLOGE("Video Renderer failed to Stop.. ");
            }
        }

        if(m_pGenericRenderer != NULL)
        {
            eErr = m_pGenericRenderer->Stop();
            if(eErr != OMX_ErrorNone)
            {
                SET_MM_ERROR_FAIL;
                WFDMMLOGE("Generic Renderer failed to Stop.. ");
            }
        }

#ifdef USE_OMX_AAC_CODEC
        if(m_pAudioDecode != NULL)
        {
            eErr = m_pAudioDecode->Stop();
            if(eErr != OMX_ErrorNone)
            {
                SET_MM_ERROR_FAIL;
                WFDMMLOGE("Audio Decode failed to Stop.. ");
            }
        }
#endif
        /*----------------------------------------------------------------------
           Delete any timers etc.
        ------------------------------------------------------------------------
        */
        if(m_hIDRDelayTimer)
        {
            MM_Timer_Release(m_hIDRDelayTimer);
            m_hIDRDelayTimer = NULL;
        }
        WFDMMLOGH("WFD Sink MM moves to INIT");
        SET_MM_STATE_INIT;
        break;

    case STREAM_PLAY:
        if(IS_MM_STATE_PLAY && m_bStreamPaused)
        {
            if(m_pAudioRenderer)
            {
                m_pAudioRenderer->restartRendering(m_bFlushAll);
            }

            if(m_pVideoRenderer)
            {
                m_pVideoRenderer->restartRendering(m_bFlushAll);
            }

            if(m_pGenericRenderer)
            {
                m_pGenericRenderer->restartRendering(m_bFlushAll);
            }

            if(m_pMediaSrc)
            {
                m_pMediaSrc->streamPlay(m_bFlushAll);
            }

            m_bStreamPaused = false;
        }
        break;

    case STREAM_PAUSE:
        if(IS_MM_STATE_PLAY && !m_bStreamPaused)
        {
            if(m_pMediaSrc)
            {
                m_pMediaSrc->streamPause();
            }

            if(m_pAudioRenderer)
            {
                m_pAudioRenderer->pauseRendering();
            }

            if(m_pVideoRenderer)
            {
                m_pVideoRenderer->pauseRendering();
            }

            if(m_pGenericRenderer)
            {
                m_pGenericRenderer->pauseRendering();
            }

            m_bStreamPaused = true;
        }
        break;

    case STREAM_FLUSH:
        if(IS_MM_STATE_PLAY)
        {
            if(m_pMediaSrc)
            {
                m_pMediaSrc->setFlushTimeStamp(m_nFlushTimestamp);
            }

            if(m_pAudioRenderer)
            {
                m_pAudioRenderer->setFlushTimeStamp(m_nFlushTimestamp);
            }

            if(m_pVideoRenderer)
            {
                m_pVideoRenderer->setFlushTimeStamp(m_nFlushTimestamp);
            }

            if(m_pGenericRenderer)
            {
                m_pGenericRenderer->setFlushTimeStamp(m_nFlushTimestamp);
            }

        }
        break;

    case STREAM_SET_DECODER_LATENCY:
        if(m_pAudioRenderer)
        {
            m_pAudioRenderer->setDecoderLatency(m_nDecoderLatency);
        }

        if(m_pVideoRenderer)
        {
            m_pVideoRenderer->setDecoderLatency(m_nDecoderLatency);
        }

        if(m_pGenericRenderer)
        {
            m_pGenericRenderer->setDecoderLatency(m_nDecoderLatency);
        }

        break;

    case RECONFIGURE_VIDEO_DECODER:
        /*----------------------------------------------------------------------
        We recieve this event after DATA_CODEC_INFO notifcation from parser
        indicating that new stream will commence henceforth. We thus need to
        destroy some of the existing components and re-create them with new
        configurations received.
            1. Make sure the state is PLAY and move to RECONFIGURE state
            2. Stop Video decoder and Video renderer to make sure buffer flow
               has stopped.
            3. Destroy Video decoder and Video renderer
            4. Clear up the video surface
            5. Create SurfaceTextureClient
            6. Create video decoder and video renderer
            7. Kick start all the components

        Note: In case of any failure in reconfiguration, update the state back
        to PLAY and move the WFDMMSink status to FAIL for correct error handling
        mechanisms.
        ------------------------------------------------------------------------
        */

        //1.
        if(IS_MM_STATE_PLAY)
        {
            SET_MM_STATE_RECONFIGURE;

            //2. && 3.
            WFDMMLOGM("Destroy all the video components");
            if(!destroyVideoComponents())
            {
                SET_MM_STATE_PLAY;
                WFDMMLOGE("Failed to destroy video components");
                SET_MM_ERROR_FAIL;
                break;
            }

            //4. && 5.
            WFDMMLOGM("Recreate the surface");
            if(!recreateVideoSurface())
            {
                SET_MM_STATE_PLAY;
                WFDMMLOGE("Failed to recreate surface");
                SET_MM_ERROR_FAIL;
                break;
            }

            //6. && 7.
            WFDMMLOGM("Start all the video components");
            if(!restartVideoComponents())
            {
                SET_MM_STATE_PLAY;
                WFDMMLOGE("Failed to start video components");
                SET_MM_ERROR_FAIL;
                break;
            }

            WFDMMLOGH("Video Decoder reconfiguration success");
            SET_MM_STATE_PLAY;
        }
        else
        {
            WFDMMLOGE1("VideoDecoder Reconfiguration called in nonPLAY[%d] state",
                m_eMMState);
            SET_MM_ERROR_FAIL;
        }
        break;

    case RECONFIGURE_AUDIO_DECODER:
        /*----------------------------------------------------------------------
        We recieve this event after DATA_CODEC_INFO notifcation from parser
        indicating that new stream will commence henceforth. We thus need to
        destroy some of the existing components and re-create them with new
        configurations received.
            1. Make sure the state is PLAY and move to RECONFIGURE state
            2. Stop Audio decoder and Audio renderer to make sure buffer flow
               has stopped.
            3. Destroy Audio decoder and Audio renderer
            4. Create Audio decoder and Audio renderer
            5. Kick start all the components

        Note: In case of any failure in reconfiguration, update the state back
        to PLAY and move the WFDMMSink status to FAIL for correct error handling
        mechanisms.
        ------------------------------------------------------------------------
        */
        //1.
        if(IS_MM_STATE_PLAY)
        {
            SET_MM_STATE_RECONFIGURE;

            //2. && 3.
            WFDMMLOGM("Destroy all the audio components");
            if(!destroyAudioComponents())
            {
                SET_MM_STATE_PLAY;
                WFDMMLOGE("Failed to destroy audio components");
                SET_MM_ERROR_FAIL;
                break;
            }

            //4. && 5
            WFDMMLOGM("Start all the audio components");
            if(!restartAudioComponents())
            {
                SET_MM_STATE_PLAY;
                WFDMMLOGE("Failed to start audio components");
                SET_MM_ERROR_FAIL;
                break;
            }

            WFDMMLOGH("Audio Decoder reconfiguration success");
            SET_MM_STATE_PLAY;
        }
        else
        {
            WFDMMLOGE1("AudioDecoder Reconfiguration called in nonPLAY[%d] state",
                m_eMMState);
            SET_MM_ERROR_FAIL;
        }
        break;

    case RECONFIGURE_VIDEO_RENDERER:
        /*--------------------------------------------------------------------------
        Call EBD and FDB once to make sure no buffers are midway while we are
        reconfiguring.
        ----------------------------------------------------------------------------
        */
        processEBD(SINK_MODULE_SELF, 0, NULL);
        processFBD(SINK_MODULE_SELF, 0, NULL);

        if(m_pVideoRenderer != NULL)
        {
            WFDMMLOGM("Stopping Renderer");
            m_pVideoRenderer->Stop();
            WFDMMLOGM("Starting Renderer");
            m_pVideoRenderer->Start();
        }
        else
        {
            WFDMMLOGM("m_pVideoRenderer is NULL. Unable to reconfigure");
        }
        break;

    default:
        break;
    }
    return;
}

/*==============================================================================

         FUNCTION:         destroyAudioComponents

         DESCRIPTION:
*//**       @brief         Stops and Destroys audio related components
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

*//*==========================================================================*/
bool WFDMMSink::destroyAudioComponents()
{
    WFDMMLOGM("Stopping AudioRenderer");
    if(m_pAudioRenderer != NULL)
    {
        m_pAudioRenderer->Stop();
    }

#ifdef USE_OMX_AAC_CODEC
    WFDMMLOGM("Stopping Audio Decoder");
    if(m_pAudioDecode != NULL)
    {
        m_pAudioDecode->Stop();
    }
#endif

    WFDMMLOGM("Destroying Audio renderer & Decoder");
    if(!destroyAudioDecoder() || !destroyAudioRenderer())
    {
        WFDMMLOGE("Failed to destroy Audio renderer/decoder");
        return false;
    }

    return true;
}
/*==============================================================================

         FUNCTION:         restartAudioComponents

         DESCRIPTION:
*//**       @brief         Create and Start all the audio related components
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

*//*==========================================================================*/
bool WFDMMSink::restartAudioComponents()
{
    WFDMMLOGM("Creating audio components");
    OMX_ERRORTYPE eErr;

    if(!createAudioDecoder() || !createAudioRenderer())
    {
        WFDMMLOGE("Failed to create audio decoder/renderer");
        return false;
    }

    WFDMMLOGH("Audio Components created. Unpause MediaSource Audio Thread");
    if(m_pMediaSrc != NULL)
    {
        m_pMediaSrc->restartAudioThread();
    }

    WFDMMLOGM("Start audio related modules");
    if(m_pAudioRenderer != NULL)
    {
        eErr = m_pAudioRenderer->Start();
        if(eErr != OMX_ErrorNone)
        {
            WFDMMLOGE("Audio Renderer failed to start");
            return false;
        }
    }

#ifdef USE_OMX_AAC_CODEC
    if(m_pAudioDecode != NULL)
    {
        eErr = m_pAudioDecode->Start();
        if(eErr != OMX_ErrorNone)
        {
            WFDMMLOGE("Audio Decode failed to start");
            return false;
        }
    }
#endif

    return true;
}

/*==============================================================================

         FUNCTION:         destroyVideoComponents

         DESCRIPTION:
*//**       @brief         Stops and Destroys video related components
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

*//*==========================================================================*/
bool WFDMMSink::destroyVideoComponents()
{
    /*--------------------------------------------------------------------------
    Call EBD and FDB once to make sure no buffers are midway while we are
    reconfiguring.
    ----------------------------------------------------------------------------
    */
    processEBD(SINK_MODULE_SELF, 0, NULL);
    processFBD(SINK_MODULE_SELF, 0, NULL);

    WFDMMLOGM("Stopping Renderer");
    if(m_pVideoRenderer != NULL)
    {
        m_pVideoRenderer->Stop();
    }

    WFDMMLOGM("Stopping Video Decoder");
    if(m_pVideoDecode != NULL)
    {
        m_pVideoDecode->Stop();
    }

    WFDMMLOGM("Destroying video decoder and renderer");
    if(!destroyVideoDecoder() ||!destroyVideoRenderer())
    {
        WFDMMLOGE("Failed to destroy video components");
        return false;
    }

    return true;
}

/*==============================================================================

         FUNCTION:         recreateVideoSurface

         DESCRIPTION:
*//**       @brief         Destroy and re-create the surface
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

*//*==========================================================================*/
bool WFDMMSink::recreateVideoSurface()
{
    if(m_pVideoSurface != NULL)
    {
        if(m_pWindow != NULL)
        {
          //  native_window_api_disconnect(m_pWindow.get(),
          //                            NATIVE_WINDOW_API_MEDIA);
            m_pWindow.clear();
            m_pWindow = NULL;
        }
        m_pVideoSurface.clear();
        m_pVideoSurface = NULL;
    }

    /*--------------------------------------------------------------------------
     Create the Surfacetextureclient here for this part of the session.
    ----------------------------------------------------------------------------
    */
    if( m_pNegotiatedCap->pSurface != NULL )
    {
        WFDMMLOGH1("WFDMMSink: received surface from upper layer%p",
                   m_pNegotiatedCap->pSurface);
        m_pVideoSurface = (Surface*)m_pNegotiatedCap->pSurface;

        if(m_pVideoSurface != NULL)
        {
#ifdef MR2
            m_pWindow       = MM_New_Args(Surface,
                                (m_pVideoSurface->getIGraphicBufferProducer()));
#else
            m_pWindow       = MM_New_Args(SurfaceTextureClient,
                                (m_pVideoSurface->getSurfaceTexture()));
#endif
            if(m_pWindow == NULL)
            {
                WFDMMLOGE("WFDMMSink Failed to create Native Window");
                return false;
            }
        }
    }
    else
    {
        WFDMMLOGE("WFDMMSink Surface is Null");
        return false;
    }

    return true;
}

/*==============================================================================

         FUNCTION:         restartVideoComponents

         DESCRIPTION:
*//**       @brief         Create and Start all the video related components
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

*//*==========================================================================*/
bool WFDMMSink::restartVideoComponents()
{
    OMX_ERRORTYPE eErr;

    WFDMMLOGM("Creating Video Decoder and Video Renderer");
    if(!createVideoDecoder() || !createVideoRenderer())
    {
        WFDMMLOGE("Failed to create video decoder/renderer");
        return false;
    }

    if(m_pMediaSrc != NULL)
    {
        WFDMMLOGM("Flush VideoQ on MediaSource");
        eErr = m_pMediaSrc->flushVideoQ();
        if(eErr != OMX_ErrorNone)
        {
            WFDMMLOGE("MediaSource failed to flush VideoQ");
            return false;
        }

        WFDMMLOGH("Video resources created. Unpause MediaSource Video Thread");
        /*Notify MediaSource to restart fetching Video Samples*/
        m_pMediaSrc->restartVideoThread();
    }

    WFDMMLOGM("Start VideoDecoder and VideoRenderer modules");
    if(m_pVideoDecode != NULL)
    {
        eErr = m_pVideoDecode->Start();
        if(eErr != OMX_ErrorNone)
        {
            WFDMMLOGE("Video Decode failed to start");
            return false;;
        }
    }

    if(m_pVideoRenderer != NULL)
    {
        eErr = m_pVideoRenderer->Start();
        if(eErr != OMX_ErrorNone)
        {
            WFDMMLOGE("Renderer failed to start");
            return false;
        }
    }

    return true;
}

/*==============================================================================

         FUNCTION:         ~WFDMMSink

         DESCRIPTION:
*//**       @brief         This is the WFDMMSink class destructor
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

*//*==========================================================================*/
WFDMMSink::~WFDMMSink()
{
    WFDMMLOGE("WFDMMSink destructor");

    //Teardown HDCP and MM sessions
    (void)teardownHDCPSession();
    (void)teardownMMSession();

    if(m_hCritSect)
    {
        MM_CriticalSection_Release(m_hCritSect);
        m_hCritSect = NULL;
    }

    if(m_hCritSectEBD)
    {
        MM_CriticalSection_Release(m_hCritSectEBD);
        m_hCritSectEBD = NULL;
    }
    if(m_hCritSectFBD)
    {
        MM_CriticalSection_Release(m_hCritSectFBD);
        m_hCritSectFBD = NULL;
    }

    if(m_hCritSectEvtHdlr)
    {
        MM_CriticalSection_Release(m_hCritSectEvtHdlr);
        m_hCritSectEvtHdlr = NULL;
    }
    if(m_pCmdThread)
    {
        MM_Delete(m_pCmdThread);
        m_pCmdThread = NULL;
    }

    if(m_pSinkStatistics)
    {
        MM_Delete(m_pSinkStatistics);
        m_pSinkStatistics = NULL;
    }

    MM_Memory_ReleaseCheckPoint();
}

/*==============================================================================

         FUNCTION:         setupHDCPSession

         DESCRIPTION:
*//**       @brief         sets up HDCP connection unless already connected to
                           same IP and port
*//**

@par     DEPENDENCIES:
                           None
*//*
         PARAMETERS:
*//**       @param         pIpAddr - IP address of the peer
                           port    - port number to listen for incoming
                                     connection

*//*     RETURN VALUE:
*//**       @return
                           None

@par     SIDE EFFECTS:

*//*==========================================================================*/
WFD_status_t WFDMMSink::setupHDCPSession
(
    char *pIpAddr,
    unsigned short port
)
{
    WFDMMLOGH("WFDMMSink::setupHDCPSession");
    CRITICAL_SECT_ENTER;

    if(!IS_DRM_STATE_ZERO)
    {
        WFDMMLOGE("WFDMMSink Not in a state to Setup HDCP");
        CRITICAL_SECT_LEAVE;
        RETURNBADSTATE;
    }

    int uniqueId = 0;
    if(m_pHDCPManager == NULL)
    {
        m_pHDCPManager = MM_New(HDCPManager);

        if(!m_pHDCPManager)
        {
            WFDMMLOGE("WFDMMSink: creating HDCP client Failed");
            SET_DRM_ERROR_RESOURCE;
            CRITICAL_SECT_LEAVE;
            return WFD_STATUS_FAIL;

        }

        SET_DRM_STATE_INIT;

        if(m_pHDCPManager && m_pHDCPManager->getHDCPManagerState()
            == HDCP_STATE_DEINIT)
        {
            HDCPStatusType eStatus =  m_pHDCPManager->initializeHDCPManager();

            if(eStatus != HDCP_SUCCESS)
            {
                /*--------------------------------------------------------------
                  MM session can still proceed. Just set DRM state to fail
                ----------------------------------------------------------------
                */
                WFDMMLOGE("Failed to initialize HDCP session");
                SET_DRM_ERROR_FAIL;
                CRITICAL_SECT_LEAVE;
                return WFD_STATUS_FAIL;
            }
            else
            {
                eStatus = m_pHDCPManager->setupHDCPSession(HDCP_MODE_RX,
                                                   pIpAddr,port);
                if(eStatus != HDCP_SUCCESS)
                {
                    WFDMMLOGE("Failed to setup HDCP session");
                    SET_DRM_ERROR_FAIL;
                    CRITICAL_SECT_LEAVE;
                    return WFD_STATUS_FAIL;
                }
            }

        }
    }
    SET_DRM_STATE_ACQUIRING;

    CRITICAL_SECT_LEAVE;
    WFDMMLOGH("WFDMMSink::setupHDCPSession.... Done");

    return WFD_STATUS_SUCCESS;
}

/*==============================================================================

         FUNCTION:         getHDCPStatus

         DESCRIPTION:
*//**       @brief         gets currents status of HDCP connection
*//**

@par     DEPENDENCIES:
                           None
*//*
         PARAMETERS:
*//**       @param         pIpAddr - ip address of peer
                           port - port number to listen


*//*     RETURN VALUE:
*//**       @return
                           WFD_status_t

@par     SIDE EFFECTS:

*//*==========================================================================*/
WFD_status_t WFDMMSink::getHDCPStatus
(
    char *pIpAddr,
    unsigned short port
)
{
    (void) pIpAddr;
    (void) port;
    CRITICAL_SECT_ENTER;
    if(m_pHDCPManager != NULL)
    {
        if(m_pHDCPManager->getHDCPManagerState() == HDCP_STATE_CONNECTED)
        {
            WFDMMLOGH("Acquired HDCP session");
            SET_DRM_STATE_ACQUIRED;
        }

        CRITICAL_SECT_LEAVE;
        return WFD_STATUS_SUCCESS;
    }
    CRITICAL_SECT_LEAVE;
    return WFD_STATUS_FAIL;
}

/*==============================================================================

         FUNCTION:         teardownHDCPSession

         DESCRIPTION:
*//**       @brief         tears down HDCP session with peer with the ip
                           specified
*//**

@par     DEPENDENCIES:
                           None
*//*
         PARAMETERS:
*//**       @param         pIpAddr - ip address of peer
                           port - port number to listen


*//*     RETURN VALUE:
*//**       @return
                           WFD_status_t

@par     SIDE EFFECTS:

*//*==========================================================================*/
WFD_status_t WFDMMSink::teardownHDCPSession
(
)
{
    WFDMMLOGH("WFDMMSink Teardown HDCP Session");
    CRITICAL_SECT_ENTER;
    if(!IS_DRM_TEARDOWN_NEEDED)
    {
        WFDMMLOGE("DRM HDCP not initialized. No teadown needed");
        CRITICAL_SECT_LEAVE;
        RETURNSUCCESS;
    }


    if(m_pHDCPManager)
    {
        m_pHDCPManager->teardownHDCPSession();
        m_pHDCPManager->deinitializeHDCPManager();
        MM_Delete(m_pHDCPManager);
        m_pHDCPManager = NULL;
    }

    RESET_DRM_ERROR;
    RESET_DRM_STATE;

    WFDMMLOGH("WFDMMSink Teardown HDCP Session... Done");
    CRITICAL_SECT_LEAVE;
    RETURNSUCCESS;
}

/*==============================================================================

         FUNCTION:         setupMMSession

         DESCRIPTION:
*//**       @brief         This is the WFDMMSink class constructor-initializes
                           the class members.
*//**

@par     DEPENDENCIES:
                           None
*//*
         PARAMETERS:
*//**       @param         WFD_device_t  -  WFD Device Type
                           WFD_MM_capability_t  - WFD Negotiated Capability
                           WFD_MM_callbacks_t - WFD Callback


*//*     RETURN VALUE:
*//**       @return
                           WFD_STATUS_SUCCESS or other error codes

@par     SIDE EFFECTS:

*//*==========================================================================*/
WFD_status_t WFDMMSink::setupMMSession
(
    WFD_device_t             eWFDDeviceType,
    WFD_MM_capability_t      *pNegotiatedCap,
    WFD_MM_Extended_capability_t *pNegotiatedCapEx,
    WFD_MM_Aux_capability_t *pNegotiatedCapAux,
    WFD_MM_callbacks_t       *pCallback
)
{
    WFDMMLOGH("WFDMMSink::setupMMSession");
    CRITICAL_SECT_ENTER;

    if( (eWFDDeviceType != WFD_DEVICE_PRIMARY_SINK &&
         eWFDDeviceType != WFD_DEVICE_SECONDARY_SINK ) ||
        ( pNegotiatedCap == NULL) ||
        (pNegotiatedCap->video_method == WFD_VIDEO_H264 &&
         pNegotiatedCap->video_config.video_config.num_h264_profiles == 0))
    {
        WFDMMLOGE("WFDMMSink::WFDMMSink bad parameters");
        CRITICAL_SECT_LEAVE;
        RETURNBADPARAM;
    }

    /**---------------------------------------------------------------------------
    Validate Codecs.
    ---------------
    This is the only place where we should be checking for
    the codecs that we support as of now. This will reduce the effort to
    add support for more codecs later. Check for TODOs elsewhere too.
    ------------------------------------------------------------------------------
    */
    if(pNegotiatedCap->video_method == WFD_VIDEO_3D
#ifndef USE_AUDIO_TUNNEL_MODE
       || pNegotiatedCap->audio_method == WFD_AUDIO_DOLBY_DIGITAL
#endif
    )
    {
        WFDMMLOGE("WFDMMSink::WFDMMSink Unsupported Codecs");
        SET_MM_ERROR_UNSUPPORTED;
        CRITICAL_SECT_LEAVE;
        RETURNUNSUPPORTED;
    }

    m_nTimeLastIdrReq = 0;

    WFDMMLOGE("Sanity check done");

    if(pCallback)
    {
        sCallBacks.av_format_change_cb = pCallback->av_format_change_cb;
        sCallBacks.capability_cb       = pCallback->capability_cb;
        sCallBacks.idr_cb              = pCallback->idr_cb;
        sCallBacks.update_event_cb     = pCallback->update_event_cb;
    }

    /**---------------------------------------------------------------------------
    Create a local copy of the negotiated capability
    ------------------------------------------------------------------------------
    */
    m_pNegotiatedCap = (WFD_MM_capability_t *)
        SINK_MALLOC( sizeof(WFD_MM_capability_t) );

    m_pAuxNegotiatedCap = (WFD_MM_Aux_capability_t *)
        SINK_MALLOC( sizeof(WFD_MM_Aux_capability_t) );
    if(m_pAuxNegotiatedCap == NULL)
    {
        WFDMMLOGE("WFDMMSink::WFDMMSink AuxCap memory allocation failed");
    }
    else
    {
        memset(m_pAuxNegotiatedCap,0,sizeof(WFD_MM_Aux_capability_t));
        m_pAuxNegotiatedCap->count = 0;
        if(pNegotiatedCapAux &&
           pNegotiatedCapAux->count > 0)
        {
            updateAuxData(pNegotiatedCapAux);
        }
    }

    //Malloc ExtCap
    m_pExtNegotiatedCap = (WFD_MM_Extended_capability_t *)
        SINK_MALLOC( sizeof(WFD_MM_Extended_capability_t));

    if(m_pExtNegotiatedCap == NULL)
    {
        WFDMMLOGE("WFDMMSink::WFDMMSink ExtCap memory allocation failed");
        SET_MM_ERROR_BADPARAM;
        CRITICAL_SECT_LEAVE;
        RETURNBADPARAM;
    }

    //Memset ExtCap
    memset(m_pExtNegotiatedCap,0,sizeof(WFD_MM_Extended_capability_t));

    //Malloc Codec
    m_pExtNegotiatedCap->extended_video_config.extended_video_config.codec =
        (WFD_extended_codec_config_t *) SINK_MALLOC(sizeof(WFD_extended_codec_config_t));

    if(m_pExtNegotiatedCap->extended_video_config.extended_video_config.codec == NULL)
    {
        WFDMMLOGE("WFDMMSink::WFDMMSink ExtCap Codec memory allocation failed");
        SET_MM_ERROR_BADPARAM;
        CRITICAL_SECT_LEAVE;
        RETURNBADPARAM;
    }

    //Memset
    memset(m_pExtNegotiatedCap->extended_video_config.extended_video_config.codec,0,sizeof(WFD_extended_codec_config_t));

    uint32 *pTemPtr = (uint32*)m_pExtNegotiatedCap->extended_video_config.extended_video_config.codec;
    memcpy(m_pExtNegotiatedCap, pNegotiatedCapEx,sizeof(WFD_MM_Extended_capability_t));
    m_pExtNegotiatedCap->extended_video_config.extended_video_config.codec =
                                                (WFD_extended_codec_config_t*)pTemPtr;
    memcpy(m_pExtNegotiatedCap->extended_video_config.extended_video_config.codec,
           pNegotiatedCapEx->extended_video_config.extended_video_config.codec,
           sizeof(WFD_extended_codec_config_t));

    if (m_pNegotiatedCap)
    {
        m_pNegotiatedCap->video_config.video_config.h264_codec =
          (WFD_h264_codec_config_t*)SINK_MALLOC(sizeof(WFD_h264_codec_config_t) *
          pNegotiatedCap->video_config.video_config.num_h264_profiles);
    }

    if(m_pNegotiatedCap &&
        m_pNegotiatedCap->video_config.video_config.h264_codec)
    {
        uint32 *pTemPtr = (uint32*)m_pNegotiatedCap->video_config.
                                                     video_config.h264_codec;
        memcpy(m_pNegotiatedCap, pNegotiatedCap, sizeof(*m_pNegotiatedCap));
        m_pNegotiatedCap->video_config.video_config.h264_codec =
            (WFD_h264_codec_config_t*)pTemPtr;

        memcpy(m_pNegotiatedCap->video_config.video_config.h264_codec,
            pNegotiatedCap->video_config.video_config.h264_codec,
            sizeof(*(m_pNegotiatedCap->video_config.video_config.h264_codec)) *
            pNegotiatedCap->video_config.video_config.num_h264_profiles);
    }
    else
    {
        WFDMMLOGE("WFDMMSink::WFDMMSink memory allocation failed");
        SET_MM_ERROR_BADPARAM;
        CRITICAL_SECT_LEAVE;
        RETURNBADPARAM;
    }

    /* If there is no extended codec */
    if(m_pExtNegotiatedCap->extended_video_config.extended_video_config.num_codec <= 0)
    {
        //Copy all codec information from NegoCap to ExtCap
        if(!copyCodecInfo())
        {
            WFDMMLOGE("WFDMMSink::Failed to copy NegCap to ExtCap");
            SET_MM_ERROR_BADPARAM;
            CRITICAL_SECT_LEAVE;
            RETURNBADPARAM;
        }
    }

    /**---------------------------------------------------------------------------
    If content protection is enabled check for HDCP status
    ------------------------------------------------------------------------------
    */
    if(pNegotiatedCap->content_protection_config.content_protection_ake_port)
    {
        if(IS_DRM_STATE_ACQUIRING)
        {
            unsigned long nTime = 0;
            unsigned long nStartTime = 0;
            MM_Time_GetTime(&nStartTime);
            nTime = nStartTime;

            /**-----------------------------------------------------------------
              If HDCP Rights are being acquired, then wait for upto <9 seconds
            --------------------------------------------------------------------
            */
            while(!IS_DRM_STATE_ACQUIRED && nTime - nStartTime < 8000)
            {
                getHDCPStatus(NULL,
                              pNegotiatedCap->content_protection_config.
                              content_protection_ake_port);
                if(!IS_DRM_STATE_ACQUIRED)
                {
                    WFDMMLOGE("WFDMMSink SetupMMSession No HDCP Rights.. Fail");
                }
                MM_Timer_Sleep(100);
                MM_Time_GetTime(&nTime);
            }
        }
    }

    if(!IS_DRM_STATE_ACQUIRED)
    {
        teardownHDCPSession();
    }
    else if(m_pHDCPManager)
    {
        char *msg = (char*)SINK_MALLOC(sizeof(int)+1);
        if(msg != NULL)
        {
         snprintf(msg,sizeof(int),"%d",m_pNegotiatedCap->audio_method);
         WFDMMLOGE1("WFD prepared Audio Play %s",msg);
         m_pHDCPManager->constructCodecAndStreamType(SINK_AUDIO_TRACK_ID,msg);
         memset(msg,0,sizeof(int)+1);
         snprintf(msg,sizeof(int),"%d",m_pNegotiatedCap->video_method);
         WFDMMLOGE1("WFD prepared Video Play %s",msg);
         m_pHDCPManager->constructCodecAndStreamType(SINK_VIDEO_TRACK_ID,msg);
        }
    }

    WFDMMLOGH("WFDMMSink::setupMMSession... Done");

    CRITICAL_SECT_LEAVE;

    RETURNSUCCESS;
}

/*==============================================================================

         FUNCTION:         dumpExtCodecInfo

         DESCRIPTION:
*//**       @brief         Dumps Extended Codec information stored locally

@par     DEPENDENCIES:
                           None
*//*
         PARAMETERS:
*//**       @param


*//*     RETURN VALUE:
*//**       @return

@par     SIDE EFFECTS:

*//*==========================================================================*/
void WFDMMSink::dumpExtCodecInfo()
{
    WFDMMLOGM("WFDMMSink:: dumpExtCodecInfo() - called");
    if(m_pExtNegotiatedCap == NULL || m_pExtNegotiatedCap->extended_video_config.extended_video_config.codec == NULL)
    {
        return;
    }

    WFDMMLOGL1("WFDMMSink: dumpExtCodecInfo: num_codec[%d]"
        ,m_pExtNegotiatedCap->extended_video_config.extended_video_config.num_codec);
    WFDMMLOGL1("WFDMMSink: dumpExtCodecInfo: name[%s]",
        m_pExtNegotiatedCap->extended_video_config.extended_video_config.codec[0].name);
    WFDMMLOGL1("WFDMMSink: dumpExtCodecInfo: profile[%d]",
        m_pExtNegotiatedCap->extended_video_config.extended_video_config.codec[0].profile);
    WFDMMLOGL1("WFDMMSink: dumpExtCodecInfo: native_bitmap[%d]",
        m_pExtNegotiatedCap->extended_video_config.extended_video_config.native_bitmap);
    WFDMMLOGL1("WFDMMSink: dumpExtCodecInfo: prefDispModeSupp[%d]",
        m_pExtNegotiatedCap->extended_video_config.extended_video_config.preferred_display_mode_supported);
    WFDMMLOGL1("WFDMMSink: dumpExtCodecInfo: decoder_latency[%d]",
        m_pExtNegotiatedCap->extended_video_config.extended_video_config.codec[0].decoder_latency);
    WFDMMLOGL1("WFDMMSink: dumpExtCodecInfo: frame_rate_cntrl_supp[%d]",
        m_pExtNegotiatedCap->extended_video_config.extended_video_config.codec[0].frame_rate_control_support);
    WFDMMLOGL1("WFDMMSink: dumpExtCodecInfo: level[%d]",
        m_pExtNegotiatedCap->extended_video_config.extended_video_config.codec[0].level);
    WFDMMLOGL1("WFDMMSink: dumpExtCodecInfo: max_hres[%d]",
        m_pExtNegotiatedCap->extended_video_config.extended_video_config.codec[0].max_hres);
    WFDMMLOGL1("WFDMMSink: dumpExtCodecInfo: max_vres[%d]",
        m_pExtNegotiatedCap->extended_video_config.extended_video_config.codec[0].max_vres);
    WFDMMLOGL1("WFDMMSink: dumpExtCodecInfo: min_slice_size[%d]",
        m_pExtNegotiatedCap->extended_video_config.extended_video_config.codec[0].min_slice_size);
    WFDMMLOGL1("WFDMMSink: dumpExtCodecInfo: slice_enc_params[%d]",
        m_pExtNegotiatedCap->extended_video_config.extended_video_config.codec[0].slice_enc_params);
    WFDMMLOGL1("WFDMMSink: dumpExtCodecInfo: ceaSupp[%d]",
        m_pExtNegotiatedCap->extended_video_config.extended_video_config.codec[0].supported_cea_mode);
    WFDMMLOGL1("WFDMMSink: dumpExtCodecInfo: hhSupp[%d]",
        m_pExtNegotiatedCap->extended_video_config.extended_video_config.codec[0].supported_hh_mode);
    WFDMMLOGL1("WFDMMSink: dumpExtCodecInfo: vesaSupp[%d]",
        m_pExtNegotiatedCap->extended_video_config.extended_video_config.codec[0].supported_vesa_mode);
}

/*==============================================================================

         FUNCTION:         copyCodecInfo

         DESCRIPTION:
*//**       @brief         Copies codec information from NegCap to ExtCap

@par     DEPENDENCIES:
                           None
*//*
         PARAMETERS:
*//**       @param


*//*     RETURN VALUE:
*//**       @return

@par     SIDE EFFECTS:

*//*==========================================================================*/
bool WFDMMSink::copyCodecInfo()
{
    WFDMMLOGM("WFDMMSink::copyCodecInfo() - called");
    if(m_pNegotiatedCap == NULL    ||
       m_pExtNegotiatedCap == NULL ||
       m_pExtNegotiatedCap->extended_video_config.extended_video_config.codec == NULL)
    {
        WFDMMLOGE("WFDMMSink:: copyCodecInfo() - Capability structures NULL");
        return false;
    }

    m_pExtNegotiatedCap->extended_video_config.extended_video_config.num_codec = 1;

    m_pExtNegotiatedCap->extended_video_config.extended_video_config.preferred_display_mode_supported =
        m_pNegotiatedCap->video_config.video_config.preferred_display_mode_supported;

    m_pExtNegotiatedCap->extended_video_config.extended_video_config.native_bitmap =
        m_pNegotiatedCap->video_config.video_config.native_bitmap;

    (m_pExtNegotiatedCap->extended_video_config.extended_video_config.codec[0]).decoder_latency =
        m_pNegotiatedCap->video_config.video_config.h264_codec->decoder_latency;

    (m_pExtNegotiatedCap->extended_video_config.extended_video_config.codec[0]).frame_rate_control_support =
        m_pNegotiatedCap->video_config.video_config.h264_codec->frame_rate_control_support;

    (m_pExtNegotiatedCap->extended_video_config.extended_video_config.codec[0]).level =
        m_pNegotiatedCap->video_config.video_config.h264_codec->h264_level;

    (m_pExtNegotiatedCap->extended_video_config.extended_video_config.codec[0]).max_hres =
        m_pNegotiatedCap->video_config.video_config.h264_codec->max_hres;

    (m_pExtNegotiatedCap->extended_video_config.extended_video_config.codec[0]).max_vres =
        m_pNegotiatedCap->video_config.video_config.h264_codec->max_vres;

    (m_pExtNegotiatedCap->extended_video_config.extended_video_config.codec[0]).min_slice_size =
        m_pNegotiatedCap->video_config.video_config.h264_codec->min_slice_size;

    /* Keep Codec Name as H.264 as default during this copy (this takes care of
       mirroring). When ExtCap is received as part of UpdateSession, make sure
       to update the codec there
     */
    strlcpy((m_pExtNegotiatedCap->extended_video_config.extended_video_config.codec[0]).name,
            "H.264",
            sizeof((m_pExtNegotiatedCap->extended_video_config.extended_video_config.codec[0]).name));

    (m_pExtNegotiatedCap->extended_video_config.extended_video_config.codec[0]).profile =
        m_pNegotiatedCap->video_config.video_config.h264_codec->h264_profile;

    (m_pExtNegotiatedCap->extended_video_config.extended_video_config.codec[0]).slice_enc_params =
        m_pNegotiatedCap->video_config.video_config.h264_codec->slice_enc_params;

    (m_pExtNegotiatedCap->extended_video_config.extended_video_config.codec[0]).supported_cea_mode =
        m_pNegotiatedCap->video_config.video_config.h264_codec->supported_cea_mode;

    (m_pExtNegotiatedCap->extended_video_config.extended_video_config.codec[0]).supported_hh_mode =
        m_pNegotiatedCap->video_config.video_config.h264_codec->supported_hh_mode;

    (m_pExtNegotiatedCap->extended_video_config.extended_video_config.codec[0]).supported_vesa_mode =
        m_pNegotiatedCap->video_config.video_config.h264_codec->supported_vesa_mode;

    dumpExtCodecInfo();
    return true;
}
/*==============================================================================

         FUNCTION:          createDataSource

         DESCRIPTION:
*//**       @brief           Creates the RTP Stream port module
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
bool WFDMMSink::createDataSource()
{

    if(!m_pNegotiatedCap)
    {
        return false;
    }

    if(m_pNegotiatedCap->transport_capability_config.eRtpPortType ==
                                                                   RTP_PORT_TCP)
    {
        if(m_pRTPStreamPort)
        {
            WFDMMLOGE("Using the existing streamport in case of TCP.");
            m_pRTPStreamPort->updateRTPPortVars();
            return true;
        }
    }
    else
    {
        if(m_pRTPStreamPort)
        {
            WFDMMLOGE("Destroy streamport in case of UDP");
            destroyDataSource();
        }
    }

    mediaSourceConfigType mCfg;

    mCfg.rtpPort = m_pNegotiatedCap->
                         transport_capability_config.port1_id;
    mCfg.rtcpPortLocal = mCfg.rtpPort + 1;

    mCfg.bIsTCP   =
          (m_pNegotiatedCap->transport_capability_config.eRtpPortType
                     == RTP_PORT_TCP) ? true: false;

    mCfg.nRtpSock = m_pNegotiatedCap->transport_capability_config.rtpSock;
    mCfg.nRtcpSock = m_pNegotiatedCap->transport_capability_config.rtcpSock;

    mCfg.nPeerIP   = m_pNegotiatedCap->peer_ip_addrs.ipv4_addr1;
    mCfg.rtcpPortRemote =
        m_pNegotiatedCap->transport_capability_config.port1_rtcp_id;

    MM_MSG_PRIO4(MM_GENERAL, MM_PRIO_HIGH,
                 "rtcpctrl: rtpPort %u, local rtcpport %u, "
                 "remote rtcpport = %u, rtcpSocket=%d",
                 (unsigned int)mCfg.rtpPort, (unsigned int)mCfg.rtcpPortLocal,
                 mCfg.rtcpPortRemote, mCfg.nRtcpSock);

    if(mCfg.rtpPort == 0)
    {
        /*----------------------------------------------------------------------
          Invalid Port Number
        ------------------------------------------------------------------------
        */
        WFDMMLOGE("Invalid RTP Port number");
        return false;
    }
    WFDMMLOGH1("Media Source RTP Port Num %u", mCfg.rtpPort);
    WFDMMLOGH2("Media Source RTP Socket Pair rtpSockFd=%d, rtcpSockfd=%d", mCfg.nRtpSock,
                mCfg.nRtcpSock);

    int rtcpIntervalMs = -1;

    if(mCfg.nRtcpSock > 0)
    {
        getCfgItem(RTCP_RR_INTERVAL_MS_KEY ,&rtcpIntervalMs);
        MM_MSG_PRIO1(MM_GENERAL, MM_PRIO_HIGH,
                     "rtcpctrl: configured rtcp interval %d millisecs", rtcpIntervalMs);
    }

    /*--------------------------------------------------------------------------
     Create a socket pair to send to RTP module
    ----------------------------------------------------------------------------
    */
    m_pRTPStreamPort = MM_New_Args(
       RTPStreamPort,
       (mCfg.rtpPort, mCfg.bIsTCP, mCfg.nRtpSock,
        mCfg.rtcpPortRemote, mCfg.nPeerIP, mCfg.nRtcpSock, rtcpIntervalMs));

   if(!m_pRTPStreamPort)
    {
        WFDMMLOGE("Cant create RTP Decoder");
        return false;
    }

    return true;
}

/*==============================================================================

         FUNCTION:          createMediaSource

         DESCRIPTION:
*//**       @brief           Creates the Media  Source Module
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
bool WFDMMSink::createMediaSource()
{
    if(!m_pNegotiatedCap)
    {
        return false;
    }

    mediaSourceConfigType sConfig;

    sConfig.eAudioFmt = m_pNegotiatedCap->audio_method;

    sConfig.bHasAudio = m_pNegotiatedCap->audio_method == WFD_AUDIO_LPCM ||
                        m_pNegotiatedCap->audio_method == WFD_AUDIO_AAC ?
                        true: false;
    sConfig.bHasVideo = m_pNegotiatedCap->video_method == WFD_VIDEO_H264 ?
                        true: false;
    sConfig.bHasGeneric = (m_pAuxNegotiatedCap != NULL)?((m_pAuxNegotiatedCap->count > 0) ? true : false):false;
    sConfig.nPeerIP   = m_pNegotiatedCap->peer_ip_addrs.ipv4_addr1;

    sConfig.rtpPort = m_pNegotiatedCap->
                         transport_capability_config.port1_id;
    sConfig.rtcpPortLocal = sConfig.rtpPort + 1;

    sConfig.bSecure   = IS_DRM_STATE_ACQUIRED ? true : false;

    sConfig.pFnDecrypt = IS_DRM_STATE_ACQUIRED ? decryptCb : NULL;

    sConfig.rtcpPortRemote =
                   m_pNegotiatedCap->transport_capability_config.port1_rtcp_id;
  //  sConfig.  = IS_DRM_STATE_ACQUIRED ? m_pNegotiatedCap->
  //                 content_protection_config.content_protection_ake_port : 0;
    sConfig.bIsTCP   =
          (m_pNegotiatedCap->transport_capability_config.eRtpPortType
                     == RTP_PORT_TCP) ? true: false;

    sConfig.nRtpSock = m_pNegotiatedCap->transport_capability_config.rtpSock;
    sConfig.nRtcpSock = m_pNegotiatedCap->transport_capability_config.rtcpSock;

    int nFrameDropMode = 0;
    getCfgItem(VIDEO_PKTLOSS_FRAME_DROP_MODE_KEY, &nFrameDropMode);

    sConfig.nFrameDropMode = nFrameDropMode;

    sConfig.pRTPStreamPort = m_pRTPStreamPort;

    m_pMediaSrc = MM_New_Args(WFDMMSinkMediaSource,
                             (SINK_MODULE_MEDIASOURCE,
                              eventHandlerCb, FBD,avInfoCb,this,
                              m_pSinkStatistics));

    if(!m_pMediaSrc)
    {
        WFDMMLOGE("Failed to create Media Source");
        return false;
    }

    if(m_pMediaSrc->Configure(&sConfig) != OMX_ErrorNone)
    {
        WFDMMLOGE("Failed to configure Media Source");
        return false;
    }
    return true;
}

/*==============================================================================

         FUNCTION:           createAudioDecoder

         DESCRIPTION:        creates AudioDecoder Instance. Place holder for now
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
bool WFDMMSink::createAudioDecoder()
{
#ifndef USE_OMX_AAC_CODEC
    return true;
#else

    if(m_pNegotiatedCap->audio_method == WFD_AUDIO_LPCM ||
       m_pNegotiatedCap->audio_method == WFD_AUDIO_INVALID ||
       m_pNegotiatedCap->audio_method == WFD_AUDIO_UNK)
    {
        return true;
    }

    audioConfigType sConfig;

    if(m_pNegotiatedCap->audio_method == WFD_AUDIO_AAC)
    {
        getAACAudioParams(m_pNegotiatedCap->audio_config.aac_codec.
                          supported_modes_bitmap);
    }
    else if(m_pNegotiatedCap->audio_method == WFD_AUDIO_DOLBY_DIGITAL)
    {
        getAC3AudioParams(m_pNegotiatedCap->audio_config.dolby_digital_codec
                          .supported_modes_bitmap);
    }

    sConfig.eAudioType   = m_pNegotiatedCap->audio_method;
    sConfig.nChannels    = m_nAudioChannels? m_nAudioChannels : 2;
    sConfig.nSampleRate  = m_nAudioSampleRate?
                                     m_nAudioSampleRate : 48000;


    m_pAudioDecode = MM_New_Args(WFDMMSinkAudioDecode, (
                                  SINK_MODULE_AUDIO_DECODER,
                                  EBD,
                                  FBD,
                                  eventHandlerCb,
                                  (int)this));

    if(!m_pAudioDecode)
    {
        WFDMMLOGE("Failed to create Audio Decoder");
        return false;
    }

    if(m_pAudioDecode->Configure(&sConfig) != OMX_ErrorNone)
    {
        WFDMMLOGE("Failed to configure Audio Decoder");
        return false;
    }

    return true;

#endif
}

/*==============================================================================

         FUNCTION:          createVideoDecoder

         DESCRIPTION:
*//**       @brief          creates Video Decoder Module
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
bool WFDMMSink::createVideoDecoder()
{
    /*To support DS, create Video Decoder from ExtCap always*/
    if(!m_pExtNegotiatedCap)
    {
        return false;
    }

    if(m_pNegotiatedCap->video_method == WFD_VIDEO_UNK ||
       m_pNegotiatedCap->video_method == WFD_VIDEO_INVALID)
    {
        WFDMMLOGH("Audio Only Session");
        return true;
    }

    m_pVideoDecode = MM_New_Args(WFDMMSinkVideoDecode,
                                 (SINK_MODULE_VIDEO_DECODER,
                                  EBD,
                                  FBD,
                                  eventHandlerCb,
                                  this,
                                  m_pSinkStatistics));

    if(!m_pVideoDecode)
    {
        WFDMMLOGE("Failed to create Video Decoder");
        return false;
    }

    videoConfigType sConfig;

    sConfig.bSecure = IS_DRM_STATE_ACQUIRED ? true : false;

    /* Updating with proper VideoCodecFormat*/
    if (m_pExtNegotiatedCap->extended_video_config.extended_video_config.codec == NULL ||
        m_pExtNegotiatedCap->extended_video_config.extended_video_config.num_codec == 0)
    {
        WFDMMLOGE("createVideoDecoder : setting negCap as method");
        sConfig.eVideoType = m_pNegotiatedCap->video_method;
    }
    else if(!strncmp(m_pExtNegotiatedCap->extended_video_config.extended_video_config.codec->name,"H.264",6))
    {
        sConfig.eVideoType = WFD_VIDEO_H264;
        WFDMMLOGE("createVideoDecoder : setting m_pExtNegotiatedCap as method");
    }

    uint32 ceaMode  = 0;
    uint32 vesaMode = 0;
    uint32 hhMode   = 0;
    if(m_pExtNegotiatedCap->extended_video_config.extended_video_config.num_codec > 0)
    {
        ceaMode  = m_pExtNegotiatedCap->extended_video_config.extended_video_config.codec->supported_cea_mode;
        vesaMode = m_pExtNegotiatedCap->extended_video_config.extended_video_config.codec->supported_vesa_mode;
        hhMode   = m_pExtNegotiatedCap->extended_video_config.extended_video_config.codec->supported_hh_mode;
    }
    else
    {
        ceaMode  = m_pNegotiatedCap->video_config.video_config.h264_codec->supported_cea_mode;
        vesaMode = m_pNegotiatedCap->video_config.video_config.h264_codec->supported_vesa_mode;
        hhMode   = m_pNegotiatedCap->video_config.video_config.h264_codec->supported_hh_mode;
    }
    uint32 nFrameWidth = 0;
    uint32 nFrameHeight = 0;
    uint32 nFrameRate = 0;

    (void)getFrameResolutionRefreshRate(ceaMode,
                                vesaMode,
                                hhMode,
                                &nFrameWidth,
                                &nFrameHeight,
                                &nFrameRate);

    if(nFrameWidth == 0 || nFrameHeight == 0)
    {
        WFDMMLOGE("Invalid Height or Width");
        return false;
    }
    WFDMMLOGE3("Height = %lu Width = %lu , framerate = %lu",nFrameHeight,nFrameWidth,nFrameRate);

    sConfig.nFrameHeight = nFrameHeight;
    sConfig.nFrameWidth  = nFrameWidth;

    sConfig.bDirectStreaming = m_pNegotiatedCap->direct_streaming_mode;


    int maxSupportedFps = 0;
    int ret = getCfgItem(MAX_FPS_SUPPORTED_KEY,&maxSupportedFps);

    if(ret < 0)
        sConfig.nFps = 0;
    else
        sConfig.nFps = maxSupportedFps;

    sConfig.nFps = 30;
    sConfig.pWindow      = m_pWindow.get();

    if(m_pVideoDecode->Configure(&sConfig) != OMX_ErrorNone)
    {
        WFDMMLOGE("Failed to configure video decoder");
        return false;
    }
    return true;
}

/*==============================================================================

         FUNCTION:          createRenderer

         DESCRIPTION:
*//**       @brief          creates Renderer Module
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
bool WFDMMSink::createAudioRenderer()
{
    m_pAudioRenderer = MM_New_Args(WFDMMSinkAudioRenderer, (
                        SINK_MODULE_RENDERER,
                        EBD,
                        eventHandlerCb,
                        this,
                        m_pSinkStatistics));

    if(!m_pAudioRenderer)
    {
        WFDMMLOGE("Failed to create WFD Audio Renderer");
        return false;
    }

    //Configure
    rendererConfigType sConfig;

    if(m_pNegotiatedCap->audio_method == WFD_AUDIO_AAC)
    {
        getAACAudioParams(m_pNegotiatedCap->audio_config.aac_codec.
                          supported_modes_bitmap);
    }
    else if(m_pNegotiatedCap->audio_method == WFD_AUDIO_DOLBY_DIGITAL)
    {
        getAC3AudioParams(m_pNegotiatedCap->audio_config.dolby_digital_codec
                          .supported_modes_bitmap);
    }
#ifdef USE_OMX_AAC_CODEC
    if(m_pNegotiatedCap->audio_method != WFD_AUDIO_INVALID &&
       m_pNegotiatedCap->audio_method != WFD_AUDIO_UNK)
    {
        sConfig.sAudioInfo.eAudioFmt = WFD_AUDIO_LPCM;
    }
    else
#endif
    sConfig.sAudioInfo.eAudioFmt   = m_pNegotiatedCap->audio_method;

    sConfig.sAudioInfo.nChannels   = m_nAudioChannels? m_nAudioChannels : 2;
    sConfig.sAudioInfo.nSampleRate = m_nAudioSampleRate?
                                     m_nAudioSampleRate : 48000;
    sConfig.sAudioInfo.nFrameLength = 480;

    sConfig.bHasAudio = m_pNegotiatedCap->audio_method == WFD_AUDIO_AAC ||
                        m_pNegotiatedCap->audio_method == WFD_AUDIO_LPCM ?
                        true: false;

    sConfig.bHasVideo = m_pNegotiatedCap->video_method == WFD_VIDEO_H264 ?
                        true: false;


    sConfig.nDecoderLatency = m_pNegotiatedCap->decoder_latency;

    int nAVSyncMode = 0;

    getCfgItem(DISABLE_AVSYNC_MODE_KEY, &nAVSyncMode);

    sConfig.bAVSyncMode = (nAVSyncMode == 0 ? TRUE : FALSE);
    WFDMMLOGH1("AVSync Mode = %d",sConfig.bAVSyncMode);

    int32 audioAVSyncDropWindow = 0;
    getCfgItem(AUDIO_AVSYNC_DROP_WINDOW_KEY,&audioAVSyncDropWindow);
    sConfig.naudioAVSyncDropWindow = audioAVSyncDropWindow;
    if(sConfig.naudioAVSyncDropWindow == 0)
    {
       WFDMMLOGH("Default Audio AV Sync drop window");
       sConfig.naudioAVSyncDropWindow = AUDIO_AV_SYNC_DROP_WINDOW;
    }
    WFDMMLOGH1("Audio AV Sync drop window = %d",sConfig.naudioAVSyncDropWindow);



    if(m_pAudioRenderer->Configure(&sConfig) != OMX_ErrorNone)
    {
        WFDMMLOGE("Failed to configure Renderer");
        return false;
    }

    return true;
}
/*==============================================================================

         FUNCTION:          getSessionInformation

         DESCRIPTION:
*//**       @brief          Parses session width,height and frame rate.
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

void WFDMMSink::getSessionInformation(uint32 *nFrameWidth,uint32 *nFrameHeight,uint32 *nFrameRate)
{
    uint32 ceaMode = m_pNegotiatedCap->video_config.video_config.
                                          h264_codec->supported_cea_mode;
    uint32 vesaMode = m_pNegotiatedCap->video_config.video_config.
                                          h264_codec->supported_vesa_mode;
    uint32 hhMode   = m_pNegotiatedCap->video_config.video_config.
                                          h264_codec->supported_hh_mode;
    *nFrameWidth = 0;
    *nFrameHeight = 0;
    *nFrameRate = 0;

    (void)getFrameResolutionRefreshRate(ceaMode,
                                vesaMode,
                                hhMode,
                                nFrameWidth,
                                nFrameHeight,
                                nFrameRate);
    ALOGE("getSessionInformation : fw = %d,fh = %d,fr = %d",*nFrameWidth,*nFrameHeight,*nFrameRate);
}

bool WFDMMSink::createVideoRenderer()
{
    m_pVideoRenderer = MM_New_Args(WFDMMSinkVideoRenderer, (
                        SINK_MODULE_RENDERER,
                        EBD,
                        eventHandlerCb,
                        AudioLatFnCallback,
                        this,
                        m_pSinkStatistics));

    if(!m_pVideoRenderer)
    {
        WFDMMLOGE("Failed to create WFD Video Renderer");
        return false;
    }

    //Configure
    rendererConfigType sConfig;
    uint32 nFrameWidth = 0;
    uint32 nFrameHeight = 0;
    uint32 nFrameRate = 0;

    getSessionInformation(&nFrameWidth,&nFrameHeight,&nFrameRate);

    sConfig.sVideoInfo.nFrameHeight = nFrameHeight;
    sConfig.sVideoInfo.nFrameWidth = nFrameWidth;

    sConfig.sVideoInfo.nFrameRate = nFrameRate;

    int nFrameDropMode = 0;
    getCfgItem(VIDEO_PKTLOSS_FRAME_DROP_MODE_KEY, &nFrameDropMode);

    sConfig.sVideoInfo.nFrameDropMode = nFrameDropMode;

    sConfig.bHasAudio = m_pNegotiatedCap->audio_method == WFD_AUDIO_AAC ||
                        m_pNegotiatedCap->audio_method == WFD_AUDIO_LPCM ?
                        true: false;

    sConfig.bHasVideo = m_pNegotiatedCap->video_method == WFD_VIDEO_H264 ?
                        true: false;

    sConfig.pWindow = m_pWindow.get();

    sConfig.nDecoderLatency = m_nDecoderLatency ? m_nDecoderLatency :
                              m_pNegotiatedCap->decoder_latency;

    int nAVSyncMode = 0;

    getCfgItem(DISABLE_AVSYNC_MODE_KEY, &nAVSyncMode);

    sConfig.bAVSyncMode = (nAVSyncMode == 0 ? TRUE : FALSE);
    WFDMMLOGH1("AVSync Mode = %d",sConfig.bAVSyncMode);

    int nAudioTrackLatencyMode = 0;

    getCfgItem(ENABLE_AUDIO_TRACK_LATENCY_MODE_KEY, &nAudioTrackLatencyMode);

    sConfig.bAudioTrackLatencyMode = (nAudioTrackLatencyMode == 1 ? TRUE : FALSE);
    WFDMMLOGH1("AudioTrackLatencyMode Mode = %d",sConfig.bAudioTrackLatencyMode);

    int32 videoAVSyncDropWindow = 0;
    getCfgItem(VIDEO_AVSYNC_DROP_WINDOW_KEY,&videoAVSyncDropWindow);
    sConfig.nvideoAVSyncDropWindow = videoAVSyncDropWindow;
    if(sConfig.nvideoAVSyncDropWindow == 0)
    {
      WFDMMLOGH("Default Video AV Sync drop window");
      sConfig.nvideoAVSyncDropWindow = VIDEO_AV_SYNC_DROP_WINDOW;
    }
    WFDMMLOGH1("Video AV Sync drop window = %d",sConfig.nvideoAVSyncDropWindow);

    if(m_pVideoRenderer->Configure(&sConfig) != OMX_ErrorNone)
    {
        WFDMMLOGE("Failed to configure Renderer");
        return false;
    }

    return true;
}


/*==============================================================================

         FUNCTION:          createGenericRenderer

         DESCRIPTION:
*//**       @brief          creates Generic Renderer Module
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
bool WFDMMSink::createGenericRenderer()
{
    if(!m_pAuxNegotiatedCap ||
       (m_pAuxNegotiatedCap && m_pAuxNegotiatedCap->count <=0))
    {
        WFDMMLOGE("WFD Generic Renderer - Not Negotiated");
        return false;
    }
    m_pGenericRenderer = MM_New_Args(WFDMMSinkGenericRenderer, (
                        SINK_MODULE_RENDERER,
                        EBD,
                        eventHandlerCb,
                        this));

    if(!m_pGenericRenderer)
    {
        WFDMMLOGE("Failed to create WFD Generic Renderer");
        return false;
    }

    rendererConfigType sConfig;

    sConfig.bHasGeneric = true;

    uint32 nFrameWidth = 0;
    uint32 nFrameHeight = 0;
    uint32 nFrameRate = 0;
    uint32 supBM = m_pAuxNegotiatedCap->supportedBitMap;

    getAuxResolution(supBM,&nFrameWidth,&nFrameHeight,&nFrameRate);

    sConfig.sGenericInfo.surfaceHeight = 1080;
    sConfig.sGenericInfo.surfaceWidth  = 1920;
    android::sp<android::SurfaceComposerClient> npSurfaceComposerClient = new android::SurfaceComposerClient();
    if(npSurfaceComposerClient == NULL)
    {
        WFDMMLOGE("WFDMMSink: SurfaceComposerClient failed, Setting Default Height/Width of Screen");
    }
    else
    {
        sp<IBinder> primaryDisplay = npSurfaceComposerClient->getBuiltInDisplay(ISurfaceComposer::eDisplayIdMain);
        DisplayInfo displayInfo;
        int ret = (int)npSurfaceComposerClient->getDisplayInfo(primaryDisplay, &displayInfo);
        if(ret == 0)
        {
            if(displayInfo.orientation == DISPLAY_ORIENTATION_90 ||
               displayInfo.orientation == DISPLAY_ORIENTATION_270)
            {
                sConfig.sGenericInfo.surfaceHeight = displayInfo.w;
                sConfig.sGenericInfo.surfaceWidth  = displayInfo.h;
            }
            else
            {
                sConfig.sGenericInfo.surfaceHeight = displayInfo.h;
                sConfig.sGenericInfo.surfaceWidth  = displayInfo.w;
            }
        }
        WFDMMLOGE3("WFDMMSink: Screen Information : Orient = %d , sH = %lu, sW = %lu",displayInfo.orientation,sConfig.sGenericInfo.surfaceHeight,sConfig.sGenericInfo.surfaceWidth);
    }

    sConfig.sGenericInfo.presentationHeight = m_nSurfaceHeight;
    sConfig.sGenericInfo.presentationWidth  = m_nSurfaceWidth;
    sConfig.sGenericInfo.sessionWidth  = nFrameWidth;
    sConfig.sGenericInfo.sessionHeight = nFrameHeight;

    sConfig.nDecoderLatency = m_pNegotiatedCap->decoder_latency;

    int nAVSyncMode = 0;

    getCfgItem(DISABLE_AVSYNC_MODE_KEY, &nAVSyncMode);

    sConfig.bAVSyncMode = (nAVSyncMode == 0 ? TRUE : FALSE);
    WFDMMLOGH1("AVSync Mode = %d",sConfig.bAVSyncMode);

    if(m_pGenericRenderer->Configure(&sConfig) != OMX_ErrorNone)
    {
        WFDMMLOGE("Failed to configure Generic Renderer");
        return false;
    }

    return true;
}

/*==============================================================================

         FUNCTION:          destroyDataSource

         DESCRIPTION:
*//**       @brief          destroys Stream Port instance
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
bool WFDMMSink::destroyDataSource()
{
    /*--------------------------------------------------------------------------
      destroyDataSource() is called as part of DEINIT and in case of TCP, we do
      not destroy the streamport when this is called during BGFG usecase. This
      check is required to keep the TCP connection intact when DEINIT is called
      as part of BGFG usecase
    ----------------------------------------------------------------------------
    */
    if(m_pNegotiatedCap &&
       m_pNegotiatedCap->transport_capability_config.eRtpPortType ==
                                                               RTP_PORT_UDP)
    {
        WFDMMLOGE("Destroying stream port in case of UDP");

        if(m_pRTPStreamPort)
        {
            MM_Delete(m_pRTPStreamPort);
            m_pRTPStreamPort = NULL;
        }
    }
    return true;
}

/*==============================================================================

         FUNCTION:          destroyMediaSource

         DESCRIPTION:
*//**       @brief          destroys Media Source instance
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
bool WFDMMSink::destroyMediaSource()
{
    if(m_pMediaSrc)
    {
        MM_Delete(m_pMediaSrc);
        m_pMediaSrc = NULL;
    }
    return true;
}

/*==============================================================================

         FUNCTION:          destroyAudioDecoder

         DESCRIPTION:
*//**       @brief          destroys Audio Decoder module. Place holder
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
bool WFDMMSink::destroyAudioDecoder()
{
#ifdef USE_OMX_AAC_CODEC
    if(m_pAudioDecode)
    {
        MM_Delete(m_pAudioDecode);
        m_pAudioDecode = NULL;
    }
#endif
    return true;
}


/*==============================================================================

         FUNCTION:         destroyVideoDecoder

         DESCRIPTION:
*//**       @brief         destroys Video decoder wraper
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
bool WFDMMSink::destroyVideoDecoder()
{
    if(m_pVideoDecode)
    {
        MM_Delete(m_pVideoDecode);
        m_pVideoDecode = NULL;
    }
    return true;
}

/*==============================================================================

         FUNCTION:          destroyAudioRenderer

         DESCRIPTION:
*//**       @brief          Destroys Audio Renderer Instance
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
bool WFDMMSink::destroyAudioRenderer()
{
    WFDMMLOGH("WFDMMSink destroying Audio renderer");
    if(m_pAudioRenderer)
    {
        MM_Delete(m_pAudioRenderer);
        m_pAudioRenderer = NULL;
    }
    return true;
}

/*==============================================================================

         FUNCTION:          destroyVideoRenderer

         DESCRIPTION:
*//**       @brief          Destroys Video Renderer Instance
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
bool WFDMMSink::destroyVideoRenderer()
{
    WFDMMLOGH("WFDMMSink destroying Video renderer");
    if(m_pVideoRenderer)
    {
        MM_Delete(m_pVideoRenderer);
        m_pVideoRenderer = NULL;
    }
    return true;
}

/*==============================================================================

         FUNCTION:          destroyGenericRenderer

         DESCRIPTION:
*//**       @brief          Destroys Generic Renderer Instance
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
bool WFDMMSink::destroyGenericRenderer()
{
    WFDMMLOGH("WFDMMSink destroying Generic renderer");
    if(m_pGenericRenderer)
    {
        MM_Delete(m_pGenericRenderer);
        m_pGenericRenderer = NULL;
    }
    return true;
}

/*==============================================================================

         FUNCTION:          reconfigureDecoder

         DESCRIPTION:
*//**       @brief          Destroy and re-create new AV decoders
*//**
@par     DEPENDENCIES:
                            None
*//*
         PARAMETERS:
*//**       @param          int nTrackId

*//*     RETURN VALUE:
*//**       @return
                            true or false
@par     SIDE EFFECTS:
                            None
*//*==========================================================================*/
bool WFDMMSink::reconfigureDecoder(int nTrackId)
{
    WFDMMLOGH1("Decoder reconfiguration event for track = %d",nTrackId);
    if(IS_MM_STATE_PLAY)
    {
        switch(nTrackId)
        {
            case SINK_VIDEO_TRACK_ID:
                if(ExecuteCommandAsync(RECONFIGURE_VIDEO_DECODER) != true)
                {
                    WFDMMLOGE("Failed to reconfigure Video Decoder");
                    return false;
                }
            break;

            case SINK_AUDIO_TRACK_ID:
                if(ExecuteCommandAsync(RECONFIGURE_AUDIO_DECODER) != true)
                {
                    WFDMMLOGE("Failed to reconfigure Audio Decoder");
                    return false;
                }
            break;

            default:
            /*Really?!!*/
            break;
        }
    }
    else
    {
        WFDMMLOGE1("Decoder reconfiguration called in non_PLAY state[%d]",
            m_eMMState);
        return false;
    }
    return true;
}

/*==============================================================================

         FUNCTION:         teardownMMSession

         DESCRIPTION:
*//**       @brief         tears down MM session.
*//**

@par     DEPENDENCIES:
                           None
*//*
         PARAMETERS:
*//**       @param         None


*//*     RETURN VALUE:
*//**       @return
                           WFD_status_t

@par     SIDE EFFECTS:

*//*==========================================================================*/
WFD_status_t WFDMMSink::teardownMMSession()
{
    CRITICAL_SECT_ENTER;
    WFDMMLOGH("WFDMMSink Teardown MM Session");
/*
    if(!validateStateChange(WFD_SINK_MM_STATE_ZERO))
    {
        CRITICAL_SECT_LEAVE;
        RETURNBADSTATE;
    }
*/
    m_bMMTearingDown = true;
    if(ExecuteCommandSync(DEINIT) != true)
    {
        WFDMMLOGE("Failed to deinitialize multimedia");
        CRITICAL_SECT_LEAVE;
        RETURNBADSTATE;
    }

    /*--------------------------------------------------------------------------
      All multimedia components have been deinitialized and destroyed as part
      of DEINIT called above. It is now safe here to destroy StreamPort as
      WFD session is now tearing down.
    ----------------------------------------------------------------------------
    */
    if(m_pRTPStreamPort)
    {
        MM_Delete(m_pRTPStreamPort);
        m_pRTPStreamPort = NULL;
    }

    closeRtcpSocket();

    if(m_pNegotiatedCap)
    {
        SINK_FREEIF(m_pNegotiatedCap->video_config.video_config.h264_codec);
    }
    SINK_FREEIF(m_pNegotiatedCap);

    if(m_pExtNegotiatedCap)
    {
        SINK_FREEIF(m_pExtNegotiatedCap->extended_video_config.extended_video_config.codec);
    }
    SINK_FREEIF(m_pExtNegotiatedCap);

    SINK_FREEIF(m_pAuxNegotiatedCap);

    m_bMMTearingDown = false;
    RESET_MM_ERROR;
    WFDMMLOGH("WFDMMSink Teardown MM Session... Done");
    CRITICAL_SECT_LEAVE;
    RETURNSUCCESS;
}

/*==============================================================================

         FUNCTION:         play

         DESCRIPTION:
*//**       @brief         starts playing the content.
*//**

@par     DEPENDENCIES:
                           None
*//*
         PARAMETERS:
*//**       @param         av_select - which stream to play
                           pCallback - Callback to notify play status


*//*     RETURN VALUE:
*//**       @return
                           WFD_status_t

@par     SIDE EFFECTS:

*//*==========================================================================*/
WFD_status_t WFDMMSink::play( WFD_AV_select_t av_select,
                              wfd_mm_stream_play_cb pCallback)
{
    (void) av_select;
    WFDMMLOGH("WFDMMSink Play");


    CHECK_ERROR_STATUS;

    if(IS_MM_STATE_PLAY)
    {
        if(pCallback)
        {
            pCallback(this, WFD_STATUS_SUCCESS);
        }

        RETURNSUCCESS;
    }


    if(ExecuteCommandSync(INIT) != true)
    {
        WFDMMLOGE("Play failed at INIT");
        if(pCallback)
        {
            pCallback( this, WFD_STATUS_FAIL);
        }
        SET_MM_ERROR_FAIL;

        RETURNFAIL;
    }


    if(ExecuteCommandSync(PLAY) != true)
    {
        WFDMMLOGE("Play failed to move to PLAY State");
        if(pCallback)
        {
            pCallback( this, WFD_STATUS_FAIL);
        }
        SET_MM_ERROR_FAIL;

        RETURNFAIL;
    }

    if(m_nMMStatus == WFD_STATUS_SUCCESS)
    {
        if(pCallback)
        {
            pCallback( this, WFD_STATUS_SUCCESS);
        }
    }
    else
    {
        WFDMMLOGE("Sink Failed to move to Play");
        if(pCallback)
        {
            pCallback( this, WFD_STATUS_FAIL);
        }
        SET_MM_ERROR_FAIL;

        RETURNFAIL;
    }


    WFDMMLOGE("WFDMMSink Play Done");
    RETURNSUCCESS;
}

/*==============================================================================

         FUNCTION:         pause

         DESCRIPTION:
*//**       @brief         starts pausing the content.
*//**

@par     DEPENDENCIES:
                           None
*//*
         PARAMETERS:
*//**       @param         av_select - which stream to pause
                           pCallback - Callback to notify play status


*//*     RETURN VALUE:
*//**       @return
                           WFD_status_t

@par     SIDE EFFECTS:

*//*==========================================================================*/
WFD_status_t WFDMMSink::pause( WFD_AV_select_t av_select,
                              wfd_mm_stream_pause_cb pCallback)
{
    (void) av_select;
    WFDMMLOGH("WFDMMSink Pause");


    if(IS_MM_STATE_PAUSE || IS_MM_STATE_DEINIT)
    {
        if(pCallback)
        {
            WFDMMLOGH("Pause/Standby called in already paused state");
            pCallback( this, WFD_STATUS_SUCCESS);
        }

        RETURNSUCCESS;
    }

    WFDMMLOGD("Pause validate state change");

    if(!validateStateChange(WFD_SINK_MM_STATE_PAUSE))
    {
        RETURNBADSTATE;
    }


    WFDMMLOGD("Move to DeINIT");
    m_bMMTearingDown = true;
    if(ExecuteCommandSync(DEINIT) != true)
    {
        WFDMMLOGE("Failed to move to DEINIT state");
        if(pCallback)
        {
            pCallback( this, WFD_STATUS_FAIL);
        }
        RETURNFAIL;
    }
    m_bMMTearingDown = false;

    WFDMMLOGE("WFDMMSink Pause Done sending callback to Sessionmanager");
    /*---------------------------------------------------------------------
     ToDo:Need to check,why the callback is sent to session manager
    -----------------------------------------------------------------------
    */

    if(pCallback)
    {
        pCallback( this, WFD_STATUS_SUCCESS);
    }


    RETURNSUCCESS;
}

/*==============================================================================

         FUNCTION:         WFDMMSinkUpdateSession

         DESCRIPTION:
*//**       @brief         updates the session parameters for sink
*//**

@par     DEPENDENCIES:
                           None
*//*
         PARAMETERS:
*//**       @param         pNegCap..the negotiated capabilities of the session
...........................pUpdateSessionCb callback to session manager


*//*     RETURN VALUE:
*//**       @return
                           WFD_status_t

@par     SIDE EFFECTS:

*//*==========================================================================*/

WFD_status_t WFDMMSink::WFDMMSinkUpdateSession(WFD_MM_capability_t *pNegCap,
                                    WFD_MM_Extended_capability_t *pNegExtCap,
                                    WFD_MM_Aux_capability_t* pNegAuxCap,
                                    wfd_mm_update_session_cb pUpdateSessionCb,
                                    bool bResetCodec)
{
    (void) pUpdateSessionCb;
    CRITICAL_SECT_ENTER;
    WFDMMLOGM("WFDMMSink::WFDMMSinkUpdateSession called");
    CHECK_NULL_ERROR(m_pNegotiatedCap,"m_pNegotiatedCap is NULL");
    CHECK_NULL_ERROR(pNegCap,"pNegCap is NULL");
    /*pNegExtCap can be NULL. Do NOT throw error!*/

    if(pNegAuxCap != NULL &&
       pNegAuxCap->count > 0)
    {
        WFDMMLOGE("WFDMMSink::WFDMMSinkUpdateSession updateAuxData");
        updateAuxData(pNegAuxCap);
    }

    m_pNegotiatedCap->direct_streaming_mode = pNegCap->direct_streaming_mode;
    WFDMMLOGH1("DSMode = %d", pNegCap->direct_streaming_mode);

    /*Update ExtCap if received here*/
    if(pNegExtCap != NULL &&
       pNegExtCap->extended_video_config.extended_video_config.num_codec > 0)
    {
        WFDMMLOGM("WFDMMSink:: Updating all the extCapability info");
        m_pExtNegotiatedCap->extended_video_config.
                             extended_video_config.native_bitmap =
                                      pNegExtCap->extended_video_config.
                                       extended_video_config.native_bitmap;

        m_pExtNegotiatedCap->extended_video_config.extended_video_config.
                                             preferred_display_mode_supported =
            pNegExtCap->extended_video_config.extended_video_config.
                                              preferred_display_mode_supported;

        m_pExtNegotiatedCap->extended_video_config.
                                              extended_video_config.num_codec =
                                           pNegExtCap->extended_video_config.
                                               extended_video_config.num_codec;

        uint16 newProfile, currLevel, newLevel;
        char* currCodec = NULL;
        char* newCodec = NULL;

        currLevel = m_pExtNegotiatedCap->extended_video_config.
                                       extended_video_config.codec->level;
        currCodec = m_pExtNegotiatedCap->extended_video_config.
                                       extended_video_config.codec->name;

        newProfile = pNegExtCap->extended_video_config.
                                         extended_video_config.codec->profile;
        newLevel = pNegExtCap->extended_video_config.
                                           extended_video_config.codec->level;
        newCodec = pNegExtCap->extended_video_config.
                                            extended_video_config.codec->name;
        m_bVideoReConfNeeded = true;

        WFDMMLOGH1("WFDMMSink:: Update local extNeg Cap with incoming codecInfo. CEA = %lu",
                          pNegExtCap->extended_video_config.
                          extended_video_config.codec->supported_cea_mode);
        memcpy(m_pExtNegotiatedCap->extended_video_config.
                         extended_video_config.codec,
             pNegExtCap->extended_video_config.extended_video_config.codec,
           sizeof(WFD_extended_codec_config_t));

        dumpExtCodecInfo();

        /* Check and update surface if it is NULL and Valid surface
           is received */
        if(WFDMMSinkUpdateSurfaceLocked(pNegCap) != WFD_STATUS_SUCCESS)
        {
            CRITICAL_SECT_LEAVE;
            return WFD_STATUS_FAIL;
        }

        CRITICAL_SECT_LEAVE;
        RETURNSUCCESS;
    }

    if(bResetCodec)
    {
        WFDMMLOGH("WFDMMSink:: ResetCodec Recvd. Compare and reset!");

        /*In case incoming matches with m_pNegotiatedCap. Sanity is done. Restore the glory!*/
        uint16 storedProfile, storedLevel, newProfile, newLevel;

        storedProfile = m_pNegotiatedCap->video_config.video_config.h264_codec->h264_profile;
        storedLevel = m_pNegotiatedCap->video_config.video_config.h264_codec->h264_level;
        newProfile = pNegCap->video_config.video_config.h264_codec->h264_profile;
        newLevel = pNegCap->video_config.video_config.h264_codec->h264_level;

        /*What about codec?!*/
        if(storedProfile == newProfile && storedLevel == newLevel)
        {
            WFDMMLOGH("WFDMMSinkUpdateSession:: Re-setting WFD Mirroring Codec");
            copyCodecInfo();
            CRITICAL_SECT_LEAVE;
            RETURNSUCCESS;
        }
        else
        {
            /*Currently return value is not being checked. Hence we may continue
            ahead even though we are proceeding with wrong codec*/
            WFDMMLOGE("WFDMMSinkUpdateSession:: Session Codec not suitable for mirroring");
            CRITICAL_SECT_LEAVE;
            return WFD_STATUS_FAIL;
        }
    }

    if(WFDMMSinkUpdateSurfaceLocked(pNegCap) != WFD_STATUS_SUCCESS)
    {
        CRITICAL_SECT_LEAVE;
        return WFD_STATUS_FAIL;
    }

    CRITICAL_SECT_LEAVE;
    RETURNSUCCESS;
}

/*==============================================================================

         FUNCTION:         WFDMMSinkUpdateSurface

         DESCRIPTION:
*//**       @brief         Updates Sink Surface for UDP and TCP and updates port
                           information accordingly.
*//**

@par     DEPENDENCIES:
                           Calling function needs to call it within critical
                           section.
*//*
         PARAMETERS:
*//**       @param         None


*//*     RETURN VALUE:
*//**       @return
                           bool

@par     SIDE EFFECTS:     None

*//*==========================================================================*/

bool WFDMMSink::WFDMMSinkUpdateSurfaceLocked(WFD_MM_capability_t * pNegCap)
{
    if(pNegCap->pSurface == NULL)
    {
        m_pNegotiatedCap->pSurface = pNegCap->pSurface;
        WFDMMLOGE("WFDMMSinkUpdateSurface: Received Sink Surface as NULL");
    }
    else
    {
        if(m_pNegotiatedCap->transport_capability_config.eRtpPortType == RTP_PORT_TCP
            && pNegCap->transport_capability_config.eRtpPortType == RTP_PORT_TCP)
        {
            WFDMMLOGE("Updated surface recvd in TCP. Move to DEINIT");
            m_bUpdatedSurface = true;
            m_bMMTearingDown = true;
            ExecuteCommandSync(DEINIT);
            m_bMMTearingDown = false;
        }

        /*----------------------------------------------------------------------
          To support audio in background feature, session remains in play when
          sink moves to background. Hence when sink returns back to foreground
          the surface update will be recived when the session state is in PLAY
        ------------------------------------------------------------------------
        */
        if(m_pNegotiatedCap->video_method == WFD_VIDEO_UNK ||
           m_pNegotiatedCap->video_method == WFD_VIDEO_INVALID)
        {
            WFDMMLOGH("Allowing relaxed surface update in audio only session");
        }
        else if(!IS_MM_STATE_DEINIT)
        {
            WFDMMLOGE("Cannot update new surface in play");
            return WFD_STATUS_FAIL;
        }

        if (m_pNegotiatedCap->pSurface != pNegCap->pSurface)
        {
            m_pNegotiatedCap->pSurface = pNegCap->pSurface;
        }

        m_pNegotiatedCap->transport_capability_config.eRtpPortType =
                     pNegCap->transport_capability_config.eRtpPortType;
        m_pNegotiatedCap->transport_capability_config.port1_id =
                     pNegCap->transport_capability_config.port1_id;

        WFDMMLOGH2("WFDMMSink:: UpdateSurface: eRtpPortType[%d] port1_id[%d]",
                m_pNegotiatedCap->transport_capability_config.eRtpPortType,
                m_pNegotiatedCap->transport_capability_config.port1_id);
    }
    RETURNSUCCESS;
}

/*==============================================================================

         FUNCTION:         IsSupportedProfile

         DESCRIPTION:
*//**       @brief         Checks if profile is supported by us
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
bool WFDMMSink::IsSupportedProfile(uint16 newProfile)
{
#ifdef FEATURE_CONFIG_FROM_FILE
    {
        readConfigFile getExtCodecList;
        int index = 0;
        uint16 storedProfile;
        memset (&getExtCodecList, 0,sizeof(readConfigFile));

        parseCfgforExtCodec(WFD_CFG_FILE_SINK, &getExtCodecList);

        if(getExtCodecList.numAVFormatsExtention == 0)
        {
            WFDMMLOGE("WFDMMSink:: Is this even possible!? No ExtCodec Support");
            return false;
        }

        for(index = 0; index < getExtCodecList.numAVFormatsExtention; index++)
        {
            storedProfile =
                (getExtCodecList.pExtCfgCapabilities->extended_video_config.extended_video_config.codec[index]).profile;
            if(storedProfile == newProfile)
            {
                WFDMMLOGH("WFDMMSink:: Profile value match found!");
                return true;
            }
        }

        WFDMMLOGE("WFDMMSink:: No profile match found in config file");
        return false;
    }
#else
    {
        WFDMMLOGE("WFDMMSink:: FEATURE_CONFIG_FROM_FILE not defined");
        return false;
    }
#endif
    return false;
}

/*==============================================================================

         FUNCTION:         validateStateChange

         DESCRIPTION:
*//**       @brief         Validate a given state change
*//**

@par     DEPENDENCIES:
                           None
*//*
         PARAMETERS:
*//**       @param         WFDSinkMMStateType


*//*     RETURN VALUE:
*//**       @return
                           bool

@par     SIDE EFFECTS:

*//*==========================================================================*/
bool WFDMMSink::validateStateChange
(
    WFDSinkMMStateType eState
)
{
    switch(eState)
    {
        case WFD_SINK_MM_STATE_PLAY:
            if(m_eMMState != WFD_SINK_MM_STATE_INIT &&
               m_eMMState != WFD_SINK_MM_STATE_PAUSE)
            {
                return false;
            }
            break;
        case WFD_SINK_MM_STATE_PAUSE:
            if(m_eMMState != WFD_SINK_MM_STATE_PLAY)
            {
                return false;
            }
            break;
        case WFD_SINK_MM_STATE_ZERO:
            if(m_eMMState == WFD_SINK_MM_STATE_ZERO)
            {
                return false;
            }
            break;
        case WFD_SINK_MM_STATE_DEINIT:
            if(m_eMMState == WFD_SINK_MM_STATE_DEINIT ||
               m_eMMState == WFD_SINK_MM_STATE_ZERO)
            {
                return false;
            }
            break;
        default:
            return false;
            break;
    }
    return true;
}
/*==============================================================================

         FUNCTION:         getAuxResolution

         DESCRIPTION:
*//**       @brief          Decodes frame dimensions and framerates from
                            CEA VESA and HH modes
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
void WFDMMSink::getAuxResolution(uint32 supportedBitMap,
                                 uint32 *nFrameWidth,
                                 uint32 *nFrameHeight,
                                 uint32 *nFramerate)
{
    WFDMMLOGE1("WFDMMSink::getAuxResolution bitmap = %d ",supportedBitMap);

    /* Last 3 bit will give mode CEA = 0 , VESA = 1, HH = 2 */
    int mode = supportedBitMap & 0x07;

    /* 5 bits give the value that is set and it is index to the table
       and not bit logic.*/
    int value = (supportedBitMap >> 3)&0x1F;

    WFDMMLOGE2("WFDMMSink::getAuxResolution mode = %x , value = %x",mode,value);

    /* CEA */
    if(mode == 0)
    {
        switch(value)
        {
            case 0:
                *nFrameWidth  = 640;
                *nFrameHeight = 480;
                break;
            case 1:
            case 2:
                *nFrameWidth  = 720;
                *nFrameHeight = 480;
                break;
            case 3:
            case 4:
                *nFrameWidth  = 720;
                *nFrameHeight = 576;
                break;
            case 5:
            case 6:
            case 10:
            case 11:
            case 15:
                *nFrameWidth  = 1280;
                *nFrameHeight = 720;
                break;
            case 7:
            case 8:
            case 9:
            case 12:
            case 13:
            case 14:
                *nFrameWidth  = 1920;
                *nFrameHeight = 1080;
                break;
            default:
                *nFrameWidth  = 640;
                *nFrameHeight = 480;
                break;
        }
    }
    /* VESA */
    else if(mode == 1)
    {
        switch(value)
        {
            case 0:
            case 1:
                *nFrameWidth  = 800;
                *nFrameHeight = 600;
                break;
            case 2:
            case 3:
                *nFrameWidth  = 1024;
                *nFrameHeight = 768;
                break;
            case 4:
            case 5:
                *nFrameWidth  = 1152;
                *nFrameHeight = 864;
                break;
            case 6:
            case 7:
                *nFrameWidth  = 1280;
                *nFrameHeight = 768;
                break;
            case 8:
            case 9:
                *nFrameWidth  = 1280;
                *nFrameHeight = 800;
                break;
            case 10:
            case 11:
                *nFrameWidth  = 1360;
                *nFrameHeight = 768;
                break;
            case 12:
            case 13:
                *nFrameWidth  = 1366;
                *nFrameHeight = 768;
                break;
            case 14:
            case 15:
                *nFrameWidth  = 1280;
                *nFrameHeight = 1024;
                break;
            case 16:
            case 17:
                *nFrameWidth  = 1400;
                *nFrameHeight = 1050;
                break;
            case 18:
            case 19:
                *nFrameWidth  = 1440;
                *nFrameHeight = 900;
                break;
            case 20:
            case 21:
                *nFrameWidth  = 1600;
                *nFrameHeight = 900;
                break;
            case 22:
            case 23:
                *nFrameWidth  = 1600;
                *nFrameHeight = 1200;
                break;
            case 24:
            case 25:
                *nFrameWidth  = 1680;
                *nFrameHeight = 1024;
                break;
            case 26:
            case 27:
                *nFrameWidth  = 1680;
                *nFrameHeight = 1050;
                break;
            case 28:
            case 29:
                *nFrameWidth  = 1920;
                *nFrameHeight = 1200;
                break;
            default:
                *nFrameWidth  = 800;
                *nFrameHeight = 600;
                break;
        }
    }
    /* HH */
    else if(mode == 2)
    {
        switch(value)
        {
            case 0:
            case 1:
                *nFrameWidth  = 800;
                *nFrameHeight = 480;
                break;
            case 2:
            case 3:
                *nFrameWidth  = 854;
                *nFrameHeight = 480;
                break;
            case 4:
            case 5:
                *nFrameWidth  = 864;
                *nFrameHeight = 480;
                break;
            case 6:
            case 7:
                *nFrameWidth  = 640;
                *nFrameHeight = 360;
                break;
            case 8:
            case 9:
                *nFrameWidth  = 960;
                *nFrameHeight = 540;
                break;
            case 10:
            case 11:
                *nFrameWidth  = 848;
                *nFrameHeight = 480;
                break;
            default:
                *nFrameWidth  = 800;
                *nFrameHeight = 480;
                break;
        }
    }
}

/*==============================================================================

         FUNCTION:         getFrameResolutionRefreshRate

         DESCRIPTION:
*//**       @brief          Decodes frame dimensions and framerates from
                            CEA VESA and HH modes
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
void WFDMMSink::getFrameResolutionRefreshRate(uint32 cea_mode,
                                           uint64 vesa_mode,
                                           uint32 hh_mode,
                                           uint32 *nFrameWidth,
                                           uint32 *nFrameHeight,
                                           uint32 *nFramerate
                                           )
{
    MM_MSG_PRIO3(MM_GENERAL, MM_PRIO_HIGH,
        "WFDMMSink::getFrameResolutionRefreshRate %d %d %d",
        cea_mode, vesa_mode, hh_mode);

    if(cea_mode)
    {
        switch (cea_mode)
        {
        case 1:
            *nFrameWidth = 640;
            *nFrameHeight = 480;
            *nFramerate = 60;
            break;
        case 2:
            *nFrameWidth = 720;
            *nFrameHeight = 480;
            *nFramerate = 60;
            break;
        case 4:
            //480i
            *nFrameWidth = 720;
            *nFrameHeight = 480;
            *nFramerate = 60;
            break;
        case 8:
            *nFrameWidth = 720;
            *nFrameHeight = 576;
            *nFramerate = 50;
            break;
        case 16:
            //576i
            *nFrameWidth = 720;
            *nFrameHeight = 576;
            *nFramerate = 50;
            break;
        case 32:
            *nFrameWidth = 1280;
            *nFrameHeight = 720;
            *nFramerate = 30;
            break;
        case 64:
            *nFrameWidth = 1280;
            *nFrameHeight = 720;
            *nFramerate = 60;
            break;
        case 128:
            *nFrameWidth = 1920;
            *nFrameHeight = 1080;
            *nFramerate = 30;
            break;
        case 256:
            *nFrameWidth = 1920;
            *nFrameHeight = 1080;
            *nFramerate = 60;
            break;
        case 512:
            //1080i 60
            *nFrameWidth = 1920;
            *nFrameHeight = 1080;
            *nFramerate = 60;
            break;
        case 1024:
            *nFrameWidth = 1280;
            *nFrameHeight = 720;
            *nFramerate = 25;
            break;
        case 2048:
            *nFrameWidth = 1280;
            *nFrameHeight = 720;
            *nFramerate = 50;
            break;
        case 4096:
            *nFrameWidth = 1920;
            *nFrameHeight = 1080;
            *nFramerate = 25;
            break;
        case 8192:
            *nFrameWidth = 1920;
            *nFrameHeight = 1080;
            *nFramerate = 50;
            break;
        case 16384:
            *nFrameWidth = 1920;
            *nFrameHeight = 1080;
            *nFramerate = 50;
            break;
        case 32768:
            *nFrameWidth = 1280;
            *nFrameHeight = 720;
            *nFramerate = 24;
            break;
        case 65536:
            *nFrameWidth = 1920;
            *nFrameHeight = 1080;
            *nFramerate = 24;
            break;
        case 131072:
            *nFrameWidth = 3840;
            *nFrameHeight = 2160;
            *nFramerate = 30;
            break;

        case 262144:
            *nFrameWidth = 3840;
            *nFrameHeight = 2160;
            *nFramerate = 60;
            break;

        case 524288:
            *nFrameWidth = 4096;
            *nFrameHeight = 2160;
            *nFramerate = 30;
            break;

        case 1048576:
            *nFrameWidth = 4096;
            *nFrameHeight = 2160;
            *nFramerate = 60;
            break;

        case 2097152:
            *nFrameWidth = 3840;
            *nFrameHeight = 2160;
            *nFramerate = 25;
            break;

        case 4194304:
            *nFrameWidth = 3840;
            *nFrameHeight = 2160;
            *nFramerate = 50;
            break;

        case 8388608:
            *nFrameWidth = 4096;
            *nFrameHeight = 2160;
            *nFramerate = 25;
            break;

        case 16777216:
            *nFrameWidth = 4096;
            *nFrameHeight = 2160;
            *nFramerate = 50;
            break;

        case 33554432:
            *nFrameWidth = 3840;
            *nFrameHeight = 2160;
            *nFramerate = 24;
            break;

        case 67108864:
            *nFrameWidth = 4096;
            *nFrameHeight = 2160;
            *nFramerate = 24;
            break;
        default:
            *nFrameWidth = 800;
            *nFrameHeight = 480;
            *nFramerate = 30;
            break;
        }

    }
    else if(hh_mode)
    {
        switch (hh_mode)
        {
        case 1:
            *nFrameWidth = 800;
            *nFrameHeight = 480;
            *nFramerate = 30;
            break;
        case 2:
            *nFrameWidth = 800;
            *nFrameHeight = 480;
            *nFramerate = 60;
            break;
        case 4:
            *nFrameWidth = 854;
            *nFrameHeight = 480;
            *nFramerate = 30;
            break;
        case 8:
            *nFrameWidth = 854;
            *nFrameHeight = 480;
            *nFramerate = 60;
            break;

        case 16:
            *nFrameWidth = 864;
            *nFrameHeight = 480;
            *nFramerate = 30;
            break;
        case 32:
            *nFrameWidth = 864;
            *nFrameHeight = 480;
            *nFramerate = 60;
            break;
        case 64:
            *nFrameWidth = 640;
            *nFrameHeight = 360;
            *nFramerate = 30;
            break;
        case 128:
            *nFrameWidth = 640;
            *nFrameHeight = 360;
            *nFramerate = 60;
            break;
        case 256:
            *nFrameWidth = 960;
            *nFrameHeight = 540;
            *nFramerate = 30;
            break;
        case 512:
            *nFrameWidth = 960;
            *nFrameHeight = 540;
            *nFramerate = 60;
            break;
        case 1024:
            *nFrameWidth = 848;
            *nFrameHeight = 480;
            *nFramerate = 30;
            break;
        case 2048:
            *nFrameWidth = 848;
            *nFrameHeight = 480;
            *nFramerate = 60;
            break;

        default:
            *nFrameWidth = 800;
            *nFrameHeight = 480;
            *nFramerate = 30;
            break;
        }
    }
    else if(vesa_mode)
    {
        switch (vesa_mode)
        {
        case 1:
            *nFrameWidth = 800;
            *nFrameHeight = 600;
            *nFramerate = 30;
            break;
        case 2:
            *nFrameWidth = 800;
            *nFrameHeight = 600;
            *nFramerate = 60;
            break;
        case 4:
            *nFrameWidth = 1024;
            *nFrameHeight = 768;
            *nFramerate = 30;
            break;
        case 8:
            *nFrameWidth = 1024;
            *nFrameHeight = 768;
            *nFramerate = 60;
            break;
        case 16:
            *nFrameWidth = 1152;
            *nFrameHeight = 864;
            *nFramerate = 30;
            break;
        case 32:
            *nFrameWidth = 1152;
            *nFrameHeight = 864;
            *nFramerate = 60;
            break;
        case 64:
            *nFrameWidth = 1280;
            *nFrameHeight = 768;
            *nFramerate = 30;
            break;
        case 128:
            *nFrameWidth = 1280;
            *nFrameHeight = 768;
            *nFramerate = 60;
            break;
        case 256:
            *nFrameWidth = 1280;
            *nFrameHeight = 800;
            *nFramerate = 30;
            break;

        case 512:
            *nFrameWidth = 1280;
            *nFrameHeight = 800;
            *nFramerate = 60;
            break;
        case 1024:
            *nFrameWidth = 1360;
            *nFrameHeight = 768;
            *nFramerate = 30;
            break;
        case 2048:
            *nFrameWidth = 1360;
            *nFrameHeight = 768;
            *nFramerate = 60;
            break;
        case 4096:
            *nFrameWidth = 1366;
            *nFrameHeight = 768;
            *nFramerate = 30;
            break;
        case 8192:
            *nFrameWidth = 1366;
            *nFrameHeight = 768;
            *nFramerate = 60;
            break;
        case 16384:
            *nFrameWidth = 1280;
            *nFrameHeight = 1024;
            *nFramerate = 30;
            break;
        case 32768:
            *nFrameWidth = 1280;
            *nFrameHeight = 1024;
            *nFramerate = 60;
            break;
        case 65536:
            *nFrameWidth = 1400;
            *nFrameHeight = 1050;
            *nFramerate = 30;
            break;
        case 131072:
            *nFrameWidth = 1400;
            *nFrameHeight = 1050;
            *nFramerate = 60;
            break;
        case 262144:
            *nFrameWidth = 1440;
            *nFrameHeight = 900;
            *nFramerate = 30;
            break;

        case 524288:
            *nFrameWidth = 1440;
            *nFrameHeight = 900;
            *nFramerate = 60;
            break;
        case 1048576:
            *nFrameWidth = 1600;
            *nFrameHeight = 900;
            *nFramerate = 30;
            break;
        case 2097152:
            *nFrameWidth = 1600;
            *nFrameHeight = 900;
            *nFramerate = 60;
            break;
        case 4194304:
            *nFrameWidth = 1600;
            *nFrameHeight = 1200;
            *nFramerate = 30;
            break;
        case 8388608:
            *nFrameWidth = 1600;
            *nFrameHeight = 1200;
            *nFramerate = 60;
            break;
        case 16777216:
            *nFrameWidth = 1680;
            *nFrameHeight = 1024;
            *nFramerate = 30;
            break;
        case 33554432:
            *nFrameWidth = 1680;
            *nFrameHeight = 1024;
            *nFramerate = 60;
            break;
        case 67108864:
            *nFrameWidth = 1680;
            *nFrameHeight = 1050;
            *nFramerate = 30;
            break;
        case 134217728:
            *nFrameWidth = 1680;
            *nFrameHeight = 1050;
            *nFramerate = 60;
            break;
        case 268435456:
            *nFrameWidth = 1920;
            *nFrameHeight = 1200;
            *nFramerate = 30;
            break;
        case 536870912:
            *nFrameWidth = 1920;
            *nFrameHeight = 1200;
            *nFramerate = 60;
            break;
        case 1073741824:
            *nFrameWidth = 2560;
            *nFrameHeight = 1600;
            *nFramerate = 30;
            break;
        case 2147483648:
            *nFrameWidth = 2560;
            *nFrameHeight = 1600;
            *nFramerate = 60;
            break;
        case 4294967296:
            *nFrameWidth = 4096;
            *nFrameHeight = 2160;
            *nFramerate = 30;
            break;
        case 8589934592:
            *nFrameWidth = 4096;
            *nFrameHeight = 2160;
            *nFramerate = 60;
            break;

        default:
            *nFrameWidth = 800;
            *nFrameHeight = 600;
            *nFramerate = 30;
            break;
        }
    }
    else
    {
        MM_MSG_PRIO(MM_GENERAL, MM_PRIO_HIGH,
            "WFDMMSource::No Modes are available setting  default values");
        *nFrameWidth = 640;
        *nFrameHeight = 480;
        *nFramerate = 60;
    }
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
void WFDMMSink::FBD(void* handle, int moduleIdx, int trackID,
                             OMX_BUFFERHEADERTYPE *pBuffer)
{
    if(handle && pBuffer)
    {
        WFDMMSink *pMe = (WFDMMSink*)handle;

        pMe->processFBD(moduleIdx, trackID, pBuffer);

    }
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
void WFDMMSink::processFBD(int moduleIdx, int trackID,
                           OMX_BUFFERHEADERTYPE *pBuffer)
{

    CRITICAL_SECT_ENTER_FBD

    switch(moduleIdx)
    {
    case SINK_MODULE_MEDIASOURCE:
        if(!IS_MM_STATE_PLAY)
        {
            WFDMMLOGE("Not in PLAY state give buffer back to MediaSrc");
            m_pMediaSrc->setFreeBuffer(trackID,
                                       pBuffer);
            break;
        }
        if(trackID == SINK_VIDEO_TRACK_ID)
        {
            if(!m_pVideoDecode ||
                m_pVideoDecode->DeliverInput(pBuffer) != OMX_ErrorNone)
            {
                /*--------------------------------------------------------------
                 In the event that deliverInput fails, make sure to send the
                 Buffer back to MediaSource. At the end of the session, media
                 source is where all buffers must be
                ----------------------------------------------------------------
                */
                WFDMMLOGE("Video Decoder not avail. Push Back");
                m_pMediaSrc->setFreeBuffer(trackID,
                                       pBuffer);
            }

        }
        else if(trackID == SINK_AUDIO_TRACK_ID)
        {
#ifdef USE_OMX_AAC_CODEC
            if(m_pNegotiatedCap->audio_method == WFD_AUDIO_AAC)
            {
                if(!m_pAudioDecode ||
                    m_pAudioDecode->DeliverInput(pBuffer) != OMX_ErrorNone)
                {
                    /*----------------------------------------------------------
                     If DeliverInput on Renderer fails return the buffer back to
                     Media Source
                    ------------------------------------------------------------
                    */
                    WFDMMLOGH("Deliver Audio To AudioDecode Failed");
                    m_pMediaSrc->setFreeBuffer(trackID,
                                               pBuffer);
                }
            }
            else
#endif
            if(!m_pAudioRenderer ||
                m_pAudioRenderer->DeliverInput(trackID,pBuffer) != OMX_ErrorNone)
            {
                /*--------------------------------------------------------------
                 If DeliverInput on Renderer fails return the buffer back to
                 Media Source
                ----------------------------------------------------------------
                */
                WFDMMLOGH("Deliver Audio To Renderer Failed");
                m_pMediaSrc->setFreeBuffer(trackID,
                                           pBuffer);
            }
        }
        else if(trackID == SINK_GENERIC_TRACK_ID)
        {
            /* Deliver Sample to Generic Renderer */
            WFDMMLOGH("Deliver Generic Sample To Renderer");
            if(!m_pGenericRenderer ||
                m_pGenericRenderer->DeliverInput(trackID,pBuffer) != OMX_ErrorNone)
            {
                /*--------------------------------------------------------------
                 If DeliverInput on Renderer fails return the buffer back to
                 Media Source
                ----------------------------------------------------------------
                */
                WFDMMLOGH("Deliver Generic To Renderer Failed");
                m_pMediaSrc->setFreeBuffer(trackID,
                                           pBuffer);
            }
        }
        break;
    case SINK_MODULE_VIDEO_DECODER:
        WFDMMLOGD("VideoDecoder FDB Push to Renderer");

        if(pBuffer && pBuffer->nFilledLen)
        {
            if((pBuffer->nFlags & OMX_BUFFERFLAG_SYNCFRAME))
            {
                /*--------------------------------------------------------------
                 Every IFrame information that is recvd from decoder is supplied
                 to mediasource so that it can take necessary decisions.
                 ---------------------------------------------------------------
                */
                WFDMMLOGD("FrameInfo: IFrame. Notify MediaSrc");
                m_pMediaSrc->videoFrameBufferInfo(FRAME_INFO_I_FRAME);
                m_bNotifyFirstFrameInfo = false;
            }
            else if(m_bNotifyFirstFrameInfo)
            {
                /*--------------------------------------------------------------
                 Convey only first PFrame info (after session start/subsequent
                 resumptions) to mediasource
                ----------------------------------------------------------------
                */
                m_bNotifyFirstFrameInfo = false;
                WFDMMLOGD("First FrameInfo: Not an IFrame. Notify MediaSrc");
                m_pMediaSrc->videoFrameBufferInfo(FRAME_INFO_P_FRAME);
            }
        }

        if(m_bPendingIDRReq && pBuffer &&
           (pBuffer->nFlags & OMX_BUFFERFLAG_SYNCFRAME) &&
           !(pBuffer->nFlags & OMX_BUFFERFLAG_DATACORRUPT))
        {
            /*------------------------------------------------------------------
              Now that we have received IDR we can reset the pending request
            --------------------------------------------------------------------
            */
            WFDMMLOGD("IDRReq Found IDR cancel Pending");
            m_bPendingIDRReq = false;
        }

        if(pBuffer && pBuffer->nFlags & OMX_BUFFERFLAG_STARTTIME)
        {
            m_nFrameCntDownStartTime = 0;
        }
        if(m_nFrameCntDownStartTime)
        {
            m_nFrameCntDownStartTime--;
            if(!m_nFrameCntDownStartTime && pBuffer)
            {
                pBuffer->nFlags |= OMX_BUFFERFLAG_STARTTIME;
            }
        }

        if(!m_pVideoRenderer ||
            m_pVideoRenderer->DeliverInput(trackID, pBuffer) != OMX_ErrorNone)
        {
            /*------------------------------------------------------------------
             If we are not able to push the frame to rendere we need to return
             the buffer to video decoder at all cost. Failing to do so may
             cause stop sequence to get stuck
            --------------------------------------------------------------------
            */
            WFDMMLOGE("Failed to Q Video Bufer to Renderer");
            m_pVideoDecode->SetFreeBuffer(pBuffer);
        }
        WFDMMLOGD("VideoDecoder FDB Pushed to Renderer");

        break;
    case SINK_MODULE_AUDIO_DECODER:
#ifdef USE_OMX_AAC_CODEC
        if(!IS_MM_STATE_PLAY || !m_pAudioRenderer)
        {
            if(m_pAudioDecode)
            {
                m_pAudioDecode->SetFreeBuffer(pBuffer);
            }
        }
        if(!m_pAudioRenderer ||
            m_pAudioRenderer->DeliverInput(trackID, pBuffer) != OMX_ErrorNone)
        {
            /*------------------------------------------------------------------
             If we are not able to push the frame to rendere we need to return
             the buffer to video decoder at all cost. Failing to do so may
             cause stop sequence to get stuck
            --------------------------------------------------------------------
            */
            WFDMMLOGE("Failed to Q Audio Bufer to Renderer");
            if(m_pAudioDecode)
            {
                m_pAudioDecode->SetFreeBuffer(pBuffer);
            }
        }
        WFDMMLOGD("AudioDecoder FDB Pushed to Renderer");
#endif
        break;
    case SINK_MODULE_SELF:
        WFDMMLOGE("WFDMMSink calling FBD");
        break;
    case SINK_MODULE_RENDERER:
    default:
        /*----------------------------------------------------------------------
         renderer you cant call this.
        ------------------------------------------------------------------------
        */
        break;
    }
    CRITICAL_SECT_LEAVE_FBD
}

/*==============================================================================

         FUNCTION:          EBD

         DESCRIPTION:       Empty Buffer Done callback from various modules
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
void WFDMMSink::EBD(void* handle, int moduleIdx, int trackID,
                    OMX_BUFFERHEADERTYPE *pBuffer)
{
    if(handle && pBuffer)
    {
        WFDMMSink *pMe = (WFDMMSink*)handle;

        pMe->processEBD(moduleIdx, trackID, pBuffer);
    }
}

/*==============================================================================

         FUNCTION:          processEBD

         DESCRIPTION:
*//**       @brief          processes Empty Buffer Done calls and send the
                            buffers to other modules for processing
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
void WFDMMSink::processEBD(int moduleIdx, int trackID,
                           OMX_BUFFERHEADERTYPE *pBuffer)
{

    CRITICAL_SECT_ENTER_EBD

    switch(moduleIdx)
    {
    case SINK_MODULE_RENDERER:
        if(trackID == SINK_VIDEO_TRACK_ID)
        {
            if(m_pVideoDecode)
            {
                m_pVideoDecode->SetFreeBuffer(pBuffer);
            }
            else
            {
                WFDMMLOGE("Decoder is null. Cant push buffer");
            }
        }
        else if(trackID == SINK_AUDIO_TRACK_ID)
        {
#ifdef USE_OMX_AAC_CODEC
            if(m_pNegotiatedCap->audio_method == WFD_AUDIO_AAC)
            {
                if(m_pAudioDecode)
                {
                    m_pAudioDecode->SetFreeBuffer(pBuffer);
                }
            }
            else
#endif
            if(m_pMediaSrc)
            {
                m_pMediaSrc->setFreeBuffer(trackID, pBuffer);
            }
            else
            {
                WFDMMLOGE("m_pMediaSrc is null. Cant push buffer");
            }
        }
        else if(trackID == SINK_GENERIC_TRACK_ID)
        {
            if(m_pMediaSrc)
            {
                m_pMediaSrc->setFreeBuffer(trackID, pBuffer);
            }
            else
            {
                WFDMMLOGE("m_pMediaSrc is null. Cant push buffer");
            }
        }
        break;
    case SINK_MODULE_VIDEO_DECODER:
        if(m_pMediaSrc)
        {
            /*------------------------------------------------------------------
               Temp Hack until video fix pass through of starttime
            --------------------------------------------------------------------
            */
            if(pBuffer->nFlags & OMX_BUFFERFLAG_STARTTIME)
            {
                m_nFrameCntDownStartTime = 2;
            }
            m_pMediaSrc->setFreeBuffer(SINK_VIDEO_TRACK_ID, pBuffer);
        }
        else
        {
            WFDMMLOGE("MediaSource is null. Push Buffer back to decoder");
        }
        break;
    case SINK_MODULE_AUDIO_DECODER:
        if(m_pMediaSrc)
        {
            m_pMediaSrc->setFreeBuffer(SINK_AUDIO_TRACK_ID, pBuffer);
        }
        else
        {
            WFDMMLOGE("MediaSource is null. Push Buffer back to decoder");
        }
        break;
    case SINK_MODULE_SELF:
        WFDMMLOGE("WFDMMSink Calling EBD at cleanup");
        break;
    case SINK_MODULE_MEDIASOURCE:
        /*----------------------------------------------------------------------
         mediasource cant call this.
        ------------------------------------------------------------------------
        */
        WFDMMLOGE("Invalid EBD from mediasource");
    default:
        break;
    }
    CRITICAL_SECT_LEAVE_EBD
}


/*==============================================================================

         FUNCTION:         eventHandler

         DESCRIPTION:
*//**       @brief         Static function that handles events from source
                           modules and mux


*//**

@par     DEPENDENCIES:      None

*//*
         PARAMETERS:
*//**       @param        pThis - this pointer
                          nModuleId - Id of the module reporting event
                          nEvent - Type of event
                          nStatus - status associated with event
                          nData  - More information about event


*//*     RETURN VALUE:
*//**       @return       None


@par     SIDE EFFECTS:

*//*==========================================================================*/
void WFDMMSink::eventHandlerCb(void *pThis, OMX_U32 nModuleId,
                               WFDMMSinkEvent nEvent,
                               OMX_ERRORTYPE nStatus, int nData)
{
    if(!pThis)
    {
        MM_MSG_PRIO(MM_GENERAL, MM_PRIO_ERROR,
                     "Invalid Me, can't handle device callback");
        return;
    }

    ((WFDMMSink *)pThis)->eventHandler(nModuleId,
                      nEvent, nStatus, nData);
    return;
}

/*==============================================================================

         FUNCTION:         eventHandler

         DESCRIPTION:
*//**       @brief         Handles events from source modules and mux


*//**

@par     DEPENDENCIES:      None

*//*
         PARAMETERS:
*//**       @param        nModuleId - id of the module reporting event
                          nEvent - Type of event
                          nStatus - status associated with event
                          nData  - More information about event


*//*     RETURN VALUE:
*//**       @return       None


@par     SIDE EFFECTS:

*//*==========================================================================*/
void WFDMMSink::eventHandler(OMX_U32 nModuleId,
                      WFDMMSinkEvent nEvent,
                      OMX_ERRORTYPE nStatus, int nData)
{
    (void)nStatus;
    switch(nEvent)
    {
    case WFDMMSINK_ERROR:
        if(nModuleId != SINK_MODULE_HDCP)
        {
            if(m_bMMTearingDown)
            {
                WFDMMLOGH("ErrorCallback. Already tearing down, Ignore");
                /*------------------------------------------------------------------
                 Set the flag MM tearing down to true to avoid multiple error
                 callbacks from flooding SM-A. The variable will be reset to false
                 when MM session is torn down
                --------------------------------------------------------------------
                */
                return;
            }
            m_bMMTearingDown = true;
        }
        WFDMMLOGH1("Error callback from module %ld", nModuleId);

        if(nModuleId == SINK_MODULE_AUDIO_DECODER)
        {
            if(sCallBacks.update_event_cb)
            {
                sCallBacks.update_event_cb(WFD_EVENT_MM_AUDIO,
                                           WFD_STATUS_RUNTIME_ERROR, NULL);
            }
        }
        else if(nModuleId == SINK_MODULE_VIDEO_DECODER)
        {
            if(sCallBacks.update_event_cb)
            {
                sCallBacks.update_event_cb(WFD_EVENT_MM_VIDEO,
                                           WFD_STATUS_RUNTIME_ERROR, NULL);
            }
        }
        else if(nModuleId == SINK_MODULE_MEDIASOURCE)
        {
            if(sCallBacks.update_event_cb)
            {
                sCallBacks.update_event_cb(WFD_EVENT_MM_RTP,
                                           WFD_STATUS_RUNTIME_ERROR, NULL);
            }
        }
        else if(nModuleId == SINK_MODULE_RENDERER)
        {
            if(sCallBacks.update_event_cb)
            {
                sCallBacks.update_event_cb(WFD_EVENT_MM_VIDEO,
                                           WFD_STATUS_RUNTIME_ERROR, NULL);
            }
        }
        else if(nModuleId == SINK_MODULE_HDCP)
        {
            if(sCallBacks.update_event_cb)
            {
                sCallBacks.update_event_cb(WFD_EVENT_MM_HDCP,
                                           WFD_STATUS_RUNTIME_ERROR, NULL);
            }
        }
        break;

    case WFDMMSINK_PACKETLOSS:
        {
            CRITICAL_SECT_ENTER_EVT;
            sCallBacks.idr_cb(this);

            CRITICAL_SECT_LEAVE_EVT;
        }
        break;

    case WFDMMSINK_DECRYPT_FAILURE:
        {
            if(nModuleId == SINK_MODULE_MEDIASOURCE)
            {
              WFDMMLOGH("Decrypt Failure is Ignored");
            }
        }
        break;

    case WFDMMSINK_VIDEO_RECONFIGURE:
        {
            WFDMMLOGH("Sink received VIDEO_RECONFIGURE event");
            if(nModuleId == SINK_MODULE_MEDIASOURCE)
            {
                if(!m_bVideoReConfNeeded)
                {
                    WFDMMLOGH("WFDMMSink:: Skipping Video Reconfiguration");
                    /*Notify MediaSource to restart fetching Video Samples since
                      we have prematurely paused it */
                    if(m_pMediaSrc)
                    {
                        m_pMediaSrc->restartVideoThread();
                    }
                    else
                    {
                        WFDMMLOGE("MediaSource absent!");
                        SET_MM_ERROR_FAIL;
                    }
                }
                else if(!reconfigureDecoder(SINK_VIDEO_TRACK_ID))
                {
                    /* Delete and re-create new video decoder*/
                    WFDMMLOGE("Sink Failed to reconfigure video Decoder");
                    SET_MM_ERROR_FAIL;
                }
            }
        }
        break;

        case WFDMMSINK_AUDIO_RECONFIGURE:
        {
            WFDMMLOGH("Sink received AUDIO_RECONFIGURE event");
            if(nModuleId == SINK_MODULE_MEDIASOURCE)
            {
                /* Delete and re-create new audio decoder*/
                if(!reconfigureDecoder(SINK_AUDIO_TRACK_ID))
                {
                    WFDMMLOGE("Sink Failed to reconfigure audio Decoder");
                    SET_MM_ERROR_FAIL;
                }
            }
        }
        break;

    case WFDMMSINK_RENDERER_RECONFIGURE:
        WFDMMLOGH("Sink received WFDMMSINK_RENDERER_RECONFIGURE event");
        if(nModuleId == SINK_MODULE_VIDEO_DECODER)
        {
            if(ExecuteCommandAsync(RECONFIGURE_VIDEO_RENDERER) != true)
            {
                WFDMMLOGE("Failed to reconfigure Video renderer");
            }
        }
        else
        {
            WFDMMLOGH1("WFDMMSINK_RENDERER_RECONFIGURE cannot be called from \
                                                    module: %d", nModuleId);
        }
        break;
    default:
        /*Handle an Unknown Event???? Give me a*/break; //TODO

    }

}


/*==============================================================================

         FUNCTION:          ExecuteCommandASync

         DESCRIPTION:
*//**       @brief          Executes commands asynchronously. Doesn't
                            block the caller until done
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
bool WFDMMSink::ExecuteCommandAsync(unsigned int command)
{

    WFDMMLOGH("WFDMMSink ExecuteCommandASync");

    if(m_nMMStatus != WFD_STATUS_SUCCESS && command != DEINIT)
    {
        /*----------------------------------------------------------------------
         Already in an error
        ------------------------------------------------------------------------
        */
        WFDMMLOGE("State change not allowed in Error");
        return false;
    }

    WFDMMLOGH1("Posting event[%d] on WFDMMSink CmdThread",command);
    if(m_pCmdThread)
    {
        m_pCmdThread->SetSignal(command);
    }

    return true;
}

/*==============================================================================

         FUNCTION:          ExecuteCommandSync

         DESCRIPTION:
*//**       @brief          Executes commands synchronously. Blocks the caller
                            until done
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
bool WFDMMSink::ExecuteCommandSync(unsigned int command)
{
    CRITICAL_SECT_ENTER

    WFDMMLOGH("WFDMMSink ExecuteCOmmandSync");

    if(m_nMMStatus != WFD_STATUS_SUCCESS && command != DEINIT)
    {
        /*----------------------------------------------------------------------
         Already in an error
        ------------------------------------------------------------------------
        */
        WFDMMLOGE("State change not allowed in Error");
        CRITICAL_SECT_LEAVE
        return false;
    }

    if(m_pCmdThread)
    {
        m_pCmdThread->SetSignal(command);
    }
    switch(command)
    {
    case DEINIT:
        while(m_eMMState != WFD_SINK_MM_STATE_DEINIT &&
               m_nMMStatus == WFD_STATUS_SUCCESS)
        {
            MM_Timer_Sleep(1);
        }
        break;
    case INIT:
        while(m_eMMState != WFD_SINK_MM_STATE_INIT &&
              m_nMMStatus == WFD_STATUS_SUCCESS)
        {
            MM_Timer_Sleep(1);
        }
        break;

    case PLAY:
        while(m_eMMState != WFD_SINK_MM_STATE_PLAY &&
              m_nMMStatus == WFD_STATUS_SUCCESS)
        {
            MM_Timer_Sleep(1);
        }
        break;
    case STOP:
        while(m_eMMState == WFD_SINK_MM_STATE_PLAY &&
              m_nMMStatus == WFD_STATUS_SUCCESS)
        {
            MM_Timer_Sleep(1);
        }
        break;
    default:
        WFDMMLOGE("Unknown command for state change");
    }

    if(m_nMMStatus != WFD_STATUS_SUCCESS)
    {
        WFDMMLOGE("State change command Failed");
        CRITICAL_SECT_LEAVE;
        return false;
    }
    CRITICAL_SECT_LEAVE
    return true;
}

/*==============================================================================

         FUNCTION:         decryptCb

         DESCRIPTION:
*//**       @brief          This functions decrypt HDCP encrypted data
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
bool WFDMMSink::decryptCb(void* handle, int streamType, int input, int output,
                          int size, char* pIV, int IVSize)
{
    if(!handle)
    {
        WFDMMLOGE("Decrypt call without a valid handle");
        return false;
    }

    WFDMMSink *pMe = (WFDMMSink *)handle;

    return pMe->decrypt(input, output, size, pIV, IVSize,
          streamType == SINK_AUDIO_TRACK_ID ? HDCP_AUDIO_TRACK_ID:
                                              HDCP_VIDEO_TRACK_ID);

}

/*==============================================================================

         FUNCTION:          decrypt

         DESCRIPTION:
*//**       @brief          Processes the decrypt call back and passed data
                            to HDCP library
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
bool WFDMMSink::decrypt(int input, int output,
                        int size, char* pIV, int IVSize, int nStreamId)
{
    if(!IS_DRM_STATE_ACQUIRED || !m_pHDCPManager)
    {
        WFDMMLOGE("WFDMMSink No Drm State Acquired");
        return false;
    }

    if(!input || !output || !size)
    {
        WFDMMLOGE("Invalid arguments in decrypt");
        return false;
    }


    unsigned char *pIVdata = IVSize ? (unsigned char*)pIV : NULL;

    HDCPStatusType eRet = m_pHDCPManager->decrypt(pIVdata,
                              (unsigned char*)(int64)input,
                              (unsigned char*)(int64)output, size, nStreamId);
    if(eRet == HDCP_SUCCESS)
    {
        WFDMMLOGD("Decrypt Success");
        return true;
    }
    else if(eRet == HDCP_UNAUTHENTICATED_CONNECTION)
    {
        if(sCallBacks.update_event_cb)
        {
            sCallBacks.update_event_cb(WFD_EVENT_MM_HDCP,
                                       WFD_STATUS_RUNTIME_ERROR, NULL);
        }
    }

    WFDMMLOGD("Decrypt Fail");
    return false;
}

/*==============================================================================

         FUNCTION:          avInfoCb

         DESCRIPTION:
*//**       @brief          On idenitifying tracks in stream MediaSource sends
                            this callback. This also carries AV Sync start
                            time
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
void WFDMMSink::avInfoCb(void* handle, avInfoType *pInfo)
{
    WFDMMSink *pMe = (WFDMMSink*)handle;

    if(!pMe)
    {
        WFDMMLOGE("avInfo Cb Invalid Args");
        return;
    }

    return pMe->setAvInfo(pInfo);
}


/*==============================================================================

         FUNCTION:          setAvInfo

         DESCRIPTION:
*//**       @brief          Uses the AV info update from MediaSource to
                            configure other modules.
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
void WFDMMSink::setAvInfo(avInfoType *pInfo)
{
    if(pInfo)
    {
        if(m_pAudioRenderer)
        {
            WFDMMLOGH1("WFDMMSink Setting base time to Audio renderer %llu",
                       pInfo->nBaseTime);
            m_pAudioRenderer->SetMediaBaseTime(pInfo->nBaseTime);
        }

        if(m_pVideoRenderer)
        {
            WFDMMLOGH1("WFDMMSink Setting base time to Video renderer %llu",
                       pInfo->nBaseTime);
            m_pVideoRenderer->SetMediaBaseTime(pInfo->nBaseTime);
        }

        if(m_pGenericRenderer)
        {
            WFDMMLOGH1("WFDMMSink Setting base time to Generic renderer %llu",
                       pInfo->nBaseTime);
            m_pGenericRenderer->SetMediaBaseTime(pInfo->nBaseTime);
        }
        m_nActualBaseTime = pInfo->nBaseTimeStream;
    }
}

/*==============================================================================

         FUNCTION:         getAACAudioParams

         DESCRIPTION:
*//**       @brief         Get AAC Audio parameters


*//**

@par     DEPENDENCIES:      None

*//*
         PARAMETERS:
*//**       @param        supported_audio_mode - supported Audio modes


*//*     RETURN VALUE:
*//**       @return       None


@par     SIDE EFFECTS:

*//*==========================================================================*/
void WFDMMSink::getAACAudioParams(uint32 supported_audio_mode )
{
    WFDMMLOGH("WFDMMSource::getAACAudioParams");
    uint16 temp = (supported_audio_mode & 0X00FFF);
    switch (temp)
    {
    case 1:
        m_nAudioChannels = 2;
        m_nAudioSampleRate = 48000;
        break;
    case 2://Deprecated
        m_nAudioChannels = 4;
        m_nAudioSampleRate = 48000;
        break;
    case 4:
        m_nAudioChannels = 6;
        m_nAudioSampleRate = 48000;
        break;
    case 8:
        m_nAudioChannels = 8;
        m_nAudioSampleRate = 48000;
        break;
    default:
        m_nAudioChannels = 2;
        m_nAudioSampleRate = 48000;
        break;
    }
}

/*==============================================================================

         FUNCTION:         getAC3AudioParams

         DESCRIPTION:
*//**       @brief         Get AC3 Audio parameters


*//**

@par     DEPENDENCIES:      None

*//*
         PARAMETERS:
*//**       @param        supported_audio_mode - supported Audio modes


*//*     RETURN VALUE:
*//**       @return       None


@par     SIDE EFFECTS:

*//*==========================================================================*/
void WFDMMSink::getAC3AudioParams(uint32 supported_audio_mode )
{
    WFDMMLOGH("WFDMMSource::getAC3AudioParams");
    uint16 temp = (supported_audio_mode & 0X00FFF);
    switch (temp)
    {
    case 1:
        m_nAudioChannels = 6;
        m_nAudioSampleRate = 48000;
        break;
    case 2://Deprecated
        m_nAudioChannels = 4;
        m_nAudioSampleRate = 48000;
        break;
    case 4:
        m_nAudioChannels = 6;
        m_nAudioSampleRate = 48000;
        break;
    case 8:
        m_nAudioChannels = 8;
        m_nAudioSampleRate = 48000;
        break;
    default:
        m_nAudioChannels = 6;
        m_nAudioSampleRate = 48000;
        break;
    }
}

/*==============================================================================

         FUNCTION:          destroyMMComponents

         DESCRIPTION:
*//**       @brief          destroys Media Source instance,
                            Video decoder wraper,
                            Audio decoder module,
                            Renderer instance
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
bool WFDMMSink::destroyMMComponents()
{
    bool flag = destroyMediaSource();
    flag &= destroyVideoDecoder();
    flag &= destroyAudioDecoder();
    flag &= destroyAudioRenderer();
    flag &= destroyVideoRenderer();
    flag &= destroyGenericRenderer();
    flag &= destroyDataSource();
    WFDMMLOGM1("WFDMMSource::destroyMMComponents flag = %d",flag);
    return flag;
}

/**

 */
/*==============================================================================

         FUNCTION:          closeRtcpSocket

         DESCRIPTION:
*//**       @brief          close the rtcp socket if valid and invalidate 
                            rtcp params in negociated capabilities
*//**
@par     DEPENDENCIES:
                            None
*//*
         PARAMETERS:
*//**       @param          None

*//*     RETURN VALUE:
*//**       @return
                            void
@par     SIDE EFFECTS:
                            None
*//*==========================================================================*/
void WFDMMSink::closeRtcpSocket()
{
    if(m_pNegotiatedCap)
    {
        WFD_transport_capability_config_t& rNegociated =
            m_pNegotiatedCap->transport_capability_config;

        if(rNegociated.rtcpSock > 0)
        {
            MM_MSG_PRIO1(MM_GENERAL, MM_PRIO_HIGH,
                 "rtcp: close rtcp sockfd %d", rNegociated.rtcpSock);

            close(rNegociated.rtcpSock);
            rNegociated.rtcpSock = -1;
        }

        m_pNegotiatedCap->transport_capability_config.port1_rtcp_id = 0;
    }
}

/*==============================================================================

         FUNCTION:         WFDMMSinkAVControl

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
                            WFD_status_t
@par     SIDE EFFECTS:
                            None
*//*==========================================================================*/
WFD_status_t WFDMMSink::WFDMMSinkAVControl
(
    WFD_MM_AV_Stream_Control_t control,
    int64 controlValue
)
{
    switch(control)
    {
    case AV_CONTROL_PLAY:
        m_bFlushAll = controlValue? true:false;
        if(m_bUpdatedSurface)
        {
            m_bUpdatedSurface = false;
            WFDMMLOGE("WFDMMSinkAVControl -- Move to INIT and then PLAY");
            if(ExecuteCommandSync(INIT) != true)
            {
                WFDMMLOGE("Play failed at INIT");
                return WFD_STATUS_FAIL;
            }

            ExecuteCommandSync(STREAM_SET_DECODER_LATENCY);

            if(ExecuteCommandSync(PLAY) != true)
            {
                WFDMMLOGE("Play failed to move to PLAY State");
                return WFD_STATUS_FAIL;
            }
        }
        else if(m_pNegotiatedCap->pSurface != NULL)
        {
            WFDMMLOGE("WFDMMSinkAVControl -- Normal STREAM_PLAY");
            ExecuteCommandSync(STREAM_PLAY);
        }
        else
        {
            WFDMMLOGE("WFDMMSinkAVControl -- surface is null. Abort Session");
            return WFD_STATUS_FAIL;
        }
        break;
    case AV_CONTROL_PAUSE:
        ExecuteCommandSync(STREAM_PAUSE);
        break;
    case AV_CONTROL_FLUSH:
        m_nFlushTimestamp = ((uint64)controlValue * 1000)/90
                                            - m_nActualBaseTime;
        ExecuteCommandSync(STREAM_FLUSH);
        break;
    case AV_CONTROL_SET_VOLUME:
        ExecuteCommandSync(STREAM_SET_VOLUME);
        break;
    case AV_CONTROL_SET_DECODER_LATENCY:
        WFDMMLOGH("Update Decoder Latency");
        m_nDecoderLatency = (uint64)controlValue * 1000;
        ExecuteCommandSync(STREAM_SET_DECODER_LATENCY);
        break;
    default:
        break;
    }
    return WFD_STATUS_SUCCESS;
}

/*==============================================================================

         FUNCTION:         AudioLatFnCallback

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
uint32 WFDMMSink::AudioLatFnCallback(void* handle)
{
    uint32 result = 0;
    if(handle)
    {
        WFDMMSink *pMe = (WFDMMSink *)handle;
        result = pMe->getAudioTrackLatency();
    }

    return result;
}

/*==============================================================================

         FUNCTION:          getAudioTrackLatency

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
                            latency of AudioTrack
@par     SIFE EFFECTS:
                            None
*//*==========================================================================*/
uint32 WFDMMSink::getAudioTrackLatency()
{
    uint32 trackLatency = 0;
    if(m_pAudioRenderer)
    {
        trackLatency = m_pAudioRenderer->getLatency();
    }

    return trackLatency;
}


/*==============================================================================

         FUNCTION:          setSurfaceProp

         DESCRIPTION:
*//**       @brief
*//**
@par     DEPENDENCIES:
                            None
*//*
         PARAMETERS:
*//**       @param          width - width of the area surface is linked to
                            height - height of area surface is linked to
                            orientation - portrait or landscape

*//*     RETURN VALUE:
*//**       @return
                            None
@par     SIFE EFFECTS:
                            None
*//*==========================================================================*/
void WFDMMSink::setSurfaceProp(int width,int height,int orientation)
{
    m_nSurfaceWidth = width;
    m_nSurfaceHeight = height;
    m_nSurfaceOrientation = orientation;
    WFDMMLOGE3("setSurfaceProp : width = %d, height = %d , orientation = %d",
               width,height,orientation);
}

/*==============================================================================

         FUNCTION:          updateAuxData

         DESCRIPTION:
*//**       @brief
*//**
@par     DEPENDENCIES:
                            None
*//*
         PARAMETERS:
*//**       @param          WFD_MM_Aux_capability_t

*//*     RETURN VALUE:
*//**       @return
                            None
@par     SIFE EFFECTS:
                            None
*//*==========================================================================*/
void WFDMMSink::updateAuxData(WFD_MM_Aux_capability_t *pAuxCap)
{
    if(m_pAuxNegotiatedCap)
    {
        m_pAuxNegotiatedCap->count = pAuxCap->count;
        m_pAuxNegotiatedCap->maxOverlay = pAuxCap->maxOverlay;
        m_pAuxNegotiatedCap->blendMode = pAuxCap->blendMode;
        m_pAuxNegotiatedCap->supportedBitMap = pAuxCap->supportedBitMap;
        WFDMMLOGE("AUX Capability Dump");
        WFDMMLOGE3("Codec Count = %d , Max Overlay = %d , blendmode = %d",
                   m_pAuxNegotiatedCap->count,m_pAuxNegotiatedCap->maxOverlay,
                   m_pAuxNegotiatedCap->blendMode);
        for(int i=0 ; i< pAuxCap->count; i++)
        {
            strlcpy(m_pAuxNegotiatedCap->name[i],
                    pAuxCap->name[i],
                    sizeof(m_pAuxNegotiatedCap->name[i]));
            WFDMMLOGE2("Codec[%d] Name = %s",i,m_pAuxNegotiatedCap->name[i]);
        }
    }
}

