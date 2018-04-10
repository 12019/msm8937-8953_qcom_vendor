/*==============================================================================
*       WFDMessages.cpp
*
*  DESCRIPTION:
*       Class definition for WFDMessages. This provides async messaging
*   services for WFD Modules
*
*
*  Copyright (c) 2015 - 2016 Qualcomm Technologies, Inc.
*  All Rights Reserved.
*  Confidential and Proprietary - Qualcomm Technologies, Inc.
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
#include "WFDMessages.h"
#include "WFDMMThreads.h"
#include "MMMalloc.h"
#include "WFDMMLogs.h"
#include "MMSignal.h"
#include "MMMemory.h"
#include "MMTimer.h"
#include "WFDMMSourceSignalQueue.h"

#ifdef LOG_TAG
#undef LOG_TAG
#define LOG_TAG "WFDMESSAGES"
#endif


#define CHECKVALID(x) if(!isValid){WFDMMLOGE("Not a valid instance");return x;}

/*==============================================================================

         FUNCTION:          WFDMessage

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
@par     SIDE EFFECTS:
                            None
*//*==========================================================================*/
WFDMessage::WFDMessage()
{

    mbRecipientActive = false;
    mbNoResponse = false;

    int nRet = 0;
    /*--------------------------------------------------------------------------
     SignalQ and Signal is used if client wants to wait until the message
     is processed
    ----------------------------------------------------------------------------
    */
    nRet = MM_SignalQ_Create(&mSignalQ);

    if(nRet)
    {
        WFDMMLOGE("Failed to create Signal Q");
        isValid = false;
        return;
    }

    /*--------------------------------------------------------------------------
     Create one signal for the caller to wait for
    ----------------------------------------------------------------------------
    */
    static int nSignalVal = 0;
    nRet = MM_Signal_Create(mSignalQ,
                           (void*)(&nSignalVal),
                            NULL,
                           &mSignal);

    if(nRet)
    {
        WFDMMLOGE("Failed to create Signal for Message");
        isValid = false;
        return;
    }
    isValid = true;

}



/*==============================================================================

         FUNCTION:         Acquire

         DESCRIPTION:      All message recipients who will touch the message
                           instance after the end of callback has to call this
                           function to make sure the message is not freed by
                           the sender.
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
@par     SIDE EFFECTS:
                            None
*//*==========================================================================*/
void WFDMessage::Acquire()
{
    mbRecipientActive = true;
}

/*==============================================================================

         FUNCTION:         SetNoResponse

         DESCRIPTION:      Call this if it is a one way message. Source not
                           interested in a response
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
@par     SIDE EFFECTS:
                            None
*//*==========================================================================*/
void WFDMessage::SetNoResponse()
{
    mbNoResponse = true;
}



/*==============================================================================

         FUNCTION:          Release

         DESCRIPTION:       Recipients can call this at the end of usage of
                            the message object
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
@par     SIDE EFFECTS:
                            None
*//*==========================================================================*/
void WFDMessage::Release()
{
    mbRecipientActive = false;
}

/*==============================================================================

         FUNCTION:          WaitForResponse

         DESCRIPTION:
*//**       @brief          Wait for response, blocks until response if sent by
                            recepient of the message.
*//**
@par     DEPENDENCIES:
                            None
*//*
         PARAMETERS:
*//**       @param          None

*//*     RETURN VALUE:
*//**       @return
                            None
@par     SIDE EFFECTS:
                            None
*//*==========================================================================*/
bool WFDMessage::WaitForResponse(int *retCode)
{
    CHECKVALID(false);

    unsigned int *pEvent = NULL;

    WFDMMLOGL("Wait for Response");
    int nRet = MM_SignalQ_Wait(mSignalQ, (void **)&pEvent);
    WFDMMLOGL("Wait for Response Done");
    if(nRet)
    {
        WFDMMLOGE("Failed to wait for response");
        return false;
    }

    if(retCode)
    {
        *retCode = mnRetCode;
    }
    return true;
}





/*==============================================================================

         FUNCTION:          SendResponse

         DESCRIPTION:
*//**       @brief          Update the response part of the message.
*//**
@par     DEPENDENCIES:
                            None
*//*
         PARAMETERS:
*//**       @param          None

*//*     RETURN VALUE:
*//**       @return
                            None
@par     SIDE EFFECTS:
                            None
*//*==========================================================================*/
bool WFDMessage::NotifyResponse(int retCode)
{
    CHECKVALID(false);

    if(mbNoResponse)
    {
        /*----------------------------------------------------------------------
         Sender is not interested in response
        ------------------------------------------------------------------------
        */
        return true;
    }

    mnRetCode = retCode;

    int nRet = MM_Signal_Set(mSignal);

    if(nRet)
    {
        WFDMMLOGE("Failed to send response");
        return false;
    }
    return true;
}



/*==============================================================================

         FUNCTION:          ~WFDMessage

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
@par     SIDE EFFECTS:
                            None
*//*==========================================================================*/
WFDMessage::~WFDMessage()
{

    while(mbRecipientActive)
    {
        WFDMMLOGL("Destructor Wait for release");
        MM_Timer_Sleep(5);
    }
    WFDMMLOGL("Destructor Wait for release Done");

    if(mSignal)
    {
        MM_Signal_Release(mSignal);
    }

    if(mSignalQ)
    {
        MM_SignalQ_Release(mSignalQ);
    }

}


////////////////////////////////////////////////////////////////////////////////
///                               WFDMessageService                          ///
////////////////////////////////////////////////////////////////////////////////




/*==============================================================================

         FUNCTION:          WFDMessageService

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
@par     SIDE EFFECTS:
                            None
*//*==========================================================================*/
WFDMessageService::WFDMessageService(void* clientData, WFDMessageCbType pFn)
{
    mbActive = true;
    mMsgQ = MM_New_Args(SignalQueue, (100, sizeof(int*)));
    if(!mMsgQ)
    {
        isValid = false;
        return;
    }

    if(pFn)
    {
        /*----------------------------------------------------------------------
         Client wants to use asyncronously using a thread in WFDMessageService
        ------------------------------------------------------------------------
        */
        mpThread = MM_New_Args(WFDMMThreads, (1));

        if(!mpThread)
        {
            WFDMMLOGE("Failed to create thread for MessageService");
            isValid = false;
            return;
        }

        mpThread->Start(ThreadEntry, 0, 256*1024, (void*)this,"messagehandler");

        mpThread->SetSignal(0);
    }


    mpFn        = pFn;
    mClientData = clientData;

    isValid = true;
}


/*==============================================================================

         FUNCTION:

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
@par     SIDE EFFECTS:
                            None
*//*==========================================================================*/
WFDMessageService::~WFDMessageService()
{
    mbActive = false;

    if(mpThread)
    {
        MM_Delete(mpThread);
    }
    WFDMMLOGE("Deleted thread for MessageService");
    if(mMsgQ)
    {
        MM_Delete(mMsgQ);
    }
}

/*==============================================================================

         FUNCTION:         ThreadEntry

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
@par     SIDE EFFECTS:
                            None
*//*==========================================================================*/
void WFDMessageService::ThreadEntry(void* handle, unsigned int code)
{
    WFDMessageService *pMe = (WFDMessageService *)handle;

    if(pMe)
    {
        return (void)pMe->Thread(code);
    }

    return;

}


/*==============================================================================

         FUNCTION:          Thread

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
@par     SIDE EFFECTS:
                            None
*//*==========================================================================*/
int WFDMessageService::Thread(unsigned int code)
{
    CHECKVALID(-1);
    WFDMMLOGM1("Msg Service Thread code = %d", code);
    while(mbActive)
    {
        if(mMsgQ)
        {
            WFDMessage *pMsg = NULL;

            if(!mMsgQ->GetSize())
            {
                WFDMMLOGH("No more Messages to process");
            }

            mMsgQ->Pop(&pMsg, sizeof(pMsg), 50);

            if(mpFn && pMsg)
            {
                pMsg->Acquire();
                mpFn(mClientData, pMsg);
                pMsg->Release();
                if(pMsg->isNoResponseSet())
                {
                    MM_Delete(pMsg);
                }
            }

        }
    }
    return 0;
}


/*==============================================================================

         FUNCTION:          getMessageObject

         DESCRIPTION:
*//**       @brief          creates and provides a new object of message class.
*//**
@par     DEPENDENCIES:
                            None
*//*
         PARAMETERS:
*//**       @param          None

*//*     RETURN VALUE:
*//**       @return
                            None
@par     SIDE EFFECTS:
                            None
*//*==========================================================================*/
WFDMessage* WFDMessageService::getMessageObject()
{
    CHECKVALID(NULL);

    return new WFDMessage;
}



/*==============================================================================

         FUNCTION:          SendMessage

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
@par     SIDE EFFECTS:
                            None
*//*==========================================================================*/
bool WFDMessageService::SendMessage(WFDMessage *pMsg)
{
    CHECKVALID(false);

    if(mMsgQ && mpThread && pMsg)
    {
        mMsgQ->Push(&pMsg, sizeof(WFDMessage*));

     //   mpThread->SetSignal(0);

        return true;
    }

    return false;
}


/*==============================================================================

         FUNCTION:          recvMessage

         DESCRIPTION:
*//**       @brief          Wait for a new message. Function blocks until
                            new message arrives of timeout happens whichever is
                            earliest.
                            Client takes care of acquiring and releasing message
*//**
@par     DEPENDENCIES:
                            None
*//*
         PARAMETERS:
*//**       @param          None

*//*     RETURN VALUE:
*//**       @return
                            true - either timeout of succesful
                            false - for all other errors
@par     SIDE EFFECTS:
                            None
*//*==========================================================================*/
bool WFDMessageService::recvMessage(WFDMessage** pMsg, int nTimeOutMs)
{
    CHECKVALID(false);

    if(mpFn)
    {
        WFDMMLOGE("recvMessage not allowed n async mode");
        return false;
    }

    if(mMsgQ && pMsg)
    {
        OMX_ERRORTYPE eErr = mMsgQ->Pop(pMsg,
                                        sizeof(pMsg), nTimeOutMs);

        if(eErr == OMX_ErrorNone || eErr == OMX_ErrorTimeout)
        {
            return true;
        }
    }

    return false;
}
