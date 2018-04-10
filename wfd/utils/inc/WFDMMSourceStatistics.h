#ifndef __WFDMMSOURCESTATISTICS_H
#define __WFDMMSOURCESTATISTICS_H
/*==============================================================================
*       WFDMMSourceStatistics.h
*
*  DESCRIPTION:
*       Class declaration for WFDMMSourceStatistics. This provides video
        statisctics for WFD MM source module
*
*  Copyright (c) 2014-2015 Qualcomm Technologies, Inc.
*  All Rights Reserved.
*  Confidential and Proprietary - Qualcomm Technologies, Inc.
*===============================================================================
*/
/*==============================================================================
                             Edit History
================================================================================
   When            Who           Why
-----------------  ------------  -----------------------------------------------
05/06/2014                     InitialDraft
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
#include "OMX_Core.h"

/*--------------------------------------------------------------
 Publicly visible constants and enums
----------------------------------------------------------------
*/
namespace WFDMMSourceStats
{
    typedef enum WFD_MM_SRC_STAT_KEY
    {
        TOT_DISP_FRAMES,
        FRAMES_DELIVERED,
        FRAMES_ENCODED,
        FRAMES_DROPPED,
        FRAMES_ENCRYPTED,
        FRAMES_MUXED,
        MIN_ENCODE_TIME,
        MAX_ENCODE_TIME,
        AVG_ENCODE_TIME,
        MIN_ENCRYPT_TIME,
        MAX_ENCRYPT_TIME,
        AVG_ENCRYPT_TIME,
        MIN_MUX_TIME,
        MAX_MUX_TIME,
        AVG_MUX_TIME,
        MIN_RDTRIP_TIME,
        MAX_RDTRIP_TIME,
        AVG_RDTRIP_TIME,
        WFD_MM_SRC_MAX_STAT_KEYS,
    }WFD_MM_SRC_STAT_KEY;
}

class WFDMMSourceStatistics
{

    public:

        /*--------------------------------------------------------------
         Interface
        ----------------------------------------------------------------
        */

        WFDMMSourceStatistics();
        ~WFDMMSourceStatistics();
        int setup();
        void recordDisplayFrame();
        void recordEncIP(const OMX_BUFFERHEADERTYPE*);
        void recordEncOP(const OMX_BUFFERHEADERTYPE*);
        void recordVideoEncryptStat(const OMX_BUFFERHEADERTYPE*);
        void recordMuxStat(const OMX_BUFFERHEADERTYPE*);
        void printStatistics()const;
        void resetStats();
        OMX_S64* getStatistics()const;

    private:

        /*--------------------------------------------------------------
         Member functions
        ----------------------------------------------------------------
        */
        int createResources();

        int releaseResources();

        /*--------------------------------------------------------------
         Member variables
        ----------------------------------------------------------------
        */

        typedef struct stat_slot_info_t
        {
            OMX_TICKS nKey;
            OMX_TICKS nEncDelTime;
            OMX_S64   nFrameNo;
            OMX_BOOL  bFrameEncoded;

            stat_slot_info_t()
            {
                nKey          = 0;
                nEncDelTime   = 0;
                nFrameNo      = 0;
                bFrameEncoded = OMX_FALSE;
            }
        }statSlotInfo;

        struct wfd_prof_stats
        {
            mutable OMX_S64     nCurrentDisplayFrames;//only for FPS duration
            OMX_S64     nTotalDisplayFrames;
            mutable OMX_S64     nCurrentFramesDelivered;//only for FPS duration
            OMX_S64     nFramesDelivered;//For entire session
            mutable OMX_S64     nCurrentFramesEncoded;//only for FPS duration
            OMX_S64     nFramesEncoded;//For entire session
            OMX_S64     nFramesDropped;
            OMX_S64     nFramesEncrypted;
            OMX_S64     nFramesMuxed;
            OMX_TICKS   nMinEncodeTime;
            OMX_TICKS   nMaxEncodeTime;
            OMX_TICKS   nTotEncodeTime;
            OMX_TICKS   nMinEncryptTime;
            OMX_TICKS   nMaxEncryptTime;
            OMX_TICKS   nTotEncryptTime;
            OMX_TICKS   nMinMuxTime;
            OMX_TICKS   nMaxMuxTime;
            OMX_TICKS   nTotMuxTime;
            OMX_TICKS   nMinRdTripTime;
            OMX_TICKS   nMaxRdTripTime;
            OMX_TICKS   nTotRdTripTime;
            mutable OMX_TICKS   nLastPrintStatTime;
        };

        statSlotInfo*                         mStatSlots;
        int                                   mStatSlotHead;
        int                                   mStatSlotTail;
        wfd_prof_stats                        mWfdProfStats;
        bool                                  mProfilingEnabled;
};
#endif /*__WFDMMSOURCESTATISTICS_H*/
