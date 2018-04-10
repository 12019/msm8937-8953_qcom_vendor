/**=============================================================================

@file
scveTextReco.hpp

@brief
SCVE API Definition for Text Reco features.

Copyright (c) 2014 Qualcomm Technologies, Inc.  All Rights Reserved.
Qualcomm Technologies Proprietary and Confidential.

=============================================================================**/

#ifndef SCVE_TEXT_RECO_HPP
#define SCVE_TEXT_RECO_HPP

#include "scveTypes.hpp"
#include "scveContext.hpp"
#include <stdint.h>

//#include "scveTextRecoResult.h"

// Blob extraction
#define SCVE_MAX_WAVELET_BLOBS_RATIO (128)      // Max number of wavelet blobs for memory allocation

// Blob clustering
#define SCVE_ROTATION_THRESHOLD (0.05f)         // Rotation threshold of blob/cluster for skew processing

// Blob classification
#define SCVE_HEIGHT_SMALL_TEXT (24)

// Post-processing
#define SCVE_NUM_HIST_BIN (64)               // Number of histogram bin of blob cluster
#define SCVE_NUM_HIST_SIZE (4)               // Histogram bin size of blob cluster


#define GRAY_PYRAMID_LV       (3)

#define MAX_PHONE_NUM           48
#define MAX_TEXT_NUM       512
#define MAX_TEXT_LENGTH       128
#define MAX_LINE_LENGTH       128
#define MAX_PATH_LENGTH       256
#define MAX_NETWORK_NODE_NUM  2048
#define MAX_FEATURE_TYPE_LENGTH  64
#define MAX_FST_PATH       64
#define WORD_IMAGE_HEIGHT     48
#define DATETIME_LENGTH       36
#define MAX_DATETIME_COUNT    10

#define NORMALIZED_IMAGE_LENGTH (960)

namespace SCVE
{

typedef struct _scveTextRecoResult
{
   uint16_t       id;
   uint16_t       line_id;
   uint32_t       category;
   SCVE::Point       vertex[4];                    // vertex[0]:left top, vertex[1]:right top, vertex[2]:right bottom, vertex[3]:left bottom
   char           text            [MAX_TEXT_LENGTH];  // UTF8 encoded
   float          cost            [MAX_TEXT_LENGTH];
   uint16_t       datetime_count;
   char           datetime    [MAX_DATETIME_COUNT][DATETIME_LENGTH]; // YYYYMMDDWWWHHMMSS-YYYYMMDDWWWHHMMSS

   uint8_t           text_length;
   SCVE::Rect        letter_boundary [MAX_TEXT_LENGTH];

} scveTextRecoResult;

enum ImageMode
{
   IMAGE_MODE_SINGLE_FRAME,
   IMAGE_MODE_CONTINUOUS_FRAME
};

// TextReco Category
enum TextReco_Category
{
   TEXT_RECO_CATEGORY_EMAIL         = 0x01,
   TEXT_RECO_CATEGORY_URL           = 0x02,
   TEXT_RECO_CATEGORY_PHONENUMBER      = 0x04,
   TEXT_RECO_CATEGORY_ADDRESS       = 0x08,
   TEXT_RECO_CATEGORY_DATETIME         = 0x10,
   TEXT_RECO_CATEGORY_GENERAL       = 0x80,
   TEXT_RECO_CATEGORY_USER_DEFINED_WORDLIST  = 0x100,
   TEXT_RECO_CATEGORY_ALL           = 0xffff
};


typedef enum _TextRecoLocale
{
   TEXT_RECO_LOCALE_USA,
   TEXT_RECO_LOCALE_KOREA,
   TEXT_RECO_LOCALE_CHINA
} TextRecoLocale;


//------------------------------------------------------------------------------
/// @brief
///    Error codes specific to TextReco
///
/// @ingroup scveTextReco
//------------------------------------------------------------------------------
enum StatusCodes_TextReco
{
   /// Returned by TextReco functions if input image is too blurry.
   SCVE_TEXT_RECO_ERROR_TOO_BLURRY                 = SCVE_TEXT_RECO_ERROR + 1,

   /// Returned by TextReco functions if text candidate regions
   /// in image are too small.
   SCVE_TEXT_RECO_ERROR_TEXT_IS_TOO_SMALL          = SCVE_TEXT_RECO_ERROR + 2,

   SCVE_TEXT_RECO_ERROR_INSUFFCIENT_INTERNAL_MEMORY   = SCVE_TEXT_RECO_ERROR + 3,

   /// Returned by TextReco functions when the size of the result array is
   /// smaller than number of text regions with the text detector
   SCVE_TEXT_RECO_ERROR_INSUFFCIENT_RESULT_ARRAY      = SCVE_TEXT_RECO_ERROR + 4,

   /// Returned by TextReco function when the index
   /// provided are not of valid text region.
   SCVE_TEXT_RECO_ERROR_INVALID_OBJECT_INDEX       = SCVE_TEXT_RECO_ERROR + 5,

   SCVE_TEXT_RECO_ERROR_QUEUE_FULL                 = SCVE_TEXT_RECO_ERROR + 6,

   /// Used only in MULTI_FRAME_MODE
   SCVE_TEXT_RECO_MULTI_FRAME_FUSION_EMPTY            = SCVE_TEXT_RECO_ERROR + 7,
   SCVE_TEXT_RECO_MULTI_FRAME_FUSION_IN_PROGRESS      = SCVE_TEXT_RECO_ERROR + 8,
   SCVE_TEXT_RECO_MULTI_FRAME_FUSION_TIMEOUT       = SCVE_TEXT_RECO_ERROR + 9,

   /// USER DEFINED WORD LIST
   SCVE_TEXT_RECO_ERROR_ENGINE_INITIALIZE          = SCVE_TEXT_RECO_ERROR + 10,
   SCVE_TEXT_RECO_ERROR_USER_DEFINED_FST_OPEN         = SCVE_TEXT_RECO_ERROR + 11,

};

typedef void(*TextReco_Callback) (
   SCVE::Status      status,
   uint8_t           cost,
   uint16_t       numTexts,
   scveTextRecoResult* pResult,
   void*          pUserData
   );

class SCVE_API TextReco
{
public:
   static TextReco* newInstance(
      Context* pScveContext,
      ImageMode mode,
      TextReco_Callback cbTextReco = NULL );

   static Status deleteInstance(TextReco* pInstance);

   virtual Status Init(
      const uint32_t category,
      const char*    locale,     // follow ISO 3166-1 alpha-3 "kor"
      const uint16_t maxResults
      ) = 0;

   virtual Status DeInit() = 0;

   virtual void GetLocale(char* locale) const;

   virtual Status GetTextnessMap(
      const Image*         pImage,
      const SCVE::Rect     roi,

      Image*               pTextnessMap
      ) = 0;

   virtual Status SetUserDictionary(
      const char*          dictionaryPath
      ) = 0;

   virtual Status Run_Sync(
      const Image*         pImage,
      SCVE::Rect           roi,        // Region of interest
      uint8_t*          sharpness,
      uint16_t*            numTexts,
      scveTextRecoResult*     resultText
      ) = 0;

   virtual Status Run_Async(
      const Image*         pImage,
      SCVE::Rect           roi,        // Region of interest
      uint8_t*          sharpness,
      uint16_t*            numTexts,
      scveTextRecoResult*     resultText,
      void*             pUserData
      ) = 0;

   virtual Status ReStart() = 0;
protected:
   TextReco();
   virtual ~TextReco();
};

const char* GetTextRecoResultString(enum StatusCodes_TextReco code);

} //namespace SCVE

#endif // SCVE_TEXT_RECO_HPP
