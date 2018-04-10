/*==============================================================================
*        @file RTPPacketTransmit.h
*
*  @par DESCRIPTION:
*       This is the implementation of the RTPPacketizer class.
*
*
*  Copyright (c) 2011-2012,2015-2016 Qualcomm Technologies, Inc.
*  All Rights Reserved.
*  Confidential and Proprietary - Qualcomm Technologies, Inc.
==============================================================================*/

/*==============================================================================

                      EDIT HISTORY FOR FILE

  $Header:

================================================================================
*/

//#undef UNICODE
#include "AEEStdDef.h"
#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <sys/socket.h>

class CRTPPacketTransmit
{
public:
    int SendPacket(const uint8 *pData, uint32 nLen);
    int SendPacket(uint8 *pRTPHeader, uint32 nHeaderLen, struct iovec *pIOV, uint32 nNumVectors);
    CRTPPacketTransmit(uint32 portNum, int32 nSocket, uint32 destIP, uint8 bRtpPortTypeUdp);
    ~CRTPPacketTransmit();
private:
    CRTPPacketTransmit();
    CRTPPacketTransmit(const CRTPPacketTransmit&);
    CRTPPacketTransmit& operator=(const CRTPPacketTransmit &);

    bool _success;
    int XmitSocket;
    sockaddr_in faraddr;
#ifdef RTCPONHOSTTEST
    int tempSock;
    sockaddr_in faraddr_rtcp;
#endif
    bool m_bRtpPortTypeUdp;
    bool m_bSockAssigned;

    struct iovec *m_ScratchIovBuffer;
    size_t m_ScratchIovBufferNumEntriesAllocated;
    static const size_t scratchIovBufferInitNumEntries = 32;
};
