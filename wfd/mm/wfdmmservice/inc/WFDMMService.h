#ifndef __WFDSERVICE_CLIENT_H__
#define __WFDSERVICE_CLIENT_H__

/*==============================================================================
*       WFDMMService.h
*
*  DESCRIPTION:
*       WFDMMService class
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
06/09/2014         SK            InitialDraft
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
#include <binder/IServiceManager.h>
#include "IWFDMMListener.h"

#define WFDMMSERVICE_STREAMING_REQ      0
#define WFDMMSERVICE_START_STREAMING    1
#define WFDMMSERVICE_STOP_STREAMING     2
#define WFDMMSERVICE_WRITE_DATA         3
#define WFDMMSERVICE_RENDER_DATA        4
#define WFDMMSERVICE_SET_WFD_LISTENER   5
#define WFDMMSERVICE_CURRENT_DS_SUPPORT 6
#define WFDMMSERVICE_FLUSH_STREAM       7
#define WFDMMSERVICE_APP_INFO           8

/* Max length for Name of application */
#define WHITELIST_APP_NAME_LENGTH 100
/* Max length for Operation that application is performing */
#define WHITELIST_OPERATION_LENGTH 10

typedef struct {
    unsigned long nLen;
    unsigned long long nTimeUs;
    bool          bFd;
    int           nFlags;
    bool          bSecure;
    union
    {
        char* pData;
        struct
        {
            int   fd;
            void*   pvtData;
        }IonFdData;
    }DataPtr;
} WriteDataType;

typedef struct {
    int codecType;
    int profile;
    int level;
    int width;
    int height;
    bool bSecure;
    bool bConstrainedProfile;
}StreamParamsType;
typedef struct {
    unsigned long long nTimeUs;
}RenderDataType;

typedef struct {
    char name[WHITELIST_APP_NAME_LENGTH];
    char operation[WHITELIST_OPERATION_LENGTH];
}WhiteListType;

typedef union streamData
{
    StreamParamsType streamParams;
    WriteDataType WriteData;
    RenderDataType RenderDataParam;
}streamDataType;

typedef int (*pFnWFDCallbackType)(void*,
                                  int,
                                  void*);

class WFDMMService: public BnWFDMMService
{
public:
    static WFDMMService* start();

    static int notifyClient(
        WFDMMService* pMine,
        int event,
        void* PvtData);

    int notifyListener(
        int event,
        void* PvtData);
    static void addCallback(
        pFnWFDCallbackType,
        void *);

    int isStreamingSuccess(
        int codecType,
        int profile,
        int level,
        int width,
        int height,
        bool bSecure,
        bool bConstrainedProfile);

    int isStreamingFeatureEnabled();

    int startStreaming();

    int writeFrame(
        unsigned long nLen,
        char* pData,
        unsigned long long time_us);

    int writeFrameFd(
        unsigned long nLen,
        int fd,
        unsigned long long time_us,
        void* pvtData,
        int   nFlags,
        bool bSecure);

    int renderFrame(unsigned long long time_us);

    int pauseStreaming();

    int endStreaming();

    int setWFDMMListener( bool bActive,
        const sp<IWFDMMListener>& WfdServiceListener);

    static void updateStreamingFeatureStatus(bool bStatus);

    int flush();

    int setAppInfo(const char *name,
                   const char *operation);

    static void updateWhiteListStatus(bool allowed);

    int isAppWhiteListed();

private:
    static android::sp<android::IServiceManager> m_pMMServiceManager;
    static sp<WFDMMService> m_pWFDMMService;
    sp<IWFDMMListener> mMMServiceListener;

};

#endif//__WFDSERVICE_CLIENT_H__
