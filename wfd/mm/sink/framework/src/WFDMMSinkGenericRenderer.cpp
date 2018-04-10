/*==============================================================================
*       WFDMMSinkGenericRenderer.cpp
*
*  DESCRIPTION:
*       Does Sync with AV and render generic data.
*
*
*  Copyright (c) 2014-16 Qualcomm Technologies, Inc.
*  All Rights Reserved.
*  Confidential and Proprietary - Qualcomm Technologies, Inc.
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

#include <cutils/properties.h>
#include "WFDMMLogs.h"
#include "WFDMMThreads.h"
#include "WFDMMSourceMutex.h"
#include "WFDMMSourceSignalQueue.h"
#include "MMMalloc.h"
#include "MMMemory.h"
#include "qmmList.h"
#include "MMTimer.h"
#include "WFDMMSinkStatistics.h"
#include "WFDMMSinkGenericRenderer.h"
#include "SkBitmap.h"
#include "SkImageDecoder.h"

#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "WFDMMGENERICRENDERER"

using namespace android;

#define CRITICAL_SECT_ENTER if(mhCritSect)                                     \
                                  MM_CriticalSection_Enter(mhCritSect);        \
/*      END CRITICAL_SECT_ENTER    */

#define CRITICAL_SECT_LEAVE if(mhCritSect)                                     \
                                  MM_CriticalSection_Leave(mhCritSect);        \
/*      END CRITICAL_SECT_LEAVE    */

#define NOTIFYERROR  mpFnHandler((void*)(int64)mClientData,                    \
                                 mnModuleId,WFDMMSINK_ERROR,                   \
                                 OMX_ErrorUndefined,                           \
                                 0 );                                          \
/*NOTIFYERROR*/

#define WFD_DUMP_GENRIC
#define OVERLAY_PRIVATE_HEADER_SIZE 18 //kamit : Needs to be updated based on doc.

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

#define GENERIC_RENDER_TOLERANCE 50000 //us

// (TCP delay - Delta Time that is used to pick for render)
#define GENERIC_CLEAR_TOLERANCE  (mCfg.nDecoderLatency - GENERIC_RENDER_TOLERANCE)


/*==============================================================================

         FUNCTION:         WFDMMSinkGenericRenderer

         DESCRIPTION:
*//**       @brief         WFDMMSinkGenericRenderer contructor
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
WFDMMSinkGenericRenderer::WFDMMSinkGenericRenderer(int moduleId,
                                     WFDMMSinkEBDType       pFnEBD,
                                     WFDMMSinkHandlerFnType   pFnHandler,
                                     void* clientData)
{

    mhCritSect = NULL;
    InitData();
    /*--------------------------------------------------------------------------

    ----------------------------------------------------------------------------
    */
    mpFnHandler = pFnHandler;
    mpFnEBD     = pFnEBD;
    mnModuleId  = moduleId;
    mClientData = clientData;

    int nRet = MM_CriticalSection_Create(&mhCritSect);
    if(nRet != 0)
    {
        mhCritSect = NULL;
        WFDMMLOGE("WFDMMSinkGenericRenderer : Error in Critical Section Create");
    }

    SETSTATEDEINIT;
}

/*==============================================================================

         FUNCTION:          ~WFDMMSinkGenericRenderer

         DESCRIPTION:
*//**       @brief          Destructor for Generic renderer class
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
WFDMMSinkGenericRenderer::~WFDMMSinkGenericRenderer()
{
    WFDMMLOGH("WFDMMSinkGenericRenderer Destructor");
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
void WFDMMSinkGenericRenderer::InitData()
{
    mGenericQ  = NULL;
    mFrameInfoFreeQ = NULL;
    mGenericPresentQ = NULL;
    mGenericClearQ = NULL;
    mFrameInfoDataFreeQ = NULL;
    mFrameInfoDataClearQ = NULL;
    mpGenericDecodeThread = NULL;
    mpGenericRenderThread = NULL;
    mpFpGeneric= NULL;
    mbDumpGeneric= false;
    mhCritSect = NULL;
    mbMediaTimeSet = false;
    mbGenericTimeReset = true;
    memset(&mCfg, 0, sizeof(mCfg));
    meState = DEINIT;
    mClientData = 0;
    mnModuleId = 0;
    mpFnEBD = NULL;
    mpFnHandler = NULL;
    mnBaseMediaTime = 0;
    mbMediaTimeSet = false;
    mnErr = 0;
    mbFlushInProgress = false;
    mnFlushTimeStamp = 0;
    mbPaused = false;
    countPNG = 1;
    for(int i = 0; i < MAX_OVERLAY_ID ; i++)
    {
        mOvInfo[i].Id = -1;
        mOvInfo[i].isData = false;
        mOvInfo[i].blendMode = 0;
        mOvInfo[i].zLayer = Z_ORDER_INIT;
        mOvInfo[i].colorFormat = PIXEL_FORMAT_RGBA_8888;
        mOvInfo[i].presentMode = 0;
        mOvInfo[i].h = 0;
        mOvInfo[i].w = 0;
        mOvInfo[i].pGenericBM = new SkBitmap();
        if(mOvInfo[i].pGenericBM == NULL)
        {
            WFDMMLOGE1("Generic BM creation failed for %d",i);
            mnErr = -1;
        }
        else
        {
            SkImageInfo info = SkImageInfo::Make(1920, 1080,
                                kRGBA_8888_SkColorType, kPremul_SkAlphaType);
            mOvInfo[i].pGenericBM->allocPixels(info);

        }
    }
    for(int i = 0; i < MAX_OVERLAY_DISP; i++)
    {
        mDispOvInfo[i].Id = 0;
        mDispOvInfo[i].isFree = true;
    }
    for(int i = 0; i <3*MAX_OVERLAY_ID ; i++)
    {
        mDispOvClearInfo[i].Id = -1;
        mDispOvClearInfo[i].isValid = false;
        mDispOvClearInfo[i].nTime = 0;
    }
    mSurfaceComposerClient = new android::SurfaceComposerClient();
    if(mSurfaceComposerClient == NULL)
    {
        WFDMMLOGE("Generic SurfaceComposerClient failed");
        mnErr = -1;
    }
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
OMX_ERRORTYPE WFDMMSinkGenericRenderer::Configure(rendererConfigType *pCfg)
{
    CHECK_ARGS_RET_OMX_ERR_3(pCfg, mpFnHandler, mpFnEBD);

    memcpy(&mCfg, pCfg, sizeof(rendererConfigType));

    if(ISERROR || (createResources() != OMX_ErrorNone))
    {
        WFDMMLOGE("GenericRenderer Create Resources Failed");
        return OMX_ErrorInsufficientResources;
    }

    /* Screen Height and Width
         mCfg.sGenericInfo.surfaceHeight
         mCfg.sGenericInfo.surfaceWidth
    */

    /* Sink Presentation Screen Height and Width
         mCfg.sGenericInfo.presentationHeight
         mCfg.sGenericInfo.presentationWidth
    */

    /* Generic Overlay Height and Width
         mCfg.sGenericInfo.sessionHeight
         mCfg.sGenericInfo.sessionWidth
    */

    /* Delta calculation is being done to translate (0,0) of
       source to corresponding point on Sink. As sink has buttons.
    */
    mnDeltaXValue = (mCfg.sGenericInfo.surfaceWidth > mCfg.sGenericInfo.presentationWidth ?
                       (mCfg.sGenericInfo.surfaceWidth - mCfg.sGenericInfo.presentationWidth) : 0);
    mnDeltaYValue = (mCfg.sGenericInfo.surfaceHeight > mCfg.sGenericInfo.presentationHeight ?
                       (mCfg.sGenericInfo.surfaceHeight - mCfg.sGenericInfo.presentationHeight) : 0);

    WFDMMLOGH2("WFDMMSINKGenericRender : mnDeltaXValue = %d, mnDeltaYValue = %d",
        mnDeltaXValue,mnDeltaYValue);

    /* Currently Sink Screen size is less due to soft key and it is on right side.
       Hence no need to translate in X direction
    */
    mnDeltaXValue = 0;
    mfXPosScaleFactor = 1;
    mfYPosScaleFactor = 1;

    if(mCfg.sGenericInfo.sessionWidth && mCfg.sGenericInfo.sessionHeight)
    {
        mfXPosScaleFactor = ((float)mCfg.sGenericInfo.presentationWidth)/
                                (mCfg.sGenericInfo.sessionWidth);
        mfYPosScaleFactor = ((float)mCfg.sGenericInfo.presentationHeight)/
                            (mCfg.sGenericInfo.sessionHeight);
    }
    WFDMMLOGH2("WFDMMSINKGenericRender : mfXPosScaleFactor = %f, mfYPosScaleFactor = %f",
        mfXPosScaleFactor,mfYPosScaleFactor);
    WFDMMLOGH2("WFDMMSINKGenericRender : sessionWidth[%d] sessionHeight[%d]",
        mCfg.sGenericInfo.sessionWidth,mCfg.sGenericInfo.sessionHeight);

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
OMX_ERRORTYPE WFDMMSinkGenericRenderer::AllocateBuffers()
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
*//**       @brief          Generic Buffers are received through this
*//**
@par     DEPENDENCIES:
                            None
*//*
         PARAMETERS:
*//**       @param          trackId - Track identifier
                            pBuffer - Buffer Header

*//*     RETURN VALUE:
*//**       @return
                            OMX_ErrorType
@par     SIDE EFFECTS:
                            None
*//*==========================================================================*/
OMX_ERRORTYPE WFDMMSinkGenericRenderer::DeliverInput(int trackId,
                                             OMX_BUFFERHEADERTYPE *pBuffer)
{

    if (ISSTATECLOSING || ISSTATEDEINIT || ISSTATECLOSED)
    {
        /*----------------------------------------------------------------------
         We can allow buffer flow in INIT and OPENED
        ------------------------------------------------------------------------
        */
        WFDMMLOGE("Output Delivered when state not in play");
        return OMX_ErrorInvalidState;
    }
    WFDMMLOGM("WFDMMSinkGenericRenderer DeliverInput");

    if (!pBuffer)
    {
        WFDMMLOGE("Invalid Arguments");
        return OMX_ErrorBadParameter;
    }

    /*--------------------------------------------------------------------------
     Check if Generic is advertized during start up. Reject any samples
     from unsupported track
    ----------------------------------------------------------------------------
    */
    /* kamit TODO : Do we need this check here ? This can be moved out and if
       renderer is already created then it is fine ? */
    if(trackId == SINK_GENERIC_TRACK_ID && !mCfg.bHasGeneric)
    {
        WFDMMLOGE("Sample for track when track not advertized");
        return OMX_ErrorBadParameter;
    }

    /*--------------------------------------------------------------------------
        Check if timebase has changed and needs reset
    ----------------------------------------------------------------------------
    */
    /* kamit TODO : This has to be inline with what ever approach we take in
       media source relating to base time and pcr correction. */
    if(trackId == SINK_GENERIC_TRACK_ID &&
      mbGenericTimeReset && pBuffer->nFilledLen)
    {
        WFDMMLOGH("Generic track Time Reset. Waiting");
        if((pBuffer->nFlags & OMX_BUFFERFLAG_STARTTIME) == 0)
        {
            WFDMMLOGE("Renderer waiting for Generic track STARTTIME");
            return OMX_ErrorInvalidState;
        }
        else
        {
            mbGenericTimeReset = false;
        }
    }

#ifdef WFD_DUMP_GENRIC
    /*This is before decoding , so that it can be checked in other
      decoder for failure */
    /* kamit TODO : Generic track dumping needs to be done differently
       In this we need to maintain each frame in new file. */
    if(countPNG <= 12)
    {
        char fileName[50];
        snprintf(fileName,50,"/data/media/%d.png",countPNG);
        countPNG++;
        if(MM_File_Create(fileName, MM_FILE_CREATE_W, &mpFpGeneric))
        {
            WFDMMLOGE("Generic : Dump File creation failed");
        }
        if(mpFpGeneric)
        {
            ssize_t bytesWritten = 0;
            //MM_File_Write(mpFpGeneric, (char*)(pBuffer->pBuffer+OVERLAY_PRIVATE_HEADER_SIZE), (pBuffer->nFilledLen - OVERLAY_PRIVATE_HEADER_SIZE), &bytesWritten);
            MM_File_Write(mpFpGeneric, (char*)(pBuffer->pBuffer), (pBuffer->nFilledLen), &bytesWritten);
            WFDMMLOGE2("Generic :file Copied : filled len %llu, bytes written %lld",(pBuffer->nFilledLen - OVERLAY_PRIVATE_HEADER_SIZE),bytesWritten);
            if(bytesWritten == (pBuffer->nFilledLen-OVERLAY_PRIVATE_HEADER_SIZE))
            {
                WFDMMLOGE1("Generic : complete size copied to file for %d",(countPNG -1));
            }
            MM_File_Release(mpFpGeneric);
            mpFpGeneric = NULL;
        }
    }
#endif

    frameInfoType *pFrameInfo = NULL;
    accessFrameInfoMeta(&pFrameInfo, popfront);
    if (!pFrameInfo)
    {
        WFDMMLOGE("Failed to get frameInfo");
        /*kamit : ToDo should we bail out or proceed*/
        return OMX_ErrorInsufficientResources;
    }

    /*No parsing here, pass it on to renderer */
    pFrameInfo->pBuffer = pBuffer;

    if(!Scheduler(trackId, pFrameInfo))
    {
        accessFrameInfoMeta(&pFrameInfo, pushrear);
        NOTIFYERROR;
        WFDMMLOGE1("WFDMMSinkGenericRenderer Failed to Schedule track = %d", trackId);
        return OMX_ErrorUndefined;
    }

    return OMX_ErrorNone;
}

/*==============================================================================

         FUNCTION:         decodeGenericHeader

         DESCRIPTION:
*//**       @brief         Decodes Generic Header data received as part of frame.
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
void WFDMMSinkGenericRenderer::decodeGenericHeader(OMX_BUFFERHEADERTYPE *pBuf,overlayInfo *ovInfo)
{

    overlayInfo *pOvInfo = ovInfo;

    /* 1 byte overlay id*/
    int nOvId = pBuf->pBuffer[0];

    /* 2 bits for Presentation mode:
          00 : Deferred Presentation Mode
          01 : Active Presentation Mode
          10 : Deactivate Presentation
          11 : Reserved
    */
    int pMode = (pBuf->pBuffer[1] >> 6) & 0x03;

    /* 1 bit data present flag*/
    int dFlag = (pBuf->pBuffer[1] >> 5 ) & 0x01;

    /* 2 bytes each for X & Y Coordinate */
    uint32 nOvPosTopLeft_x = ((pBuf->pBuffer[2])<<0x08)|(pBuf->pBuffer[3]);
    uint32 nOvPosTopLeft_y = ((pBuf->pBuffer[4])<<0x08)|(pBuf->pBuffer[5]);

    /* 2 bytes each for width and height */
    uint32 nOvWidth =  ((pBuf->pBuffer[6])<<0x08)|(pBuf->pBuffer[7]);
    uint32 nOvHeight = ((pBuf->pBuffer[8])<<0x08)|(pBuf->pBuffer[9]);

    /* 1 byte for Z-order used for blending */
    int nOvZlayer = Z_ORDER_INIT + pBuf->pBuffer[10];

    /* Lower 4 bits for blending mode */
    int nBlendMode = (pBuf->pBuffer[11] & 0x0F);

    /* Lower 5 bits for color bitWidth */
    int nColorWidth = (pBuf->pBuffer[12] & 0x1F);

    /*WFDMMLOGE3("Generic Frame Data : Overlay Id = %d, pMode = %d, dFlag = %d",nOvId,pMode,dFlag);
    WFDMMLOGE2("Generic Frame Data : x = %lu, y = %lu ",nOvPosTopLeft_x,nOvPosTopLeft_y);
    WFDMMLOGE2("Generic Frame Data : width = %lu, height = %lu ",nOvWidth,nOvHeight);
    WFDMMLOGE3("Generic Frame Data : Zlayer = %d, nBlendMode = %d, nColorWidth = %d",nOvZlayer,nBlendMode,nColorWidth);*/

    ALOGE("Generic Frame Data : Overlay Id = %d, pMode = %d, dFlag = %d",nOvId,pMode,dFlag);
    ALOGE("Generic Frame Data : x = %lu, y = %lu ",nOvPosTopLeft_x,nOvPosTopLeft_y);
    ALOGE("Generic Frame Data : width = %lu, height = %lu ",nOvWidth,nOvHeight);
    ALOGE("Generic Frame Data : Zlayer = %d, nBlendMode = %d, nColorWidth = %d",nOvZlayer,nBlendMode,nColorWidth);

    pOvInfo->isData = dFlag;
    pOvInfo->Id  = nOvId;
    pOvInfo->x   = nOvPosTopLeft_x;
    pOvInfo->y   = nOvPosTopLeft_y;
    pOvInfo->w   = nOvWidth;
    pOvInfo->h   = nOvHeight;
    pOvInfo->blendMode = nBlendMode;
    pOvInfo->presentMode = pMode;
    pOvInfo->zLayer = nOvZlayer;
    pOvInfo->colorFormat = PIXEL_FORMAT_RGBA_8888;//kamit : Todo it should be derived from received data.
}


/*==============================================================================

         FUNCTION:         decodeGeneric

         DESCRIPTION:
*//**       @brief         Decodes Generic frame received.
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
bool WFDMMSinkGenericRenderer::decodeGeneric(OMX_BUFFERHEADERTYPE *pBuf,frameInfoType *pFrameInfo)
{
    int eRet = false;

    /* Null check*/
    if(!pBuf || !pFrameInfo)
    {
        WFDMMLOGE1("decodeGeneric : Buffer/ pFrameinfo Null buf = %p",pBuf);
        return eRet;
    }

    pFrameInfo->pBuffer->nTimeStamp = pBuf->nTimeStamp;
    overlayInfo *pOvInfo = (overlayInfo *)pFrameInfo->pBuffer->pBuffer;

    decodeGenericHeader(pBuf,pOvInfo);

    if(pOvInfo->isData)
    {
        /* Data is present, Pass for decoding */
        void *pDataPtr = pBuf->pBuffer + OVERLAY_PRIVATE_HEADER_SIZE;
        //WFDMMLOGE3("Generic Frame Data : data1 = %x, data2 = %x, data3 = %x",pBuf->pBuffer[0],pBuf->pBuffer[1],pBuf->pBuffer[2]);
        //ALOGE("decodeGeneric Frame Data : data1 = %x, data2 = %x, data3 = %x",pBuf->pBuffer[0],pBuf->pBuffer[1],pBuf->pBuffer[2]);
        WFDMMLOGE3("decodeGeneric Frame Data : data1 = %x, data2 = %x, data3 = %x",((char*)pDataPtr)[0],((char*)pDataPtr)[1],((char*)pDataPtr)[2]);
        uint32 nGenericSize  = pBuf->nFilledLen - OVERLAY_PRIVATE_HEADER_SIZE;
        //WFDMMLOGE1("Generic Frame Data : size = %lu",nGenericSize);
        WFDMMLOGE1("decodeGeneric Frame Data : size = %lu",nGenericSize);

        if(SkImageDecoder::DecodeMemory(pDataPtr,nGenericSize,
                                        pOvInfo->pGenericBM) == false)
        {
            /* Return is either True or False*/
            //WFDMMLOGE1("Generic Frame decoding failed, BM = %p",mOvInfo[nIndex].pGenericBM);
            ALOGE("Generic Frame decoding failed");
            return eRet;
        }
    }
    eRet = true;

    //WFDMMLOGE1("decodeGeneric : Decoding done for ID = %d",nOvId);
    ALOGE("decodeGeneric : Decoding done for ID = %d",pOvInfo->Id);
    return eRet;
}

/*==============================================================================

         FUNCTION:         createResources

         DESCRIPTION:
*//**       @brief         creates resources for WFDMMSinkGenericRenderer
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
OMX_ERRORTYPE WFDMMSinkGenericRenderer::createResources()
{

    if(!createThreadsAndQueues())
    {
        WFDMMLOGE("Renderer Failed to create Threads and Queues");
        return OMX_ErrorInsufficientResources;
    }
    if(mCfg.bHasGeneric)
    {
        if(!createGenericResources())
        {
            WFDMMLOGE("GenericRenderer failed to create resources");
            NOTIFYERROR;
            return OMX_ErrorInsufficientResources;
        }
    }

    prepareDumpFiles();

    return OMX_ErrorNone;
}

/*==============================================================================

         FUNCTION:          createGenericTrack

         DESCRIPTION:
*//**       @brief          creates Generic Track
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

bool WFDMMSinkGenericRenderer::createGenericTrack()
{
    /* kamit : TODO Initial configuration and api's to be called */
    return true;
}

/*==============================================================================

         FUNCTION:          createGenericResources

         DESCRIPTION:
*//**       @brief          creates Generic Resources.
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
bool WFDMMSinkGenericRenderer::createGenericResources()
{
    /* kamit : TODO : Check if some thing is needed here ?
       if not we can remove this as not required in this case */
    if(createGenericTrack() == false)
    {
        WFDMMLOGE("Failed to create Generic Track");
        return false;
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
bool WFDMMSinkGenericRenderer::createThreadsAndQueues()
{
    /*--------------------------------------------------------------------------
      Create threads if session has generic data
    ----------------------------------------------------------------------------
    */
    if(mCfg.bHasGeneric)// kamit : TODO : is this check required here ?
    {
        /* Threads are created here */

        // This will be used for decoding of PNG data.
        mpGenericDecodeThread = MM_New_Args(WFDMMThreads, (1));

        if(!mpGenericDecodeThread)
        {
            WFDMMLOGE("Failed to create Generic Decode Thread");
            return false;
        }
        mpGenericDecodeThread->Start(GenericDecodeThreadEntry, -2,
                             65536*2, this, "WFDSinkGenericRendererDecode");

        //This will be used to present PNG data over Screen
        mpGenericRenderThread = MM_New_Args(WFDMMThreads, (1));

        if(!mpGenericRenderThread)
        {
            WFDMMLOGE("Failed to create Generic Render Thread");
            return false;
        }
        mpGenericRenderThread->Start(GenericRenderThreadEntry, -5,
                             65536*2, this, "WFDSinkGenericRendererRender");
    }


    mGenericQ = MM_New_Args(SignalQueue, (100, sizeof(int*)));
    if(!mGenericQ)
    {
        WFDMMLOGE("Failed to allocate the mGenericQ");
        return false;
    }

    mGenericPresentQ = MM_New_Args(SignalQueue, (100, sizeof(int*)));
    if(!mGenericPresentQ)
    {
        WFDMMLOGE("Failed to allocate the mGenericPresentQ");
        return false;
    }

    mGenericClearQ = MM_New_Args(SignalQueue, (100, sizeof(int*)));
    if(!mGenericClearQ)
    {
        WFDMMLOGE("Failed to allocate the mGenericClearQ");
        return false;
    }

    /*--------------------------------------------------------------------------
     Frame Info Q is used to keep a list of frameInfo structures. These are
     meta data associated with each buffer that is going to be scheduled for
     decoding
    ----------------------------------------------------------------------------
    */
    mFrameInfoFreeQ = MM_New_Args(SignalQueue,
                                  (2*MAX_OVERLAY_ID, sizeof(int*)));
    if(!mFrameInfoFreeQ)
    {
        WFDMMLOGE("Failed to allocate the mFrameInfoFreeQ");
        return false;
    }
    /*--------------------------------------------------------------------------
     Populate the FreeQ with all the frameInfo structure objects that we have
    ----------------------------------------------------------------------------
    */
    OMX_BUFFERHEADERTYPE *pBufferHdr;
    for(int i = 0; i < 2*MAX_OVERLAY_ID; i++)
    {

        uint8 *pFrameInfoPtr = (uint8*)(msFrameInfo + i);
        mFrameInfoFreeQ->Push(&pFrameInfoPtr, sizeof(int*));
    }

    /*--------------------------------------------------------------------------
     Frame Info Q is used to keep a list of frameInfo structures. These are
     meta data associated with each buffer that is going to be scheduled for
     rendering. They will contain png decoded data.
    ----------------------------------------------------------------------------
    */
    mFrameInfoDataFreeQ = MM_New_Args(SignalQueue,
                                  (MAX_OVERLAY_ID, sizeof(int*)));
    if(!mFrameInfoDataFreeQ)
    {
        WFDMMLOGE("Failed to allocate the FrameInfoDataFreeQ");
        return false;
    }
    /*--------------------------------------------------------------------------
     Populate the FreeQ with all the frameInfo structure objects that we have
    ----------------------------------------------------------------------------
    */
    for(int i = 0; i < MAX_OVERLAY_ID; i++)
    {
        pBufferHdr = NULL;
        pBufferHdr = (OMX_BUFFERHEADERTYPE*)
                   MM_Malloc(sizeof(OMX_BUFFERHEADERTYPE));
        if(!pBufferHdr)
        {
            WFDMMLOGE("Failed to allocate Generic Render BufferHdr");
            return false;
        }

        memset(pBufferHdr, 0, sizeof(OMX_BUFFERHEADERTYPE));

        (msFrameInfoData + i)->pBuffer = pBufferHdr;
        pBufferHdr->pBuffer = (OMX_U8 *)&mOvInfo[i];

        if(!pBufferHdr->pBuffer)
        {
            WFDMMLOGE("Failed to allocate Generic Render Buffers");
            return false;
        }
        pBufferHdr->nAllocLen = sizeof(overlayInfo);

        uint8 *pFrameInfoPtr = (uint8*)(msFrameInfoData + i);
        mFrameInfoDataFreeQ->Push(&pFrameInfoPtr, sizeof(int*));
    }

    /*--------------------------------------------------------------------------
     Frame Info Q is used to keep a list of frameInfo structures. These are
     meta data associated with each buffer that is going to be scheduled for
     rendering. They will contain png decoded data.
    ----------------------------------------------------------------------------
    */
    mFrameInfoDataClearQ = MM_New_Args(SignalQueue,
                                  (3*MAX_OVERLAY_ID, sizeof(int*)));
    if(!mFrameInfoDataClearQ)
    {
        WFDMMLOGE("Failed to allocate the mFrameInfoDataClearQ");
        return false;
    }
    /*--------------------------------------------------------------------------
     Populate the FreeQ with all the frameInfo structure objects that we have
    ----------------------------------------------------------------------------
    */
    for(int i = 0; i < 3*MAX_OVERLAY_ID; i++)
    {
        pBufferHdr = NULL;
        pBufferHdr = (OMX_BUFFERHEADERTYPE*)
                   MM_Malloc(sizeof(OMX_BUFFERHEADERTYPE));
        if(!pBufferHdr)
        {
            WFDMMLOGE("Failed to allocate Generic Render BufferHdr");
            return false;
        }

        memset(pBufferHdr, 0, sizeof(OMX_BUFFERHEADERTYPE));

        (msFrameInfoClear + i)->pBuffer = pBufferHdr;
        pBufferHdr->pBuffer = (OMX_U8 *)&mDispOvClearInfo[i];

        if(!pBufferHdr->pBuffer)
        {
            WFDMMLOGE("Failed to allocate Generic Render Buffers");
            return false;
        }
        pBufferHdr->nAllocLen = sizeof(overlayClearInfo);

        uint8 *pFrameInfoPtr = (uint8*)(msFrameInfoClear + i);
        mFrameInfoDataClearQ->Push(&pFrameInfoPtr, sizeof(int*));
    }
    return true;
}

/*==============================================================================

         FUNCTION:          accessFrameInfoMeta

         DESCRIPTION:
*//**       @brief          Objects of frameInfo are used to store Generic
                            buffers in queues before being rendered.
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
bool WFDMMSinkGenericRenderer::accessFrameInfoMeta(frameInfoType **pFrameInfo,
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
            WFDMMLOGE("WFDMMSGR:accessFrameInfoMeta:FrameInfo Popped is NULL");
        }
        else if(action == pushrear || action == pushfront)
        {
            if(!(*pFrameInfo))
            {
                WFDMMLOGE("WFDMMSGR:accessFrameInfoMeta:Invalid FrameInfo Pushed");
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
    WFDMMLOGE("WFDMMSGR:accessFrameInfoMeta:Failed to operate on FrameInfo Q");
    CRITICAL_SECT_LEAVE
    return false;
}

/*==============================================================================

         FUNCTION:          accessFrameInfoData

         DESCRIPTION:
*//**       @brief          Objects of frameInfo are used to store Generic
                            buffers in queues before being rendered.
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
bool WFDMMSinkGenericRenderer::accessFrameInfoData(frameInfoType **pFrameInfo,
                                            actionType action)
{
    CRITICAL_SECT_ENTER
    /*--------------------------------------------------------------------------
     Note that this Q supports only push and pop. Therefore poprear and popfront
     maps to pop and pushrear and pushfront maps to push
    ----------------------------------------------------------------------------
    */
    if(mFrameInfoDataFreeQ && pFrameInfo)
    {
        if(action == poprear || action == popfront)
        {
            *pFrameInfo = NULL;

            mFrameInfoDataFreeQ->Pop(pFrameInfo, sizeof(pFrameInfo), 0);

            if(*pFrameInfo)
            {
                /*--------------------------------------------------------------
                 Found a free frameinfo object
                ----------------------------------------------------------------
                */
                CRITICAL_SECT_LEAVE
                return true;
            }
            WFDMMLOGE("WFDMMSGR:accessFrameInfoData:FrameInfo Popped is NULL");
        }
        else if(action == pushrear || action == pushfront)
        {
            if(!(*pFrameInfo))
            {
                WFDMMLOGE("WFDMMSGR:accessFrameInfoData:Invalid FrameInfo Pushed");
                CRITICAL_SECT_LEAVE;
                return false;
            }
            OMX_ERRORTYPE eErr =
                mFrameInfoDataFreeQ->Push(pFrameInfo, sizeof(pFrameInfo));

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
    WFDMMLOGE("WFDMMSGR:accessFrameInfoData:Failed to operate on FrameInfo Q");
    CRITICAL_SECT_LEAVE
    return false;
}

/*==============================================================================

         FUNCTION:          accessFrameInfoClear

         DESCRIPTION:
*//**       @brief          Objects of frameInfo are used to store Generic
                            buffers in queues before being rendered.
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
bool WFDMMSinkGenericRenderer::accessFrameInfoClear(frameInfoType **pFrameInfo,
                                            actionType action)
{
    CRITICAL_SECT_ENTER
    /*--------------------------------------------------------------------------
     Note that this Q supports only push and pop. Therefore poprear and popfront
     maps to pop and pushrear and pushfront maps to push
    ----------------------------------------------------------------------------
    */
    if(mFrameInfoDataClearQ && pFrameInfo)
    {
        if(action == poprear || action == popfront)
        {
            *pFrameInfo = NULL;

            mFrameInfoDataClearQ->Pop(pFrameInfo, sizeof(pFrameInfo), 0);

            if(*pFrameInfo)
            {
                /*--------------------------------------------------------------
                 Found a free frameinfo object
                ----------------------------------------------------------------
                */
                CRITICAL_SECT_LEAVE
                return true;
            }
            WFDMMLOGE("WFDMMSGR:accessFrameInfoClear:FrameInfo Popped is NULL");
        }
        else if(action == pushrear || action == pushfront)
        {
            if(!(*pFrameInfo))
            {
                WFDMMLOGE("WFDMMSGR:accessFrameInfoClear:Invalid FrameInfo Pushed");
                CRITICAL_SECT_LEAVE;
                return false;
            }
            OMX_ERRORTYPE eErr =
                mFrameInfoDataClearQ->Push(pFrameInfo, sizeof(pFrameInfo));

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
    WFDMMLOGE("WFDMMSGR:accessFrameInfoClear:Failed to operate on FrameInfo Q");
    CRITICAL_SECT_LEAVE
    return false;
}


/*==============================================================================

         FUNCTION:          Scheduler

         DESCRIPTION:
*//**       @brief          Schedules Generic Frames for rendering
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
bool WFDMMSinkGenericRenderer::Scheduler(int trackId, frameInfoType *pFrameInfo)
{

    /*--------------------------------------------------------------------------
     TODO - do any adjustments based on different propagation delays here.
     For this delays of audio and video paths has to be measured.
    ----------------------------------------------------------------------------
    */
    pFrameInfo->nTSArrival = getCurrentTimeUs();
    pFrameInfo->nTSSched   = pFrameInfo->pBuffer->nTimeStamp;

    if(trackId == SINK_GENERIC_TRACK_ID)// kamit  TODO : is this check required
    {
        /* kamit : TODO : This complete logic has to be in sync with mediasource logic*/
        if(!mbMediaTimeSet)
        {
            mnBaseMediaTime = 0;
            mnBaseMediaTime = getCurrentTimeUs();
            mbMediaTimeSet  = true;
        }
        WFDMMLOGH("Push Generic to Pending Q");
        if(mGenericQ)
        {
            mGenericQ->Push(&pFrameInfo, sizeof(&pFrameInfo));
        }
        return true;
    }

    return false;

}

/*==============================================================================

         FUNCTION:          PushGeneric4Render

         DESCRIPTION:
*//**       @brief          Render the latest generic frame
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
bool WFDMMSinkGenericRenderer::PushGeneric4Render(frameInfoType *pFrameInfo)
{
    /*--------------------------------------------------------------------------
      See if there are Generic frames pending for rendering
    ----------------------------------------------------------------------------
    */

    if(pFrameInfo != NULL)
    {
        //WFDMMLOGE("PushGeneric4Render : pFrameInfo is valid");
        ALOGE("PushGeneric4Render : pFrameInfo is valid");
        overlayInfo *pGeneric = (overlayInfo*)pFrameInfo->pBuffer->pBuffer;
        if(pGeneric != NULL)
        {
            ALOGE("PushGeneric4Render : pFrameInfo : pGeneric is valid");
            if(CreateSurface(pGeneric) == true)
            {
                int nIndex = pGeneric->nIndex;
                ALOGE("PushGeneric4Render : pFrameInfo : pGeneric surface created");
                native_window_set_buffer_count((ANativeWindow*)mDispOvInfo[nIndex].surface.get(), 3);

                android::SurfaceComposerClient::openGlobalTransaction();
                /* Layer to be presented */
                mDispOvInfo[nIndex].surfaceControl->setLayer(pGeneric->zLayer);
                /*Position left , top */
                int x_pos =(int)(pGeneric->x * mfXPosScaleFactor) + mnDeltaXValue;
                int y_pos =(int)(pGeneric->y * mfYPosScaleFactor) + mnDeltaYValue;
                ALOGE("PushGeneric4Render : pFrameInfo : x_pos= %d , y_pos = %d",x_pos,y_pos);
                mDispOvInfo[nIndex].surfaceControl->setPosition(x_pos,y_pos);
                mDispOvInfo[nIndex].surfaceControl->show();
                android::SurfaceComposerClient::closeGlobalTransaction(true);

                ANativeWindow_Buffer sBuffer;
                mDispOvInfo[nIndex].surface->lock(&sBuffer, NULL);

                uint32 bmBytesPerPixel = pGeneric->pGenericBM->bytesPerPixel();
                uint32 bmHeight = pGeneric->pGenericBM->height();
                uint32 bmwidth = pGeneric->pGenericBM->width();
                uint32 bmEffectiveRowBytes = bmwidth * bmBytesPerPixel;
                uint32 bmRowBytes = pGeneric->pGenericBM->rowBytes();
                ALOGE("PushGeneric4Render : pFrameInfo : BM Height = %lu , width = %lu",bmHeight,bmwidth);
                ALOGE("PushGeneric4Render : pFrameInfo : BM bpp = %lu , rowbytes = %lu",bmBytesPerPixel,bmRowBytes);


                size_t bufBytesPerPixel = bytesPerPixel(sBuffer.format);
                /* bytes per row of actual image of destination buffer */
                size_t bufEffectiveRowBytes  = sBuffer.width * bufBytesPerPixel;
                /* bytes per row of final image(including padding) of destination buffer */
                size_t bufRowBytes = sBuffer.stride * bufBytesPerPixel;
                ALOGE("PushGeneric4Render : pFrameInfo : sBuffer Height = %d , width = %d",sBuffer.height,sBuffer.width);
                ALOGE("PushGeneric4Render : pFrameInfo : sBuffer bpp = %lu , stride = %d",bufBytesPerPixel,sBuffer.stride);

                /*Copy data to be sent to native window */
                uint8* bmPixel = (uint8*)pGeneric->pGenericBM->getPixels();
                uint8_t* img = reinterpret_cast<uint8_t*>(sBuffer.bits);
                if(bmPixel != NULL)
                {
                    for (uint32 y = 0; y < bmHeight; y++) {
                        memcpy(&img[y * bufRowBytes],(char*)&(bmPixel[y * bmEffectiveRowBytes]), bmEffectiveRowBytes);
                    }
                }
                mDispOvInfo[nIndex].surface->unlockAndPost();
                //WFDMMLOGE("PushGeneric4Render : pFrameInfo : pGeneric : Posting to surface");
                ALOGE("PushGeneric4Render : pFrameInfo : pGeneric : Posting to surface");
            }
        }
    }

    return true;
}

/*==============================================================================

         FUNCTION:          CreateSurface

         DESCRIPTION:
*//**       @brief          Check if already allocated surface can be used else
                            create a new one. If there is none created then
                            create new Surface.
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
bool WFDMMSinkGenericRenderer::CreateSurface(overlayInfo *generic)
{
    /* Not checking for NULL as it has to be checked in caller
       function */
    bool  bRet = false;
    int nClearIndex = ClearGeneric(generic->Id);
    int nInputIndex = -1;

    {
        /* If there was a proper clearIndex then it is fine */
        if(nClearIndex >= 0 && nClearIndex < MAX_OVERLAY_DISP)
        {
            nInputIndex = nClearIndex;
        }
        else /* Find out a proper one and use it */
        {
            for(int i = 0; i < MAX_OVERLAY_DISP ; i++)
            {
                if(mDispOvInfo[i].isFree == true)
                {
                    nInputIndex = i;
                    break;
                }
            }
        }

        if(nInputIndex < 0)
        {
            ALOGE("CreateSurface : No Free index for id = %d",generic->Id);
            return bRet;
        }

        ALOGE("CreateSurface : Creating new surface for id = %d",generic->Id);
        mDispOvInfo[nInputIndex].surfaceControl=
                    mSurfaceComposerClient->createSurface
                        (
                         String8("OverSurf"),
                         generic->w,
                         generic->h,
                         generic->colorFormat,
                         0
                        );
        if(mDispOvInfo[nInputIndex].surfaceControl.get() != NULL)
        {
            mDispOvInfo[nInputIndex].surface =
                mDispOvInfo[nInputIndex].surfaceControl->getSurface();
            mDispOvInfo[nInputIndex].surfaceControl->setMatrix(mfXPosScaleFactor,0,0,mfYPosScaleFactor);
            generic->nIndex = nInputIndex;
            bRet = true;
        }
        mDispOvInfo[nInputIndex].Id = generic->Id;
        mDispOvInfo[nInputIndex].isFree = (!bRet);
        ALOGE("CreateSurface : mDispOvInfo[%d].id = %d",nInputIndex,mDispOvInfo[nInputIndex].Id);
        ALOGE("CreateSurface : mDispOvInfo[%d].isFree = %d",nInputIndex,mDispOvInfo[nInputIndex].isFree);
        ALOGE("CreateSurface : mDispOvInfo[%d].surfaceControl = %p",nInputIndex,mDispOvInfo[nInputIndex].surfaceControl.get());
    }
    return bRet;
}

/*==============================================================================

         FUNCTION:          ClearGeneric

         DESCRIPTION:
*//**       @brief          Clear the Generic Frame being rendered.
*//**
@par     DEPENDENCIES:
                            None
*//*
         PARAMETERS:
*//**       @param          Takes the overlay id which needs to be cleared.

*//*     RETURN VALUE:
*//**       @return
                            true or false
@par     SIDE EFFECTS:
                            None
*//*==========================================================================*/
int WFDMMSinkGenericRenderer::ClearGeneric(int id)
{
    //bool eRet = true;
    int nRet = -1;
    ALOGE("ClearGeneric called for %d",id);
    for(int i = 0; i < MAX_OVERLAY_DISP ; i++)
    {
        if(mDispOvInfo[i].Id == id)
        {
            nRet = i;
            ALOGE("ClearGeneric: index %d found for id %d",i,id);
            break;
        }
    }
    if(nRet >= 0 && nRet < MAX_OVERLAY_DISP)
    {
        ALOGE("ClearGeneric : surface cleared mDispOvInfo[%d].id = %d",nRet,id);
        ALOGE("ClearGeneric : surface cleared mDispOvInfo[%d].surfaceControl = %p",nRet,mDispOvInfo[nRet].surfaceControl.get());
        if(mDispOvInfo[nRet].surfaceControl.get() != NULL)
        {
            mDispOvInfo[nRet].surfaceControl->clear();
        }
        mDispOvInfo[nRet].isFree = true;
    }
    return nRet;
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
OMX_ERRORTYPE WFDMMSinkGenericRenderer::deinitialize()
{

    if(!ISSTATECLOSED || !ISSTATEINIT)
    {
        SETSTATECLOSING;
    }

    WFDMMLOGH("Wait for all buffers to be returned");

    int nGenricQLen, nPresentQLen , nClearQLen;
    while (1)
    {
        nGenricQLen   = mGenericQ ? mGenericQ->GetSize() : 0;
        nPresentQLen  = mGenericPresentQ ? mGenericPresentQ->GetSize() : 0;
        nClearQLen    = mGenericClearQ ? mGenericClearQ->GetSize() : 0;

        frameInfoType *pFrameInfo = NULL;
        for(int i = 0 ; i < nPresentQLen ; i++)
        {
            mGenericPresentQ->Pop(&pFrameInfo,sizeof(&pFrameInfo),0);
            accessFrameInfoData(&pFrameInfo, pushfront);
        }

        pFrameInfo = NULL;
        for(int i = 0 ; i < nClearQLen ; i++)
        {
            mGenericClearQ->Pop(&pFrameInfo,sizeof(&pFrameInfo),0);
            accessFrameInfoClear(&pFrameInfo, pushfront);
        }

        nPresentQLen  = mGenericPresentQ ? mGenericPresentQ->GetSize() : 0;
        nClearQLen    = mGenericClearQ ? mGenericClearQ->GetSize() : 0;

        WFDMMLOGE3("QueueLength at exit Generic - %d , present - %d, clear - %d",
                    nGenricQLen,nPresentQLen,nClearQLen);

        if(!nGenricQLen && !nPresentQLen && !nClearQLen)
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
    if(mpGenericDecodeThread)
    {
        MM_Delete(mpGenericDecodeThread);
        mpGenericDecodeThread = NULL;
    }

    if(mpGenericRenderThread)
    {
        MM_Delete(mpGenericRenderThread);
        mpGenericRenderThread = NULL;
    }

    if(mGenericQ)
    {
        MM_Delete(mGenericQ);
        mGenericQ = NULL;
    }

    if(mGenericPresentQ)
    {
        MM_Delete(mGenericPresentQ);
        mGenericPresentQ = NULL;
    }

    if(mGenericClearQ)
    {
        MM_Delete(mGenericClearQ);
        mGenericClearQ = NULL;
    }

    if(mFrameInfoFreeQ)
    {
        MM_Delete(mFrameInfoFreeQ);
        mFrameInfoFreeQ = NULL;
    }

    if(mFrameInfoDataFreeQ)
    {
        for(int i = 0; i < MAX_OVERLAY_ID; i++)
        {
            if(msFrameInfoData[i].pBuffer != NULL)
            {
                MM_Free(msFrameInfoData[i].pBuffer);
                msFrameInfoData[i].pBuffer = NULL;
            }
        }
        MM_Delete(mFrameInfoDataFreeQ);
        mFrameInfoDataFreeQ = NULL;
    }

    if(mFrameInfoDataClearQ)
    {
        for(int i = 0; i < 3*MAX_OVERLAY_ID; i++)
        {
            if(msFrameInfoClear[i].pBuffer != NULL)
            {
                MM_Free(msFrameInfoClear[i].pBuffer);
                msFrameInfoClear[i].pBuffer = NULL;
            }
        }
        MM_Delete(mFrameInfoDataClearQ);
        mFrameInfoDataClearQ = NULL;
    }

    if (mpFpGeneric)
    {
        MM_File_Release(mpFpGeneric);
        mpFpGeneric = NULL;
    }

    for(int i = 0; i < MAX_OVERLAY_ID ; i++)
    {
        mOvInfo[i].Id = -1;
        mOvInfo[i].blendMode = 0;
        mOvInfo[i].zLayer = Z_ORDER_INIT;
        mOvInfo[i].colorFormat = PIXEL_FORMAT_RGBA_8888;
        mOvInfo[i].presentMode = 0;
        mOvInfo[i].h = 0;
        mOvInfo[i].w = 0;
        if(mOvInfo[i].pGenericBM != NULL)
        {
            delete mOvInfo[i].pGenericBM;
            mOvInfo[i].pGenericBM = NULL;
        }
    }

    ALOGE("Clearing in Deinitialize all available overlay");

    for(int i = 0; i < MAX_OVERLAY_DISP; i++)
    {
        ALOGE("Clearing mDispOvInfo[%d].surfaceControl = %p",i,mDispOvInfo[i].surfaceControl.get());
        if(mDispOvInfo[i].surfaceControl.get()!=NULL)
        {
            mDispOvInfo[i].surfaceControl->clear();
            ALOGE("Clearing mDispOvInfo[%d].surfaceControl clear called",i);
        }
        mDispOvInfo[i].isFree = true;
    }

    if (mhCritSect)
    {
        MM_CriticalSection_Release(mhCritSect);
        mhCritSect = NULL;
    }

    return OMX_ErrorNone;
}

/*==============================================================================

         FUNCTION:         GenericDecodeThreadEntry

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
void WFDMMSinkGenericRenderer::GenericDecodeThreadEntry(void* pThis,
                                         unsigned int nSignal)
{
    CHECK_ARGS_RET_VOID_1(pThis);

    WFDMMSinkGenericRenderer *pMe = (WFDMMSinkGenericRenderer*)pThis;

    if(pMe)
    {
        pMe->GenericDecodeThread(nSignal);
    }
}

/*==============================================================================

         FUNCTION:         GenericRenderThreadEntry

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
void WFDMMSinkGenericRenderer::GenericRenderThreadEntry(void* pThis,
                                         unsigned int nSignal)
{
    CHECK_ARGS_RET_VOID_1(pThis);

    WFDMMSinkGenericRenderer *pMe = (WFDMMSinkGenericRenderer*)pThis;

    if(pMe)
    {
        pMe->GenericRenderThread(nSignal);
    }
}


/*==============================================================================

         FUNCTION:         GenericDecodeThread

         DESCRIPTION:
*//**       @brief         Generic Thread for rendering data on Surface
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
void WFDMMSinkGenericRenderer::GenericDecodeThread(unsigned int nSignal)
{
    (void) nSignal;
    WFDMMLOGH("WFDMMSGR:GDT:Decode Thread Entered");
    int nDropCount = 0;
    uint64 timedecider;

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
        mGenericQ->Peek(&pFrameInfo,
                     sizeof(&pFrameInfo));

        if(pFrameInfo)
        {
            /* This check is not required as buffer is already validated */
            if(pFrameInfo->pBuffer == NULL)
            {
                WFDMMLOGE("WFDMMSGR:GDT:Generic Frame not valid");
                mGenericQ->Pop(&pFrameInfo,sizeof(&pFrameInfo),0);
                MM_Timer_Sleep(2);
                continue;
            }
            WFDMMLOGE("WFDMMSGR:GDT:Got a Pending Generic Frame");

            overlayInfo pGeneric;
            decodeGenericHeader(pFrameInfo->pBuffer, &pGeneric);
            if(pGeneric.isData && pGeneric.presentMode == 1)
            {
                /* PNG data. It needs to be decoded and pushed on to
                   mGenericPresentQ to get scheduled for presentation */

                /* Apply AV Sync to this frame.*/
                uint64 bufferTS = pFrameInfo->pBuffer->nTimeStamp;
                timedecider = (bufferTS - getCurrentTimeUs())+ mCfg.nDecoderLatency;

                if((((int64)timedecider <= GENERIC_AV_SYNC_DROP_WINDOW)&&
                      mCfg.bAVSyncMode) ||
                      ISSTATECLOSING ||
                     (mbFlushInProgress && (bufferTS <= mnFlushTimeStamp)))
                {
                    /* It is late or state is closing or flush is on.
                       This frame will be dropped.
                    */
                    WFDMMLOGE2("WFDMMSGR:GDT:Renderer drop\
                                Genericframe TS =%llu, lateby = %lld",
                                bufferTS,(int64)timedecider);

                    pFrameInfo->pBuffer->nFilledLen = 0;
                    mGenericQ->Pop(&pFrameInfo,sizeof(&pFrameInfo),0);
                    mpFnEBD(mClientData, mnModuleId,
                            SINK_GENERIC_TRACK_ID,pFrameInfo->pBuffer);
                    accessFrameInfoMeta(&pFrameInfo, pushfront);

                    if(mGenericQ->GetSize() == 0)
                    {
                        MM_Timer_Sleep(2);
                    }
                }
                else
                {
                    /* Decode it here */
                    frameInfoType *pFrameInfoData = NULL;
                    accessFrameInfoData(&pFrameInfoData, poprear);
                    if(pFrameInfoData != NULL)
                    {
                        if(decodeGeneric(pFrameInfo->pBuffer,
                                         pFrameInfoData) == false)
                        {
                            WFDMMLOGE("WFDMMSGR:GDT:Decoding failed");
                            accessFrameInfoData(&pFrameInfoData, pushfront);
                        }
                        else
                        {
                            mGenericPresentQ->Push(&pFrameInfoData,
                                                    sizeof(&pFrameInfoData));
                            overlayClearInfo *pOvDispInfo =
                                (overlayClearInfo *)pFrameInfoData->pBuffer->pBuffer;
                            WFDMMLOGE2("WFDMMSGR:GDT: Pushed to PresentQ id = %d, ts = %llu",pOvDispInfo->Id,pFrameInfo->pBuffer->nTimeStamp);
                        }
                        /* pFrameInfo->pBuffer is utilized, it should be
                           returned back to get new frames.*/
                        pFrameInfo->pBuffer->nFilledLen = 0;
                        mGenericQ->Pop(&pFrameInfo,sizeof(&pFrameInfo),0);
                        mpFnEBD(mClientData, mnModuleId,
                                SINK_GENERIC_TRACK_ID,pFrameInfo->pBuffer);
                        accessFrameInfoMeta(&pFrameInfo, pushfront);
                    }
                    else
                    {
                        WFDMMLOGE("WFDMMSGR:GDT:Output Buffer not available");
                        MM_Timer_Sleep(10);
                    }
                }
            }
            else if(pGeneric.presentMode == 0 ||
                    pGeneric.presentMode == 2)
            {
                /* It's a clear instruction. Queue it to mGenericClearQ.
                   And at every presentation we will check it. If there is
                   no presentation in 500ms then also we will clear ID's
                   present in it. */
                frameInfoType *pFrameInfoClear = NULL;
                accessFrameInfoClear(&pFrameInfoClear,poprear);
                if(pFrameInfoClear != NULL)
                {
                    overlayClearInfo *pOvDispInfo =
                        (overlayClearInfo *)pFrameInfoClear->pBuffer->pBuffer;
                    pOvDispInfo->Id = pGeneric.Id;
                    pOvDispInfo->isValid = true;
                    pOvDispInfo->nTime = pFrameInfo->pBuffer->nTimeStamp;
                    //MM_Time_GetTime(&pOvDispInfo->nTime);
                    mGenericClearQ->Push(&pFrameInfoClear,
                                          sizeof(&pFrameInfoClear));
                    WFDMMLOGE2("WFDMMSGR:GDT: Pushed to ClearQ id = %d , ts = %llu",pOvDispInfo->Id,pOvDispInfo->nTime);

                    /* pFrameInfo->pBuffer is utilized, it should be
                       returned back to get new frames.*/
                    pFrameInfo->pBuffer->nFilledLen = 0;
                    mGenericQ->Pop(&pFrameInfo,sizeof(&pFrameInfo),0);
                    mpFnEBD(mClientData, mnModuleId,
                            SINK_GENERIC_TRACK_ID,pFrameInfo->pBuffer);
                    accessFrameInfoMeta(&pFrameInfo, pushfront);
                }
                else
                {
                    WFDMMLOGE("WFDMMSGR:GDT:Clear Buffer not available");
                    MM_Timer_Sleep(10);
                }
            }
            else
            {
                WFDMMLOGE("WFDMMSGR:GDT:Something Wrong. Shouldn't be here");
                pFrameInfo->pBuffer->nFilledLen = 0;
                mGenericQ->Pop(&pFrameInfo,sizeof(&pFrameInfo),0);
                mpFnEBD(mClientData, mnModuleId,
                        SINK_GENERIC_TRACK_ID,pFrameInfo->pBuffer);
                accessFrameInfoMeta(&pFrameInfo, pushfront);
                if(mGenericQ->GetSize() == 0)
                {
                    MM_Timer_Sleep(10);
                }
            }
        }
        else
        {
            MM_Timer_Sleep(5);
        }
    }
    WFDMMLOGD("WFDMMSGR:GDT:Decode Thread ended");
}

/*==============================================================================

         FUNCTION:         GenericRenderThread

         DESCRIPTION:
*//**       @brief         Generic Thread for rendering data on Surface
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
void WFDMMSinkGenericRenderer::GenericRenderThread(unsigned int nSignal)
{
    (void) nSignal;
    WFDMMLOGH("WFDMMSGR:GRT:Render Thread Entered");
    int nDropCount = 0;
    int64 timedecider;

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
        mGenericPresentQ->Peek(&pFrameInfo,
                                sizeof(&pFrameInfo));

        if(pFrameInfo)
        {
            WFDMMLOGL("WFDMMSGR:GRT:Got a Frame for Presentation");

            uint64 bufferTS = pFrameInfo->pBuffer->nTimeStamp;
            timedecider = (int64)((bufferTS - getCurrentTimeUs())
                                  + mCfg.nDecoderLatency);
            WFDMMLOGL1("WFDMMSGR:GRT:timedecider = %lld",timedecider);
            if(timedecider < GENERIC_RENDER_TOLERANCE)
            {
                WFDMMLOGE2("WFDMMSGR:GRT:Present buffer TS = %llu , timedecider = %lld us",bufferTS,timedecider);
                /* Send frame for presentation.*/
                if(!PushGeneric4Render(pFrameInfo))
                {
                    WFDMMLOGE("WFDMMSGR:GRT:Failed to render Data");
                }
                /* Check and Clear off overlay if any.*/
                CheckAndClearGeneric();
                pFrameInfo->pBuffer->nFilledLen = 0;
                mGenericPresentQ->Pop(&pFrameInfo,sizeof(&pFrameInfo),0);
                accessFrameInfoData(&pFrameInfo, pushfront);
            }
            else
            {
                CheckAndClearGeneric();
                MM_Timer_Sleep(2);
            }
        }
        else
        {
            /* Check and Clear overlays if its time */
            CheckAndClearGeneric();
            MM_Timer_Sleep(10);
            WFDMMLOGL("WFDMMSGR:GRT:pFrameInfo null");
        }
    }
    WFDMMLOGE("WFDMMSGR:GRT:Render Thread ended");
}

/*==============================================================================

         FUNCTION:         CheckAndClearGeneric

         DESCRIPTION:
*//**       @brief         Check and clear generic frames from screen
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

void WFDMMSinkGenericRenderer::CheckAndClearGeneric()
{
    frameInfoType *pFrameInfo = NULL;
    //unsigned long nCurrTime = 0;
    //MM_Time_GetTime(&nCurrTime);

    uint64 nCurrTime = getCurrentTimeUs();
    int nQSize = mGenericClearQ->GetSize();
    WFDMMLOGL1("WFDMMSGR:CACG:Called Size = %d",nQSize);
    for(int i = 0; i < nQSize ; i++)
    {
        mGenericClearQ->Peek(&pFrameInfo,
                              sizeof(&pFrameInfo));
        if(pFrameInfo)
        {
            overlayClearInfo *ovClearInfo =
                (overlayClearInfo *)(pFrameInfo->pBuffer->pBuffer);
            int64 nTimeDiff = nCurrTime - ovClearInfo->nTime;
            WFDMMLOGL3("WFDMMSGR:CACG: id = %d,isValid = %d ,nTimeDiff = %lld",ovClearInfo->Id,ovClearInfo->isValid,nTimeDiff);
            if((ovClearInfo->isValid &&
               nTimeDiff > (int64)(GENERIC_CLEAR_TOLERANCE)) || ISSTATECLOSING)
            {
                if(ClearGeneric(ovClearInfo->Id)>=0)
                {
                    WFDMMLOGE1("WFDMMSGR:CACG:Clear Done for id = %d",ovClearInfo->Id);
                }
                else
                {
                    WFDMMLOGE1("WFDMMSGR:CACG:Clear failed for id = %d",ovClearInfo->Id);
                }
                pFrameInfo->pBuffer->nFilledLen = 0;
                mGenericClearQ->Pop(&pFrameInfo,sizeof(&pFrameInfo),0);
                accessFrameInfoClear(&pFrameInfo, pushfront);
            }
            else
            {
                break;
            }
        }
    }
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

int WFDMMSinkGenericRenderer::state(int state, bool get_true_set_false)
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
uint64 WFDMMSinkGenericRenderer::getCurrentTimeUs()
{
    struct timespec sTime;
    clock_gettime(CLOCK_MONOTONIC, &sTime);

    uint64 currTS = ((OMX_U64)sTime.tv_sec * 1000000/*us*/)
                          + ((OMX_U64)sTime.tv_nsec / 1000);
    return currTS - mnBaseMediaTime;

}

/*==============================================================================

         FUNCTION:          Stop

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
OMX_ERRORTYPE WFDMMSinkGenericRenderer::Stop()
{

    SETSTATECLOSING;

    uint32 nGenericPendingSize = mGenericQ ? mGenericQ->GetSize() : 0;

    /*--------------------------------------------------------------------------
      Wait for all buffers to be returned
    ----------------------------------------------------------------------------
    */
    WFDMMLOGH("Waiting for Generic Renderer buffers to be returned");

    unsigned long nStartMs = 0;
    unsigned long nCurrMs = 0;
    bool bIsStartTimeSet = false;

    while(nGenericPendingSize)
    {
        MM_Timer_Sleep(2);
        nGenericPendingSize = mGenericQ ? mGenericQ->GetSize() : 0;

        WFDMMLOGH1("Generic Pending = %d",nGenericPendingSize);
        if(nGenericPendingSize == 0)
        {
            break;
        }
    }

    /*--------------------------------------------------------------------------
     Now we have returned all buffers. We can move to INIT.
    ----------------------------------------------------------------------------
    */
    SETSTATECLOSED;

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
OMX_ERRORTYPE WFDMMSinkGenericRenderer::Start()
{
    if(!ISSTATEINIT)
    {
        WFDMMLOGE("Generic Renderer not in Init when started");
        return OMX_ErrorInvalidState;
    }
    WFDMMLOGH("WFDMMSinkGenericRenderer Start");

    SETSTATEOPENED;

    if(mpGenericDecodeThread)
    {
        mpGenericDecodeThread->SetSignal(0);
    }

    if(mpGenericRenderThread)
    {
        mpGenericRenderThread->SetSignal(0);
    }
    return OMX_ErrorNone;
}

/*==============================================================================

         FUNCTION:          SetMediaBaseTime

         DESCRIPTION:       This will initialize base time for Generic Track.
                            Same will be used for deciding rendering time.
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
void WFDMMSinkGenericRenderer::SetMediaBaseTime(uint64 nTime)
{
    WFDMMLOGH("Generic Renderer Setting Media Base Time");
    /*-------------------------------------------------------------------------
      Renderer shall not receive any new frames when Media Base Time is being
      set. Renderer will flush out its existing data and only then will honor
      the new base time
    ---------------------------------------------------------------------------
    */
    mbGenericTimeReset= true;
    mnFlushTimeStamp = (uint64)-1;
    mbFlushInProgress = true;

    unsigned long nStartMs = 0;
    unsigned long nCurrMs = 0;
    MM_Time_GetTime(&nStartMs);

    while(mGenericQ && mGenericQ->GetSize())
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

         FUNCTION:          prepareDumpFiles

         DESCRIPTION:
*//**       @brief          prepare the Dump files
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
void WFDMMSinkGenericRenderer::prepareDumpFiles()
{
    /* kamit : TODO : Prepare files for dumping. Check to make sure dumping
                      Code is at one place */
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
void WFDMMSinkGenericRenderer::pauseRendering(void)
{
    mbPaused = true;
}

/*==============================================================================

         FUNCTION:          restartRendering

         DESCRIPTION:
*//**       @brief          Restart rendering of the buffers
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
void WFDMMSinkGenericRenderer::restartRendering(bool bFlush)
{
    if(mbPaused && bFlush)
    {
        mnFlushTimeStamp = (uint64)-1;
        mbFlushInProgress = true;

        unsigned long nStartMs = 0;
        unsigned long nCurrMs = 0;
        MM_Time_GetTime(&nStartMs);

        while(mGenericQ && mGenericQ->GetSize())
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
    mbPaused = false;
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
void WFDMMSinkGenericRenderer::setFlushTimeStamp(uint64 nTS)
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
void WFDMMSinkGenericRenderer::setDecoderLatency(uint64 nLatencyInms)
{
    WFDMMLOGH("Generic Renderer Setting decode latency");
    CRITICAL_SECT_ENTER;
    mCfg.nDecoderLatency = nLatencyInms;
    CRITICAL_SECT_LEAVE;
}

