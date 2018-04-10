/*==============================================================================
*       WFDMMSourceAudioEncode.cpp
*
*  DESCRIPTION:
*       Abstract audio encoder class implementation
*
*
*  Copyright (c) 2014, 2016 Qualcomm Technologies, Inc.
*  All Rights Reserved.
*  Confidential and Proprietary - Qualcomm Technologies, Inc.
*===============================================================================
*/
/*==============================================================================
                             Edit History
================================================================================
   When            Who           Why
-----------------  ------------  -----------------------------------------------
04/02/2014         SK            InitialDraft
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
#include "WFDMMSourceAudioEncode.h"
#include "WFDMMSourceAACEncode.h"
#include "WFDMMSourceAC3Encode.h"
#include "MMMemory.h"
#include "WFDMMLogs.h"

#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "WFDMMSRCAUDIOENC"

/*==============================================================================

         FUNCTION:          create

         DESCRIPTION:
*//**       @brief          Creates an instance of audio encoder of the
                            desired codec
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
WFDMMSourceAudioEncode* WFDMMSourceAudioEncode::create(OMX_AUDIO_CODINGTYPE eFmt)
{
    WFDMMSourceAudioEncode *pAudioEnc = NULL;

    if(eFmt == OMX_AUDIO_CodingAAC)
    {
        WFDMMLOGH("Create AAC Encoder");
        pAudioEnc = MM_New(WFDMMSourceAACEncode);
    }
    else if(eFmt == OMX_AUDIO_CodingAC3)
    {
        WFDMMLOGH("Create AC3 Encoder");
        pAudioEnc = MM_New(WFDMMSourceAC3Encode);
    }
    return pAudioEnc;
}


/*==============================================================================

         FUNCTION:          ~WFDMMSourceAudioEncode

         DESCRIPTION:
*//**       @brief          destructor.
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
WFDMMSourceAudioEncode::~WFDMMSourceAudioEncode()
{
    WFDMMLOGH("Destructor WFDMMSourceAudioEncode");
}

