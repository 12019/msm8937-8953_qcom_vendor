/*==============================================================================
*       WFDMMSinkVideoDecode.cpp
*
*  DESCRIPTION:
*       Abstracts OMX calls to video decoder and provides more sensible APIs
*
*
*  Copyright (c) 2013-2016 Qualcomm Technologies, Inc.
*  All Rights Reserved.
*  Confidential and Proprietary - Qualcomm Technologies, Inc.
*===============================================================================


/*==============================================================================
**               Includes and Public Data Declarations
**==============================================================================
*/

/* =============================================================================

                     INCLUDE FILES FOR MODULE

================================================================================
*/
#include "WFDMMLogs.h"
#include "WFDMMSinkVideoDecode.h"
#include "WFDMMSourceMutex.h"
#include "WFDMMSourceSignalQueue.h"
#include "MMMalloc.h"
#include "MMMemory.h"
#include "qmmList.h"
#include "MMTimer.h"
#include "OMX_QCOMExtns.h"
#include "WFDMessages.h"
#include "WFDUtils.h"
#include "WFDMMSinkStatistics.h"

#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "WFDMMSINKVIDEODECODE"

using namespace android;

#ifdef SYSTRACE_PROFILE
#define ATRACE_TAG ATRACE_TAG_VIDEO
#include <utils/Trace.h>
#endif

#define MIN_VIDEO_BUFFERS 4
#define MAX_FRAME_RATE 60
#define MAX_FRAME_RATE_4K 30

/*Input buffer size need to be aligned 4KB(for non secure)
  and 1 MB aligned for Secure.
*/
#define MAX_INPUT_BUFFER_SIZE_480P  (1 * 1024 * 1024)
#define MAX_INPUT_BUFFER_SIZE_720P  (2 * 1024 * 1024)
#define MAX_INPUT_BUFFER_SIZE_1080P (3 * 1024 * 1024)
#define MAX_INPUT_BUFFER_SIZE_4K    (7 * 1024 * 1024)

#define MAX_INPUT_RESOLUTION_480P  (640 * 480)
#define MAX_INPUT_RESOLUTION_720P  (1280 * 720)
#define MAX_INPUT_RESOLUTION_1080P (1920 * 1080)
#define MAX_INPUT_RESOLUTION_4K    (4096 * 2160)

#ifndef MAX
#define MAX(x,y) ((x) > (y) ? (x) : (y))
#endif

#ifndef OMX_SPEC_VERSION
#define OMX_SPEC_VERSION 0x00000101
#endif
#define OMX_INIT_STRUCT(_s_, _name_)            \
    memset((_s_), 0x0, sizeof(_name_));          \
    (_s_)->nSize = sizeof(_name_);               \
    (_s_)->nVersion.nVersion = OMX_SPEC_VERSION


#define CRITICAL_SECT_ENTER if(mhCritSect)                                    \
                                  MM_CriticalSection_Enter(mhCritSect);       \
/*      END CRITICAL_SECT_ENTER    */

#define CRITICAL_SECT_LEAVE if(mhCritSect)                                    \
                                  MM_CriticalSection_Leave(mhCritSect);       \
/*      END CRITICAL_SECT_LEAVE    */

#define CRITICAL_SECTION_ENTER(x) if(x)\
                                  MM_CriticalSection_Enter(x)\
/*      END CRITICAL_SECTION_ENTER*/

#define CRITICAL_SECTION_LEAVE(x) if(x)\
                                  MM_CriticalSection_Leave(x)\
/*      END CRITICAL_SECTION_LEAVE*/

#define NOTIFYERROR  mpFnHandler((void*)(uint64)mClientData,                   \
                                 mnModuleId,WFDMMSINK_ERROR,                   \
                                 OMX_ErrorUndefined,                           \
                                 0 );

typedef struct graphBuffer
{
    sp<GraphicBuffer> pGraphicBuffer;
}graphBufferType;

struct CmdType
{
    OMX_EVENTTYPE   eEvent;
    OMX_COMMANDTYPE eCmd;
    OMX_U32         sCmdData;
    OMX_ERRORTYPE   eResult;
};

typedef enum component_commands{
    INITIATE_PORT_RECONFIG,
    PROCESS_PORT_RECONFIG,
    COMPLETE_PORT_RECONFIG,
    OMX_FREE_OUTPUT_BUFFERS
}CommandsType;

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

#define SETSTATECLOSING (state(CLOSING, false))
#define SETSTATEINIT    (state(INIT   , false))
#define SETSTATEDEINIT  (state(DEINIT , false))
#define SETSTATEOPENED  (state(OPENED , false))

/*==============================================================================

         FUNCTION:         WFDMMSinkVideoDecode

         DESCRIPTION:
*//**       @brief         WFDMMSinkVideoDecode contructor
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
WFDMMSinkVideoDecode::WFDMMSinkVideoDecode(int moduleId,
                                     WFDMMSinkEBDType       pFnEBD,
                                     WFDMMSinkFBDType       pFnFBD,
                                     WFDMMSinkHandlerFnType pFnHandler,
                                     void* clientData,
                                     WFDMMSinkStatistics* pStats)
{

    InitData();
    /*--------------------------------------------------------------------------

    ----------------------------------------------------------------------------
    */
    mpFnHandler = pFnHandler;
    mpFnEBD     = pFnEBD;
    mpFnFBD     = pFnFBD;
    mnModuleId  = moduleId;
    mClientData = clientData;
    m_pStatInst = pStats;

    mhCritSect = NULL;
    int nRet = MM_CriticalSection_Create(&mhCritSect);
    if(nRet != 0)
    {
        mhCritSect = NULL;
        WFDMMLOGE("Error in Critical Section Create");
    }
    nRet = MM_CriticalSection_Create(&mhPortReconCritSec);
    if(nRet != 0)
    {
        mhPortReconCritSec = NULL;
        WFDMMLOGE("Error in Port Recon Critical Section Create");
    }

    /* Creating the message service to handle some of the send commands
    to the OMX component in a separate thread */
    mpMsgService = MM_New_Args(WFDMessageService, (this, ReceiveMessage));

    if (!mpMsgService)
    {
        WFDMMLOGE("Failed to create WFDMessageService");
        return;
    }

    SETSTATEDEINIT;

}

/*==============================================================================

         FUNCTION:         ~WFDMMSinkVideoDecode

         DESCRIPTION:       Destructor
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
WFDMMSinkVideoDecode::~WFDMMSinkVideoDecode()
{
    /*--------------------------------------------------------------------------
     Call destroy resources to make sure destructor is not called in the
     middle of play
    ----------------------------------------------------------------------------
    */
    WFDMMLOGH("Entering ~WFDMMSinkVideoDecode ");
    destroyResources();

    WFDMMLOGH("Done with destroyResources of Sink VDEC");

    if (mpMsgService)
    {
        MM_Delete(mpMsgService);
        mpMsgService = NULL;
    }

    if(mhPortReconCritSec)
    {
        MM_CriticalSection_Release(mhPortReconCritSec);
        mhPortReconCritSec = NULL;
    }

    if (mhCritSect)
    {
        MM_CriticalSection_Release(mhCritSect);
        mhCritSect = NULL;
    }
    WFDMMLOGH("WFDMMSinkVideoDecode Destructor...Done");
}

/*==============================================================================

         FUNCTION:          InitData

         DESCRIPTION:       Initializes class variables
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
void WFDMMSinkVideoDecode::InitData()
{
    memset(&mCfg, 0, sizeof(mCfg));
    mpFnHandler = NULL;
    mpFnFBD = NULL;
    mpFnEBD = NULL;
    mOutputBufCollectorQ = NULL;
    mVideoOutputQ = NULL;
    mVideoInputQ = NULL;
    mGraphicBufQ = NULL;
    mSignalQ = NULL;
    mpWindow = NULL;
    mnOutputPortIndex = 0;
    mnInputPortIndex = 0;
    mnNumInputBuffers = 0;
    mnNumOutputBuffers = 0;
    mnNumDequeuedBufs = 0;
    mPortReconfigInProgress = 0;
    meColorFmt = OMX_COLOR_FormatYUV420SemiPlanar;
    mhVdec = NULL;
    meCompressionFmt = OMX_VIDEO_CodingAVC;
    mnModuleId = 0;
    mClientData = 0;
    mUsage = 0;
    mhCritSect = NULL;
    mhPortReconCritSec = NULL;
    meState = DEINIT;
    mbInputPortFound = false;
    mbOutputPortFound = false;
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
OMX_ERRORTYPE WFDMMSinkVideoDecode::Configure(videoConfigType *pCfg)
{
    CHECK_ARGS_RET_OMX_ERR_4(pCfg,
                             pCfg->nFrameWidth, pCfg->nFrameHeight, mpFnFBD);

    memcpy(&mCfg, pCfg, sizeof(videoConfigType));

    mpWindow = pCfg->pWindow;

    if(mpWindow == NULL)
    {
        WFDMMLOGE("Invalid Native Window pointer");
        NOTIFYERROR;
        return OMX_ErrorBadParameter;
    }

    SETSTATEINIT;
    if(createResources() != OMX_ErrorNone)
    {
        WFDMMLOGE("VideoDec Failed to createResources");
        NOTIFYERROR;
        return OMX_ErrorInsufficientResources;
    }
    return OMX_ErrorNone;
}

/*==============================================================================

         FUNCTION:          AllocateVideoBuffers

         DESCRIPTION:
*//**       @brief         Allocates Video Buffers
*//**
@par     DEPENDENCIES:
                            None
*//*
         PARAMETERS:
*//**       @param          None

*//*     RETURN VALUE:
*//**       @return
                            OMX_ErrorNone or other Errors
@par     SIFE EFFECTS:
                            None
*//*==========================================================================*/
OMX_ERRORTYPE WFDMMSinkVideoDecode::AllocateVideoBuffers()
{
    OMX_ERRORTYPE eErr = OMX_ErrorNone;

    eErr = allocateVideoInputBuffers();

    if (eErr != OMX_ErrorNone)
    {
        WFDMMLOGE("Failed to allocate Input Buffers");
        return eErr;
    }

    eErr = allocateVideoOutputBuffers();

    if (eErr != OMX_ErrorNone)
    {
        WFDMMLOGE("Failed to allocate Output Buffers");
        return eErr;
    }

    return eErr;
}

/*==============================================================================

         FUNCTION:          allocateVideoInputBuffers

         DESCRIPTION:
*//**       @brief          allocates Video Input Buffers OMX component allocs
                            the buffers.
*//**
@par     DEPENDENCIES:
                            None
*//*
         PARAMETERS:
*//**       @param          None

*//*     RETURN VALUE:
*//**       @return
                            OMX_ErrorNone or other Error
@par     SIFE EFFECTS:
                            None
*//*==========================================================================*/
OMX_ERRORTYPE WFDMMSinkVideoDecode::allocateVideoInputBuffers()
{
    OMX_ERRORTYPE eErr = OMX_ErrorNone;

    /*--------------------------------------------------------------------------
      We will use allocate Buffer model to let the decoder decide the buffers it
      needs
    ----------------------------------------------------------------------------
    */

    mnNumInputBuffers = MAX(mnNumInputBuffers, MIN_VIDEO_BUFFERS);

    OMX_PARAM_PORTDEFINITIONTYPE sPortDef;
    OMX_INIT_STRUCT(&sPortDef, OMX_PARAM_PORTDEFINITIONTYPE);

    sPortDef.nPortIndex = mnInputPortIndex;
    eErr = OMX_GetParameter(mhVdec,
                                OMX_IndexParamPortDefinition,
                                (OMX_PTR) &sPortDef);

    if (eErr != OMX_ErrorNone)
    {
        WFDMMLOGE("Failed to get PortDef while allocating buf");
        return eErr;
    }

    /*--------------------------------------------------------------------------
      Update the new number of buffers
    ----------------------------------------------------------------------------
    */
    sPortDef.nBufferCountActual = mnNumInputBuffers;
    eErr = OMX_SetParameter(mhVdec,
                                OMX_IndexParamPortDefinition,
                                (OMX_PTR) &sPortDef);

    if (eErr != OMX_ErrorNone)
    {
        WFDMMLOGE("Failed to Set PortDef while allocating buf");
        return eErr;
    }

    /*--------------------------------------------------------------------------
     Create Buffers one by one and maintain locally in a queue. Will be useful
     when freeing the buffers
    ----------------------------------------------------------------------------
    */
    OMX_BUFFERHEADERTYPE *pBuffer;

    for (unsigned int i = 0; i < mnNumInputBuffers; i++)
    {
        eErr = OMX_AllocateBuffer(mhVdec,
                                 &pBuffer,
                                  mnInputPortIndex,
                                  NULL,
                                  sPortDef.nBufferSize);

        if(eErr != OMX_ErrorNone)
        {
            WFDMMLOGE("Failed to allocateBuffer");
            return eErr;
        }

        buff_hdr_extra_info_sink* extra =
        static_cast<buff_hdr_extra_info_sink*>(MM_Calloc(1,sizeof(*extra)));

        pBuffer->pOutputPortPrivate = static_cast<OMX_PTR>(extra);

        mVideoInputQ->Push(&pBuffer, sizeof(&pBuffer));
    }

    return eErr;
}

/*==============================================================================

         FUNCTION:          allocateVideoOutputBuffers

         DESCRIPTION:
*//**       @brief          Allocates output buffers using native window apis
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
OMX_ERRORTYPE WFDMMSinkVideoDecode::allocateVideoOutputBuffers()
{
    OMX_ERRORTYPE eErr = OMX_ErrorUndefined;

    OMX_PARAM_PORTDEFINITIONTYPE sPortDef;

    mnNumOutputBuffers = mnNumDequeuedBufs = 0; // Reset in case the API's fail

    OMX_INIT_STRUCT(&sPortDef, OMX_PARAM_PORTDEFINITIONTYPE);

    if(mpWindow == NULL)
    {
        WFDMMLOGE("Invalid Surface set to the renderer");
        return OMX_ErrorBadParameter;
    }

    sPortDef.nPortIndex = mnOutputPortIndex;

    eErr = OMX_GetParameter(mhVdec,
                            OMX_IndexParamPortDefinition,
                            (OMX_PTR) &sPortDef);

    if (eErr != OMX_ErrorNone)
    {
        WFDMMLOGE("Failed to get Output PortDef");
        return eErr;
    }

    /* Update the parameters from the new port definition */
    mCfg.nFrameHeight = sPortDef.format.video.nFrameHeight;
    mCfg.nFrameWidth  = sPortDef.format.video.nFrameWidth;
    meColorFmt = sPortDef.format.video.eColorFormat;

    mnNumOutputBuffers = sPortDef.nBufferCountMin;

    /*--------------------------------------------------------------------------
     Correctly account for number of buffers after port reconfiguration has been
     initiated. Helps in reducing buffer count during initial decoder creation
     and thereby reducing memory footprint.
    ----------------------------------------------------------------------------
    */
    if(mPortReconfigInProgress)
    {
        mnNumOutputBuffers += 4;
    }

    OMX_INIT_STRUCT(&sPortDef, OMX_PARAM_PORTDEFINITIONTYPE);

    /*--------------------------------------------------------------------------
     Disconnect any connection with window
    ----------------------------------------------------------------------------
    */
    int nErr = native_window_api_disconnect(mpWindow.get(),
                                  NATIVE_WINDOW_API_MEDIA);
    if (nErr)
    {
        WFDMMLOGE("Warning Failed to disconnect from Native Window");
    }

    /*--------------------------------------------------------------------------
     Disconnect any software rendering to the window
    ----------------------------------------------------------------------------
    */
    nErr = native_window_api_disconnect(mpWindow.get(),
                                  NATIVE_WINDOW_API_CPU);
    if (nErr)
    {
        WFDMMLOGE("Warning Failed to disconnect from Native Window");
    }


    nErr = native_window_api_connect(mpWindow.get(),
                                  NATIVE_WINDOW_API_MEDIA);
    if(nErr)
    {
        WFDMMLOGH("Warning Failed to connect to native window");
    }
    /*--------------------------------------------------------------------------
      Allocate graphics buffers
    ----------------------------------------------------------------------------
    */
    nErr = native_window_set_buffers_geometry(mpWindow.get(),
                                              mCfg.nFrameWidth,
                                              mCfg.nFrameHeight,
                                              meColorFmt);

    if(nErr != 0)
    {
        WFDMMLOGE("Failed to set Buffer geometry");
        return OMX_ErrorUndefined;
    }

    /*--------------------------------------------------------------
      We will scale the video to fit to the screen
    ----------------------------------------------------------------
    */
    nErr = native_window_set_scaling_mode(mpWindow.get(),
                    NATIVE_WINDOW_SCALING_MODE_SCALE_TO_WINDOW);
    if(nErr)
    {
        WFDMMLOGE("Failed to set Scaling mode");
        return OMX_ErrorUndefined;
    }

    android_native_rect_t sCrop;

    sCrop.left     = 0;
    sCrop.top      = 0;
    sCrop.right    = mCfg.nFrameWidth;
    sCrop.bottom   = mCfg.nFrameHeight;

    WFDMMLOGH4("WFDMMVideoDecode SetCrop %d %d %d %d",
               sCrop.left, sCrop.top, sCrop.right, sCrop.bottom);

    nErr = native_window_set_crop(mpWindow.get(),
                                      &sCrop);
    if(nErr != 0)
    {
        WFDMMLOGE("Failed to set crop on native window..");
    }


    OMX_INDEXTYPE eIndex;
    GetAndroidNativeBufferUsageParams sParams;
    OMX_INIT_STRUCT(&sParams, GetAndroidNativeBufferUsageParams);

    sParams.nPortIndex = mnOutputPortIndex;

    eErr = OMX_GetExtensionIndex(mhVdec,
           (OMX_STRING)"OMX.google.android.index.getAndroidNativeBufferUsage",
                                &eIndex);

    if (eErr != OMX_ErrorNone)
    {
        WFDMMLOGE("Failed to get Native USage extension");
        return eErr;
    }

    eErr = OMX_GetParameter(mhVdec, eIndex, &sParams);

    if(eErr != OMX_ErrorNone)
    {
        WFDMMLOGE("Failed to get Native Buffer Usage");
        return eErr;
    }

    mUsage = sParams.nUsage;


    if (mCfg.bSecure)
    {
        mUsage |= GRALLOC_USAGE_PROTECTED;

        int nAuth = 0;
        nErr = mpWindow->query(mpWindow.get(),
                              NATIVE_WINDOW_QUEUES_TO_WINDOW_COMPOSER,
                              &nAuth);
        if (nErr != 0)
        {
            WFDMMLOGE("Failed to Authenticate Native window");
            return OMX_ErrorDynamicResourcesUnavailable;
        }
        if (nAuth != 1)
        {
            WFDMMLOGE("Failed to Authenticate Native window");
            return OMX_ErrorNotReady;
        }

    }

    nErr = native_window_set_usage(mpWindow.get(),
                                   mUsage |
                                   GRALLOC_USAGE_HW_TEXTURE |
                                   GRALLOC_USAGE_EXTERNAL_DISP);

    if(nErr != 0)
    {
        WFDMMLOGE("Failed to set usage for native window");
        return OMX_ErrorUndefined;
    }

    /*--------------------------------------------------------------------------
       Check minimum number of buffers needed for native window
    ----------------------------------------------------------------------------
    */
    int nMinBufs = 0;

    nErr = mpWindow->query(mpWindow.get(),
                           NATIVE_WINDOW_MIN_UNDEQUEUED_BUFFERS,
                           &nMinBufs);

    if (nErr != 0)
    {
        WFDMMLOGE("Failed to get min buffers");
        return OMX_ErrorInsufficientResources;
    }

    mnNumOutputBuffers = mnNumOutputBuffers + nMinBufs; // Should be Sum?

    /*--------------------------------------------------------------------------
       Set the updated buffercount to Native window
    ----------------------------------------------------------------------------
    */

    WFDMMLOGH3(" Total output buffers: mnNumOutputBuffers: %d,    \
        mnNumDequeuedBufs: %d, nMinBufs: %d", mnNumOutputBuffers, \
        mnNumDequeuedBufs, nMinBufs);

    nErr = native_window_set_buffer_count(
                            mpWindow.get(), mnNumOutputBuffers);

    if (nErr != 0)
    {
        WFDMMLOGE("Set Buffer count failed :(");
        return OMX_ErrorUndefined;
    }

    /*--------------------------------------------------------------------------
       Set the updated buffercount to Decoder
    ----------------------------------------------------------------------------
    */
    sPortDef.nPortIndex = mnOutputPortIndex;

    eErr = OMX_GetParameter(mhVdec,
                            OMX_IndexParamPortDefinition,
                            (OMX_PTR) &sPortDef);

    if (eErr != OMX_ErrorNone)
    {
        WFDMMLOGE(" Failed to get Output PortDef");
        return eErr;
    }

    sPortDef.nBufferCountActual = mnNumOutputBuffers;

    eErr = OMX_SetParameter(mhVdec,
                            OMX_IndexParamPortDefinition,
                            (OMX_PTR) &sPortDef);

    if (eErr != OMX_ErrorNone)
    {
        WFDMMLOGE(" Failed to Set Output PortDef for Buf Count Actual");
        return eErr;
    }

    /*--------------------------------------------------------------------------
       Set the buffers using Use Buffer model to the device
    ----------------------------------------------------------------------------
    */

    eErr  = OMX_GetExtensionIndex(
            mhVdec,
           (OMX_STRING)"OMX.google.android.index.enableAndroidNativeBuffers",
            &eIndex);

    if (eErr != OMX_ErrorNone)
    {
        WFDMMLOGE("Failed to get enableAndroidNativeBuffer Index");
        return eErr;
    }

    {
        EnableAndroidNativeBuffersParams sParams;

        OMX_INIT_STRUCT(&sParams, EnableAndroidNativeBuffersParams);

        sParams.nPortIndex = mnOutputPortIndex;
        sParams.enable     = OMX_TRUE;

        eErr = OMX_SetParameter(mhVdec, eIndex, &sParams);

        if (eErr != OMX_ErrorNone)
        {
            WFDMMLOGE("Failed to enabled Android Native Buffer Usage");
            return eErr;
        }

    }
    /*--------------------------------------------------------------------------
      Firstly find the index for graphic buffer usage from vdec
    ----------------------------------------------------------------------------
    */

    eErr = OMX_GetExtensionIndex(mhVdec,
             (OMX_STRING)"OMX.google.android.index.useAndroidNativeBuffer",
              &eIndex);

    if (eErr != OMX_ErrorNone)
    {
        WFDMMLOGH("Failed to get extension Index for native buffer");
        return eErr;
    }

    OMX_BUFFERHEADERTYPE *pBuffer;

    for(unsigned int i = 0; i < mnNumOutputBuffers; i++)
    {
        /*----------------------------------------------------------------------
          Dequeue Buffers one by one from Native window and pass to decoder
        ------------------------------------------------------------------------
        */
        ANativeWindowBuffer *pBuf;
        nErr = native_window_dequeue_buffer_and_wait(mpWindow.get(),
                                                     &pBuf);
        if (nErr != 0)
        {
            WFDMMLOGE1("Failed to dequeue [%d] Native window buffer",i);
            return OMX_ErrorUndefined;
        }

        /*----------------------------------------------------------------------
         Wrap it in Graphic Buffer
        ------------------------------------------------------------------------
        */
        graphBufferType *pGraphBufHolder = MM_New(graphBufferType);
        pGraphBufHolder->pGraphicBuffer = MM_New_Args(GraphicBuffer,
                                                     (pBuf, false));

        /*----------------------------------------------------------------------
          Push grahicBuffer pointers if we need to to clean up later.
        ------------------------------------------------------------------------
        */
        if (pGraphBufHolder->pGraphicBuffer == NULL)
        {
            WFDMMLOGE("Failed to allocate pGraphicBuffer");
            mpWindow->cancelBuffer(mpWindow.get(),
                                   pBuf,
                                   -1);
            pGraphBufHolder->pGraphicBuffer.clear();
            MM_Delete(pGraphBufHolder);
            return OMX_ErrorInsufficientResources;
        }

        /*----------------------------------------------------------------------
          Create Picture Info type and store it in Appdata. This will be useful
          to pass picture and crop Info to the renderer
        ------------------------------------------------------------------------
        */
        pictureInfoType *pPictInfo =
                        (pictureInfoType *)MM_Malloc(sizeof(pictureInfoType));

        if (!pPictInfo)
        {
            WFDMMLOGE("Failed to allocate pGraphicBuffer");
            mpWindow->cancelBuffer(mpWindow.get(),
                                   pBuf,
                                   -1);
            pGraphBufHolder->pGraphicBuffer.clear();
            MM_Delete(pGraphBufHolder);
            return OMX_ErrorInsufficientResources;
        }

        memset(pPictInfo, 0, sizeof(pictureInfoType));
        /*----------------------------------------------------------------------
          Fill Only the graphicBuffer pointer now. remaining Info will be
          filled on a frame by frame basis
        ------------------------------------------------------------------------
        */

        pPictInfo->pGraphicBuffer = pGraphBufHolder->pGraphicBuffer.get();

        OMX_VERSIONTYPE sVer;
        sVer.nVersion = OMX_SPEC_VERSION;

        UseAndroidNativeBufferParams sParams =
                   {
                       sizeof(UseAndroidNativeBufferParams),
                       sVer,
                       0,
                       NULL,
                       NULL,
                       pGraphBufHolder->pGraphicBuffer
                   };

        sParams.nPortIndex = mnOutputPortIndex;
        sParams.pAppPrivate = (void*)pPictInfo;
        sParams.bufferHeader = &pBuffer;

        eErr = OMX_SetParameter(mhVdec, eIndex, &sParams);

        if(eErr != OMX_ErrorNone || !pBuffer || !pBuffer->pAppPrivate)
        {
            WFDMMLOGE("Failed to set a graphic Buffer to vdec");
            mpWindow->cancelBuffer(mpWindow.get(),
                    (pGraphBufHolder->pGraphicBuffer.get())->getNativeBuffer(),
                                    -1);
            pGraphBufHolder->pGraphicBuffer.clear();
            MM_Delete(pGraphBufHolder);
            return eErr;
        }

        /*----------------------------------------------------------------------
          We maintain special pointers with Graphic Buffer separately,
          we can store actual graphic buffer in pAppData for better portability
        ------------------------------------------------------------------------
        */
        if (mGraphicBufQ)
        {
            mGraphicBufQ->Push(&pGraphBufHolder, sizeof(&pGraphBufHolder));
        }

        buff_hdr_extra_info_sink* extra =
        static_cast<buff_hdr_extra_info_sink*>(MM_Calloc(1,sizeof(*extra)));

        pBuffer->pInputPortPrivate = static_cast<OMX_PTR>(extra);

        mOutputBufCollectorQ->Push(&pBuffer, sizeof(pBuffer));
        mVideoOutputQ->Push(&pBuffer, sizeof(&pBuffer));
        mnNumDequeuedBufs++;
    }

    mnNumDequeuedBufs  = mnNumOutputBuffers-NUM_BUFS_HELD_IN_NATIVE_WINDOW;
    WFDMMLOGH1("Updating deq buff count to [%d]",mnNumDequeuedBufs);

    return eErr;
}


/*==============================================================================

         FUNCTION:         createResources

         DESCRIPTION:
*//**       @brief         creates resources for WFDMMSinkVideoDecode
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
OMX_ERRORTYPE WFDMMSinkVideoDecode::createResources()
{
    if(!createThreadsAndQueues())
    {
        WFDMMLOGE("WFDVideoDec Failed to create Threads And Queues");
        return OMX_ErrorInsufficientResources;
    }

    if(!createVideoResources())
    {
        WFDMMLOGE("WFDVideoDec Failed to create VideoResources");
        return OMX_ErrorInsufficientResources;
    }

    return OMX_ErrorNone;
}

/*==============================================================================

         FUNCTION:         createVideoResources

         DESCRIPTION:
*//**       @brief         creates all video dynamic resources
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
bool WFDMMSinkVideoDecode::createVideoResources()
{
    /*--------------------------------------------------------------------------
     Create OMX decoder. This is going to be a long function
    ----------------------------------------------------------------------------
    */

    static OMX_CALLBACKTYPE sCallbacks =
    {
        EventCallback,
        EmptyDoneCallback,
        FillDoneCallback
    };


    OMX_ERRORTYPE eRet = OMX_ErrorUndefined;

    switch (mCfg.eVideoType)
    {
    case WFD_VIDEO_H264:
        meCompressionFmt = OMX_VIDEO_CodingAVC;
        if (mCfg.bSecure)
        {
            eRet = OMX_GetHandle(&mhVdec,
                  (OMX_STRING)"OMX.qcom.video.decoder.avc.secure",
                  this,
                  &sCallbacks);
            WFDMMLOGD("Opening Video Comp OMX.qcom.video.decoder.avc.secure");
        }
        else
        {
            eRet = OMX_GetHandle(&mhVdec,
                  (OMX_STRING)"OMX.qcom.video.decoder.avc",
                  this,
                  &sCallbacks);
        }
        break;
    default:
        WFDMMLOGE("Unsupported Video Codec");
        return false;
    }

    if(eRet != OMX_ErrorNone || !mhVdec)
    {
        WFDMMLOGE1("Error getting component handle = %x", eRet);
        return false;
    }

    WFDMMLOGH("Video OMX Component created");

    /*--------------------------------------------------------------------------
     At this point the component will be in State LOADED
    ----------------------------------------------------------------------------
    */


    OMX_PORT_PARAM_TYPE sPortParam;
    OMX_VIDEO_PARAM_PORTFORMATTYPE sPortFmt;
    OMX_PARAM_PORTDEFINITIONTYPE     sPortDef;

    memset(&sPortParam, 0, sizeof(sPortParam));
    memset(&sPortDef, 0, sizeof(sPortDef));
    memset(&sPortFmt, 0, sizeof(sPortFmt));
    OMX_INIT_STRUCT(&sPortParam, OMX_PORT_PARAM_TYPE);
    OMX_INIT_STRUCT(&sPortDef, OMX_PARAM_PORTDEFINITIONTYPE);
    OMX_INIT_STRUCT(&sPortFmt, OMX_VIDEO_PARAM_PORTFORMATTYPE);

    /*--------------------------------------------------------------------------
      Find the available ports
    ----------------------------------------------------------------------------
    */
    eRet = OMX_GetParameter(mhVdec,
                            OMX_IndexParamVideoInit,
                            (OMX_PTR)&sPortParam);

    if (eRet != OMX_ErrorNone || sPortParam.nPorts == 0)
    {
        WFDMMLOGE("Error in getting Video Ports");
        return false;
    }

    WFDMMLOGH1("Found Video Ports %lu", sPortParam.nPorts);

    /*--------------------------------------------------------------------------
       Find Port Directions
    ----------------------------------------------------------------------------
    */
    for (uint32 i = 0; i < sPortParam.nPorts; i++)
    {
        sPortDef.nPortIndex = i;
        eRet = OMX_GetParameter(mhVdec,
                                OMX_IndexParamPortDefinition,
                                (OMX_PTR) &sPortDef);
        if (eRet != OMX_ErrorNone)
        {
            WFDMMLOGE("Failed to get PortDef for Direction");
            return false;
        }

        if (sPortDef.eDir == OMX_DirInput)
        {
            WFDMMLOGH("Input port found");

            if (!mbInputPortFound)
            {
                mbInputPortFound = true;
                mnInputPortIndex = i;
                mnNumInputBuffers = sPortDef.nBufferCountMin;
                sPortDef.format.video.eCompressionFormat = OMX_VIDEO_CodingAVC;
                sPortDef.format.video.nFrameWidth = mCfg.nFrameWidth;
                sPortDef.format.video.nFrameHeight = mCfg.nFrameHeight;
                if(mCfg.nFps > 0)
                    sPortDef.format.video.xFramerate = mCfg.nFps << 16;
                else if((mCfg.nFrameWidth*mCfg.nFrameHeight) > (1920*1088))
                    sPortDef.format.video.xFramerate = MAX_FRAME_RATE_4K<< 16;
                else
                    sPortDef.format.video.xFramerate = MAX_FRAME_RATE<< 16;
     //           sPortDef.format.video.nSliceHeight = mCfg.nFrameHeight;
     //           sPortDef.format.video.nStride      = mCfg.nFrameWidth;
                eRet = OMX_SetParameter(mhVdec,
                                        OMX_IndexParamPortDefinition,
                                        (OMX_PTR) &sPortDef);

                if (eRet != OMX_ErrorNone)
                {
                    WFDMMLOGE("Faled to configure Input Port with codec");
                    return false;
                }

            }
        }

        if (sPortDef.eDir == OMX_DirOutput)
        {
            WFDMMLOGH("Output port found");
            if (!mbOutputPortFound)
            {
                mbOutputPortFound = true;
                mnOutputPortIndex = i;
                mnNumOutputBuffers = sPortDef.nBufferCountMin;
//                sPortDef.format.video.eColorFormat = meColorFmt;
                sPortDef.format.video.nFrameWidth = mCfg.nFrameWidth;
                sPortDef.format.video.nFrameHeight = mCfg.nFrameHeight;
                if(mCfg.nFps > 0)
                    sPortDef.format.video.xFramerate = mCfg.nFps << 16;
                else if((mCfg.nFrameWidth*mCfg.nFrameHeight) > (1920*1088))
                    sPortDef.format.video.xFramerate = MAX_FRAME_RATE_4K<< 16;
                else
                    sPortDef.format.video.xFramerate = MAX_FRAME_RATE<< 16;
//                sPortDef.format.video.nSliceHeight = mCfg.nFrameHeight;
//                sPortDef.format.video.nStride      = mCfg.nFrameWidth;
                meColorFmt = sPortDef.format.video.eColorFormat;

                eRet = OMX_SetParameter(mhVdec,
                                        OMX_IndexParamPortDefinition,
                                        (OMX_PTR) &sPortDef);

                if (eRet != OMX_ErrorNone)
                {
                    WFDMMLOGE("Faled to configure Output Port with color");
                    return false;
                }

            }

        /*--------------------------------------------------------------------------
         Enable Smooth streaming
        ----------------------------------------------------------------------------
        */
#if 0 //Retaining the code, just in case we want to bring this back
            eRet = OMX_SetParameter(mhVdec,
                            (OMX_INDEXTYPE)OMX_QcomIndexParamEnableSmoothStreaming,
                             &sPortDef);

            if (eRet != OMX_ErrorNone)
            {
                WFDMMLOGE("Failed to set smooth streaming");
            }
#endif
            /* Set dynamic buffer mode */
            if(0) //Not sure of the significance in UseNativeBuffer mode
            {
                OMX_INDEXTYPE index;
                OMX_STRING name;
                StoreMetaDataInBuffersParams metadata_params;
                metadata_params.nPortIndex = i;
                metadata_params.bStoreMetaData = OMX_TRUE;
                name = const_cast<OMX_STRING>(
                        "OMX.google.android.index.storeMetaDataInBuffers");
                eRet = OMX_GetExtensionIndex(mhVdec, name, &index);
                if (eRet != OMX_ErrorNone)
                {
                    WFDMMLOGE(" Failed to get dynamic buffer mode index");
                }
                eRet = OMX_SetParameter(mhVdec, index, &metadata_params);
                if (eRet != OMX_ErrorNone)
                {
                    WFDMMLOGE(" Failed to set dynamic buffer mode");
                }
                WFDMMLOGH(" Successfully set dynamic buffer mode index");
            }
        }
    }

    WFDMMLOGH("Found Input/Output Ports");

    /*--------------------------------------------------------------------------
      Disable frame parsing
    ----------------------------------------------------------------------------
    */

    OMX_QCOM_PARAM_PORTDEFINITIONTYPE sQPortDef;
    OMX_INIT_STRUCT(&sQPortDef, OMX_QCOM_PARAM_PORTDEFINITIONTYPE);

    sQPortDef.nPortIndex = mnInputPortIndex;
    sQPortDef.nFramePackingFormat =
                        OMX_QCOM_FramePacking_OnlyOneCompleteFrame;
    sQPortDef.nMemRegion = OMX_QCOM_MemRegionSMI;
    sQPortDef.nCacheAttr =  OMX_QCOM_CacheAttrNone;

    eRet = OMX_SetParameter(mhVdec,
                           (OMX_INDEXTYPE)OMX_QcomIndexPortDefn,
                            &sQPortDef);

    if(eRet != OMX_ErrorNone)
    {
        WFDMMLOGE("Failed to disable frame parsing");
    }


    /*--------------------------------------------------------------------------
     Set Decode Order for quicker delivery of o/p frames
    ----------------------------------------------------------------------------
    */
    QOMX_VIDEO_DECODER_PICTURE_ORDER sPictureOrder;

    OMX_INIT_STRUCT(&sPictureOrder, QOMX_VIDEO_DECODER_PICTURE_ORDER);

    sPictureOrder.nPortIndex          = mnOutputPortIndex;

    sPictureOrder.eOutputPictureOrder = QOMX_VIDEO_DECODE_ORDER;

    if (mCfg.bDirectStreaming)
    {
        WFDMMLOGE("Set Display order");
        sPictureOrder.eOutputPictureOrder = QOMX_VIDEO_DISPLAY_ORDER;
    }
    else
    {
        WFDMMLOGE("Set Decode order");
    }

    eRet = OMX_SetParameter(mhVdec,
                      (OMX_INDEXTYPE)OMX_QcomIndexParamVideoDecoderPictureOrder,
                      &sPictureOrder);

    if (eRet != OMX_ErrorNone)
    {
        WFDMMLOGE("Failed to set Decode Order");
    }

    /*--------------------------------------------------------------------------
     Set custom Buffer Size on input port for proper memory allocation based on
     negotiated WFD session resolution.
    ----------------------------------------------------------------------------
    */
    QOMX_VIDEO_CUSTOM_BUFFERSIZE sCustomBufferSize;
    OMX_INIT_STRUCT(&sCustomBufferSize, QOMX_VIDEO_CUSTOM_BUFFERSIZE);

    sCustomBufferSize.nPortIndex  = mnInputPortIndex;
    uint32 nCurResolution = mCfg.nFrameHeight * mCfg.nFrameWidth;

    if(nCurResolution <= MAX_INPUT_RESOLUTION_480P)
    {
        sCustomBufferSize.nBufferSize = MAX_INPUT_BUFFER_SIZE_480P;
    }
    else if(nCurResolution <= MAX_INPUT_RESOLUTION_720P)
    {
        sCustomBufferSize.nBufferSize = MAX_INPUT_BUFFER_SIZE_720P;
    }
    else if(nCurResolution <= MAX_INPUT_RESOLUTION_1080P)
    {
        sCustomBufferSize.nBufferSize = MAX_INPUT_BUFFER_SIZE_1080P;
    }
    else
    {
        sCustomBufferSize.nBufferSize = MAX_INPUT_BUFFER_SIZE_4K;
    }

    WFDMMLOGH1("Setting custom buffer size on input port to %d MB",
        sCustomBufferSize.nBufferSize);

    eRet = OMX_SetParameter(mhVdec,
                  (OMX_INDEXTYPE)OMX_QcomIndexParamVideoCustomBufferSize,
                  &sCustomBufferSize);
    if (eRet != OMX_ErrorNone)
    {
        WFDMMLOGE("Failed to set Custom Buffer Size. Ignoring.");
    }

    /*--------------------------------------------------------------------------
      Get ready to move the state to Idle.
    ----------------------------------------------------------------------------
    */
    eRet = SetState(OMX_StateIdle, OMX_FALSE);

    if(AllocateVideoBuffers() != OMX_ErrorNone)
    {
        WFDMMLOGE("Failed to allocate Video Buffers");
        return false;
    }

    if(WaitState(OMX_StateIdle) != OMX_ErrorNone)
    {
        WFDMMLOGE("Failed to move to IDLE state");
    }

    /*--------------------------------------------------------------------------
      Buffer Allocations are Done. Now move the Component to executing
    ----------------------------------------------------------------------------
    */
    eRet = SetState(OMX_StateExecuting, OMX_TRUE);

    if (eRet != OMX_ErrorNone)
    {
        WFDMMLOGE("FAiled to move the component to executing");
        return false;
    }

    WFDMMLOGH("WFDMMSinkVIdeoDecode moves to Executing");

    SETSTATEINIT;
    return true;
}


/*==============================================================================

         FUNCTION:         destroyResources

         DESCRIPTION:
*//**       @brief         releases dynamic resources
*//**
@par     DEPENDENCIES:
                           None
*//*
         PARAMETERS:
*//**       @param         None

*//*     RETURN VALUE:
*//**       @return
                           OMX_ErrorNone or other error
@par     SIDE EFFECTS:
                           None
*//*==========================================================================*/
OMX_ERRORTYPE WFDMMSinkVideoDecode::destroyResources()
{
    CRITICAL_SECTION_ENTER(mhPortReconCritSec);

    if(!destroyVideoResources())
    {
        CRITICAL_SECTION_LEAVE(mhPortReconCritSec);
        return OMX_ErrorUndefined;
    }

    if(!destroyThreadsAndQueues())
    {
        CRITICAL_SECTION_LEAVE(mhPortReconCritSec);
        return OMX_ErrorUndefined;
    }

    CRITICAL_SECTION_LEAVE(mhPortReconCritSec);

    return OMX_ErrorNone;
}

/*==============================================================================

         FUNCTION:         destroyVideoResources

         DESCRIPTION:
*//**       @brief         release vide dynamic resources
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
bool WFDMMSinkVideoDecode::destroyVideoResources()
{
    if (!mhVdec)
    {
        return false;
    }
    /*--------------------------------------------------------------------------
     destroy OMX decoder.
    ----------------------------------------------------------------------------
    */

    OMX_ERRORTYPE eRet = OMX_ErrorUndefined;

    /*--------------------------------------------------------------------------
      Wait for all video buffers to be back
    ----------------------------------------------------------------------------
    */

    uint32 nNumInBuffersInQ = 0;
    uint32 nNumOutBuffersInQ = 0;

    WFDMMLOGH("Waiting for all Buffers to be returned");

    if (ISSTATEOPENED)
    {
        Stop();
    }
    /* Collecting output buffers */
    CollectOutputBuffer();

    /*--------------------------------------------------------------------------
      At this point the assumption is that buffer exchanges have been stopped
    ----------------------------------------------------------------------------
    */

    /*--------------------------------------------------------------------------
      Move state to Idle
    ----------------------------------------------------------------------------
    */
    /*--------------------------------------------------------------------------
      Get ready to move the state to Idle.
    ----------------------------------------------------------------------------
    */
    eRet = SetState(OMX_StateIdle, OMX_TRUE);

    if(eRet != OMX_ErrorNone)
    {
        WFDMMLOGE("Failed to move to Idle");
    }

    eRet = SetState(OMX_StateLoaded, OMX_FALSE);
    if(eRet != OMX_ErrorNone)
    {
        WFDMMLOGE("Failed to move to State loaded");
    }


    WFDMMLOGH("Video OMX Component created");

    if(!freeVideoBuffers())
    {
        WFDMMLOGE("Failed to free Video Buffers");
    }

    eRet = WaitState(OMX_StateLoaded);

    if (eRet != OMX_ErrorNone)
    {
        WFDMMLOGH("Failed to wait to move to loaded");
    }

    OMX_FreeHandle(mhVdec);

    mhVdec = NULL;

    WFDMMLOGH("Cleaned up video resources");

    return true;
}

/*==============================================================================

         FUNCTION:          freeVideoBuffers

         DESCRIPTION:
*//**       @brief          frees Video Buffers
*//**
@par     DEPENDENCIES:
                            None
*//*
         PARAMETERS:
*//**       @param          None

*//*     RETURN VALUE:
*//**       @return
                            true for success else false
@par     SIFE EFFECTS:
                            None
*//*==========================================================================*/
bool WFDMMSinkVideoDecode::freeVideoBuffers()
{
    OMX_ERRORTYPE eErr = OMX_ErrorNone;

    (void)freeVideoInputBuffers();
    (void)freeVideoOutputBuffers();

    return true;
}

/*==============================================================================

         FUNCTION:          freeVideoInputBuffers

         DESCRIPTION:
*//**       @brief          Asks OMX module to free input buffers
*//**
@par     DEPENDENCIES:
                            None
*//*
         PARAMETERS:
*//**       @param          None

*//*     RETURN VALUE:
*//**       @return
                            OMX_ErrorNone for success or other error
@par     SIFE EFFECTS:
                            None
*//*==========================================================================*/
OMX_ERRORTYPE WFDMMSinkVideoDecode::freeVideoInputBuffers()
{

    OMX_BUFFERHEADERTYPE *pBuffer = NULL;
    OMX_ERRORTYPE  eErr = OMX_ErrorNone;
    uint32 qSize = mVideoInputQ? mVideoInputQ->GetSize():0;

    if(mhVdec && mVideoInputQ && qSize)
    {
        for(uint32 i = 0; i < qSize; i++)
        {
            mVideoInputQ->Pop(&pBuffer, sizeof(&pBuffer), 0);
            if(pBuffer)
            {

                if(pBuffer->pOutputPortPrivate)
                {
                    buff_hdr_extra_info_sink * extra =
                    static_cast<buff_hdr_extra_info_sink*>
                    (pBuffer->pOutputPortPrivate);
                    MM_Free(extra);
                    pBuffer->pOutputPortPrivate = NULL;
                }

                eErr = OMX_FreeBuffer(mhVdec, mnInputPortIndex, pBuffer);

                if (eErr != OMX_ErrorNone)
                {
                    WFDMMLOGE("Failed to Free Buffer");
                }
                pBuffer = NULL;
            }
        }
    }
    return OMX_ErrorNone;
}

/*==============================================================================

         FUNCTION:          freeVideoOutputBuffers

         DESCRIPTION:
*//**       @brief          Uses native window APIs to free output buffers
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
OMX_ERRORTYPE WFDMMSinkVideoDecode::freeVideoOutputBuffers()
{

    OMX_BUFFERHEADERTYPE *pBuffer = NULL;
    graphBufferType      *pGrphBuf = NULL;
    OMX_ERRORTYPE  eErr = OMX_ErrorNone;

    uint32 nqSize = mOutputBufCollectorQ->GetSize();

    for (uint32 i = 0; i < nqSize; i++)
    {
        mOutputBufCollectorQ->Pop(&pBuffer, sizeof(&pBuffer), 0);

        int nErr = 0;

        if(pBuffer)
        {
            if (pBuffer->pAppPrivate)
            {
                pictureInfoType *pPicture =
                               (pictureInfoType *)pBuffer->pAppPrivate;

                GraphicBuffer *pGraphicBuffer =
                               (GraphicBuffer *)pPicture->pGraphicBuffer;

                if (pGraphicBuffer)
                {
                    WFDMMLOGH("VideoDecode: Cancel Graphic Buffer");
                    nErr = mpWindow->cancelBuffer(mpWindow.get(),
                                        pGraphicBuffer->getNativeBuffer(),
                                                   -1);
                    if (nErr != 0)
                    {
                        WFDMMLOGE("Failed to cancel graphic buffer");

                    }
                }
                else
                {
                    WFDMMLOGE("Failed to cancel graphic Buffer");
                }

            }
        }
    }

    nqSize = mVideoOutputQ? mVideoOutputQ->GetSize():0;
    if(mhVdec && mVideoOutputQ && nqSize)
    {
        for(uint32 i = 0; i < nqSize; i++)
        {
            mVideoOutputQ->Pop(&pBuffer, sizeof(&pBuffer), 0);


            int nErr = 0;
            if(pBuffer)
            {
                if (pBuffer->pAppPrivate)
                {
                    pictureInfoType *pPicture =
                                   (pictureInfoType *)pBuffer->pAppPrivate;

                    MM_Delete(pPicture);


                }

                if(pBuffer->pInputPortPrivate)
                {
                    buff_hdr_extra_info_sink * extra =
                    static_cast<buff_hdr_extra_info_sink*>
                    (pBuffer->pInputPortPrivate);
                    MM_Free(extra);
                    pBuffer->pInputPortPrivate = NULL;
                }

                eErr = OMX_FreeBuffer(mhVdec, mnOutputPortIndex, pBuffer);

                if (eErr != OMX_ErrorNone)
                {
                    WFDMMLOGE("Failed to Free Buffer");
                }
                pBuffer = NULL;
            }

            mGraphicBufQ->Pop(&pGrphBuf, sizeof(&pGrphBuf), 0);

            /*------------------------------------------------------------------
             Not sure if this is needed
            --------------------------------------------------------------------
            */
            if (pGrphBuf)
            {
                pGrphBuf->pGraphicBuffer.clear();
            }
            MM_Delete(pGrphBuf);
            pGrphBuf = NULL;
        }
    }
    int nErr = native_window_api_disconnect(mpWindow.get(),
                                  NATIVE_WINDOW_API_MEDIA);
    if(nErr)
    {
        WFDMMLOGH("Failed to disconnect from native window");
    }
    return OMX_ErrorNone;
}

/*==============================================================================

         FUNCTION:          destroyThreadsAndQueues

         DESCRIPTION:
*//**       @brief          Frees the threads and Queues used in this module
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
bool WFDMMSinkVideoDecode::destroyThreadsAndQueues()
{
    if(mSignalQ)
    {
        MM_Delete(mSignalQ);
        mSignalQ = NULL;
    }

    if(mVideoOutputQ)
    {
        MM_Delete(mVideoOutputQ);
        mVideoOutputQ = NULL;
    }

    if (mVideoInputQ)
    {
        MM_Delete(mVideoOutputQ);
        mVideoOutputQ = NULL;
    }

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
bool WFDMMSinkVideoDecode::createThreadsAndQueues()
{
    mSignalQ = MM_New_Args(SignalQueue,(100, sizeof(CmdType)));
    mVideoOutputQ = MM_New_Args(SignalQueue, (100, sizeof(int*)));
    mVideoInputQ  = MM_New_Args(SignalQueue, (100, sizeof(int*)));
    mOutputBufCollectorQ = MM_New_Args(SignalQueue, (100, sizeof(int*)));
    mGraphicBufQ  = MM_New_Args(SignalQueue, (100, sizeof(int*)));

    if(!mSignalQ || !mVideoOutputQ || !mVideoInputQ
        || !mOutputBufCollectorQ || !mGraphicBufQ)
    {
        WFDMMLOGE("Failed to create one of the Quueues");
        return false;
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
OMX_ERRORTYPE WFDMMSinkVideoDecode::deinitialize()
{

    SETSTATECLOSING;

    WFDMMLOGH("Wait for all buffers to be returned");


    /*--------------------------------------------------------------------------
      Close Threads and signal queues
    ----------------------------------------------------------------------------
    */
    if (mSignalQ)
    {
        MM_Delete(mSignalQ);
        mSignalQ = NULL;
    }

    if (mOutputBufCollectorQ)
    {
        MM_Delete(mOutputBufCollectorQ);
        mOutputBufCollectorQ = NULL;
    }

    if (mVideoInputQ)
    {
        MM_Delete(mVideoInputQ);
        mVideoInputQ = NULL;
    }

    if (mGraphicBufQ)
    {
        MM_Delete(mGraphicBufQ);
        mGraphicBufQ = NULL;
    }

    if (mVideoOutputQ)
    {
        MM_Delete(mVideoInputQ);
        mVideoInputQ = NULL;
    }

    return OMX_ErrorNone;
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
int WFDMMSinkVideoDecode::state(int state, bool get_true_set_false)
{
    if(get_true_set_false == true)
    {
        /*----------------------------------------------------------------------
          Just return the current state
        ------------------------------------------------------------------------
        */

    }
    else
    {
        CRITICAL_SECT_ENTER;
        meState = state;
        CRITICAL_SECT_LEAVE;
    }

    return meState;
}

/*==============================================================================

         FUNCTION:          EventCallback

         DESCRIPTION:
*//**       @brief          Function for openmax component event callback
*//**
@par     DEPENDENCIES:
                            None
*//*
         PARAMETERS:
*//**       @param          None

*//*     RETURN VALUE:
*//**       @return
                            OMX_ErrorNone or Other error
@par     SIFE EFFECTS:
                            None
*//*==========================================================================*/
OMX_ERRORTYPE WFDMMSinkVideoDecode::EventCallback(
      OMX_IN OMX_HANDLETYPE hComponent,
      OMX_IN OMX_PTR pAppData,
      OMX_IN OMX_EVENTTYPE eEvent,
      OMX_IN OMX_U32 nData1,
      OMX_IN OMX_U32 nData2,
      OMX_IN OMX_PTR pEventData)
{
    (void) hComponent;
    (void) pEventData;
    OMX_ERRORTYPE result = OMX_ErrorNone;

    WFDMMLOGH1("Event callback with %d event", eEvent);
    if (eEvent == OMX_EventCmdComplete)
    {
        if ((OMX_COMMANDTYPE) nData1 == OMX_CommandStateSet)
        {
            WFDMMLOGH("Event callback with state change");

            switch ((OMX_STATETYPE) nData2)
            {
            case OMX_StateLoaded:
                WFDMMLOGH("state status OMX_StateLoaded");
                break;
            case OMX_StateIdle:
                WFDMMLOGH("state status OMX_StateIdle");
                break;
            case OMX_StateExecuting:
                WFDMMLOGH("state status OMX_StateExecuting");
                break;
            case OMX_StatePause:
                WFDMMLOGH("state status OMX_StatePause");
                break;
            case OMX_StateInvalid:
                WFDMMLOGH("state status OMX_StateInvalid");
                break;
            case OMX_StateWaitForResources:
                WFDMMLOGH("state status OMX_StateWaitForResources");
                break;
            default:
                WFDMMLOGH("state status Invalid");
                break;
            }

            CmdType cmd;
            cmd.eEvent = OMX_EventCmdComplete;
            cmd.eCmd = OMX_CommandStateSet;
            cmd.sCmdData = nData2;
            cmd.eResult = result;

            result = ((WFDMMSinkVideoDecode*)pAppData)->
                                mSignalQ->Push(&cmd, sizeof(cmd));
        }
        else if ((OMX_COMMANDTYPE) nData1 == OMX_CommandFlush)
        {
            WFDMMLOGH("Event callback with flush status");
/*            CmdType cmd;
            cmd.eEvent = OMX_EventCmdComplete;
            cmd.eCmd = OMX_CommandFlush;
            cmd.sCmdData = 0;
            cmd.eResult = result;
            result = ((WFDMMSinkVideoDecode*)pAppData)->
                                                  mSignalQ->Push(&cmd,
                                                               sizeof(cmd));*/
            if((nData2 == 1) &&
            (((WFDMMSinkVideoDecode*)pAppData)->mPortReconfigInProgress))
            {
                WFDMessage *pMsg =
                ((WFDMMSinkVideoDecode*)pAppData)->mpMsgService->getMessageObject();

                WFDMMLOGH(" Starting OMX_FreeBuffer()");

                if (!pMsg)
                {
                    WFDMMLOGE("No memory to allocate messages");
                    return OMX_ErrorInsufficientResources;
                }

                pMsg->SetNoResponse();

                pMsg->nNumArgs = 0;

                pMsg->mnCmd = OMX_FREE_OUTPUT_BUFFERS;

                ((WFDMMSinkVideoDecode*)pAppData)->mpMsgService->SendMessage(pMsg);
            }
        }
        else if ((OMX_COMMANDTYPE) nData1 == OMX_CommandPortDisable)
        {
            WFDMMLOGH(" OMX_CommandPortDisable completed successfully");

            if(((WFDMMSinkVideoDecode*)pAppData)->mPortReconfigInProgress)
            {
                WFDMMLOGH(" Port reconfig in progress, sending message \
                PROCESS_PORT_RECONFIG");

                WFDMessage *pMsg =
                ((WFDMMSinkVideoDecode*)pAppData)->mpMsgService->getMessageObject();

                if (!pMsg)
                {
                    WFDMMLOGE("No memory to allocate messages");
                    return OMX_ErrorInsufficientResources;
                }

                pMsg->SetNoResponse();

                pMsg->nNumArgs = 0;

                pMsg->mnCmd = PROCESS_PORT_RECONFIG;

                ((WFDMMSinkVideoDecode*)pAppData)->mpMsgService->SendMessage(pMsg);

               result = OMX_ErrorNone;
            }
        }
        else if ((OMX_COMMANDTYPE) nData1 == OMX_CommandPortEnable)
        {
            WFDMMLOGH(" OMX_CommandPortEnable completed successfully");

            if(((WFDMMSinkVideoDecode*)pAppData)->mPortReconfigInProgress)
            {
                WFDMMLOGH(" OMX_Completing port reconfiguration");

                WFDMessage *pMsg =
                ((WFDMMSinkVideoDecode*)pAppData)->mpMsgService->getMessageObject();

                if (!pMsg)
                {
                    WFDMMLOGE("No memory to allocate messages");
                    return OMX_ErrorInsufficientResources;
                }

                pMsg->SetNoResponse();

                pMsg->nNumArgs = 0;

                pMsg->mnCmd = COMPLETE_PORT_RECONFIG;

                ((WFDMMSinkVideoDecode*)pAppData)->mpMsgService->SendMessage(pMsg);
            }
        }
        else
        {
            WFDMMLOGE("error status");
            WFDMMLOGE("Unimplemented command");
        }
    }
    else if (eEvent == OMX_EventError)
    {
        WFDMMLOGH("async error");
        CmdType cmd;
        cmd.eEvent = OMX_EventError;
        cmd.eCmd = OMX_CommandMax;
        cmd.sCmdData = 0;
        cmd.eResult = (OMX_ERRORTYPE) nData1;
                  ((WFDMMSinkVideoDecode*) pAppData)->mpFnHandler(
           (void*)(uint64)((WFDMMSinkVideoDecode*) pAppData)->mClientData,
                  ((WFDMMSinkVideoDecode*) pAppData)->mnModuleId,
                                                      WFDMMSINK_ERROR,
                                                      OMX_ErrorUndefined,
                                                      0);
        return result;
    }
    else if (eEvent == OMX_EventBufferFlag)
    {
        WFDMMLOGH("got buffer flag event");
    }
    else if (eEvent == OMX_EventPortSettingsChanged)
    {
        WFDMMLOGH(" getting a port reconfig event");

        if (nData2 == OMX_IndexConfigCommonOutputCrop)
        {
            WFDMMLOGH("PortSettingsCHanged==>OMX_IndexConfigCommonOutputCrop");
            //((WFDMMSinkVideoDecode*) pAppData)->HandlePortReconfig();
        }
        else if(nData2 == 0 || nData2 == OMX_IndexParamPortDefinition)
        {
            WFDMMLOGH(" Port settings changed");
            WFDMessage *pMsg =
            ((WFDMMSinkVideoDecode*)pAppData)->mpMsgService->getMessageObject();

            if (!pMsg)
            {
                WFDMMLOGE("No memory to allocate messages");
                return OMX_ErrorInsufficientResources;
            }

            pMsg->SetNoResponse();

            pMsg->nNumArgs = 0;

            pMsg->mnCmd = INITIATE_PORT_RECONFIG;

            ((WFDMMSinkVideoDecode*)pAppData)->mpMsgService->SendMessage(pMsg);

            WFDMMLOGH(" Sent message to disable output port");
            result = OMX_ErrorNone;
            return(result);
        }
        else
        {
            WFDMMLOGH(" invalid port settings change event");
        }
    }
    else
    {
        WFDMMLOGH("Unimplemented event");
    }

    return result;
}

/*==============================================================================

         FUNCTION:          EmptyDoneCallback

         DESCRIPTION:
*//**       @brief          OMX component calls this when data in a buffer
                            is consumed
*//**
@par     DEPENDENCIES:
                            None
*//*
         PARAMETERS:
*//**       @param          None

*//*     RETURN VALUE:
*//**       @return
                            OMX_ErrorNone or Other error
@par     SIFE EFFECTS:
                            None
*//*==========================================================================*/
OMX_ERRORTYPE WFDMMSinkVideoDecode::EmptyDoneCallback(
      OMX_IN OMX_HANDLETYPE hComponent,
      OMX_IN OMX_PTR pAppData,
      OMX_IN OMX_BUFFERHEADERTYPE* pBuffer)
{
    (void) hComponent;
    OMX_ERRORTYPE result = OMX_ErrorNone;

    if (!pBuffer || !pAppData)
    {
        WFDMMLOGE("EmptyDoneCallBack:Invalid Args");
        return OMX_ErrorBadParameter;
    }

    WFDMMLOGH1("WFDMMSinkVdec::EmptyDoneCallback nFilledLen = %lu",
               pBuffer->nFilledLen);
    if (((WFDMMSinkVideoDecode*)pAppData)->mpFnEBD &&
        (((WFDMMSinkVideoDecode*)pAppData)->state(0,true) == OPENED))
    {
        ((WFDMMSinkVideoDecode*)pAppData)->mpFnEBD(
                     ((WFDMMSinkVideoDecode*)pAppData)->mClientData,
                     ((WFDMMSinkVideoDecode*)pAppData)->mnModuleId,
                      SINK_VIDEO_TRACK_ID,
                      pBuffer);
    }
    else
    {
        WFDMMLOGE1("WFDMMSinkVdec::EmptyDoneCallback incorrect state = %d",
            ((WFDMMSinkVideoDecode*)pAppData)->state(0,true));
    }

    return result;
}

/*==============================================================================

         FUNCTION:          setupPictureInfo

         DESCRIPTION:
*//**       @brief          sets up picture dimensions and color format in the
                            buffer header
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
bool WFDMMSinkVideoDecode::setupPictureInfo(OMX_BUFFERHEADERTYPE *pBuffer)
{
    if (!pBuffer || !pBuffer->pAppPrivate || !mhVdec)
    {
        WFDMMLOGE("setupPictureInfo:Invalid Args");
        return false;
    }

    /*--------------------------------------------------------------------------
      Create Picture Info Structure to carry the information of the current
      Picture
    ----------------------------------------------------------------------------
    */
    pictureInfoType *pPictureInfo = (pictureInfoType *)pBuffer->pAppPrivate;

    OMX_INIT_STRUCT(&pPictureInfo->rect, OMX_CONFIG_RECTTYPE);

    pPictureInfo->rect.nPortIndex = mnOutputPortIndex;

    OMX_ERRORTYPE eErr = OMX_GetConfig(mhVdec,
                                       OMX_IndexConfigCommonOutputCrop,
                                       &pPictureInfo->rect);

    if (eErr != OMX_ErrorNone)
    {
        WFDMMLOGE("Failed to get Output Crop");
        pPictureInfo->rect.nWidth = mCfg.nFrameWidth;
        pPictureInfo->rect.nHeight = mCfg.nFrameHeight;
        pPictureInfo->rect.nLeft   = 0;
        pPictureInfo->rect.nTop    = 0;

    }

    pPictureInfo->nHeight = mCfg.nFrameHeight;
    pPictureInfo->nWidth  = mCfg.nFrameWidth;

    pPictureInfo->eColorFmt = meColorFmt;

    return true;
}

/*==============================================================================

         FUNCTION:          FillDoneCallback

         DESCRIPTION:
*//**       @brief          Callback indicatin OMX Module has completed
                            filling the buffer
*//**
@par     DEPENDENCIES:
                            None
*//*
         PARAMETERS:
*//**       @param          None

*//*     RETURN VALUE:
*//**       @return
                            OMX_ErrorNone or other error code
@par     SIFE EFFECTS:
                            None
*//*==========================================================================*/
OMX_ERRORTYPE WFDMMSinkVideoDecode::FillDoneCallback(
      OMX_IN OMX_HANDLETYPE hComponent,
      OMX_IN OMX_PTR pAppData,
      OMX_IN OMX_BUFFERHEADERTYPE* pBuffer)
{

#ifdef SYSTRACE_PROFILE
    ATRACE_CALL();
#endif

    (void) hComponent;
    if (!pBuffer || !pBuffer->pAppPrivate || !pAppData)
    {
        WFDMMLOGE("FillDoneCallBack:Invalid Args");
        return OMX_ErrorBadParameter;
    }

    OMX_ERRORTYPE result = OMX_ErrorNone;
    WFDMMLOGM2("WFDMMSinkVdec::FillDoneCallback Size = %lu, TS = %d",
               pBuffer->nFilledLen, (uint32)pBuffer->nTimeStamp);

    WFDMMSinkVideoDecode* pMe = static_cast<WFDMMSinkVideoDecode*>(pAppData);

#ifdef SYSTRACE_PROFILE
            ATRACE_ASYNC_END("Decode frame", static_cast<int32_t>(pBuffer->nTimeStamp));
#endif

    if (pMe->mpFnEBD && (0 == pMe->mPortReconfigInProgress))
    {
        pMe->setupPictureInfo(pBuffer);

        if(WFD_LIKELY(pMe->m_pStatInst))
        {
            pMe->m_pStatInst->recordDecOP(pBuffer);

            if(WFD_UNLIKELY(pBuffer->nFilledLen == 0))
            {
                /*--------------------------------------------------------------------------
                  This buffer is never going to make it to the actual renderer
                  record it for statistics purpose
                ----------------------------------------------------------------------------
                */
                pMe->m_pStatInst->recordVideoFrameDrop(pBuffer,false);
            }
        }

        pMe->mpFnFBD(pMe->mClientData, pMe->mnModuleId, SINK_VIDEO_TRACK_ID,
                      pBuffer);
    }
    else
    {
        /* Save it. no need to send to renderer, since we are in flush. */

        ((WFDMMSinkVideoDecode*)pAppData)->mOutputBufCollectorQ->Push(&pBuffer,
                                                             sizeof(&pBuffer));
    }

    return result;
}

/*==============================================================================

         FUNCTION:          SetState

         DESCRIPTION:
*//**       @brief          Asks OMX module to move to particular state
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
OMX_ERRORTYPE WFDMMSinkVideoDecode::SetState
(
    OMX_STATETYPE eState,
    OMX_BOOL bSynchronous
)
{
    OMX_ERRORTYPE result = OMX_ErrorNone;
    if(!mhVdec)
    {
        return OMX_ErrorInsufficientResources;
    }

    result = OMX_SendCommand(mhVdec,
                             OMX_CommandStateSet,
                            (OMX_U32) eState,
                             NULL);

    if (result == OMX_ErrorNone)
    {
        if (bSynchronous == OMX_TRUE)
        {
            result = WaitState(eState);
            if (result != OMX_ErrorNone)
            {
                WFDMMLOGE("failed to wait state");
            }
        }
    }
    else
    {
        WFDMMLOGE("failed to set state");
    }
    return result;
}

/*==============================================================================

         FUNCTION:          WaitState

         DESCRIPTION:
*//**       @brief          Waits for state transition to complete
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
OMX_ERRORTYPE WFDMMSinkVideoDecode::WaitState
(
    OMX_STATETYPE eState
)
{
    OMX_ERRORTYPE result = OMX_ErrorUndefined;
    CmdType cmd;
    uint32  nNumIter = 50;

    while (result != OMX_ErrorNone && nNumIter--)
    {
        result =  mSignalQ->Pop(&cmd, sizeof(cmd), 100);

        if (result != OMX_ErrorTimeout && result != OMX_ErrorNone)
        {
            WFDMMLOGE("Error in popping from Signal Queue");
            return result;
        }
    }

    result = cmd.eResult;

    if (cmd.eEvent == OMX_EventCmdComplete)
    {
        if (cmd.eCmd == OMX_CommandStateSet)
        {
            if ((OMX_STATETYPE) cmd.sCmdData == eState)
            {
                //meState = (OMX_STATETYPE) cmd.sCmdData;
            }
            else
            {
                WFDMMLOGE1("wrong state (%d)", (int) cmd.sCmdData);
                result = OMX_ErrorUndefined;
            }
        }
/*        else if(cmd.eCmd == OMX_CommandFlush)
        {
            WFDMMLOGH("VideoDecode Flush completed");
            result = OMX_ErrorNone;
        }*/
        else
        {
            WFDMMLOGE("expecting state change");
            result = OMX_ErrorUndefined;
        }
    }
    else
    {
        WFDMMLOGH("expecting cmd complete");
        if(mpFnHandler)
        {
            mpFnHandler(mClientData,
                        mnModuleId,
                        WFDMMSINK_ERROR,
                        OMX_ErrorUndefined,
                        0);
        }
        result = OMX_ErrorUndefined;
    }

    return result;
}

/*==============================================================================

         FUNCTION:          DeliverInput

         DESCRIPTION:
*//**       @brief          Provides an input buffer for processing
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
OMX_ERRORTYPE WFDMMSinkVideoDecode::DeliverInput
(
    OMX_BUFFERHEADERTYPE *pBufferHdr
)
{

#ifdef SYSTRACE_PROFILE
    ATRACE_CALL();
#endif

    OMX_ERRORTYPE eErr = OMX_ErrorUndefined;

    if (!pBufferHdr)
    {
        WFDMMLOGE("Invalid Buffer Pointer in DeliverInput");
        return OMX_ErrorBadParameter;
    }

    if(!ISSTATEOPENED || !mhVdec)
    {
        WFDMMLOGE("Emptythisbuffer in invalid state");
        return OMX_ErrorIncorrectStateOperation;
    }

    if (pBufferHdr->nInputPortIndex != mnInputPortIndex)
    {
        WFDMMLOGE("Invalid PortIndex in DeliverInput");
        return OMX_ErrorBadPortIndex;
    }
    WFDMMLOGH("Calling Video Empty this Buffer");

#ifdef SYSTRACE_PROFILE
    ATRACE_ASYNC_BEGIN("Decode frame", static_cast<int32_t>(pBuffer->nTimeStamp));
#endif

    if(WFD_LIKELY(m_pStatInst))
    {
        m_pStatInst->recordDecIP(pBufferHdr);
    }

    eErr = OMX_EmptyThisBuffer(mhVdec, pBufferHdr);

    if (eErr != OMX_ErrorNone)
    {
        WFDMMLOGE("FAiled to call EmptythisBuffer");
        return eErr;
    }
    return OMX_ErrorNone;
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
OMX_ERRORTYPE WFDMMSinkVideoDecode::SetFreeBuffer
(
    OMX_BUFFERHEADERTYPE *pBuffer
)
{
    OMX_ERRORTYPE eErr = OMX_ErrorUndefined;

    if (!pBuffer)
    {
        WFDMMLOGE("Invalid Buffer Pointer in DeliverInput");
        return OMX_ErrorBadParameter;
    }

    if (pBuffer->nOutputPortIndex != mnOutputPortIndex)
    {
        WFDMMLOGE("Invalid PortIndex in DeliverInput");
        return OMX_ErrorBadPortIndex;
    }

    if(!ISSTATEOPENED || !mhVdec || mPortReconfigInProgress)
    {
        /*----------------------------------------------------------------------
          This module is the rightful owner of the buffer at death ot stop of
          the module. So hold the buffer in the collectorQ
        ------------------------------------------------------------------------
        */
        mOutputBufCollectorQ->Push(&pBuffer, sizeof(&pBuffer));
        return OMX_ErrorNone;
    }
    else
    {
        eErr = OMX_FillThisBuffer(mhVdec, pBuffer);

        if (eErr != OMX_ErrorNone)
        {
            WFDMMLOGE("Failed to call FillthisBuffer");
            mOutputBufCollectorQ->Push(&pBuffer, sizeof(&pBuffer));
            return eErr;
        }

    }

    return OMX_ErrorNone;
}

/*==============================================================================

         FUNCTION:          Stop

         DESCRIPTION:
*//**       @brief          Stops buffer flow
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
OMX_ERRORTYPE WFDMMSinkVideoDecode::Stop()
{
    if(!ISSTATEOPENED)
    {
        /*----------------------------------------------------------------------
           If the state is not opened we dont need to stop buffer flow.

           WARNING. This call must be made from the same thread that has created
           the resources. Otherwise there may be race conditions. - SK
        ------------------------------------------------------------------------
        */
        WFDMMLOGE("Video State not opened Stop is Done!!!");
        return OMX_ErrorNone;
    }

    /*----------------------------------------------------------------------
       If port reconfiguration is ongoing, wait till it completes
       As port reconfiguration happens in different thread, pause can
       be issued in command thread and can lead to issues.
     ------------------------------------------------------------------------
    */

    WFDMMLOGH("Waiting for port reconfigure to complete!!!");
    while (mPortReconfigInProgress ==1)
    {
      MM_Timer_Sleep(2);
    }
    WFDMMLOGH("Done: Waiting for port reconfigure to complete!!!");

    /*--------------------------------------------------------------------------
      Now this is tricky. But with some care we can do the trick.

      Things to do.
      1. Change the state to something like stopping.
      2. Stop the buffer flow into decoder.
      3. Allow any outflow of buffer from decoder.
      4. Use the collector Q to collect all buffers that has been allocated
         for output. Remember that VideoDecoder is the rightful owner of these
         buffers.
      5. Flush the input/output port. Owner of input buffers at stop is the
         mediasource and output is collector queue
      6. Wait for buffers to flow normally and finally get collected in the
         collector Q.

    ----------------------------------------------------------------------------
    */

    /* 1.)*/SETSTATECLOSING;

    /* 2.) This will be done by SetFreeBuffer*/

    /* 3.) This will be done by EmptyBufferDone and FillBufferDone callbacks*/

    /* 4 )*/
    /* 5 )*/

    if (mhVdec)
    {
        OMX_ERRORTYPE eErr = OMX_ErrorNone;
        eErr = OMX_SendCommand(mhVdec,
                                 OMX_CommandFlush,
                                 OMX_ALL,
                                 NULL);

        if (eErr != OMX_ErrorNone)
        {
            WFDMMLOGE("Failed to issue flush command");
            /*------------------------------------------------------------------
               Nothing much to do. This will lead to some problem
            --------------------------------------------------------------------
            */
        }
    }

    return OMX_ErrorNone;
}

/*==============================================================================

         FUNCTION:          CollectOutputBuffer

         DESCRIPTION:
*//**       @brief          Make the VideoDecoder to collect output buffers.
                            It waits for all output buffers to be back.
*//**
@par     DEPENDENCIES:
                            Video renderer should have been stopped. Else it
                            will be stuck infinitely.
*//*
         PARAMETERS:
*//**       @param          None

*//*     RETURN VALUE:
*//**       @return
                            OMX_ErrorNone
@par     SIDE EFFECTS:
                            None
*//*==========================================================================*/
OMX_ERRORTYPE WFDMMSinkVideoDecode::CollectOutputBuffer()
{
    WFDMMLOGH("Waiting for output Buffers to be collected");
    while ((uint32)mOutputBufCollectorQ->GetSize() < mnNumDequeuedBufs)
    {
        WFDMMLOGH2("mOutputBufCollectorQ->GetSize() = %d, mnNumDequeuedBufs = %d",
                mOutputBufCollectorQ->GetSize(),mnNumDequeuedBufs);
        MM_Timer_Sleep(2);
    }
    WFDMMLOGH("OutputBuffers Collected");
    return OMX_ErrorNone;
}

/*==============================================================================

         FUNCTION:          Start

         DESCRIPTION:
*//**       @brief          Make the VideoDecoder ready for buffer exchange.
                            Gives o/p buffers to OMX component and i/p buffers
                            to media source
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
OMX_ERRORTYPE WFDMMSinkVideoDecode::Start()
{
    OMX_BUFFERHEADERTYPE *pBuffer;
    /*--------------------------------------------------------------------------
      1. In this we need to do two things. One send all output buffers to decoder
    ----------------------------------------------------------------------------
    */
    /* Making sure that all buffers are with
       the component before Start*/
    CollectOutputBuffer();

    for(uint32 i = 0; i < mnNumDequeuedBufs; i++)
    {
        pBuffer = NULL;
        if (mhVdec)
        {
            mOutputBufCollectorQ->Pop(&pBuffer, sizeof(&pBuffer), 0);

            if(!pBuffer)
            {
                WFDMMLOGE("Failed to Pop Buf from Output Collector");
                return OMX_ErrorUndefined;
            }
            /*------------------------------------------------------------------
              Push the buffers one by one to decoders output port
            --------------------------------------------------------------------
            */
            if(OMX_FillThisBuffer(mhVdec,pBuffer) != OMX_ErrorNone)
            {
                WFDMMLOGE("Failed to push buffer for FTB");
                mOutputBufCollectorQ->Push(&pBuffer, sizeof(&pBuffer));
                return OMX_ErrorUndefined;
            }
        }
    }

    /*--------------------------------------------------------------------------
      Cancel the last buffers back to Native Window
    ----------------------------------------------------------------------------
    */
    for (uint32 i = mnNumDequeuedBufs;
           i < mnNumOutputBuffers; i++)
    {
        WFDMMLOGH("Cancel Initial Buffers");
        mOutputBufCollectorQ->Pop(&pBuffer, sizeof(&pBuffer), 0);
        pBuffer->nFilledLen = 0;
        mpFnFBD(mClientData, mnModuleId, SINK_VIDEO_TRACK_ID, pBuffer);
    }

    /*--------------------------------------------------------------------------
     2. Send all Input buffers to client to fill.
    ----------------------------------------------------------------------------
    */

    for(uint32 i = 0; i < mnNumInputBuffers; i++)
    {
        pBuffer = NULL;
        if (mhVdec)
        {
            mVideoInputQ->Pop(&pBuffer, sizeof(&pBuffer), 0);

            if(!pBuffer)
            {
                WFDMMLOGE("Failed to Pop Buf from Input Queue");
                return OMX_ErrorUndefined;
            }

            WFDMMLOGE1("Popped InputBuffer %p", pBuffer);
            /*------------------------------------------------------------------
              Push the buffers one by one to Client saying EBD
            --------------------------------------------------------------------
            */
            if (mpFnEBD)
            {
                mpFnEBD(mClientData,mnModuleId, SINK_VIDEO_TRACK_ID, pBuffer);
            }

            mVideoInputQ->Push(&pBuffer, sizeof(&pBuffer));
        }
    }

    SETSTATEOPENED;

    return OMX_ErrorNone;
}

/*==============================================================================

         FUNCTION:          GetOutputPortDefinition

         DESCRIPTION:
*//**       @brief          When OMX module reports port settings changed, this
                            function handles it
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
bool WFDMMSinkVideoDecode::GetOutputPortDefinition()
{
    OMX_PARAM_PORTDEFINITIONTYPE sPortDef;

    OMX_INIT_STRUCT(&sPortDef, OMX_PARAM_PORTDEFINITIONTYPE);

    sPortDef.nPortIndex = mnOutputPortIndex;

    OMX_ERRORTYPE eErr =
         OMX_GetParameter(mhVdec,
                          OMX_IndexParamPortDefinition,
                         &sPortDef);
    if (eErr != OMX_ErrorNone)
    {
        WFDMMLOGE("PortSetting Changed Fail in GetPortDef");
        return false;
    }

    mCfg.nFrameHeight = sPortDef.format.video.nFrameHeight;
    mCfg.nFrameWidth  = sPortDef.format.video.nFrameWidth;

    WFDMMLOGH2("AFter Port Change Height %d Width %d",
                mCfg.nFrameHeight, mCfg.nFrameWidth);

    return true;
}
/*==============================================================================

         FUNCTION:          CompletePortReconfiguration

         DESCRIPTION:
*//**       @brief          Completes post port reconfig steps
                            Sends the FTB's to the decoder after port reconfig
                            Sends cancel buffers to renderer
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
bool WFDMMSinkVideoDecode::CompletePortReconfiguration()
{
    OMX_BUFFERHEADERTYPE *pBuffer;
    /*--------------------------------------------------------------------------
    1. In this we need to do two things. One send all output buffers to decoder
    ----------------------------------------------------------------------------
    */
    for(uint32 i = 0; i < mnNumDequeuedBufs; i++)
    {
        pBuffer = NULL;
        if (mhVdec)
        {
            mOutputBufCollectorQ->Pop(&pBuffer, sizeof(&pBuffer), 0);

            if(!pBuffer)
            {
                WFDMMLOGE("Failed to Pop Buf from Output Collector");
                return OMX_ErrorUndefined;
            }
            /*------------------------------------------------------------------
              Push the buffers one by one to decoders output port
            --------------------------------------------------------------------
            */
            if(OMX_FillThisBuffer(mhVdec,pBuffer) != OMX_ErrorNone)
            {
                WFDMMLOGE("Failed to push buffer for FTB");
                mOutputBufCollectorQ->Push(&pBuffer, sizeof(&pBuffer));
                return OMX_ErrorUndefined;
            }
        }
    }

    /*--------------------------------------------------------------------------
      Cancel the last buffers back to Native Window
    ----------------------------------------------------------------------------
    */
    for (uint32 i = mnNumDequeuedBufs;
           i < mnNumOutputBuffers; i++)
    {
        WFDMMLOGH("Cancel Initial Buffers");
        mOutputBufCollectorQ->Pop(&pBuffer, sizeof(&pBuffer), 0);
        pBuffer->nFilledLen = 0;
        mpFnFBD(mClientData, mnModuleId, SINK_VIDEO_TRACK_ID, pBuffer);
    }

    return true;
}

/*==============================================================================

         FUNCTION:         getCurrentTimeUS

         DESCRIPTION:
*//**       @brief         Returns the current timestamp. Does not adjust
                           the value with respect to basetime. Used only for
                           statistics purposes!
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
uint64 WFDMMSinkVideoDecode::getCurrentTimeUs()
{
    struct timespec sTime;
    clock_gettime(CLOCK_MONOTONIC, &sTime);
    uint64 currTS = ((OMX_U64)sTime.tv_sec * 1000000/*us*/)
           + ((OMX_U64)sTime.tv_nsec / 1000);
    return currTS /*- mnBaseMediaTime*/;
}

/*==============================================================================

         FUNCTION:          ReceiveMessage

         DESCRIPTION:
*//**       @brief          Process Incoming messages asynchronously
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
void WFDMMSinkVideoDecode::ReceiveMessage
(
    void* handle,
    WFDMessage *pMsg
)
{
    WFDMMSinkVideoDecode *pMe = (WFDMMSinkVideoDecode*)handle;
    OMX_ERRORTYPE eErr = OMX_ErrorNone;
    OMX_BUFFERHEADERTYPE *pBuffer = NULL;
    uint32 nqSize;
    if (pMe && pMsg)
    {
        WFDMMLOGH1("Received message [%d]",pMsg->mnCmd);
        //Acquire crtitical section before checking state
        CRITICAL_SECTION_ENTER(pMe->mhPortReconCritSec);
        int decState = pMe->state(0,true);
        if(decState != OPENED)
        {
            WFDMMLOGH1("Not entertaining message in state [%d]",
                decState);
            CRITICAL_SECTION_LEAVE(pMe->mhPortReconCritSec);
            return;
        }
        switch(pMsg->mnCmd)
        {
        case INITIATE_PORT_RECONFIG:
            // 1. Set mPortReconfigInProgress, so that the class
            //    knows we are in the middle of port reconfig.
            // 2. Reconfigure the renderer.
            // 3. Send port disable, also takes care of flush.
            pMe->mPortReconfigInProgress = 1; /* Better to set before
                                    sending command to avoid race conditions */
            WFDMMLOGH("INITIATE_PORT_RECONFIG");
            /* reconfigure the renderer */
            pMe->mpFnHandler((void*)(uint64)pMe->mClientData,
                                                pMe->mnModuleId,
                                                WFDMMSINK_RENDERER_RECONFIGURE,
                                                OMX_ErrorUndefined,
                                                0);

            /* Flush the decoder */
            eErr = OMX_SendCommand(pMe->mhVdec, OMX_CommandFlush,
                                    pMe->mnOutputPortIndex, 0);

            if (eErr != OMX_ErrorNone)
            {
                WFDMMLOGE("Failed to issue flush command");
            }
            WFDMMLOGH1(" Sending port disable command on port: %d",
                                                pMe->mnOutputPortIndex);
            pMe->mnOutputPortIndex = 1;
            eErr = OMX_SendCommand(pMe->mhVdec, OMX_CommandPortDisable,
                                                pMe->mnOutputPortIndex, 0);

            if (eErr != OMX_ErrorNone)
            {
                pMe->mPortReconfigInProgress = 0;
                WFDMMLOGE(" error OMX_CommandPortDisable failed");
            }
            else
            {
                WFDMMLOGH(" Send command OMX_CommandPortDisable \
                successful, waiting for complete...");
            }
            pMsg->NotifyResponse(eErr);
            break;

        case PROCESS_PORT_RECONFIG:
            WFDMMLOGH(" Inside PROCESS_PORT_RECONFIG");

            eErr = pMe->allocateVideoOutputBuffers();

            if (eErr != OMX_ErrorNone)
            {
                pMe->mPortReconfigInProgress = 0;
                pMsg->NotifyResponse(eErr);
                WFDMMLOGE("Failed to allocate Output Buffers");
                break;
            }

            WFDMMLOGH2("New mnNumOutputBuffers:[%d], mnNumDequeuedBufs:[%d]",
                    pMe-> mnNumOutputBuffers, pMe->mnNumDequeuedBufs);

            WFDMMLOGH(" Sending port enable command");

            eErr = OMX_SendCommand(pMe->mhVdec, OMX_CommandPortEnable,
                                                pMe->mnOutputPortIndex, 0);

            if (eErr != OMX_ErrorNone)
            {
                WFDMMLOGE(" error sending command \
                OMX_CommandPortEnable failed");
            }
            pMsg->NotifyResponse(eErr);
            break;

        case COMPLETE_PORT_RECONFIG:
            WFDMMLOGH1(" Inside COMPLETE_PORT_RECONFIG [%d]",pMe->mPortReconfigInProgress);
            if(pMe->mPortReconfigInProgress)
            {
                /* needs to be set before next call since, FTB's are
                sent in that function */
                pMe->mPortReconfigInProgress = 0;
                pMe->CompletePortReconfiguration();
                WFDMMLOGH(" Port reconfiguration complete");
            }
            pMsg->NotifyResponse(eErr);
            break;

        case OMX_FREE_OUTPUT_BUFFERS:
            WFDMMLOGH(" Inside OMX_FREE_OUTPUT_BUFFERS");
            WFDMMLOGH("Waiting for output Buffers to be collected");
            while ((uint32)pMe->mOutputBufCollectorQ->GetSize() <
                                                        pMe->mnNumOutputBuffers)
            {
                MM_Timer_Sleep(2);
            }
            WFDMMLOGH2("All output buffers collected, collected: %d, total: %d",
            (uint32)pMe->mOutputBufCollectorQ->GetSize(), pMe->mnNumOutputBuffers);
            pMe->freeVideoOutputBuffers();
            pMsg->NotifyResponse(eErr);
            break;

        default:
            WFDMMLOGE(" Error, Unknown command in \
                            WFDMMSinkVideoDecode::ReceiveMessage");
            break;
        }
        CRITICAL_SECTION_LEAVE(pMe->mhPortReconCritSec);
    }
    else
    {
        WFDMMLOGE(" WFDMMSinkVideoDecode::ReceiveMessage failed, \
        pointers are NULL");
    }
}
