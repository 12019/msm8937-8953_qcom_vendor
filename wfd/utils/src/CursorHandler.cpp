/*==============================================================================
*       CursorHandler.cpp
*
*  DESCRIPTION:
*       Class deinition for CursorHandler. This provides implemtation of the
*       CursorHandler class
*
*  Copyright (c) 2015 Qualcomm Technologies, Inc.
*  All Rights Reserved.
*  Confidential and Proprietary - Qualcomm Technologies, Inc.
*===============================================================================
*/
/*==============================================================================
                                 Edit History
    ================================================================================
       When            Who           Why
    -----------------  ------------  -----------------------------------------------
    10/10/2015                       InitialDraft
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
#include "WFDMMLogs.h"
#include "CursorHandler.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <errno.h>
#include "MMMemory.h"
#include "WFDMMThreads.h"
#include <poll.h>
#include "WFDMMSourceSignalQueue.h"
#include <cstdlib>
#include <cutils/properties.h>

/*========================================================================
 *                             Macros
 *==========================================================================*/

#ifdef LOG_TAG
#undef LOG_TAG
#endif

#define LOG_TAG "CursorHandler"

#ifndef WFD_RETRY_TEMP_FAILURE
/* Used to retry system calls that can return EINTR. */
#define WFD_RETRY_TEMP_FAILURE(exp) ({     \
    typeof (exp) _rc;                      \
    do {                                   \
        _rc = (exp);                       \
    } while (_rc == -1 && errno == EINTR); \
    _rc; })
#endif

#define WFD_UNLIKELY( exp )  (__builtin_expect( !!(exp), 0 ))

#define MAX_PNG_BUFFERS (2 + 1) // 2 for Processing + 1 for client

#define RTP_HEADER_LENGTH 12

#define MAX_MSG_LENGTH 18

#define THREAD_STACK_SIZE 32768

#define DATA_PULLER_POLL_TIMEOUT 100//Spec mentions a 100ms re-transmission

#define MAX_PNG_DATA_SIZE 100*1024//100 KB

#define MSG_TYPE_SIZE 1

#define PACKET_MSG_SIZE 2

#define POSITION_UPDATE_PKT_SIZE 7

#define FIRST_SLURP_SIZE (RTP_HEADER_LENGTH + MSG_TYPE_SIZE  + PACKET_MSG_SIZE)

#define ARRAY_NTOHS(data,i) (((0x00FF & data[i])<<8) | (0x00FF & data[i+1]))

#define ARRAY_NTOHL(data,i) (((0x000000FF & data[i])<<24)| \
                             ((0x000000FF & data[i+1])<<16)| \
                             ((0x000000FF & data[i+2])<<8) | \
                              (0x000000FF & data[i+3]))

namespace wfdUtils
{

/*========================================================================
 *                             Enums, structs, constants, etc.
 *==========================================================================*/

enum {//Constants for data puller thread
    POLL_FOR_DATA,
    STOP_POLLING,
    DATA_PULLER_MAX_SIGNALS,
};

enum ExtensionActionType {
    POSITION_UPDATE  = 0x1,
    SHAPE_START      = 0x2,
    SHAPE_CONTINUE   = 0x3,
};

enum CursorImageType{
    DISABLED_CURSOR  = 0x1,
    MASKED_COLOR_PNG = 0x2,
    COLOR_CURSOR_PNG = 0x3,
};

enum DumpMaskBits{
    NETWORK_IP_DUMP  = 1<<0,
    FILE_READ        = 1<<1,
    PNG_DUMP         = 1<<2,
};

static const char chDumpPropString[] = "persist.debug.wfd.chdump";
static const char nwDumpLoc[] = "/data/media/nwdump.txt";

/*========================================================================
 *                             Helper functions.
 *==========================================================================*/

/*==============================================================================

         FUNCTION:         isGreaterSeqNo

                  DESCRIPTION:
         *//**       @brief         This checks whether the second sequence
                                    number is greater than the first sequence
                                    number

@par     DEPENDENCIES:
                           None
*//*
         PARAMETERS:
*//**       @param         seq1 First sequence number
                           seq2 Second sequnce number

*//*     RETURN VALUE:
*//**       @return
                           true if second sequence number is higher
                           false if second sequence number is lower

@par     SIDE EFFECTS:

*//*==========================================================================*/

static inline bool isGreaterSeqNo(uint16_t seq1, uint16_t seq2)
{
    static const uint16_t ROLLOVER_THRESH = 60000;
    if(seq2 >= seq1)
    {
        return true;
    }
    else if(seq2 < seq1 && (std::abs(seq2-seq1) > ROLLOVER_THRESH))
    {
        return true;
    }
    else
    {
        return false;
    }
}

/*==============================================================================

         FUNCTION:         PngBufferHeaderType Constructor

                  DESCRIPTION:
         *//**       @brief         This is the PngBufferHeaderType class constructor-initializes
                                    the structure members.


@par     DEPENDENCIES:
                           None
*//*
         PARAMETERS:
*//**       @param         None

*//*     RETURN VALUE:
*//**       @return
                           None

@par     SIDE EFFECTS:

*//*==========================================================================*/

PngBufferHeaderType::PngBufferHeaderType():
nSize(sizeof(*this)),
nAllocLen(MAX_PNG_DATA_SIZE),
nFilledLen(0),
nTotalImageDataSize(0),
nFlags(0),
nCursorImageId(0),
nXPos(0),
nYPos(0),
nHotXPos(0),
nHotYPos(0),
pImageData(new uint8_t[MAX_PNG_DATA_SIZE])
{
    WFDMMLOGE1("Creating a PNG Buffer header with a buffer of %d size\n",
        MAX_PNG_DATA_SIZE);
}

/*==============================================================================

         FUNCTION:         ~PngBufferHeaderType

         DESCRIPTION:
*//**       @brief         This is the PngBufferHeaderType structure destructor
*//**

@par     DEPENDENCIES:
                           None
*//*
         PARAMETERS:
*//**       @param         None

*//*     RETURN VALUE:
*//**       @return
                           None

@par     SIDE EFFECTS:

*//*==========================================================================*/
PngBufferHeaderType::~PngBufferHeaderType()
{
    if(pImageData)
    {
        delete pImageData;
        pImageData = NULL;
        WFDMMLOGE("Dtor of PngBufferHeaderType\n");
    }
}


/*==============================================================================

         FUNCTION:         CursorHandler

         DESCRIPTION:
*//**       @brief         This is the CursorHandler class constructor-initializes
                           the class members.
*//**

@par     DEPENDENCIES:
                           None
*//*
         PARAMETERS:
*//**       @param         None

*//*     RETURN VALUE:
*//**       @return
                           None

@par     SIDE EFFECTS:

*//*==========================================================================*/

CursorHandler::CursorHandler(int sockFd,CursorCallbacks* cb):
m_nUDPSocket(sockFd),
m_pRTPPacket(NULL),
m_pScratchBuffer(NULL),
m_nScratchReadIndex(0),
m_nScratchWriteIndex(0),
m_pClientCallback(cb),
m_hDataPuller(NULL),
m_bPollForData(false),
m_nCurrSeqNum(0),
m_nPrevSeqNum(0),
m_nShapeUpdateSeqNo(0),
m_nCurrCursorImageId(0),
m_pPNGPayloadQ(NULL),
m_pClientQ(NULL),
m_bIsFirstCursorImageID(false),
m_bIsFirstShapeUpdate(false),
m_hNetworkIPDump(NULL),
m_hLocalFileRead(NULL),
m_nPngDumpCnt(0)
{
    WFDMMLOGE("CursorHandler ctor\n");
}

/*==============================================================================

         FUNCTION:         ~CursorHandler

         DESCRIPTION:
*//**       @brief         This is the CursorHandler class destructor
*//**

@par     DEPENDENCIES:
                           None
*//*
         PARAMETERS:
*//**       @param         None

*//*     RETURN VALUE:
*//**       @return
                           None

@par     SIDE EFFECTS:

*//*==========================================================================*/

CursorHandler::~CursorHandler()
{
    WFDMMLOGE("CursorHandler dtor\n");
}

/*==============================================================================

         FUNCTION:         Start

         DESCRIPTION:
*//**       @brief         Client needs to call this to start receiving cursor
                                data
*//**
@par     DEPENDENCIES:
                           None
*//*
         PARAMETERS:
*//**       @param         NONE

*//*     RETURN VALUE:
*//**       @return        0 on success 1 on failure

@par     SIDE EFFECTS:
                           None
*//*==========================================================================*/
int CursorHandler::start()
{

    if(createResources())
    {
        WFDMMLOGE("Failed to create Resources, cant't proceed\n");
        return 1;
    }

    return 0;
}

/*==============================================================================

         FUNCTION:         Stop

         DESCRIPTION:
*//**       @brief         Client needs to call this to stop this module
*//**
@par     DEPENDENCIES:
                           None
*//*
         PARAMETERS:
*//**       @param         NONE

*//*     RETURN VALUE:
*//**       @return        0 on success 1 on failure

@par     SIDE EFFECTS:
                           None
*//*==========================================================================*/
int CursorHandler::stop()
{
    releaseResources();
    return 0;
}

/*==============================================================================

         FUNCTION:         setFreeBuffer

         DESCRIPTION:
*//**       @brief         Client uses this to return a PNGBufferHeader back
                           to the CursorHandler module

*//**
@par     DEPENDENCIES:
                           None
*//*
         PARAMETERS:
*//**       @param         pBufferHdr The PngBufferHeaderType to be returned

*//*     RETURN VALUE:
*//**       @return        0 on success 1 on failure

@par     SIDE EFFECTS:
                           None
*//*==========================================================================*/

int CursorHandler::setFreeBuffer(PngBufferHeaderType* pBufferHdr)
{
    if(!pBufferHdr || !m_pPNGPayloadQ)
    {
        WFDMMLOGE1("Invalid state to call %s\n",__FUNCTION__);
        return 1;
    }
    if(m_pPNGPayloadQ->Push(&pBufferHdr,sizeof(pBufferHdr)))
    {
        WFDMMLOGE("Failed to push buffer into PayloadQ\n");
        return 1;
    }
    else
    {
        return 0;
    }
}

/*==============================================================================

         FUNCTION:         dequeBuffer

         DESCRIPTION:
*//**       @brief         Client uses this to get a PNGBufferHeader

*//**
@par     DEPENDENCIES:
                           None
*//*
         PARAMETERS:
*//**       @param         ppBufferHdr The pointer to the PngBufferHeaderType

*//*     RETURN VALUE:
*//**       @return        0 on success 1 on failure

@par     SIDE EFFECTS:
                           None
*//*==========================================================================*/

int CursorHandler::dequeBuffer(PngBufferHeaderType** ppBufferHdr)
{
    if(!ppBufferHdr || !m_pClientQ)
    {
        WFDMMLOGE1("Invalid state to call %s",__FUNCTION__);
        return 1;
    }

    int nRetryIteration = 500;
    OMX_ERRORTYPE result = OMX_ErrorUndefined;
    while (result != OMX_ErrorNone && nRetryIteration--)
    {
        result = m_pClientQ->Pop(ppBufferHdr, sizeof(PngBufferHeaderType*), 10);
        if (result != OMX_ErrorTimeout && result != OMX_ErrorNone)
        {
            WFDMMLOGE("Error in popping from Signal Queue\n");
            return 1;
        }
    }

    WFDMMLOGE1("ppBufferHdr = %x\n",ppBufferHdr);
    return 0;
}

/*==============================================================================

         FUNCTION:         createResources

         DESCRIPTION:
*//**       @brief         Allocates resources for Cursorhandler module
*//**
@par     DEPENDENCIES:
                           None
*//*
         PARAMETERS:
*//**       @param         NONE

*//*     RETURN VALUE:
*//**       @return        0 on success 1 on failure

@par     SIDE EFFECTS:
                           None
*//*==========================================================================*/
int CursorHandler::createResources()
{
    manageDumps();

    if(!m_pRTPPacket)
    {
        m_pRTPPacket = MM_New_Array(uint8_t,(RTP_HEADER_LENGTH +
        MAX_MSG_LENGTH + MAX_PNG_DATA_SIZE));//Munificent!
        if(!m_pRTPPacket)
        {
            WFDMMLOGE("Failed to allocate RTP buffer\n");
            return 1;
        }
    }
    if (!m_pScratchBuffer)
    {
        m_pScratchBuffer = MM_New_Array(uint8_t,1024000);
        if(!m_pScratchBuffer)
        {
            WFDMMLOGE("Failed to allocate RTP scratch buffer\n");
            return 1;
        }
    }
    if(!m_pPNGPayloadQ)
    {
        m_pPNGPayloadQ = MM_New_Args(SignalQueue, (MAX_PNG_BUFFERS,
            sizeof(PngBufferHeaderType*)));
        if(!m_pPNGPayloadQ)
        {
            WFDMMLOGE("Failed to create PNG payload queue\n");
            return 1;
        }
    }

    if(!m_pClientQ)
    {
        m_pClientQ = MM_New_Args(SignalQueue, (MAX_PNG_BUFFERS,
            sizeof(PngBufferHeaderType*)));
        if(!m_pClientQ)
        {
            WFDMMLOGE("Failed to create Client queue\n");
            return 1;
        }
    }

    /*--------------------------------------------------------------------------
       Now we actually create the MAX_PNG_BUFFERS PNGBufferHeaders and push it
       to the payload queue
    ----------------------------------------------------------------------------
    */

    for(int i = 0; i< MAX_PNG_BUFFERS; ++i)
    {
        PngBufferHeaderType * pngBufferHdr = MM_New(PngBufferHeaderType);
        if(!pngBufferHdr)
        {
            WFDMMLOGE("Failed to allocate PNG Buffer Header\n");
            return 1;
        }
        m_pPNGPayloadQ->Push(&pngBufferHdr,sizeof(pngBufferHdr));
    }

    if(!m_hDataPuller)
    {
        m_hDataPuller = MM_New_Args(WFDMMThreads,(DATA_PULLER_MAX_SIGNALS));

        if(!m_hDataPuller)
        {
            WFDMMLOGE("Failed to create data puller thread\n");
            return 1;
        }

        m_hDataPuller->Start(dataPullerCb,0,THREAD_STACK_SIZE,this,"DataPuller");
        m_bPollForData = true;
        m_hDataPuller->SetSignal(POLL_FOR_DATA);
    }
    return 0;
}

/*==============================================================================

         FUNCTION:         releaseResources

         DESCRIPTION:
*//**       @brief         Releases all resources acquired earlier
*//**
@par     DEPENDENCIES:
                           None
*//*
         PARAMETERS:
*//**       @param         NONE

*//*     RETURN VALUE:
*//**       @return        0 on success 1 on failure

@par     SIDE EFFECTS:
                           None
*//*==========================================================================*/
int CursorHandler::releaseResources()
{

    if(m_hDataPuller)
    {
        m_bPollForData = false;
        m_hDataPuller->SetSignal(STOP_POLLING);
        MM_Delete(m_hDataPuller);
        m_hDataPuller = NULL;
    }

    if(m_pRTPPacket)
    {
        MM_Delete_Array(m_pRTPPacket);
        m_pRTPPacket = NULL;
    }
    if (m_pScratchBuffer)
    {
        MM_Delete_Array(m_pScratchBuffer);
        m_pScratchBuffer = NULL;
    }

    if(m_nUDPSocket > 0)
    {
        WFD_RETRY_TEMP_FAILURE(close(m_nUDPSocket));
        m_nUDPSocket = -1;
    }

    PngBufferHeaderType* pngBufferHdr;

    if(m_pClientQ)
    {
        while(m_pClientQ->GetSize())
        {
            WFDMMLOGE1("ClientQ size is %d", m_pClientQ->GetSize());
            pngBufferHdr = NULL;
            m_pClientQ->Pop(&pngBufferHdr,sizeof(pngBufferHdr),0);
            if(pngBufferHdr)
            {
                MM_Delete(pngBufferHdr);
            }
        }
        MM_Delete(m_pClientQ);
        m_pClientQ = NULL;
    }

    if(m_pPNGPayloadQ)
    {
        while(m_pPNGPayloadQ->GetSize())
        {
            WFDMMLOGE1("PNGPayloadQ size is %d", m_pPNGPayloadQ->GetSize());
            pngBufferHdr = NULL;
            m_pPNGPayloadQ->Pop(&pngBufferHdr,sizeof(pngBufferHdr),0);
            if(pngBufferHdr)
            {
                MM_Delete(pngBufferHdr);
            }
        }
        MM_Delete(m_pPNGPayloadQ);
        m_pPNGPayloadQ = NULL;
    }

    if(m_hNetworkIPDump)
    {
        fclose(m_hNetworkIPDump);
        m_hNetworkIPDump = NULL;
    }

    if(m_hLocalFileRead)
    {
        fclose(m_hLocalFileRead);
        m_hLocalFileRead = NULL;
    }

    return 0;
}

/*==============================================================================

         FUNCTION:         readNetworkData

         DESCRIPTION:
*//**       @brief         Tries to read requested size of data into the RTP
                buffer.
*//**
@par     DEPENDENCIES:
                           None
*//*
         PARAMETERS:
*//**       @param         len The amount of max data to be read from network

*//*     RETURN VALUE:
*//**       @return        0 on success 1 on failure

@par     SIDE EFFECTS:
                           None
*//*==========================================================================*/

int CursorHandler::readNetworkData(size_t len)
{
    WFDMMLOGE1("asked to read  %d\n", len);
    if(WFD_UNLIKELY(m_hLocalFileRead))
    {
        fread(m_pRTPPacket,1,len,m_hLocalFileRead);
        return 0;
    }
    else
    {
        /*--------------------------------------------------------------------------
         We are using poll so EAGAIN or EWOULDBLOCK should be out of the
         question, others are genuine errors then, since we already handled EINTR
        --------------------------------------------------------------------------*/
         ssize_t bytesRead = 0;
         int     flags     = MSG_PEEK;
         /* If scratch buffer has the necessary amount of data, read from it
            If it does not, peek the currently available data and replenish it
            Copy over the required data */
         int deficit = len - (m_nScratchWriteIndex - m_nScratchReadIndex + 1);
         if (deficit <= 0)
         {
             //Oh we have plenty of data. Do not bother the sockets
             memcpy(m_pRTPPacket,m_pScratchBuffer+m_nScratchReadIndex,len);
             m_nScratchReadIndex += len;
             return 0;
         }
         bytesRead = WFD_RETRY_TEMP_FAILURE(
                     recvfrom(m_nUDPSocket,static_cast<void*>(m_pRTPPacket),
                              10240,flags,0,0));
         WFDMMLOGE1("Peek returned %d\n", bytesRead);
         flags = 0;
         bytesRead = WFD_RETRY_TEMP_FAILURE(recvfrom(
            m_nUDPSocket,static_cast<void*>(m_pScratchBuffer+m_nScratchWriteIndex),
            bytesRead,flags,0,0));

        if(bytesRead == -1)
        {
            WFDMMLOGE1("recvFrom failed due to %s\n", strerror(errno));
            return 1;
        }
        else if(bytesRead == 0)
        {
            WFDMMLOGE("Peer closed the connection remotely\n");
            return 0;
        }
        else
        {
            m_nScratchWriteIndex += bytesRead;
            /*--------------------------------------------------------------------------
               Unable to read the exact amount of data requested is pretty
               catastrophic for this module since we are dealing with 1 RTP packet
               of a certain size here and after ensuring through poll etc. of data
               availability, it makes no sense to proceed further
            ----------------------------------------------------------------------------
            */
            memcpy(m_pRTPPacket, m_pScratchBuffer+m_nScratchReadIndex, len);
            m_nScratchReadIndex+=len;
            WFDMMLOGE2("read %d bytes of data, read only %d\n",len,bytesRead);
            return 0;;
        }
    }
}

/*=============================================================================

         FUNCTION:          manageDumps

         DESCRIPTION:
*//**       @brief          Helper method to check if dumping is enabled or not
                            and accordingly manage the dump File handles

*//**
@par     DEPENDENCIES:
                            None
*//*
         PARAMETERS:
*//*         @param
                            None

*//*     RETURN VALUE:
*//**       @return
                            None

@par     SIDE EFFECTS:
                            None
*//*=========================================================================*/

void CursorHandler::manageDumps()
{
    char szTemp[PROPERTY_VALUE_MAX];
    int ret = property_get(chDumpPropString,szTemp,NULL);
    if (ret > 0)
    {
        int i = atoi(szTemp);
        WFDMMLOGE2("Found %s property with value %.8x",chDumpPropString,i);
        if(i>0)
        {

            if(i&NETWORK_IP_DUMP)
            {
                WFDMMLOGE("Enabling i/p RTP netowrk dumps");
                m_hNetworkIPDump = fopen(nwDumpLoc,"wb");
                if(!m_hNetworkIPDump)
                {
                    WFDMMLOGE1("Failed to open %s",nwDumpLoc);
                }
            }

            if(i&FILE_READ)
            {
                WFDMMLOGE1("Enabling reading network data from %s",nwDumpLoc);
                m_hLocalFileRead = fopen(nwDumpLoc,"rb");
                if(!m_hLocalFileRead)
                {
                    WFDMMLOGE1("Failed to open %s",nwDumpLoc);
                }
            }

            if(i&PNG_DUMP)
            {
                WFDMMLOGE("Enabling PNG data dumping");
                m_nPngDumpCnt = 1;
            }

        }
    }
}

/*=============================================================================

         FUNCTION:          dataPullerCb

         DESCRIPTION:
*//**       @brief          Entry point for dataPuller thread

*//**
@par     DEPENDENCIES:
                            None
*//*
         PARAMETERS:
*//**       @param          None

*//*     RETURN VALUE:
*//**       @return         None

@par     SIDE EFFECTS:
                            None
*//*=========================================================================*/

void CursorHandler::dataPullerCb(void* pClientData, unsigned int signal)
{
    if(!pClientData)
    {
        WFDMMLOGE1("Invalid client data in Thread callback %s\n",
            __FUNCTION__);
        return;
    }
    static_cast<CursorHandler*>(pClientData)->dataPuller(signal);
}

/*=============================================================================

         FUNCTION:          dataPuller

         DESCRIPTION:
*//**       @brief          This function is where the dataPuller thread
                            keeps on polling for data from network. Once it
                            receives data, it processes the RTP message and
                            then based on the payload received, either intimates
                            the client of a position update in the cursor or of
                            a shape update, in which case it will signal the
                            client of a shape update once it is done with the
                            processing of the PNG payload.

*//**
@par     DEPENDENCIES:
                            None
*//*
         PARAMETERS:
*//**       @param          None

*//*     RETURN VALUE:
*//**       @return         0 on success 1 on failure

@par     SIDE EFFECTS:
                            None
*//*=========================================================================*/

int CursorHandler::dataPuller(int signal)
{

    if(!m_pPNGPayloadQ || !m_pClientCallback || !m_pRTPPacket)
    {
        WFDMMLOGE("Not enough stuff to proceed\n");
        return 1;
    }

    switch(signal)
    {
        case POLL_FOR_DATA:
        {
            struct pollfd pfds[1];
            pfds[0].fd = m_nUDPSocket;
            pfds[0].events = POLLIN;
            pfds[0].revents = 0;

            struct timeval tv;
            tv.tv_sec = 15;
            tv.tv_usec = 1000*1000;//1 sec
            setsockopt((int)m_nUDPSocket, SOL_SOCKET, SO_RCVTIMEO,
                        (struct timeval *)&tv, (socklen_t)sizeof(struct timeval));

            while(m_bPollForData)
            {
                pfds[0].revents = 0;
                int ret =
                WFD_RETRY_TEMP_FAILURE(poll(pfds,1,DATA_PULLER_POLL_TIMEOUT));
                if(ret < 0)
                {
                    WFDMMLOGE1("Failed to poll for data due to %s\n",
                        strerror(errno));
                    return 1;
                }

                if (pfds[0].revents & POLLIN)
                {
                    /*--------------------------------------------------------------------------
                       This is the strategy to read one packet in 2 rounds of
                       recvFrom, given that we know that since poll has
                       confirmed availability of atleast one UDP packet:

                       1. All messages are encapsulated in a RTP header, read it

                       2. Read the message type and the packet message size

                       3. This ends the first round of reading

                       4. Then based on the message type, we read further data

                       5. The second slurp size will be based on the packet

                    ----------------------------------------------------------------------------
                    */
                    if(readNetworkData(FIRST_SLURP_SIZE))
                    {
                        m_pClientCallback->notify(CURSOR_HANDLER_NETWORK_ERROR);
                        return 1;
                    }
                    /*--------------------------------------------------------------------------
                       Now that we have the basic data to proceed, here's what we
                       do:

                       1. Validate the RTP header data for sanity

                       2. Read the message type

                       3. Based on this, proceed to the 3 types of actions
                    ----------------------------------------------------------------------------
                    */
                    m_nPrevSeqNum = m_nCurrSeqNum;
                    if(processRTPHeader(m_pRTPPacket,m_nCurrSeqNum))
                    {
                        WFDMMLOGE("Bogus RTP Packet\n");
                        m_pClientCallback->notify(CURSOR_HANDLER_UNKNOWN_ERROR);
                        return 1;
                    }
                    else
                    {
                        /*--------------------------------------------------------------------------
                           Now that we have sanitized the RTP header, we move ahead to
                           the message type
                        ----------------------------------------------------------------------------
                        */
                        int index = 12;//12 bytes of RTP header have been parsed
                        /*--------------------------------------------------------------------------
                           Now we move on to parsing the actual message.
                           Since we have read in the action type and the
                           packet size, lets extract those first.

                           1. The first byte is the Action Type

                           2. The next 2 bytes are the packet size in
                           network order

                           3. Now based on each action type, we process the
                           remaining data. Stay tuned for the remaining
                           story in each function.
                        ----------------------------------------------------------------------------
                        */
                        ExtensionActionType action =
                        static_cast<ExtensionActionType>(m_pRTPPacket[index++]);
                        uint16_t pktSize = ARRAY_NTOHS(m_pRTPPacket,index);
                        switch (action)
                        {
                            case POSITION_UPDATE:
                                if(processPositionUpdate(pktSize))
                                {
                                    m_pClientCallback->notify(CURSOR_HANDLER_INTERNAL_ERROR);
                                }
                                break;
                            case SHAPE_START:
                                if(processShapeStart(pktSize))
                                {
                                    m_pClientCallback->notify(CURSOR_HANDLER_INTERNAL_ERROR);
                                }
                                break;
                            case SHAPE_CONTINUE:
                                if(processShapeContinuity(pktSize))
                                {
                                    m_pClientCallback->notify(CURSOR_HANDLER_INTERNAL_ERROR);
                                }
                                break;
                            default:
                                WFDMMLOGE("Really??\n");
                                return 1;
                        }
                    }
                }

            }
        }
        break;

        case STOP_POLLING:
            {
                WFDMMLOGE("Received STOP_POLLING signal\n");
            }
        break;

        default:
            WFDMMLOGE("Really??\n");
    }
    return 0;
}

/*=============================================================================

         FUNCTION:          processRTPHeader

         DESCRIPTION:
*//**       @brief          This function validates the RTP header received as
                            per the restrictions imposed by the spec.

*//**
@par     DEPENDENCIES:
                            None
*//*
         PARAMETERS:
*//**       @param          None

*//*     RETURN VALUE:
*//**       @return         0 on success 1 on failure

@par     SIDE EFFECTS:
                            None
*//*=========================================================================*/

int CursorHandler::processRTPHeader(const uint8_t* pRTPPacket,
                                                    uint16_t& seqNum)
{
    if(!pRTPPacket)
    {
        WFDMMLOGE1("Invalid argumensts for %s\n",__FUNCTION__);
        return 1;
    }

    /*--------------------------------------------------------------------------
       For this protocol the RTP header should have these values

       1. V Version is to be 2

       2. P Padding is to be 0

       3. X No Extensions

       4. CC No CSRC

       5. M No Marker is to be used

       6. PT Payload Type is to be 0

       7. Timestamp is not used and 0

       8. SSRC identifier not used and 0

       Now start sanitizing against these.
    ----------------------------------------------------------------------------
    */


    //first two bits of first byte gives version,current version of RTP is 2
    int versionNumber = pRTPPacket[0] >> 6;
    if( versionNumber != 2)
    {
        WFDMMLOGE1("Version number %d is incorrect in RTP Packet\n",versionNumber);
        return 1;
    }

    //third bit of first byte indicates the presence of padding
    int padding = pRTPPacket[0]& 0x20 ;
    if(padding != 0)
    {
        WFDMMLOGE1("Padding %d is incorrect in RTP Packet\n",padding);
        return 1;
    }

    //4th bit of the first byte indicates the presence of extension bit
    int extension = pRTPPacket[0]& 0x10;
    if(extension != 0)
    {
        WFDMMLOGE1("Extension %d is incorrect in RTP Packet\n",extension);
        return 1;
    }

    //last 4 bits of first octet gives the number of 4 byte CSRC's
    int numCSRCs = pRTPPacket[0] & 0x0F;
    if(numCSRCs != 0)
    {
        WFDMMLOGE1("CSRC %d is incorrect in RTP Packet\n",numCSRCs);
        return 1;
    }

    //second byte's first bit is "marker" bit
    int marker = pRTPPacket[1] >> 7;
    if(marker != 0)
    {
        WFDMMLOGE1("Marker %d is incorrect in RTP header\n",marker);
        return 1;
    }

    //second byte's last seven bits gives the "payload type"
    int payloadType = pRTPPacket[1] &0x7F;
    if(payloadType != 0)
    {
        WFDMMLOGE1("Payload type %d is incorrect in RTP header\n",payloadType);
        return 1;
    }

    //5 to 8 bytes give time stamp in network byte order
    int timeStamp = ARRAY_NTOHL(pRTPPacket, 4);
    if(timeStamp != 0)
    {
        WFDMMLOGE1("Time stamp %d is incorrect in RTP header\n",timeStamp);
        return 1;
    }

    // 9 to 12 bytes give SSRC value in network byte order
    int ssrc = ARRAY_NTOHL(pRTPPacket,8);
    if(ssrc != 0)
    {
        WFDMMLOGE1("SSRC %d is incorrect in RTP header\n",ssrc);
        return 1;
    }

    //3rd and 4th bytes has sequence number(in network byte order i.e. big  endian)
    seqNum = ARRAY_NTOHS(pRTPPacket,2);
    WFDMMLOGE1("Received Sequence Number %u\n",seqNum);

    return 0;
}

/*=============================================================================

         FUNCTION:          processPositionUpdate

         DESCRIPTION:
*//**       @brief          This function process a position update message

*//**
@par     DEPENDENCIES:
                            None
*//*
         PARAMETERS:
*//**       @param          None

*//*     RETURN VALUE:
*//**       @return         0 on success 1 on failure

@par     SIDE EFFECTS:
                            None
*//*=========================================================================*/

int CursorHandler::processPositionUpdate(uint16_t pktSize)
{
    if(!m_pPNGPayloadQ || !m_pClientCallback || !m_pRTPPacket)
    {
        WFDMMLOGE("Not enough stuff to proceed\n");
        return 1;
    }

    /*--------------------------------------------------------------------------
        And the story continues...

       Now after having read the first set of the message, we read remaining
       part.

       1. Validate the packet size

       2. We know the packet size. Now we have already read off the first byte
         of the packet for the action type and the next 2 bytes for the packet
         size. So we need to read off the Packet Size -3 bytes more to get the
         complete UDP message in the next recvFrom
    ----------------------------------------------------------------------------
    */
    if(pktSize != POSITION_UPDATE_PKT_SIZE)
    {
        WFDMMLOGE1("Invalid packet size %d received for position update\n",pktSize);
        return 1;
    }

    size_t readLen = pktSize - (MSG_TYPE_SIZE + PACKET_MSG_SIZE);

    if(readNetworkData(readLen))
    {
        return 1;
    }


    if(WFD_UNLIKELY(!m_bIsFirstShapeUpdate))
    {
        m_nShapeUpdateSeqNo = m_nCurrSeqNum;
        m_bIsFirstShapeUpdate = true;
    }

    /*--------------------------------------------------------------------------
       1. Check if we need to inform client of position update after seqNum
       validation

       1. 1st 2 bytes are the X Pos

       2. Next 2 bytes are the Y pos
    ----------------------------------------------------------------------------
    */
    if(!isGreaterSeqNo(m_nPrevSeqNum,m_nCurrSeqNum))
    {
        WFDMMLOGE2("Ignoring position update because %u is older than %u\n",
            m_nCurrSeqNum,m_nPrevSeqNum);
        return 0;
    }

    if(!isGreaterSeqNo(m_nShapeUpdateSeqNo,m_nCurrSeqNum))
    {
        WFDMMLOGE2("Ignoring position update because %u shape update is older than %u\n",
            m_nCurrSeqNum,m_nShapeUpdateSeqNo);
        return 0;
    }

    int16_t xPos = ARRAY_NTOHS(m_pRTPPacket,0);
    int16_t yPos = ARRAY_NTOHS(m_pRTPPacket,2);
    m_pClientCallback->positionUpdate(xPos,yPos);

    return 0;
}

/*=============================================================================

         FUNCTION:          processShapeStart

         DESCRIPTION:
*//**       @brief          This function process a shape start message

*//**
@par     DEPENDENCIES:
                            None
*//*
         PARAMETERS:
*//**       @param          None

*//*     RETURN VALUE:
*//**       @return         0 on success 1 on failure

@par     SIDE EFFECTS:
                            None
*//*=========================================================================*/

int CursorHandler::processShapeStart(uint16_t pktSize)
{
    if(!m_pPNGPayloadQ || !m_pClientCallback || !m_pRTPPacket)
    {
        WFDMMLOGE("Not enough stuff to proceed\n");
        return 1;
    }

    /*--------------------------------------------------------------------------
       And the story continues...

       Now after having read the first set of the message, we read remaining
       part.

       1. We check for the simple use case whether this is the only shape
       start message. If yes, just put the PNG payload and signal client.

       2. If we know that the data will come then we wait for the shape update
       payloads.
    ----------------------------------------------------------------------------
    */
    size_t readLen = pktSize - (MSG_TYPE_SIZE + PACKET_MSG_SIZE);

    if(readNetworkData(readLen))
    {
        return 1;
    }

    /*--------------------------------------------------------------------------
       1. First 4 bytes are the total Image Data Size

       2. Next 2 bytes are the Cursor Image ID

       3. Next 2 bytes are the X Pos

       4. Next 2 bytes are ther Y Pos

       5. Next 1 byte is the Cursor Image Type

       6. Next 2 bytes are the HotSpot X

       7. Next 2 bytes are the HotSpot Y
    ----------------------------------------------------------------------------
    */
    int index = 0;//track on the rtp buffer
    uint32_t totalImagedataSize = ARRAY_NTOHL(m_pRTPPacket,0);
    index+=4;
    WFDMMLOGE1("Total Image data size is %u\n",totalImagedataSize);

    uint16_t cursorImageID = ARRAY_NTOHS(m_pRTPPacket,4);
    index+=2;
    WFDMMLOGE1("Cursor image ID is %u\n",cursorImageID);

    if(WFD_UNLIKELY(!m_bIsFirstCursorImageID))
    {
        m_nCurrCursorImageId = cursorImageID;
        m_bIsFirstCursorImageID = true;
    }

    if(cursorImageID < m_nCurrCursorImageId)
    {
        WFDMMLOGE2("Received Cursor ID %u is stale compared to %u\n"\
        "Hence ignoring the shape start message",
            cursorImageID, m_nCurrCursorImageId);
        return 0;
    }
    else
    {
        m_nCurrCursorImageId = cursorImageID;
    }

    m_nShapeUpdateSeqNo = m_nCurrSeqNum;

    int16_t xPos = ARRAY_NTOHS(m_pRTPPacket,6);
    index+=2;
    WFDMMLOGE1("X pos is %d\n",xPos);

    int16_t yPos = ARRAY_NTOHS(m_pRTPPacket,8);
    index+=2;
    WFDMMLOGE1("Y pos is %d\n",yPos);

    uint8_t cursorImageType = m_pRTPPacket[10];
    index+=1;
    WFDMMLOGE1("cursorImageType is %.2x\n",cursorImageType);

    if(cursorImageType == DISABLED_CURSOR)
    {
        m_pClientCallback->showCursorUpdate(false);
        return 0;
    }
    else
    {
        m_pClientCallback->showCursorUpdate(true);
    }

    uint16_t hotSpotX = ARRAY_NTOHS(m_pRTPPacket,11);
    index+=2;
    WFDMMLOGE1("hostSpotX is %u\n",hotSpotX);

    uint16_t hotSpotY = ARRAY_NTOHS(m_pRTPPacket,13);
    index+=2;
    WFDMMLOGE1("hostSpotY is %u\n",hotSpotX);

    /*--------------------------------------------------------------------------
       1. Pop off a buffer from the PNG payload Q
    ----------------------------------------------------------------------------
    */
    PngBufferHeaderType* pngBufferHdr = NULL;

    /*--------------------------------------------------------------------------
       All this rigmarole is to address a deficiency in SignalQ
       implementation. Grump :( :(
    ----------------------------------------------------------------------------
    */
    int nRetryIteration = 500;
    OMX_ERRORTYPE result = OMX_ErrorUndefined;
    while (result != OMX_ErrorNone && nRetryIteration--)
    {
        result = m_pPNGPayloadQ->Pop(&pngBufferHdr, sizeof(pngBufferHdr), 10);
        if (result != OMX_ErrorTimeout && result != OMX_ErrorNone)
        {
            WFDMMLOGE("Error in popping from Signal Queue");
            return 1;
        }
    }

    if(!pngBufferHdr)
    {
        WFDMMLOGE("Failed to pop off buffer from Payload Queue");
        return 1;
    }
    /*--------------------------------------------------------------------------
       2. Populate it with relevant stuff
    ----------------------------------------------------------------------------
    */
    pngBufferHdr->nTotalImageDataSize = totalImagedataSize;
    pngBufferHdr->nCursorImageId = cursorImageID;
    pngBufferHdr->nXPos = xPos;
    pngBufferHdr->nYPos = yPos;
    pngBufferHdr->nHotXPos = hotSpotX;
    pngBufferHdr->nHotYPos = hotSpotY;

    /*--------------------------------------------------------------------------
       3. Populate the actual payload so that client can use this
       buffer independently
    ----------------------------------------------------------------------------
    */
    memcpy(pngBufferHdr->pImageData,m_pRTPPacket +index,readLen-index);

    /*--------------------------------------------------------------------------
       4. Increase the amount of data that filledLen reflects
    ----------------------------------------------------------------------------
    */
    pngBufferHdr->nFilledLen= readLen - index;

    /*--------------------------------------------------------------------------
       from the above calculation total 15 bytes + the earlier read 3 ( 1 byte
       of msg size and 2 bytes of packet size) we have 18 bytes. So if the
       total image data size is = packet size -18, then the entire image is
       contained in this message itself and no more shape continuitites.
    ----------------------------------------------------------------------------
    */

    if(static_cast<uint32_t>(pktSize - 18) == totalImagedataSize)
    {
        /*--------------------------------------------------------------------------
           This is the special case where the shape start message is self
           contained and no more shape continuities will follow.
           Push it back to the ClinetQ for later client access
        ----------------------------------------------------------------------------
        */
        m_pClientQ->Push(&pngBufferHdr,sizeof(pngBufferHdr));
        m_pClientCallback->shapeUpdate();
        if(WFD_UNLIKELY(m_nPngDumpCnt))
        {
            char dumpFilename[PATH_MAX];
            snprintf(dumpFilename, sizeof(dumpFilename),
                "/data/media/%u_%lld_.png",cursorImageID,m_nPngDumpCnt++);
            FILE* fp = fopen(dumpFilename, "wb");
            if(fp)
            {
                fwrite(static_cast<void*>(pngBufferHdr->pImageData)
                    ,sizeof(*(pngBufferHdr->pImageData)),
                    pngBufferHdr->nTotalImageDataSize,fp);
                fclose(fp);
            }
        }
    }
    else
    {
        /*--------------------------------------------------------------------------
           Inform client that a new shape has arrived and push buffer to
           PayloadQ
        ----------------------------------------------------------------------------
        */
        m_pPNGPayloadQ->Push(&pngBufferHdr,sizeof(pngBufferHdr));
        m_pClientCallback->shapeStart();
    }

    return 0;
}

/*=============================================================================

         FUNCTION:          processShapeContinuity

         DESCRIPTION:
*//**       @brief          This function process a shape continuity message.

*//**
@par     DEPENDENCIES:
                            None
*//*
         PARAMETERS:
*//**       @param          None

*//*     RETURN VALUE:
*//**       @return         0 on success 1 on failure

@par     SIDE EFFECTS:
                            None
*//*=========================================================================*/
int CursorHandler::processShapeContinuity(uint16_t pktSize)
{

    if(!m_pPNGPayloadQ || !m_pClientCallback || !m_pRTPPacket)
    {
        WFDMMLOGE("Not enough stuff to proceed\n");
        return 1;
    }

    /*--------------------------------------------------------------------------
       And the story continues (all pun intended)...

       Now after having read the first set of the message, we read remaining
       part.

       1. Not much to be done here. Just read the data and the payload and copy
       it to the pngbuffer that has the shape data for the cursorImageId
    ----------------------------------------------------------------------------
    */

    size_t readLen = pktSize - (MSG_TYPE_SIZE + PACKET_MSG_SIZE);

    if(readNetworkData(readLen))
    {
        return 1;
    }

    /*--------------------------------------------------------------------------
       1. First 4 bytes are the total Image Data Size

       2. Next 2 bytes are the Cursor Image ID

       3. Next 4 bytes are the packet payload offset

       4. Remaining is the image payload data
    ----------------------------------------------------------------------------
    */

    int index = 0;//track on the rtp buffer

    uint32_t totalImagedataSize = ARRAY_NTOHL(m_pRTPPacket,0);
    index+=4;
    WFDMMLOGE1("Total Image data size is %u\n",totalImagedataSize);

    uint16_t cursorImageID = ARRAY_NTOHS(m_pRTPPacket,4);
    index+=2;
    WFDMMLOGE1("Cursor image ID is %u\n",cursorImageID);

    if(m_nCurrCursorImageId != cursorImageID)
    {
        WFDMMLOGE("WARNING: shape continuity of another Cusror ID received");
    }

    m_nShapeUpdateSeqNo = m_nCurrSeqNum;

    int32_t packetPayloadOffset = ARRAY_NTOHL(m_pRTPPacket,6);
    index+=4;
    WFDMMLOGE1("Packet payload offset is %d\n",packetPayloadOffset);

    /*--------------------------------------------------------------------------
       1. Pop off a buffer from the PNG payload Q
    ----------------------------------------------------------------------------
    */
    PngBufferHeaderType* pngBufferHdr = NULL;

    while(m_pPNGPayloadQ->GetSize())
    {
        pngBufferHdr = NULL;
        m_pPNGPayloadQ->Pop(&pngBufferHdr, sizeof(pngBufferHdr), 0);
        if(pngBufferHdr)
        {
            if(pngBufferHdr->nCursorImageId == cursorImageID)
            {
                /*-------------------------------------------------------------
                Ensure that we are uploading payload in correct buffer
                ---------------------------------------------------------------
                */
                WFDMMLOGE1("Found a PNG buffer for CursorImage ID %u",
                    cursorImageID);
                break;
            }
            else
            {
                /*-------------------------------------------------------------
                Don't forget to push the buffer back at any cost
                ---------------------------------------------------------------
                */
                m_pPNGPayloadQ->Push(&pngBufferHdr,sizeof(pngBufferHdr));
            }
        }
    }

    /*--------------------------------------------------------------------------
       Now we have a PNG Buffer header to work on.
    ----------------------------------------------------------------------------
    */
    if(pngBufferHdr)
    {
        /*--------------------------------------------------------------------------
           Do a sanity check for offsets
        ----------------------------------------------------------------------------
        */
        memcpy(pngBufferHdr->pImageData + packetPayloadOffset,
            m_pRTPPacket +index,readLen-index);
        pngBufferHdr->nFilledLen+= readLen-index;
        /*--------------------------------------------------------------------------
           Now check if the filled length has equalled Total image size. If yes
           then we have a full payload and notify client of a shape update.
        ----------------------------------------------------------------------------
        */
        if(pngBufferHdr->nFilledLen == totalImagedataSize)
        {
            m_pClientQ->Push(&pngBufferHdr,sizeof(pngBufferHdr));
            m_pClientCallback->shapeUpdate();
            if(WFD_UNLIKELY(m_nPngDumpCnt))
            {
                char dumpFilename[PATH_MAX];
                snprintf(dumpFilename, sizeof(dumpFilename),
                    "/data/media/%u_%lld_.png",cursorImageID,m_nPngDumpCnt++);
                FILE* fp = fopen(dumpFilename, "wb");
                if(fp)
                {
                    fwrite(static_cast<void*>(pngBufferHdr->pImageData)
                        ,sizeof(*(pngBufferHdr->pImageData)),
                        pngBufferHdr->nTotalImageDataSize,fp);
                    fclose(fp);
                }
            }
        }
        else
        {
            /*-----------------------------------------------------------------
               Else we still need to work on the buffer, push it back to the
               payloadQ
            -------------------------------------------------------------------
            */
            m_pPNGPayloadQ->Push(&pngBufferHdr,sizeof(pngBufferHdr));
        }
    }
    return 0;
}

}
