/*==============================================================================
*       WFDMMSourcePNGEncode.cpp
*
*  DESCRIPTION:
*       Implementation of PNG encode class derived from WFDMMSourceImageEncode.
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
01/13/2015         SK            InitialDraft
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
#include "WFDMMSourcePngEncode.h"
#include "MMMalloc.h"
#include "MMMemory.h"
#include "WFDMMLogs.h"
#include "png.h"

/* Disabling this feature for now
#ifndef CORNER_CHECK_FEATURE
#define CORNER_CHECK_FEATURE
#endif
*/

/*==============================================================================

         FUNCTION:          WriteDataFn

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
static void WriteDataFn
(
    png_structp pPngPtr,
    png_bytep   pData,
    png_size_t  nLen
)
{
    if(!pPngPtr)
    {
        return;
    }

    buffInfoType* pBuf = (buffInfoType*)png_get_io_ptr(pPngPtr);

    if(pBuf && pBuf->pBuf && pData)
    {
        if((pBuf->nCurrOffset + (unsigned int)nLen) < pBuf->nTotalSize)
        {
            memcpy(pBuf->pBuf + pBuf->nCurrOffset, pData, nLen);
            pBuf->nCurrOffset += (unsigned int)nLen;
        }
        else
        {
            WFDMMLOGE1("%s: Buffer Overflow", __FUNCTION__);
            pBuf->bOverflow = true;
        }
    }
}


/*==============================================================================

         FUNCTION:          ErrorHandlerFn

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
static void ErrorHandlerFn
(
    png_structp     pPngPtr,
    png_const_charp pMsg
)
{
    (void)pPngPtr;
    WFDMMLOGE2(" %s: png operation failed %s", __FUNCTION__, pMsg);
}


/*==============================================================================

         FUNCTION:          WFDMMSourcePNGEncode()

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
WFDMMSourcePNGEncode::WFDMMSourcePNGEncode()
{
    //memset(this, 0, sizeof(this));
    mpPngPtr    = NULL;
    mpPngInfo   = NULL;

    mnWidth     = 0;
    mnHeight    = 0;
    meColorType = WFD_IMG_COLOR_FMT_ARGB8888;
    mpImage     = NULL;
    mpHeadNode  = NULL;
    mnBPP = 0;

}


/*==============================================================================

         FUNCTION:          ~WFDMMSourcePNGEncode()

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
WFDMMSourcePNGEncode::~WFDMMSourcePNGEncode()
{
    Deinitialize();
}


/*==============================================================================

         FUNCTION:          Configure

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
int WFDMMSourcePNGEncode::Configure(wfdImgConfigParamsType *pCfg)
{
    if(!pCfg)
    {
        WFDMMLOGE1("%s: Invalid Arguments", __FUNCTION__);
        return -1;
    }
    mnWidth = pCfg->nWidth;
    mnHeight  = pCfg->nHeight;
    meColorType = pCfg->eColor;


    if(meColorType != WFD_IMG_COLOR_FMT_ARGB8888)
    {
        WFDMMLOGE1("%s: Unsupported color type", __FUNCTION__);
        return -1;
    }

    if(meColorType == WFD_IMG_COLOR_FMT_ARGB8888)
    {
        mnBPP = 4;
        mpImage = (unsigned char*)MM_Malloc(mnHeight * mnWidth * mnBPP);

        if(!mpImage)
        {
            WFDMMLOGE("%s: Failed to alloc memory");
            return -1;
        }
        FillInversePreMultTable();
    }
    return 0;

}

/*==============================================================================

         FUNCTION:          GetNonTransparentRegions

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
int WFDMMSourcePNGEncode::GetNonTransparentRegions
(
    unsigned int nMaxRegions,
    unsigned char* __restrict__ pImg,
    unsigned int *numRegions,
    imageRegionInfoNodeType **pRegions,
    bool *bTransparent,
    unsigned int *nPercentTransparent
)
{
    if(!pImg || !mnWidth || !mnHeight || !numRegions || !mpImage)
    {
        WFDMMLOGE1("%s: Pre requisites not met", __FUNCTION__);
        return -1;
    }

    if(nMaxRegions == 0)
    {
        *numRegions = 0;
        return 0;
    }

    if(nMaxRegions == 1)
    {
        *numRegions = 1;
        mpHeadNode = (imageRegionInfoNodeType *)
                                MM_Malloc(sizeof(imageRegionInfoNodeType));
        if(mpHeadNode == NULL)
        {
            WFDMMLOGE1("%s: mpHeadNode is NULL", __FUNCTION__);
            return -1;
        }

        mpHeadNode->nHeight  = mnHeight;
        mpHeadNode->pNext    = NULL;
        mpHeadNode->nWidth   = mnWidth;
        mpHeadNode->x0       = 0;
        mpHeadNode->y0       = 0;
        mpHeadNode->pImg     = mpImage;
        mpHeadNode->nStride  = mnWidth;

        *pRegions = mpHeadNode;
        return 0;
    }

    mpHeadNode = NULL;
    imageRegionInfoNodeType *pCurrNode = NULL;

    unsigned int rgbVal32;
    unsigned int * __restrict__ pImg32 = (unsigned int*)pImg;
    unsigned int * __restrict__ pDestInt32 = (unsigned int *)mpImage;

    unsigned int nConsecTransparentY = 0;
    unsigned int nRegions = 0;

    bool         bLookForNewRegion = true;

    unsigned int x;
    unsigned int y;

    unsigned int nMultiplier = 0;
    unsigned int R;
    unsigned int G;
    unsigned int B;

    unsigned int nCurrXmin = 0;
    unsigned int nCurrXmax = 0;
    unsigned int nCurrYMin = 0;
    unsigned int nCurrYMax = 0;

    unsigned int nNonTransparentPixels = 0;

    /*
     Maintaining transparency values of all 4 corners for future use where we
     may have better design to drop/capture overlay.
     */
    unsigned int nNumTopLeftOpaquePixels = 0;
    unsigned int nNumTopRightOpaquePixels = 0;
    unsigned int nNumBottomLeftOpaquePixels = 0;
    unsigned int nNumBottomRightOpaquePixels = 0;

#define CORNER_SQUARE_LEN 5
#define CORNER_OPAQUE_THRESHOLD 10

    for(y =0; y < mnHeight; y++)
    {
        for(x = 0; x < mnWidth; x++)
        {
            rgbVal32 = *pImg32++;

            if(rgbVal32 & 0xff000000)
            {
                /*--------------------------------------------------------------
                 Found a non transparent byte
                ----------------------------------------------------------------
                */

                nNonTransparentPixels++;
#ifdef CORNER_CHECK_FEATURE
                if(x < CORNER_SQUARE_LEN && y < CORNER_SQUARE_LEN)
                {
                    nNumTopLeftOpaquePixels++;
                }

                if(x < CORNER_SQUARE_LEN && y >= (mnHeight - CORNER_SQUARE_LEN))
                {
                    nNumBottomLeftOpaquePixels++;
                }

                if(x >= (mnWidth - CORNER_SQUARE_LEN) && y < CORNER_SQUARE_LEN)
                {
                    nNumTopRightOpaquePixels++;
                }

                if(x >= (mnWidth - CORNER_SQUARE_LEN) &&
                   y >= (mnHeight - CORNER_SQUARE_LEN))
                {
                    nNumBottomRightOpaquePixels++;
                }
#endif //CORNER_CHECK_FEATURE

                nConsecTransparentY = 0;
                nCurrYMax = y;

                if(bLookForNewRegion)
                {
                    nCurrYMin = y;
                    bLookForNewRegion = false;
                    nCurrXmin = x;
                    nCurrXmax = x;
                }

                if(x < nCurrXmin)
                {
                    nCurrXmin = x;
                }

                if(x > nCurrXmax)
                {
                    nCurrXmax = x;
                }

                B = (rgbVal32 >> 16) & 0xFF;
                G = (rgbVal32 >> 8) & 0xFF;
                R = (rgbVal32 >> 0) & 0xFF;

                nMultiplier = msPreMultTable[rgbVal32 >> 24];

                R = R * nMultiplier + (1 << 23) >> 24;
                G = G * nMultiplier + (1 << 23) >> 24;
                B = B * nMultiplier + (1 << 23) >> 24;

                rgbVal32 = (rgbVal32 >> 24) << 24;

                rgbVal32 |= (B << 16);
                rgbVal32 |= (G << 8);
                rgbVal32 |= (R << 0);

                *pDestInt32++ = rgbVal32;
            }
            else
            {
                *pDestInt32++ = 0;
            }
        }
        nConsecTransparentY++;

        if((nConsecTransparentY >= mnHeight/nMaxRegions && nCurrYMax) ||
           (y == mnHeight -1 && nCurrYMax != 0))
        {
            /*------------------------------------------------------------------
             If 10% of height worth transparent region is found close previos
             rectangular region and look for new one.
            --------------------------------------------------------------------
            */
/*
            if(nCurrYMax - nCurrYMin + 1 == 36 &&
               nCurrXmax - nCurrXmin + 1 == 1280)
            {
                nCurrYMin = 0;
                nCurrYMax = 0;
                nCurrXmin = 0;
                nCurrXmax = 0;
                bLookForNewRegion = true;
                continue;
            }
*/
            nConsecTransparentY = 0;
            if(!mpHeadNode)
            {
                mpHeadNode = (imageRegionInfoNodeType *)
                                MM_Malloc(sizeof(imageRegionInfoNodeType));
                pCurrNode = mpHeadNode;
            }
            else if(pCurrNode)
            {
                pCurrNode->pNext = (imageRegionInfoNode *)
                                MM_Malloc(sizeof(imageRegionInfoNode));
                pCurrNode = pCurrNode->pNext;
            }

            if(!pCurrNode)
            {
                *numRegions = 0;
                return -1;
            }

            pCurrNode->nHeight = nCurrYMax - nCurrYMin + 1;
            pCurrNode->pNext = NULL;
            pCurrNode->nWidth = nCurrXmax - nCurrXmin + 1;
            pCurrNode->x0 = nCurrXmin;
            pCurrNode->y0 = nCurrYMin;
            pCurrNode->pImg = mpImage;
            pCurrNode->nStride = mnWidth;

            nCurrYMin = 0;
            nCurrYMax = 0;
            nCurrXmin = 0;
            nCurrXmax = 0;
            bLookForNewRegion = true;


            nRegions++;

        }
    }

    *numRegions = nRegions;
    *pRegions = mpHeadNode;

    if(bTransparent)
    {
        *bTransparent = nNonTransparentPixels ? false : true;
    }

    if(nPercentTransparent)
    {
#ifdef CORNER_CHECK_FEATURE
        WFDMMLOGL4("4_CORNER: TL[%d] TR[%d] BL[%d] BR[%d]",
                    nNumTopLeftOpaquePixels,
                    nNumTopRightOpaquePixels,
                    nNumBottomLeftOpaquePixels,
                    nNumBottomRightOpaquePixels);

        if(nNumTopLeftOpaquePixels > CORNER_OPAQUE_THRESHOLD    ||
           nNumTopRightOpaquePixels > CORNER_OPAQUE_THRESHOLD   ||
           nNumBottomLeftOpaquePixels > CORNER_OPAQUE_THRESHOLD ||
           nNumBottomRightOpaquePixels > CORNER_OPAQUE_THRESHOLD)
        {
            /*
              Override the nNonTransparentPixels to complete image size which
              causes the resulting nPercentageTransparency to drop down to
              ZERO; thus marking the overlay frame to be dropped off!
            */
            nNonTransparentPixels = mnWidth * mnHeight;
        }
#endif //CORNER_CHECK_FEATURE

        *nPercentTransparent =
            100 - ((nNonTransparentPixels  * 100)/(mnWidth * mnHeight));
    }

    return 0;

}

/*==============================================================================

         FUNCTION:          Encode

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
int WFDMMSourcePNGEncode::Encode(imageRegionInfoNodeType *pSubImage,
                       unsigned int   nQuality,
                       unsigned char *pOut,
                       unsigned int  *pLen)
{
    if(!pSubImage || !pOut || !pLen)
    {
        WFDMMLOGE1("%s: Invalid Args", __FUNCTION__);
        return -1;
    }

    unsigned int   nCurrOffset;
    unsigned int   nTotalSize;

    buffInfoType *pBuf = (buffInfoType *)MM_Malloc(sizeof(buffInfoType));

    if(!pBuf)
    {
        WFDMMLOGE1("%s Failed to alloc mem", __FUNCTION__);
        return -1;
    }

    pBuf->pBuf       = pOut;
    pBuf->nTotalSize = *pLen;
    pBuf->nCurrOffset = 0;
    pBuf->bOverflow   = false;

    if(libpngInit(pBuf, pSubImage) != 0)
    {
        MM_Free(pBuf);
        WFDMMLOGE1("%s: Failed to init png lib", __FUNCTION__);
        return -1;
    }

    if(libpngEncode(pSubImage) != 0 ||
       pBuf->bOverflow)
    {
        MM_Free(pBuf);
        (void)libpngClose();
        WFDMMLOGE1("Failed to encode PNG. is OverFlow? %d", pBuf->bOverflow);
        return -1;
    }
    (void)libpngClose();
    *pLen = pBuf->nCurrOffset;


    return 0;

}


/*==============================================================================

         FUNCTION:         Deinitialize

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
int WFDMMSourcePNGEncode::Deinitialize()
{
/*
    while(mpHeadNode != NULL)
    {
        imageRegionInfoNodeType *pTempNode = mpHeadNode;
        mpHeadNode = mpHeadNode->pNext;
        MM_Free(pTempNode);
    }
*/

    if(mpImage)
    {
        MM_Free(mpImage);
        mpImage = NULL;
    }

    mpPngPtr    = NULL;
    mpPngInfo   = NULL;

    mnWidth     = 0;
    mnHeight    = 0;

    mpHeadNode     = NULL;
    mnBPP = 0;

    return 0;
}


/*==============================================================================

         FUNCTION:          libpngInit

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
int WFDMMSourcePNGEncode::libpngInit(buffInfoType *pBuf,
                                     imageRegionInfoNodeType *pNode)
{

    const char libpng_ver[128] = "1.6.10";
    mpPngPtr = (void*)png_create_write_struct(libpng_ver,
                                      NULL,
                                      ErrorHandlerFn,
                                      NULL);

    if(mpPngPtr == NULL)
    {
        WFDMMLOGE1("%s Failed to open PNG Encoder", __FUNCTION__);
        return -1;
    }


    mpPngInfo = (void*)png_create_info_struct((png_structp)mpPngPtr);

    if (mpPngInfo == NULL)
    {
        WFDMMLOGE1("%s Failed to create Info struct", __FUNCTION__);
        png_destroy_write_struct((png_structpp)(&mpPngPtr), NULL);
        mpPngPtr = NULL;
        return -1;
    }



    png_set_write_fn((png_structp)mpPngPtr, (void*)pBuf, WriteDataFn, NULL);





    png_set_IHDR((png_structp)mpPngPtr, (png_infop)mpPngInfo,
                 pNode->nWidth,
                 pNode->nHeight,
                 8 /*bit depth*/,
                 PNG_COLOR_MASK_COLOR | PNG_COLOR_MASK_ALPHA/*color Type*/,
                 PNG_INTERLACE_NONE,
                 PNG_COMPRESSION_TYPE_BASE,
                 PNG_FILTER_TYPE_BASE);



    png_color_8 sBit = {8, 8, 8, 0, 8};

    png_set_sBIT((png_structp)mpPngPtr, (png_infop)mpPngInfo, &sBit);

    png_write_info((png_structp)mpPngPtr, (png_infop)mpPngInfo);

    WFDMMLOGD("PNG Init success");

    return 0;
}

/*==============================================================================

         FUNCTION:          libpngClose

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
int WFDMMSourcePNGEncode::libpngClose()
{
    if(mpPngPtr && mpPngInfo)
    {
        png_write_end((png_structp)mpPngPtr, (png_infop)mpPngInfo);

        png_destroy_write_struct((png_structpp)(&mpPngPtr),
                                 (png_infopp)(&mpPngInfo));

        mpPngPtr = NULL;mpPngInfo = NULL;
    }

    WFDMMLOGD("PNG Close success");

    return 0;
}

/*==============================================================================

         FUNCTION:          libpngEncode

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
int WFDMMSourcePNGEncode::libpngEncode(imageRegionInfoNodeType *pNode)
{
    if(!pNode)
    {
        WFDMMLOGE1("%s Invalid Args", __FUNCTION__);
        return -1;
    }

    unsigned char *pStart =
          pNode->pImg + pNode->nStride * pNode->y0 * mnBPP +
          pNode->x0 * mnBPP;


    for(int i = 0; i < pNode->nHeight; i++)
    {
        png_write_rows((png_structp)mpPngPtr, &pStart, 1);

        pStart += pNode->nStride * mnBPP;
    }

    return 0;
}

/*==============================================================================

         FUNCTION:          FillInversePreMultTable

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
void WFDMMSourcePNGEncode::FillInversePreMultTable()
{
    for(unsigned int i = 1; i < 256; i++)
    {
        msPreMultTable[i] = 0xFF000000 / i; //0xFF000000 is 255 in Q24
    }
}
