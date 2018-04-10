#ifndef __WFDMMPNGENCODE_H
#define __WFDMMPNGENCODE_H
/*==============================================================================
*       WFDMMPngEncode.h
*
*  DESCRIPTION:
*       PNG Image Encoder class.
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

#include "WFDMMSourceImageEncode.h"

typedef struct buffInfo
{
    unsigned char *pBuf;
    unsigned int   nCurrOffset;
    unsigned int   nTotalSize;
    bool           bOverflow;
}buffInfoType;


class WFDMMSourcePNGEncode:public WFDMMSourceImageEncode
{
public:

    WFDMMSourcePNGEncode();

    virtual ~WFDMMSourcePNGEncode();

    //! Configure the the module.
    //!
    //! \param pCfg - Tells the full image resolution and quality for encode.
    //!
    //! \return int
    virtual int Configure(wfdImgConfigParamsType *pCfg);


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
    //! \return int         - 0 success, -1 failure
    virtual int GetNonTransparentRegions(unsigned int nMaxRegions,
                                         unsigned char* __restrict__ pImg,
                                         unsigned int *numRegions,
                                         imageRegionInfoNodeType **pRegions,
                                         bool *bTransparent,
                                         unsigned int* nPercentageTransparency);

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
    //! \return int      - 0 success, -1 failure
    virtual int Encode(imageRegionInfoNodeType *pSubImage,
                       unsigned int   nQuality,
                       unsigned char *pOut,
                       unsigned int  *nLen);


    //! Deinitialize the module. Multiple images can be encoded
    //! without Deinitializing each time. After deinitialize configure
    //! may be called again to process images of different resolution.
    //!
    //! \return int
    virtual int Deinitialize();

private:

    int libpngInit(buffInfoType *pBuf, imageRegionInfoNodeType *pNode);

    int libpngClose();

    int libpngEncode(imageRegionInfoNode *pNode);

    void FillInversePreMultTable();

    void          *mpPngPtr;
    void          *mpPngInfo;

    unsigned int   mnWidth;
    unsigned int   mnHeight;
    unsigned int   meColorType;
    unsigned char *mpImage;
    imageRegionInfoNodeType *mpHeadNode;

    unsigned int   msPreMultTable[256];
    unsigned int   mnBPP;


};
#endif /*__WFDMMPNGENCODE_H*/
