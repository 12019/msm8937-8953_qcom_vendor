/*==============================================================================
*       IWFDMMService.cpp
*
*  DESCRIPTION:
*       WFD Service Binder Interface implementation
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

int BpWFDMMService::isStreamingSuccess
(
    int codecType,
    int profile,
    int level,
    int width,
    int height,
    bool bSecure,
    bool bConstrainedProfile
)
{
    Parcel sIn;
    Parcel sOut;

    sIn.writeInterfaceToken(IWFDMMService::getInterfaceDescriptor());
    sIn.writeInt32(codecType);
    sIn.writeInt32(profile);
    sIn.writeInt32(level);
    sIn.writeInt32(width);
    sIn.writeInt32(height);
    sIn.writeInt32(bSecure);
    sIn.writeInt32(bConstrainedProfile);

    remote()->transact(STREAMING_REQ, sIn, &sOut);

    return sOut.readInt32();
}


int BpWFDMMService::isStreamingFeatureEnabled()
{
    Parcel sIn;
    Parcel sOut;

    sIn.writeInterfaceToken(IWFDMMService::getInterfaceDescriptor());

    remote()->transact(CURRENT_DS_SUPPORT, sIn, &sOut);

    return sOut.readInt32();

}

int BpWFDMMService::startStreaming()
{
    Parcel sIn;
    Parcel sOut;

    sIn.writeInterfaceToken(IWFDMMService::getInterfaceDescriptor());

    remote()->transact(START_STREAM, sIn, &sOut);

    return 0;
}

int BpWFDMMService::writeFrame
(
    unsigned long nLen,
    char* pData,
    unsigned long long time_us
)
{
    Parcel sIn;
    Parcel sOut;


    sIn.writeInterfaceToken(IWFDMMService::getInterfaceDescriptor());

    sIn.writeInt64((unsigned long long)time_us);
    sIn.writeInt32((int)nLen);

    if (pData && nLen) {
        sIn.write(pData, nLen);
    }

    remote()->transact(WRITE_FRAME, sIn, &sOut);

    return 0;
}

int BpWFDMMService::writeFrameFd
(
    unsigned long nLen,
    int fd,
    unsigned long long time_us,
    void* pData,
    int nFlags,
    bool bSecure
)
{
    Parcel sIn;
    Parcel sOut;


    sIn.writeInterfaceToken(IWFDMMService::getInterfaceDescriptor());

    sIn.writeInt32((int)nLen);
    sIn.writeInt64((unsigned long long)time_us);

    if(fd)
    {
        sIn.writeFileDescriptor((int)fd);
    }
    sIn.writeInt64((long)pData);
    sIn.writeInt32((int)nFlags);
    sIn.writeInt32((int)bSecure);

    remote()->transact(WRITE_FRAME_FD, sIn, &sOut);

    return 0;
}

int BpWFDMMService::renderFrame
(
    unsigned long long time_us
)
{
    Parcel sIn;
    Parcel sOut;

    sIn.writeInterfaceToken(IWFDMMService::getInterfaceDescriptor());
    sIn.writeInt64((unsigned long long)time_us);
    remote()->transact(RENDER_FRAME, sIn, &sOut);

    return 0;

}

int BpWFDMMService::pauseStreaming()
{
    Parcel sIn;
    Parcel sOut;

    sIn.writeInterfaceToken(IWFDMMService::getInterfaceDescriptor());

    remote()->transact(PAUSE_STREAM, sIn, &sOut);

    return 0;
}

int BpWFDMMService::endStreaming()
{
    Parcel sIn;
    Parcel sOut;

    sIn.writeInterfaceToken(IWFDMMService::getInterfaceDescriptor());

    remote()->transact(END_STREAM, sIn, &sOut);

    return 0;
}

int BpWFDMMService::setWFDMMListener( bool bActive,
            const sp<IWFDMMListener>& WfdMMServiceListener) {
    Parcel sIn, reply;
    sIn.writeInterfaceToken(IWFDMMService::getInterfaceDescriptor());
    int bIntActive = (int)bActive;
    sIn.writeInt32(bIntActive);
    sIn.writeStrongBinder(WfdMMServiceListener->asBinder(WfdMMServiceListener));
    remote()->transact(SET_WFD_LISTENER, sIn, &reply);
    return 0;
}

int BpWFDMMService::flush()
{
    Parcel sIn;
    Parcel sOut;

    sIn.writeInterfaceToken(IWFDMMService::getInterfaceDescriptor());

    remote()->transact(FLUSH_STREAM, sIn, &sOut);

    return 0;
}

int BpWFDMMService::setAppInfo(const char *name, const char *operation)
{
    Parcel data, reply;
    char action[] = "SET_APP_INFO";
    data.writeInterfaceToken(IWFDMMService::getInterfaceDescriptor());
    /*Write Data to parcel -> App name and operation*/
    data.writeCString(action);
    data.writeCString(name);
    data.writeCString(operation);
    remote()->transact(APP_INFO, data, &reply);
    return 0;
}

int BpWFDMMService::isAppWhiteListed()
{
    Parcel data,reply;
    char action[] = "GET_APP_INFO";
    data.writeInterfaceToken(IWFDMMService::getInterfaceDescriptor());
    data.writeCString(action);
    remote()->transact(APP_INFO, data, &reply);
    return reply.readInt32();
}

IMPLEMENT_META_INTERFACE(WFDMMService, "wfd.native.mm.service");

int BnWFDMMService::onTransact
(
    uint32_t code,
    const Parcel& data,
    Parcel* reply,
    uint32_t flags
)
{
    (void)flags;
    int result = 0;
    switch (code)
    {
    case STREAMING_REQ:
        {
               CHECK_INTERFACE(IWFDMMService, data, reply);
               int codecType = data.readInt32();
               int profile = data.readInt32();
               int level = data.readInt32();
               int width = data.readInt32();
               int height = data.readInt32();
               bool bSecure = data.readInt32();
               bool bConstrainedProfile = data.readInt32();
               int result = isStreamingSuccess(codecType,
                                               profile,
                                               level,
                                               width,
                                               height,
                                               bSecure,
                                               bConstrainedProfile);
               reply->writeInt32(result);
        }
           break;
       case START_STREAM:
           startStreaming();
           break;
       case PAUSE_STREAM:
           pauseStreaming();
           break;
       case END_STREAM:
           endStreaming();
           break;
       case WRITE_FRAME:
           {
               CHECK_INTERFACE(IWFDMMService, data, reply);
               unsigned long long nTimeUs = data.readInt64();
               unsigned long nLen = data.readInt32();
               char *pData = NULL;
               if(nLen)
               {

                   pData = (char*)malloc(nLen);
               }
               if(pData)
               {
               data.read(pData, nLen);
               writeFrame(nLen, pData, nTimeUs);
               }
           }
           break;
       case RENDER_FRAME:
           {
               CHECK_INTERFACE(IWFDMMService, data, reply);
               unsigned long long nTimeUs = data.readInt64();
               renderFrame(nTimeUs);
               break;
           }
       case SET_WFD_LISTENER:
           {
               CHECK_INTERFACE(IWFDMMService, data, reply);
               bool bActive = (bool)data.readInt32();
               const sp<IWFDMMListener> WfdMMServiceListener =
                   interface_cast<IWFDMMListener> (data.readStrongBinder());
               setWFDMMListener(bActive,WfdMMServiceListener);
               break;
           }
    case WRITE_FRAME_FD:
        {
            CHECK_INTERFACE(IWFDMMService, data, reply);
            unsigned long nLen = data.readInt32();
            unsigned long long nTimeUs = data.readInt64();

            int fd = (int)data.readFileDescriptor();
            void* pData = (void*)data.readInt64();
            int nFlags = (int)data.readInt32();
            bool bSecure =(bool)data.readInt32();
            writeFrameFd(nLen, fd, nTimeUs, pData, nFlags, bSecure);
            break;
        }
    case CURRENT_DS_SUPPORT:
        {
            CHECK_INTERFACE(IWFDMMService, data, reply);
            int result = isStreamingFeatureEnabled();
            reply->writeInt32(result);
            break;
        }
        case FLUSH_STREAM:
        {
            CHECK_INTERFACE(IWFDMMService, data, reply);
             flush();
            break;
        }
        break;
    case APP_INFO:
        {
            CHECK_INTERFACE(IWFDMMService, data, reply);
            const char *action = data.readCString();
            if(!strcmp(action,"SET_APP_INFO"))
            {
                const char *name = data.readCString();
                const char *operation = data.readCString();
                int result = setAppInfo(name,operation);
                reply->writeInt32(result);
            }
            else if(!strcmp(action,"GET_APP_INFO"))
            {
                int result = isAppWhiteListed();
                reply->writeInt32(result);
            }
        }
        break;
    }
    return result;
}


