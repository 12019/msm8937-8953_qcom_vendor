/*==============================================================================
 WFDWhiteListing.cpp
 DESCRIPTION:
    Code to receive event about application being started and also to notify
    about it to WFD SM layer. This accepts the activity name and accordingly
    decides the operation based on function in which it receives, and passes
    on these information further.

 Copyright (c) 2015 Qualcomm Technologies, Inc.
 All Rights Reserved.
 Confidential and Proprietary - Qualcomm Technologies, Inc.
==============================================================================*/
/*==============================================================================
                             Edit History
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

#include "WFDWhiteListing.h"
#include "MMDebugMsg.h"
#include <binder/IServiceManager.h>
#include "common_log.h"
#include <utils/String16.h>


/*==============================================================================

         FUNCTION:      activity_trigger_init

         DESCRIPTION:
         @brief         This is called when library is loaded. It is required
                        to be in sync with list of api being called after library
                        is loaded.
*//**

@par     DEPENDENCIES:
                        None
*//*
         PARAMETERS:
*//**       @param      None

*//*     RETURN VALUE:
*//**       @return     None

@par     SIDE EFFECTS:  None

*//*==========================================================================*/
void activity_trigger_init(void)
{
    MM_MSG_PRIO(MM_GENERAL,MM_PRIO_ERROR,
                "WFDWhiteListing : library initialized successfully");
}

/*==============================================================================

         FUNCTION:      activity_trigger_deinit

         DESCRIPTION:
         @brief         This is called when library handle is deleted.
                        It is required to be in sync with list of api being
                        called after library is loaded.
*//**

@par     DEPENDENCIES:
                        None
*//*
         PARAMETERS:
*//**       @param      None

*//*     RETURN VALUE:
*//**       @return     None

@par     SIDE EFFECTS:  None

*//*==========================================================================*/
void activity_trigger_deinit(void)
{
    MM_MSG_PRIO(MM_GENERAL,MM_PRIO_ERROR,"WFDWhiteListing : deinited");
}

/*==============================================================================

         FUNCTION:      activity_trigger_start

         DESCRIPTION:
         @brief         This function is called when ever an application is
                        started.
*//**

@par     DEPENDENCIES:
                        None
*//*
         PARAMETERS:
*//**       @param      name  - Name of activity (format -> package/activity)
                        flags - this info is not related to WFD.

*//*     RETURN VALUE:
*//**       @return     None

@par     SIDE EFFECTS:  None

*//*==========================================================================*/

void activity_trigger_start(const char *name, int *flags) {
    MM_MSG_PRIO1(MM_GENERAL,MM_PRIO_MEDIUM,"WFDWhiteListing : - %s started",name);
    (void)flags;
    sp<WFDMMServiceClient> serviceClient;
    activity_trigger_prepare(serviceClient);
    if(serviceClient.get() != NULL)
    {
        serviceClient->setAppInfo((char*)name,"START");
    }
}

/*==============================================================================

         FUNCTION:      activity_trigger_resume

         DESCRIPTION:
         @brief         This function is called when an activity resumes.
*//**

@par     DEPENDENCIES:
                        None
*//*
         PARAMETERS:
*//**       @param      name - Name of activity (format -> package/activity)

*//*     RETURN VALUE:
*//**       @return     None

@par     SIDE EFFECTS:  None

*//*==========================================================================*/
void activity_trigger_resume(const char *name) {
    MM_MSG_PRIO1(MM_GENERAL,MM_PRIO_MEDIUM,"WFDWhiteListing : - %s resume",name);
    sp<WFDMMServiceClient> serviceClient;
    activity_trigger_prepare(serviceClient);
    if(serviceClient.get() != NULL)
    {
        serviceClient->setAppInfo((char*)name,"RESUME");
    }
}

/*==============================================================================

         FUNCTION:      activity_trigger_prepare

         DESCRIPTION:
         @brief         This is called to prepare a binder, so that name and
                        operation can be sent further.
*//**

@par     DEPENDENCIES:
                        None
*//*
         PARAMETERS:
*//**       @param      [out]servInstance - Binder for wfd.native.mm.service

*//*     RETURN VALUE:
*//**       @return     None

@par     SIDE EFFECTS:  None

*//*==========================================================================*/
void activity_trigger_prepare(sp<WFDMMServiceClient>& serviceClient)
{
    serviceClient = new WFDMMServiceClient(NULL,NULL);
}
