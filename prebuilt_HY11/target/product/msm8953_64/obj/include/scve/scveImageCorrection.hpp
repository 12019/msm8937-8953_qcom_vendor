/**=============================================================================

@file
scveImageCorrection.hpp

@brief
SCVE API Definition for Text Reco features.

Copyright (c) 2014 Qualcomm Technologies, Inc.  All Rights Reserved.
Qualcomm Technologies Proprietary and Confidential.

=============================================================================**/

#ifndef SCVE_IMAGE_CORRECTION_HPP
#define SCVE_IMAGE_CORRECTION_HPP


#include "scveTypes.hpp"
#include "scveContext.hpp"
#include <fastcv/fastcv.h>

namespace SCVE
{

   //------------------------------------------------------------------------------
   /// @brief
   ///    Error codes specific to ImageCorrection
   ///
   /// @ingroup scveImageCorrection
   //------------------------------------------------------------------------------
   enum StatusCodes_ImageCorrection
   {
      /// Returned by TextReco functions if input image is too blurry.
      SCVE_IMAGE_CORRECTION_ERROR_INSUFFICIENT_INTERNAL_MEMORY = SCVE_IMAGE_CORRECTION_ERROR + 1,
   };

   // This enum will be a part of SCVE
   enum eImageMode
   {
      IMAGEMODE_SINGLE_FRAME,
      IMAGEMODE_CONTINUOUS_FRAME
   };

   enum eRegionExtractionMode
   {
      REGION_EXTRACTION_QUADRILATERAL,
      REGION_EXTRACTION_CURVED_BOUNDARY,
      REGION_EXTRACTION_CURVED_TEXTLINE,
      REGION_EXTRACTION_AUTO
   };

   enum eRectificationScale
   {
      SCALE_HIGH_RESOLUTION,
      SCALE_LOW_RESOLUTION,
   };

   enum eImageEnhancementMode
   {
      IMAGE_ENHANCEMENT_WHITEBOARD,
      IMAGE_ENHANCEMENT_PROJECTOR,
      IMAGE_ENHANCEMENT_DOCUMENT,
      IMAGE_ENHANCEMENT_BUSINESSCARD,
      IMAGE_ENHANCEMENT_SCREEN,
      IMAGE_ENHANCEMENT_AUTO
   };

   struct Region
   {
   public:
      uint32_t    region_id;
      Point                vertex[4];
      float32_t               cost;

      Region()
         : cost(0.0f)
      {

      }
   };


   class SCVE_API ImageCorrection
   {
   public:
      static ImageCorrection* newInstance(Context* pScveContext, eImageMode mode);
      static Status deleteInstance(ImageCorrection* pInstance);

      virtual Status Init() = 0;
      virtual Status DeInit() = 0;

      // rectification of perspective distortion with edge
      virtual Status GetRegionCandidates(
         /* input */
         const Image*               pImage,              // color image(NV21)
         const SCVE::Rect           roi,
         const eRegionExtractionMode      mode,
         const uint16_t             maxCandidates,

         /* output */
         uint16_t*                  numCandidates,
         Region*                    regionCandidates
         ) = 0;

      virtual Status GetRegionBoundaryPixels(
         /* input */
         const uint32_t             region_id,
         const uint32_t             maxPixels,

         /* output */
         Point*                     pixels,
         uint32_t*                  numPixels
         ) = 0;

      virtual Status GetRectifiedImageDims(
         /* input */
         const Image*               pImage,
         const uint16_t             numOfRegions,
         const Region*              pRegions,

         const eRectificationScale     scaleMode,

         /* output */
         SCVE::Rect*                dims              // array of rect
         ) = 0;

      virtual Status GetRectifiedImages(
         /* input */
         const Image*               pImage,              // original image
         const uint16_t             numOfRegions,
         const Region*              pRegions,

         /* output */
         Image*                     pOutImages           // array of output images
         ) = 0;

      // image enhancement
      virtual Status EnhanceImage(
         /* input */
         const Image*               pImage,
         const uint16_t             numOfRegions,
         const Region*              pRegions,

         const eImageEnhancementMode      mode,

         /* output */
         Image*                     pOutImage            // same dimension with pImage
         ) = 0;

      virtual Status DetectMoirePattern(
         /* input */
         const Image* pImage,
         /* output */
         bool* isMoireDetected,
         Region* moireRegions
         )  = 0;

   protected:
      ImageCorrection();
      virtual ~ImageCorrection();
   };

} // namespace SCVE

#endif
