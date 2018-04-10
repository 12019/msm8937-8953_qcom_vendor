/**=============================================================================

@file
scveCleverCapture.hpp

@brief
SCVE API Definition for CleverCapture features.

Copyright (c) 2015  Qualcomm Technologies, Inc.
All Rights Reserved.
Confidential and Proprietary - Qualcomm Technologies, Inc.

=============================================================================**/

//=============================================================================
///@defgroup scveCleverCapture Clever Capture
///@brief Defines API for SCVE-CleverCapture feature
///@ingroup scve
//=============================================================================

#ifndef SCVE_CLEVERCAPTURE_HPP
#define SCVE_CLEVERCAPTURE_HPP

#include "scveTypes.hpp"
#include "scveContext.hpp"

namespace SCVE
{

//------------------------------------------------------------------------------
/// @brief
///    CleverCapture_Precision structure is used for obtaining Clever Capture results of
///    different precisions from CleverCapture
///
/// @ingroup scveCleverCapture
//------------------------------------------------------------------------------
typedef enum
{
   /// High precision mode -
   SCVE_CLEVERCAPTURE_PRECISION_LOW     = 0,
   /// Low precision mode  -
   SCVE_CLEVERCAPTURE_PRECISION_HIGH    = 1,
}CleverCapture_Precision;

//------------------------------------------------------------------------------
/// @brief
///    Structure used by CleverCapture post processing algorithm to return results
/// @ingroup scveCleverCapture
//------------------------------------------------------------------------------
struct ppm_pkt_t{
    uint32_t frame_id;//Alias to frame_id within cc_score_t
    uint32_t *rank;//A rank per frame in the burst; 0 implies drop. This array
                   // has to be allocated and freed by the user (array size should be equal to
                   // ‘numPicsPerClick’ passed during instance creation

};

//------------------------------------------------------------------------------
/// @brief
///    Structure used by CleverCapture post processing algorithm to return results
/// @ingroup scveCleverCapture
//------------------------------------------------------------------------------
struct cc_score_t{
    uint32_t frame_id;//Unique key returned for a frame submitted to CleverCapture
    bool     is_interesting; // Whether Frame is Interesting or Not
    uint32_t reserved;//Reserved field
};

class CleverCapture;

//------------------------------------------------------------------------------
/// @brief
///    Needs to be updated.
/// @details
///    Needs to be updated.
/// @param status
///    Contains the status of the Async request.
/// @param pSessionUserData
///    User-data that was set by you in the CleverCapture::newInstance function.
/// @param pTaskUserData
///    User-data that was set by you in the ABCD function
///    which corresponds to this callback.
/// @remarks
///    - See 'Using Async Functions' section in the documentation of
///      CleverCapture class.
/// @ingroup scveCleverCapture
//------------------------------------------------------------------------------
typedef void (*CleverCapture_CleverCaptureCallback)(Status status,
                                void* pSessionUserData,
                                void* pTaskUserData);


//------------------------------------------------------------------------------
/// @brief
///    Needs to be updated.
/// @details
///    Needs to be updated.
/// @param status
///    Contains the status of the Async request.
/// @param pSessionUserData
///    User-data that was set by you in the CleverCapture::newInstance function.
/// @param pTaskUserData
///    User-data that was set by you in the ABCD function
///    which corresponds to this callback.
/// @remarks
///    - See 'Using Async Functions' section in the documentation of
///      CleverCapture class.
/// @ingroup scveCleverCapture
//------------------------------------------------------------------------------
typedef void (*CleverCapture_RegCallback)(Status status,
                                void* pSessionUserData,
                                void* pTaskUserData);


//------------------------------------------------------------------------------
/// @brief
///    SCVE::CleverCapture class that implements SCVE's CleverCapture feature.
///    As of now only Sync variety is supported
///
/// @details
///    - It can checkf for interesting frames in ROI Rectangles (that can be registered
///      at run-time using RegisterROI_Sync) over frames of an input video sequence.
///    - Supports the following color formats - SCVE::SCVE_COLORFORMAT_GREY_8BIT,
///      SCVE_COLORFORMAT_YUV_NV12 and SCVE_COLORFORMAT_YUV_NV21.
///    - Provides two CleverCapture modes for precision -
///      SCVE_CLEVERCAPTURE_PRECISION_HIGH and SCVE_CLEVERCAPTURE_PRECISION_LOW.
///    - Async functions are not supported.
///
/// @ingroup scveCleverCapture
//------------------------------------------------------------------------------
class SCVE_API CleverCapture
{
   public:

      //------------------------------------------------------------------------------
      /// @brief
      ///    Creates a new instance of Clever Capture feature.
      /// @details
      ///    This is a static factory-function that creates a new instance of
      ///    CleverCapture feature. You will not be able to instanciate the class
      ///    by calling the constructor directly. You will be able to create a
      ///    new instance only through this function.
      /// @param pScveContext
      ///    Context under which this feature should be instantiated.
      /// @param sDimension
      ///    ImageDimension structure, which indicates the dimensions of the
      ///    images on which CleverCapture alogrithm is rum
      /// @param eColorFormat
      ///    Sets what color format images will be provided. CleverCapture supports SCVE_COLORFORMAT_YUV_NV12,
      ///    SCVE_COLORFORMAT_YUV_NV21 and SCVE_COLORFORMAT_GREY_8BIT
      /// @param numPicsPerClick
      ///    Number of pictures captured in a single takepicture request.
      /// @param cbCleverCapture
      ///    [\a Not supported, set to NULL]
      /// @param cbRegistration
      ///    [\a Not supported, set to NULL]
      /// @param ePrecision
      ///    [\a optional] Denotes the precision mode in which the CleverCapture should
      ///    operate in. By default, it will always operate in high-precision mode.
      ///    However, the user can chose to switch to low-precision mode, which will
      ///    reduce the computation load on the processor. The consequence of that
      ///    is the results generated by the CleverCapture will be less precise
      ///    (will vary by a few pixels compared to high-precision mode).
      /// @param pSessionUserData
      ///    A private pointer the user can set with this session, and this pointer
      ///    will be provided as parameter to all callback functions originating
      ///    from the current session. This could be used to associate a callback
      ///    to this session. This can only be set once, while creating a new instance
      ///    of the CleverCapture. This value will not/cannot-be changed for life of a
      ///    CleverCapture session.
      /// @retval Non-NULL
      ///    If the initialization was successful. The returned value is a
      ///    pointer to the newly created CleverCapture instance.
      /// @retval NULL
      ///    If the initialization has failed. Check logs for any error messages if
      ///    this occurs.
      ///
      /// @remarks
      ///    - Use the symmetric CleverCapture::deleteInstance function to de-initialize
      ///    and cleaup the CleverCapture instances created by this function.
      ///    - If the fucntion returns NULL, check the logs and look for any error messages.
      //------------------------------------------------------------------------------
      static CleverCapture* newInstance       ( Context*                     pScveContext,
                                             ImageDimension              sDimension,
                                             ColorFormat                 eColorFormat,
                                             uint32_t                    nPicsPerClick,
                                             CleverCapture_CleverCaptureCallback cbCleverCapture = NULL,
                                             CleverCapture_RegCallback       cbRegistration = NULL,
                                             CleverCapture_Precision         ePrecision = SCVE_CLEVERCAPTURE_PRECISION_HIGH,
                                             void*                 pSessionUserData = NULL );

      //------------------------------------------------------------------------------
      /// @brief
      ///    De-initializes and cleans up all the resources associated with the
      ///    CleverCapture instance.
      /// @details
      ///    This is a factory-destructor method that will cleanup all resources
      ///    associated an CleverCapture instance. The same delete function will be
      ///    able to handle instances created for different operation modes.
      /// @param pCleverCapture
      ///    Pointer to the CleverCapture instance. This is the same pointer that is
      ///    returned by the CleverCapture::newInstance function.
      ///
      /// @retval SCVE_SUCCESS If the deletion was successful.
      /// @retval SCVE_ERROR_INTERNAL If any error occured.
      //------------------------------------------------------------------------------
      static Status deleteInstance         ( CleverCapture*              pCleverCapture );

      //------------------------------------------------------------------------------
      /// @brief
      ///    A synchronous function to register a Region-of-Interest (ROI) with CleverCapture
      ///
      /// @details
      ///    Once the registration is successful and complete, all subsequent
      ///    IsCleverCapture_Sync calls will process on registered ROIs.
      /// @param sROI
      ///    Co-ordinates of the ROI rectangle that represents the ROI
      /// @param pIndex
      ///    A pointer to an integer. On successful registeration a unique 'index' is populated
      ///    to the address location pointed. This index could then be used with
      ///    CleverCapture::UnregisterROI_Sync function.
      /// @return
      ///    Returns a status code from SCVE::StatusCodes
      //------------------------------------------------------------------------------
      virtual Status RegisterROI_Sync   ( Rect         sROI,
                                          uint32_t*    pIndex ) = 0;


      //------------------------------------------------------------------------------
      /// @brief
      ///    Unregisters a previously registered ROI.
      /// @details
      ///    Takes the ROI index provided by RegisterROI functions, and
      ///    unregisters the ROI from any further CleverCapture. This will facilitate
      ///    unregistering an ROI in the middle of a CleverCapture session.
      /// @param nIndex
      ///    Index of the ROI to be unregistered. This should be same as what was
      ///    returned by register-ROI functions.
      /// @return
      ///    Returns a status code from SCVE::StatusCodes
      /// @remarks
      ///    While unregistering every ROI before 'deleting' an instance is a
      ///    good practice, it is not a required activity. 'Deleting' an instance
      ///    also involves unregistering all ROIs that are not already unregistered.
      //------------------------------------------------------------------------------
      virtual Status UnregisterROI_Sync      ( uint32_t nIndex ) = 0;

      //------------------------------------------------------------------------------
      /// @brief
      ///    A synchronous function that executes all CleverCapture algorithm on all previously
      ///    registered ROIs in the image provided.
      /// @details
      ///    - This is a synchronous function, on return the results of
      ///    the algorithm will be available as a score provided as a parameter.
      ///    - If the provided image does not contain any interesting action/moment the
      ///    score will be set to zero
      /// @param pImage
      ///    Image on which the CleverCapture will be performed. The dimensions of this
      ///    image should match with ImageDimension that was provided while creating
      ///    the CleverCapture instance.
      /// @param score
      ///    Returns a score indicating if pImage is a CleverCapture. Set to zero if not a CleverCapture.
      /// @return
      ///    Returns a status code from SCVE::StatusCodes
      /// @remarks
      ///    - If the function returns SCVE_SUCCESS, it means that function has
      ///    successfully completed.
      //------------------------------------------------------------------------------
      virtual Status isInterestingFrame_Sync ( Image*              pImage,
                                               cc_score_t*         pScore ) = 0;

      //------------------------------------------------------------------------------
      /// @brief
      ///    Submit a frame to the post processing module.
      /// @details
      ///    Post processing module will normalize scores across multiple submissions.
      ///    This will help user to select most relevant clever captures from all the set of
      ///    clever captures captured.
      /// @param frameId
      ///    FrameId that is being submitted for post processing.
      /// @return
      ///    Returns a status code from SCVE::StatusCodes
      /// @remarks
      ///    - If the function returns SCVE_SUCCESS, it means that function has
      ///    successfully completed.
      //------------------------------------------------------------------------------
      virtual Status submitForEval( uint32_t       nFrameId ) = 0;

      //------------------------------------------------------------------------------
      /// @brief
      ///    To be called before end of a CleverCapture session. It will return a set of ranks for
      ///    each CleverCapture - of the given ROI - submitted for post-processing
      /// @details
      ///    Returns a list of CleverCapture frame numbers - of the given ROI - along with their ranks
      ///    assigned by the post processing algorithm. User can decide to keep or
      ///    discard frames using these ranks.
      /// @param pFrameList
      ///    Returns a list of frames along with their score.
      /// @param nSizeFrameList
      ///    Size of List of frame for which Rank is calculated
      /// @param roiId
      ///    ID of ROI for which Rank is calculated
      ///
      /// @return
      ///    Returns a status code from SCVE::StatusCodes
      /// @remarks
      ///    - If the function returns SCVE_SUCCESS, it means that function has
      ///    successfully completed.
      //------------------------------------------------------------------------------
      virtual Status getCleverCaptures( ppm_pkt_t*     pFrameList,
                                    uint32_t       nSizeFrameList,
                                    uint32_t       roiId) = 0;

   protected:
      //------------------------------------------------------------------------------
      /// @brief
      ///    Protected destructor. Please use the static CleverCapture::deleteInstance
      ///    function for de-initialize and cleaning-up the CleverCapture instances.
      //------------------------------------------------------------------------------
      virtual bool isGood()= 0;
      virtual ~CleverCapture ( );

}; //class CleverCapture

} //namespace SCVE

#endif //SCVE_CLEVERCAPTURE_HPP
