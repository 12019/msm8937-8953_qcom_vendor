/*==============================================================================
*       WFDMMSourceImageEncode.cpp
*
*  DESCRIPTION:
*       Implementation of WFD Image Encoder class.
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
#include "WFDMMSourcePngEncode.h"
#include "MMMemory.h"



/*==============================================================================

         FUNCTION:          Create

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
WFDMMSourceImageEncode* WFDMMSourceImageEncode::Create
(
    wfdImgCodingType eFmt
)
{
    if(eFmt == WFD_IMG_CODING_PNG)
    {
        return MM_New(WFDMMSourcePNGEncode);
    }
    else
    {
        return NULL;
    }
}
