#ifndef __WFDMMIMAGEENCODE_H
#define __WFDMMIMAGEENCODE_H
/*==============================================================================
*       WFDMMImageEncode.h
*
*  DESCRIPTION:
*       Base class for all image encoder modules
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


typedef enum wfdImgColor
{
    WFD_IMG_COLOR_FMT_ARGB8888
}wfdImgColorType;

typedef enum wfdImgCoding
{
    WFD_IMG_CODING_PNG,
    WFD_IMG_CODING_JPEG
}wfdImgCodingType;

typedef struct wfdImgConfigParams
{
    unsigned int    nWidth;
    unsigned int    nHeight;
    wfdImgColorType eColor;
}wfdImgConfigParamsType;


typedef struct imageRegionInfoNode
{
    unsigned int         nWidth;
    unsigned int         nHeight;
    unsigned int         x0;
    unsigned int         y0;
    unsigned char       *pImg;
    imageRegionInfoNode *pNext;
    unsigned int         nStride;
}imageRegionInfoNodeType;


class WFDMMSourceImageEncode
{
public:
    //! Creates the encoder for the specified type
    //!
    //! \param eFmt. One of wfdImgCodingType
    //!
    //! \return WFDMMSourceImageEncode*
    static WFDMMSourceImageEncode *Create(wfdImgCodingType eFmt);

    WFDMMSourceImageEncode(){;};

    virtual ~WFDMMSourceImageEncode(){;};

    //! Configure the the module.
    //!
    //! \param pCfg - Tells the full image resolution and quality for encode.
    //!
    //! \return int
    virtual int Configure(wfdImgConfigParamsType *pCfg) = 0;


    //! This function is optional. Only needed if caller wants to
    //! encode only non transparent portions of the main image.
    //!
    //! \param nMaxRegions  - Num max regions to be identified
    //! \param pImg         - Pointer to the image
    //! \param numRegions   - Pointer to the uint storage to return the
    //!                       number of idenitified regions
    //! \param pRegions     - A linked list to the information of the
    //!                       identified sections.
    //!
    //! \return int
    virtual int GetNonTransparentRegions(unsigned int nMaxRegions,
                                         unsigned char* __restrict__ pImg,
                                         unsigned int *numRegions,
                                         imageRegionInfoNodeType **pRegions,
                                         bool *bTransparent,
                                         unsigned int *bPercentageTransparency)= 0;

    //! Encode one region
    //!
    //! \param pSubImage - Info of the image to be encoded. This can be the
    //!                    output of the function GetNonTransparentRegions
    //! \param nQuality  - defines quality for lossy compression and compression
    //!                    level for lossless. Range is 0 to 100.
    //! \param pOut      - Pointer to output buffer
    //! \param nLen      - Initially carries size of buffer. On completion of
    //!                    the function it returns the size of compressed image
    //!
    //! \return int
    virtual int Encode(imageRegionInfoNodeType *pSubImage,
                       unsigned int   nQuality,
                       unsigned char *pOut,
                       unsigned int  *nLen) = 0;


    //! Deinitialize the module. Multiple images can be encoded
    //! without Deinitializing each time. After deinitialize configure
    //! may be called again to process images of different resolution.
    //!
    //! \return int
    virtual int Deinitialize() = 0;

};
#endif /*__WFDMMIMAGEENCODE_H*/
