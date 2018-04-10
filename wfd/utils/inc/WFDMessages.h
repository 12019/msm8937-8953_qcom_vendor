#ifndef __WFDMESSAGES_H
#define __WFDMESSAGES_H
/*==============================================================================
*       WFDMessages.h
*
*  DESCRIPTION:
*       Class declaration for WFDMessages. This provides async messaging
*   services for WFD MM Modules
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
09/23/2014         SK            InitialDraft
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
#include "WFDMMThreads.h"
#include "WFDMMLogs.h"


class SignalQueue;

class WFDMessage
{
public:
    WFDMessage();
    ~WFDMessage();
    void          Acquire();
    void          Release();
    bool          WaitForResponse(int *);
    bool          NotifyResponse(int);
    void          SetNoResponse();
    bool          isNoResponseSet(){return mbNoResponse;};
    int           mnCmd;
    void*         mpResp;
    int           mnRetCode;
    void*         cmdData[20];
    unsigned int  nNumArgs;

private:

    MM_HANDLE     mSignalQ;
    MM_HANDLE     mSignal;
    bool          mbRecipientActive;
    bool          isValid;
    bool          mbNoResponse;
};


//! Callback function if client needs Message Service to provide
//! Async message delivery
typedef void (*WFDMessageCbType)(void*, WFDMessage*);

class WFDMessageService
{
public:
    WFDMessageService(void* clientData, WFDMessageCbType pFn);

    ~WFDMessageService();

    WFDMessage*      getMessageObject();

    bool             SendMessage(WFDMessage *pMsg);

    bool             recvMessage(WFDMessage**);

    bool             recvMessage(WFDMessage** pMsg, int nTimeOutMs);

private:
    static void      ThreadEntry(void* handle, unsigned int code);
    int              Thread(unsigned int);
    WFDMMThreads    *mpThread;
    SignalQueue     *mMsgQ;
    bool             isValid;
    bool             mbActive;
    WFDMessageCbType mpFn;
    void            *mClientData;
};
#endif /*__WFDMESSAGES_H*/
