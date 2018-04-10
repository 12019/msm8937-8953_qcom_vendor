#ifndef _SCVE_OBJECT_MATTING_H_
#define _SCVE_OBJECT_MATTING_H_
/**==============================================================================

@file
scveObjectMatting.hpp

@brief
Header file for SCVE Object Matting.

Copyright (c) 2014 Qualcomm Technologies, Inc.  All Rights Reserved.
Qualcomm Technologies Proprietary and Confidential.

==============================================================================**/

#include "scveTypes.hpp"
#include "scveContext.hpp"

namespace SCVE
{

enum StatusCodes_ObjectMatting
{
   SCVE_OBJECT_MATTING_ERROR_TOO_FEW_FOREGROUND_REGION = SCVE_OBJECT_MATTING_ERROR + 1,
   SCVE_OBJECT_MATTING_ERROR_TOO_FEW_BACKGROUND_REGION = SCVE_OBJECT_MATTING_ERROR + 2,
   SCVE_OBJECT_MATTING_ERROR_NO_UNKNOWN_REGION         = SCVE_OBJECT_MATTING_ERROR + 3,
};

typedef enum
{
    SCVE_OBJECT_MATTING_UNKNOWN = 0, // matte of this pixel is unknown
    SCVE_OBJECT_MATTING_BACKGROUND = 1, // background label
    SCVE_OBJECT_MATTING_FOREGROUND = 2, // foreground label
    SCVE_OBJECT_MATTING_MATTE = 3 // matte calculated
} ObjectMattingType;

typedef void (*ObjectMattingCB)(Status status,
                                void* pSessionUserData,
                                void* pTaskUserData);

class SCVE_API ObjectMatting
{
public:
   // @brief
   // instantiate ObjectMatting instance
   // @param cbFunc
   //     callback function
   // @param pSessionUserData
   //     anything user want to preverse
   static ObjectMatting* newInstance(   Context *pScveContext,
                                        ObjectMattingCB cbFunc = NULL,
                                        void *pSessionUserData = NULL);

   // @brief
   // delete ObjectMatting instance
   static Status deleteInstance(ObjectMatting *pInstance);

   // @brief
   // Method of sync call.
   // @param pSrc
   //     Input image. 4-channel rgba image
   virtual Status init(const Image *pSrc) = 0;

   // @brief
   // Method of sync call.
   // @param pTrimap
   //     Input mask image. 1-channel image specifying the region corresponding to
   //     input image defined by ObjectMattingType
   // @param pMatte
   //     Output matting image. 1-channel image showing the alpha matte
   // @param pForeground
   //     Output image. 4-channel rgba image
   virtual Status Run_Sync( const Image *pTrimap,
                                 Image *pMatte,
                                  Image *pForeground = NULL,
                                  Image *pBackground = NULL) = 0;

   // @brief
   // Method of sync call.
   // @param pTrimap
   //     Input mask image. 1-channel image specifying the region corresponding to
   //     input image defined by ObjectMattingType
   // @param pMatte
   //     Output matting image. 1-channel image showing the alpha matte
   // @param pForeground
   //     Output image. 4-channel rgba image
   virtual Status Run_Async( const Image *pTrimap,
                                  Image *pMatte,
                                   Image *pForeground = NULL,
                                   Image *pBackground = NULL,
                             void* pTaskUserData = NULL) = 0;

protected:
   // @brief
   // destructor
   virtual ~ObjectMatting();
};

}

#endif //_SCVE_OBJECT_MATTING_H_
