/**=============================================================================

@file
   scveFaceRecognition.hpp

@brief
   SCVE API Definition for Face Recognition feature.

Copyright (c) 2015 Qualcomm Technologies, Inc.
All Rights Reserved.
Confidential and Proprietary - Qualcomm Technologies, Inc.

=============================================================================**/

//=============================================================================
///@defgroup scveFaceRecognition Face-Recognition
///@brief Defines API for SCVE-Face-Recognition
///@ingroup scve
//=============================================================================

#ifndef SCVE_FACE_RECOGNITION_HPP
#define SCVE_FACE_RECOGNITION_HPP

#include "scveFaceRecognitionTypes.hpp"
#include "scveContext.hpp"

namespace SCVE
{

//------------------------------------------------------------------------------
/// @brief
///    SCVE::FaceRecognition class that implements SCVE's Face Recognition feature.
///
/// @details
///    - It can detect and recognize face
///    - It can enroll detected face to a database and also unenroll specific feature ID
///      from the database
///    - Face Recognition only use the gray scale image
///    - Provides two recognition modes for appropriate use cases -
///      SCVE_FACERECOGNITION_STILL_IMAGE and SCVE_FACERECOGNITION_VIDEO
///
/// @ingroup scveFaceRecognition
//------------------------------------------------------------------------------
class SCVE_API FaceRecognition
{
public:
   //------------------------------------------------------------------------------
   /// @brief
   ///    Creates a new instance of Face Recognition feature.
   /// @details
   ///    This is a static factory-function that creates a new instance of
   ///    FaceRecognition feature. You will not be able to instanciate the class
   ///    by calling the constructor directly. You will be able to create a
   ///    new instance only through this function.
   /// @param pScveContext
   ///    [Input] Context under which this feature should be instantiated.
   /// @param sDBFileName
   ///    [Input] FR database file name. If exist read the existing data to get the
   ///    enrolled faces otherwise there is no enrolled faces to begin.
   /// @param eMode
   ///    [Input] Mode of operation to be either image or video.
   ///
   /// @retval Non-NULL
   ///    If the initialization was successful. The returned value is a
   ///    pointer to the newly created Face Recognition instance.
   /// @retval NULL
   ///    If the initialization has failed. Check logs for any error messages if
   ///    this occurs.
   //------------------------------------------------------------------------------
   static FaceRecognition* newInstance(Context            *pScveContext,
                                       const char         *sDBFileName = NULL,
                                       FR_ImageMode       eMode        = SCVE_FACERECOGNITION_VIDEO);

   //------------------------------------------------------------------------------
   /// @brief
   ///    De-initializes and cleans up all the resources associated with the
   ///    Face Recognition instance.
   /// @details
   ///    This is a factory-destructor method that will cleanup all resources
   ///    associated an Face Recognition instance. The same delete function will be
   ///    able to handle instances created for different operation modes.
   /// @param pFRInstance
   ///    Pointer to the Face Recognition instance. This is the same pointer that is
   ///    returned by the FaceRecognition::newInstance function.
   ///
   /// @retval SCVE_SUCCESS
   ///    If the deletion was successful.
   /// @retval SCVE_ERROR_INTERNAL
   ///    If any error occured.
   //------------------------------------------------------------------------------
   static Status deleteInstance(FaceRecognition *pFRInstance);

   //------------------------------------------------------------------------------
   /// @brief
   ///    Initialize face recognition engine.
   /// @details
   ///    This is the initialization method that prepares the internal data structure
   ///    based on the given parameter.
   /// @param sMaxDimension
   ///    [Input] Maximum image size of the input image.
   /// @param nMaxFaces
   ///    [Input] Maximum number of faces per image that can be detected.
   /// @param nMaxRegFaces
   ///    [Input] Maximum number of faces that can be enrolled to database.
   /// @param nMinFaceSizeToDetect
   ///    [Input] Minimum face size to detect in pixels for width and height.
   ///    This minimum face size is for detection face process. Smaller size will increase
   ///    the distance between detected face and camera with the expense of more face
   ///    detection process time.
   ///    To get a good result in recognizing face, it's recommended to have minimum 80x80
   ///    face size.
   /// @param sDBFileName
   ///    [Input] FR database file name. If exist, read the existing data to get the
   ///    enrolled faces. If NULL, read the data from the latest database file that might be
   ///    passed by newInstance() or SaveDatabase().
   ///    If NULL and previously never get any database file from newInstance() or SaveDatabase(),
   ///    then there will be no enrolled faces to begin.
   ///
   /// @retval SCVE_SUCCESS
   ///    If the deletion was successful.
   /// @retval SCVE_FACERECOGNITION_ERROR_INVALID_DATABASE
   ///    If there is related database error. Some possible errors are
   ///    - database is not accessible
   ///    - unmatch database and FR engine version
   ///    - unmatch max allowable registered faces at database and at the init parameter
   /// @retval SCVE_ERROR_INTERNAL
   ///    If any error occured.
   //------------------------------------------------------------------------------
   virtual Status Init(ImageDimension sMaxDimension,
                       uint32_t nMaxFaces            = SCVE_FACE_RECOGNITION_DEFAULT_MAX_FACES,
                       uint32_t nMaxRegFaces         = SCVE_FACE_RECOGNITION_DEFAULT_MAX_REGISTERED_FACES,
                       uint32_t nMinFaceSizeToDetect = SCVE_FACE_RECOGNITION_DEFAULT_MIN_FACE_SIZE_TO_DETECT,
                       const char *sDBFileName       = NULL) = 0;

   //------------------------------------------------------------------------------
   /// @brief
   ///    De-initialize face recognition engine.
   /// @details
   ///    This is the deinitialization method that clean up the internal data structure
   ///    after calling Init(). This API need to be called before re-Init().
   ///
   /// @retval SCVE_SUCCESS
   ///    If the deletion was successful.
   /// @retval SCVE_ERROR_INTERNAL
   ///    If any error occured.
   //------------------------------------------------------------------------------
   virtual Status Deinit() = 0;

   //------------------------------------------------------------------------------
   /// @brief
   ///    Detect faces in the image
   /// @details
   ///    This is the face detection method. It will detect number of faces in the
   ///    image. The maximum of faces detected will be less than or equal to
   ///    nMaxFaces that already defined previously at Init() API.
   /// @param pImage
   ///    [Input] The input image in grayscale format
   /// @param pFaceInfo
   ///    [Output] Array of face information found by face detection
   /// @param nMaxRegFaces
   ///    [Output] Total number of faces found in the image
   ///
   /// @retval SCVE_SUCCESS
   ///    If the deletion was successful.
   /// @retval SCVE_ERROR_INVALID_PARAM
   ///    If one of the parameters is invalid.
   /// @retval SCVE_ERROR_INTERNAL
   ///    If any error occured.
   //------------------------------------------------------------------------------
   virtual Status DetectFaces(const Image *pImage,
                              FR_FaceInfo *pFaceInfo,
                              uint32_t    *nDetectedFaces ) = 0;

   //------------------------------------------------------------------------------
   /// @brief
   ///    Recognize face in the image
   /// @details
   ///    This is the face recognition method. It will recognize one face in the
   ///    image based on the given face info. It will output number of results
   ///    in descending order based on the matching score. The best possible
   ///    matching result will be the first index at pResult array.
   /// @param pImage
   ///    [Input] The input image in grayscale format.
   /// @param pFaceInfo
   ///    [Input] One face information to be recognized.
   /// @param nMaxResults
   ///    [Input] Maximum results that needs to be returned. This should be less
   ///    than or equal to total enrolled faces.
   /// @param pResult
   ///    [Output] FaceRecognition result that contains the matching score and
   ///    associated person Id. The results are in descending order based on the
   ///    matching score.
   /// @param pNumResults
   ///    [Output] Actual number of results returned by the engine.
   ///
   /// @retval SCVE_SUCCESS
   ///    If the deletion was successful.
   /// @retval SCVE_ERROR_INVALID_PARAM
   ///    If one of the parameters is invalid.
   /// @retval SCVE_FACERECOGNITION_ERROR_SKIP_RECOGNITION
   ///    Only at SCVE_FACERECOGNITION_VIDEO Mode, if the face is being skip for the current image.
   ///    The number of results will be zero that indicates no result.
   /// @retval SCVE_FACERECOGNITION_ERROR_INVALID_FACEINFO
   ///    Only at SCVE_FACERECOGNITION_VIDEO Mode, if the face quality is not acceptable for
   ///    recognition.
   /// @retval SCVE_ERROR_INTERNAL
   ///    If any other errors occured.
   //------------------------------------------------------------------------------
   virtual Status RecognizeFace(const Image        *pImage,
                                const FR_FaceInfo  *pFaceInfo,
                                uint32_t           nMaxResults,
                                FR_Result          *pResult,
                                uint32_t           *pNumResults) = 0;

   //------------------------------------------------------------------------------
   /// @brief
   ///    Enroll face and link it to an ID
   /// @details
   ///    This is the face enrolling method. It will link the face landmark with
   ///    the id (sFeatureID)
   /// @param pImage
   ///    [Input] The input image in grayscale format.
   /// @param pFaceInfo
   ///    [Input] Face information to be enrolled.
   /// @param sFeatureID
   ///    [Input] Id of the face to be enrolled.
   ///
   /// @retval SCVE_SUCCESS
   ///    If the deletion was successful.
   /// @retval SCVE_ERROR_INVALID_PARAM
   ///    If one of the parameters is invalid.
   /// @retval SCVE_ERROR_INTERNAL
   ///    If any error occured.
   //------------------------------------------------------------------------------
   virtual Status EnrollFace(const Image        *pImage,
                             const FR_FaceInfo  *pFaceInfo,
                             const char         *sFeatureID) = 0;

   //------------------------------------------------------------------------------
   /// @brief
   ///    Enroll face, link it to an ID, and add this set of face & ID to
   ///    an existing file.
   /// @details
   ///    This is the face enrolling and add it to the selected database file.
   ///    It will link the face landmark with the id (sFeatureID) and add this set
   ///    at the end of the selected database file. The file must exist.
   ///
   /// @param pImage
   ///    [Input] The input image in grayscale format.
   /// @param pFaceInfo
   ///    [Input] Face information to be enrolled.
   /// @param sFeatureID
   ///    [Input] Id of the face to be enrolled.
   /// @param sDBFileName
   ///    [Input] Existing database path to be saved.
   ///
   /// @retval SCVE_SUCCESS
   ///    If the deletion was successful.
   /// @retval SCVE_ERROR_INVALID_PARAM
   ///    If one of the parameters is invalid.
   /// @retval SCVE_ERROR_INTERNAL
   ///    If any error occured, e.g. the file doesn't exist.
   //------------------------------------------------------------------------------
   virtual Status EnrollFaceAndAddToDatabase(const Image        *pImage,
                                             const FR_FaceInfo  *pFaceInfo,
                                             const char         *sFeatureID,
                                             const char         *sDBFileName) = 0;

   //------------------------------------------------------------------------------
   /// @brief
   ///    Get the IDs of enrolled samples from the database
   /// @details
   ///    This is a method to get all the IDs that has been enrolled to database.
   /// @param pIdList
   ///    [Output] Array of ID. Each ID contains 20 char. Memory for pIdList should
   ///    be allocated at least nMaxRegFaces * 20 bytes.
   /// @param nCount
   ///    [Output] Total no of faces present.
   ///
   /// @retval SCVE_SUCCESS
   ///    If the deletion was successful.
   /// @retval SCVE_ERROR_INVALID_PARAM
   ///    If one of the parameters is invalid.
   /// @retval SCVE_ERROR_INTERNAL
   ///    If any error occured.
   //------------------------------------------------------------------------------
   virtual Status GetEnrolledIDsFromDatabase( char      **pIdList,
                                              uint32_t  *nCount) = 0;

   //------------------------------------------------------------------------------
   /// @brief
   ///    Get the total number of enrolled faces from the database.
   /// @param nUsers
   ///    [Output] Total no of enrolled faces.
   ///
   /// @retval SCVE_SUCCESS
   ///    If the deletion was successful.
   /// @retval SCVE_ERROR_INVALID_PARAM
   ///    If one of the parameters is invalid.
   /// @retval SCVE_ERROR_INTERNAL
   ///    If any error occured.
   //------------------------------------------------------------------------------
   virtual Status GetNumDatabaseUsers(uint32_t *nUsers) = 0;

   //------------------------------------------------------------------------------
   /// @brief
   ///    Delete an ID from the database.
   /// @param sFeatureID
   ///    [Input] Id of the person to be removed
   ///
   /// @retval SCVE_SUCCESS
   ///    If the deletion was successful.
   /// @retval SCVE_ERROR_INVALID_PARAM
   ///    If one of the parameters is invalid.
   /// @retval SCVE_ERROR_INTERNAL
   ///    If any error occured.
   //------------------------------------------------------------------------------
   virtual Status RemoveFromDatabase(const char *sFeatureID) = 0;

   //------------------------------------------------------------------------------
   /// @brief
   ///    Save the database.
   /// @param sDBFileName
   ///    [Input] Database path to be saved.
   ///
   /// @retval SCVE_SUCCESS
   ///    If the deletion was successful.
   /// @retval SCVE_ERROR_INVALID_PARAM
   ///    If one of the parameters is invalid.
   /// @retval SCVE_ERROR_INTERNAL
   ///    If any error occured.
   //------------------------------------------------------------------------------
   virtual Status SaveDatabase(const char *sDBFileName) = 0;

   //------------------------------------------------------------------------------
   /// @brief
   ///    Get the database information, i.e. database version, max allowable
   ///    registered faces, current registered faces.
   /// @param nDatabaseVersion
   ///    [Output] Database version.
   /// @param nMaxRegisteredFaces
   ///    [Output] Max registered faces allow at this database.
   /// @param nRegisteredFaces
   ///    [Output] Total number of registered faces.
   ///
   /// @retval SCVE_SUCCESS
   ///    If the deletion was successful.
   /// @retval SCVE_ERROR_INVALID_PARAM
   ///    If one of the parameters is invalid.
   /// @retval SCVE_ERROR_INTERNAL
   ///    If any error occured.
   //------------------------------------------------------------------------------
   virtual Status GetDatabaseInfo(const char   *sDBFileName,
                                  uint32_t     *nDatabaseVersion,
                                  uint32_t     *nMaxRegisteredFaces,
                                  uint32_t     *nRegisteredFaces) = 0;

protected:
   //------------------------------------------------------------------------------
   /// @brief
   ///    Protected destructor. Please use the static FaceRecognition::deleteInstance
   ///    function for de-initialize and cleaning-up the FaceRecognition instances.
   //------------------------------------------------------------------------------
   virtual ~FaceRecognition() = 0;
}; //class FaceRecognition

} //namespace SCVE

#endif //SCVE_FACE_RECOGNITION_HPP

