#ifndef _CURSORHANDLER_H
#define _CURSORHANDLER_H
/*==============================================================================
*       CursorHandler.h
*
*  DESCRIPTION:
*       Class declaration for CursorHandler. This provides cursor related
*       handling APIs for client to accordingly update its PNG cursor related
*       shape/position update
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

class WFDMMThreads;
class SignalQueue;
namespace wfdUtils
{

typedef struct PngBufferHeaderType
{
    uint32_t nSize; /*Size of the tructure*/
    uint32_t nAllocLen; /*Allocated buffer size of pImageData*/
    uint32_t nFilledLen;/*How much data is actually there in the buffer*/
    uint32_t nTotalImageDataSize; /*Actual data size of pImageData*/
    uint32_t nFlags; /*Any flags, unused right now*/
    uint16_t nCursorImageId; /*Unique ID of the cursor*/
    int16_t nXPos; /*X Position of the cursor*/
    int16_t nYPos; /*Y Position of the cursor*/
    int16_t nHotXPos; /*X Position of the cursor*/
    int16_t nHotYPos; /*Y Position of the cursor*/
    uint8_t* pImageData; /*Actual PNG Image*/
    PngBufferHeaderType();
    ~PngBufferHeaderType();
}PngBufferHeaderType;

typedef enum CursorCallbackEvents{
    CURSOR_HANDLER_NETWORK_ERROR,
    CURSOR_HANDLER_INTERNAL_ERROR,
    CURSOR_HANDLER_UNKNOWN_ERROR,
}CursorCallbackEvents;

class CursorCallbacks
{
    public:

        CursorCallbacks() {}

        virtual ~CursorCallbacks() {}

        /*--------------------------------------------------------------------------
            Inform client of position update
        ----------------------------------------------------------------------------
        */
        virtual int positionUpdate(int16_t xPos, int16_t yPos) = 0;

        /*--------------------------------------------------------------------------
            Inform client that a shape Update has started
        ----------------------------------------------------------------------------
        */
        virtual int shapeStart() = 0;

        /*--------------------------------------------------------------------------
            Inform client that a PNG is ready
        ----------------------------------------------------------------------------
        */
        virtual int shapeUpdate() = 0;

        /*--------------------------------------------------------------------------
            Inform client whether to show Hardware Cursor in next VSYNC or not
        ----------------------------------------------------------------------------
        */
        virtual int showCursorUpdate(bool show) = 0;

        /*--------------------------------------------------------------------------
            Inform client of CursorCallbackEvents
        ----------------------------------------------------------------------------
        */
        virtual int notify(CursorCallbackEvents) = 0;

    private:

        CursorCallbacks& operator = (const CursorCallbacks&);
        CursorCallbacks(const CursorCallbacks&);

};

class CursorHandler
{

    public:

        CursorHandler(int sockFd, CursorCallbacks* cb);

        ~CursorHandler();

        int start();

        int stop();

        int setFreeBuffer(PngBufferHeaderType* pBufferHdr);

        int dequeBuffer(PngBufferHeaderType** ppBufferHdr);

    private:

        int createResources();
        int releaseResources();
        static void dataPullerCb(void* pClientData, unsigned int signal);
        int dataPuller(int signal);
        int processRTPHeader(const uint8_t* pRTPPacket, uint16_t& seqNum);
        int processPositionUpdate(uint16_t pktSize);
        int processShapeStart(uint16_t pktSize);
        int processShapeContinuity(uint16_t pktSize);
        int readNetworkData(size_t len);
        void manageDumps();
        int m_nUDPSocket;
        uint8_t* m_pRTPPacket;
        uint8_t* m_pScratchBuffer;
        int32_t  m_nScratchReadIndex;
        int32_t  m_nScratchWriteIndex;
        CursorCallbacks* m_pClientCallback;
        WFDMMThreads* m_hDataPuller;
        bool m_bPollForData;
        uint16_t m_nCurrSeqNum;
        uint16_t m_nPrevSeqNum;
        uint16_t m_nShapeUpdateSeqNo;
        uint16_t m_nCurrCursorImageId;
        SignalQueue* m_pPNGPayloadQ;
        SignalQueue* m_pClientQ;
        bool m_bIsFirstCursorImageID;
        bool m_bIsFirstShapeUpdate;
        FILE* m_hNetworkIPDump;
        FILE* m_hLocalFileRead;
        long long m_nPngDumpCnt;
};
}
#endif //_CURSORHANDLER_H
