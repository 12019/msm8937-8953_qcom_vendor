/* =============================================================================
                             WFDMMSourceOverlayApp.cpp
DESCRIPTION

Copyright (c) 2015  Qualcomm Technologies, Inc. All Rights Reserved
Qualcomm Technologies Proprietary and Confidential.
============================================================================= */

//Includes
#include "SkBitmap.h"
#include "SkImageEncoder.h"
#include "SkStream.h"
#include "SkData.h"
#include <stdio.h>
#include <stdlib.h>
#include "OMX_Core.h"
#include "OMX_Video.h"
#include "gralloc_priv.h"
#include <gui/IGraphicBufferProducer.h>
#include <media/stagefright/SurfaceMediaSource.h>
#include "WFDMMSourceImageEncode.h"

using namespace android;


void GetCurTime(OMX_TICKS& );
OMX_ERRORTYPE pngEncode(private_handle_t *pPvtHandle,
                        imageRegionInfoNodeType *pRegion);
//MACROS
#define PNG_BUFFER_SIZE (1080*1920*4) /*1080p captures*/
#define PNG_WIDTH 1920
#define PNG_HEIGHT 1080

//Main

static WFDMMSourceImageEncode *pImgEncoder = NULL;
int main(int argc, char* argv[])
{
    ALOGE("WFDMMSourceOverlayApp:: main() - ENTER");

    //Local variables
    long lSize = 0;
    void* buffer;

    // Open RAW file
    FILE* fRAWFile = fopen("/data/media/testAppRAWData.raw","rb");
    if(fRAWFile == NULL)
    {
        ALOGE("WFDMMSourceOverlayApp:: main(): File open failed for fRAWFile");
        return 1;
    }




    //Get file size
    fseek(fRAWFile, 0 , SEEK_END);
    lSize = ftell(fRAWFile);
    rewind(fRAWFile);

    //allocate memory
    buffer = (void*) malloc(sizeof(char) * lSize);
    ALOGE("WFDMMSourceOverlayApp:: main(): Allocating size [%ld]",lSize);
    if(buffer == NULL)
    {
        fclose(fRAWFile);
        ALOGE("WFDMMSourceOverlayApp:: main(): failed to allocate buffer");
        return 1;
    }


    pImgEncoder = WFDMMSourceImageEncode::Create(WFD_IMG_CODING_PNG);

    if(!pImgEncoder)
    {
        free(buffer);
        fclose(fRAWFile);
        ALOGE("WFDMMSourceOverlayApp:: main(): failed to allocate buffer");
        return 1;
    }

    //Read the data
    size_t result;
    result = fread(buffer, 1, lSize, fRAWFile);
    if(result != lSize)
    {
        ALOGE("WFDMMSourceOverlayApp:: main():failed to read complete buffer");
       //Clean up
        fclose(fRAWFile);
        free(buffer);
        return 1;
    }

    fclose(fRAWFile);


    unsigned int nNumRegions = 0;
    imageRegionInfoNodeType *pRegions = NULL;

#if 1
    imageRegionInfoNodeType *pCurrRegion = NULL;
    OMX_TICKS beforeRead = 0;
    OMX_TICKS afterRead  = 0;
    OMX_TICKS diffRead   = 0;


    wfdImgConfigParamsType sCfg;
    sCfg.eColor = WFD_IMG_COLOR_FMT_ARGB8888;
    sCfg.nHeight = PNG_HEIGHT;
    sCfg.nWidth = PNG_WIDTH;

    pImgEncoder->Configure(&sCfg);

    GetCurTime(beforeRead);
    pImgEncoder->GetNonTransparentRegions(20, (unsigned char*)buffer, &nNumRegions, &pRegions, NULL, NULL);
    GetCurTime(afterRead);
    //Print statistics
    diffRead = afterRead - beforeRead;
    ALOGE("WFDMMSourceOverlayApp:: GetTransformedNonTransparentRegions time[%lld]",diffRead);
    fprintf(stderr,"WFDMMSourceOverlayApp:: GetTransformedNonTransparentRegions time[%lld]\r\n",diffRead);

    pCurrRegion = pRegions;

#else
    imageRegionInfoNodeType *pCurrRegion = (imageRegionInfoNodeType*)malloc(sizeof(imageRegionInfoNodeType));
#endif

    fprintf(stderr, "\r\nNum of Regions = %d \n \n ", nNumRegions);
#if 1
    if(nNumRegions == 0)
    {
        return 0;
    }
#else
    //Call encode

    pCurrRegion->height = PNG_HEIGHT;
    pCurrRegion->width = PNG_WIDTH;
    pCurrRegion->x0 = 0;
    pCurrRegion->y0 = 0;
    pCurrRegion->pNext = NULL;
#endif
    while(pCurrRegion != NULL)
    {
        fprintf(stderr, "calling PNG Encode %lu %lu %lu \r\n", pCurrRegion->nWidth,pCurrRegion->nHeight, pCurrRegion->y0);
        pngEncode((private_handle_t*)buffer,pCurrRegion );
        pCurrRegion = pCurrRegion->pNext;
    }

    delete pImgEncoder;

    pImgEncoder = NULL;

    //Exit!
    ALOGE("WFDMMSourceOverlayApp:: main() - EXIT");
    return 0;
}


//Helper function
OMX_ERRORTYPE pngEncode(private_handle_t *pPvtHandle,
                        imageRegionInfoNodeType *pRegion)
{
    ALOGE("WFDMMSourceOverlayApp:: pngEncode() - ENTER");

    //Local variables
    OMX_ERRORTYPE result = OMX_ErrorNone;
    OMX_TICKS beforeRead = 0;
    OMX_TICKS afterRead  = 0;
    OMX_TICKS diffRead   = 0;
    unsigned int nLen = pRegion->nHeight * pRegion->nWidth * 4;

    unsigned char *pBuf = (unsigned char *)malloc(nLen);



    GetCurTime(beforeRead);
    pImgEncoder->Encode(pRegion, 100, pBuf, &nLen);
    GetCurTime(afterRead);

    //Print statistics
    diffRead = afterRead - beforeRead;
    ALOGE("WFDMMSourceOverlayApp:: pngEncode(): result of encodeStream time[%lld]",diffRead);
    fprintf(stderr,"WFDMMSourceOverlayApp:: pngEncode(): result of encodeStream time[%lld]\r\n",diffRead);


    char sOutName[512];
    static int index = 0;
    ALOGE("WFDMMSourceOverlayApp:: pngEncode(): DUMP_TO_FILE");
    snprintf(sOutName, 512, "%s%d.png", "/data/media/testAppPNGData", index++);
    FILE *fPNGTempFile = fopen(sOutName,"wb");
    if(fPNGTempFile == NULL)
    {
        ALOGE("WFDMMSourceOverlayApp:: pngEncode(): File open failed for fPNGTempFile");
        return OMX_ErrorInsufficientResources;
    }
    fwrite(pBuf,sizeof(char),nLen,fPNGTempFile);
    free(pBuf);
    fclose(fPNGTempFile);


    ALOGE("WFDMMSourceOverlayApp:: pngEncode() - EXIT");
    return OMX_ErrorNone;
}

//Helper function
void GetCurTime(OMX_TICKS& lTime)
{
    static const OMX_S32 WFD_TIME_NSEC_IN_MSEC = 1000000;
    static const OMX_S32 WFD_TIME_NSEC_IN_USEC = 1000;
    struct timespec tempTime;
    clock_gettime(CLOCK_MONOTONIC, &tempTime);
    lTime =(OMX_TICKS)(((unsigned long long)tempTime.tv_sec *
                                            WFD_TIME_NSEC_IN_MSEC)
                     + ((unsigned long long)tempTime.tv_nsec /
                                             WFD_TIME_NSEC_IN_USEC));
}


