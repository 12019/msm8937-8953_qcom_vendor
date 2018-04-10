/**=============================================================================

@file
   scveFaceRecognitionTypes.hpp

@brief
   SCVE Types Definition for Face Recognition feature.

Copyright (c) 2015 Qualcomm Technologies, Inc.
All Rights Reserved.
Confidential and Proprietary - Qualcomm Technologies, Inc.

=============================================================================**/

//=============================================================================
///@defgroup scveFaceRecognition Face-Recognition
///@brief Defines Types for SCVE-Face-Recognition
///@ingroup scve
//=============================================================================

#ifndef SCVE_FACE_RECOGNITION_TYPES_HPP
#define SCVE_FACE_RECOGNITION_TYPES_HPP

#include "scveTypes.hpp"

namespace SCVE
{

#define SCVE_FACE_RECOGNITION_DEFAULT_MAX_FACES                6
#define SCVE_FACE_RECOGNITION_DEFAULT_MAX_REGISTERED_FACES     100
#define SCVE_FACE_RECOGNITION_DEFAULT_MIN_FACE_SIZE_TO_DETECT  80
#define SCVE_FACE_RECOGNITION_FEATURE_ID_LEN                   20
#define SCVE_FACE_RECOGNITION_DEFAULT_GOOD_SCORE               200

//------------------------------------------------------------------------------
/// @brief
///    Error codes specific to FaceRecognition
///
/// @ingroup scveFaceRecognition
//------------------------------------------------------------------------------
enum StatusCodes_FaceRecognition
{
   SCVE_FACERECOGNITION_ERROR_INVALID_FEATURE_TYPE          = SCVE_FACERECOGNITION_ERROR + 1,
   SCVE_FACERECOGNITION_ERROR_INVALID_DATABASE              = SCVE_FACERECOGNITION_ERROR + 2,
   SCVE_FACERECOGNITION_ERROR_FEATURE_NOT_FOUND             = SCVE_FACERECOGNITION_ERROR + 3,
   SCVE_FACERECOGNITION_ERROR_INVALID_FACEINFO              = SCVE_FACERECOGNITION_ERROR + 4,
   SCVE_FACERECOGNITION_ERROR_SKIP_RECOGNITION              = SCVE_FACERECOGNITION_ERROR + 5
};

//------------------------------------------------------------------------------
/// @brief
///    Face Recognition image mode
///
/// @ingroup scveFaceRecognition
//------------------------------------------------------------------------------
typedef enum
{
   /// For offline processing of large image collection where real time feedback
   /// is not critical
   SCVE_FACERECOGNITION_STILL_IMAGE = 0,
   /// For camera previe mode where real time feedback is very critical
   SCVE_FACERECOGNITION_VIDEO       = 1
} FR_ImageMode;

//------------------------------------------------------------------------------
/// @brief
///    Face landmark location structure to store location of the key elements of
///    eyes, mouth, and nose that are used to recognize the face
/// @param sLeftEye
///    x and y location of left eye
/// @param sRightEye
///    x and y location of right eye
/// @param sLeftEyeInnerCorner
///    x and y location of inner corner of left eye
/// @param sLeftEyeOuterCorner
///    x and y location of outer corner of left eye
/// @param sRightEyeInnerCorner
///    x and y location of inner corner of right eye
/// @param sRightEyeOuterCorner
///    x and y location of outer corner of right eye
/// @param sMouthLeft
///    x and y location of left side of the mouth
/// @param sMouthRight
///    x and y location of right side of the mouth
/// @param sNoseLeft
///    x and y location of left side of the nose
/// @param sNoseRight
///    x and y location of right side of the nose
/// @param sMouthUp
///    x and y location of upper side of the mouth
///
/// @ingroup scveFaceRecognition
//------------------------------------------------------------------------------
typedef struct {
   Point sLeftEye;
   Point sRightEye;
   Point sLeftEyeInnerCorner;
   Point sLeftEyeOuterCorner;
   Point sRightEyeInnerCorner;
   Point sRightEyeOuterCorner;
   Point sMouthLeft;
   Point sMouthRight;
   Point sNoseLeft;
   Point sNoseRight;
   Point sMouthUp;
} FR_LandmarkLocation;

//------------------------------------------------------------------------------
/// @brief
///    Face info structure to store face information that will be used to recognize
///    face
/// @param nID
///    ID Number
/// @param sLeftTop
///    Left-Top bounding box face coordinates
/// @param sRightTop
///    Right-Top bounding box face coordinates
/// @param sLeftBottom
///    Left-Bottom bounding box face coordinates
/// @param sRightBottom
///    Right-Bottom bounding box face coordinates
/// @param sLandmarkLocation
///    Face Landmark location coordinates
///
/// @ingroup scveFaceRecognition
//------------------------------------------------------------------------------
typedef struct {
   int32_t              nID;
   Point                sLeftTop;
   Point                sRightTop;
   Point                sLeftBottom;
   Point                sRightBottom;
   FR_LandmarkLocation  sLandmarkLocation;
} FR_FaceInfo;

//------------------------------------------------------------------------------
/// @brief
///    FR_Result structure is used for obtaining results from the face recognition
///    operation
/// @param nMatchScore
///    The matching score.
///    Score 150 may give an acceptable matching result
///    Score 175 usually give a good matching result.
/// @param sPID
///    The associated person ID.
///
/// @ingroup scveFaceRecognition
//------------------------------------------------------------------------------
typedef struct
{
   float   nMatchScore;
   uint8_t sPID[SCVE_FACE_RECOGNITION_FEATURE_ID_LEN];
} FR_Result;

} //namespace SCVE

#endif //SCVE_FACE_RECOGNITION_HPP

