/*==============================================================================
 WFDWhiteListing.h

 DESCRIPTION:
    Code to receive event about application being started and also to notify
    about it to WFD SM layer.

 Copyright (c) 2015 Qualcomm Technologies, Inc.
 All Rights Reserved.
 Confidential and Proprietary - Qualcomm Technologies, Inc.
*===============================================================================
*/
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
#include "IWFDMMService.h"
#include "WFDMMServiceClient.h"

#ifdef __cplusplus
extern "C" {
#endif
    void activity_trigger_init(void);
    void activity_trigger_start(const char *name, int *flags);
    void activity_trigger_resume(const char *name);
    void activity_trigger_deinit(void);
    void activity_trigger_prepare(sp<WFDMMServiceClient>& serviceClient);
#ifdef __cplusplus
}
#endif
