#ifndef __WFDMMSINKSTATISTICS_H
#define  __WFDMMSINKSTATISTICS_H
/*==============================================================================
*       WFDMMSinkStatistics.h
*
*  DESCRIPTION:
*       Class declaration for WFDMMSinkStatistics. This provides audio video
*       statisctics for WFD mm sink module
*
*  Copyright (c) 2013-2015 Qualcomm Technologies, Inc.
*  All Rights Reserved.
*  Confidential and Proprietary - Qualcomm Technologies, Inc.
*===============================================================================
*/
/*==============================================================================
                             Edit History
================================================================================
   When            Who           Why
-----------------  ------------  -----------------------------------------------
03/25/2013         SK            InitialDraft
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

typedef struct rendererStats
{
    bool nIsVideoFrame;
    bool nIsLate;
    int64_t nTimeDecider;
    uint64_t nSessionTime;
    uint64_t nArrivalTime;
    uint32_t nAudioTrackLatency;
    uint32_t nAddedAudioTrackLatency;
}rendererStatsType;

class WFDMMSinkStatistics
{
public:

    /*--------------------------------------------------------------
     Interface
    ----------------------------------------------------------------
    */

    WFDMMSinkStatistics();
    ~WFDMMSinkStatistics();
    int setup();
    void PrintStatistics()const;
    void UpdateDropsOrLateby(const rendererStatsType& nRendererObj);
    void recordMediaSrcIP(const OMX_BUFFERHEADERTYPE*);
    void recordDecryptStatistics(const OMX_BUFFERHEADERTYPE*);
    void recordDecIP(const OMX_BUFFERHEADERTYPE*);
    void recordDecOP(const OMX_BUFFERHEADERTYPE*);
    void recordVideoFrameDrop(const OMX_BUFFERHEADERTYPE*, bool);
    void recordSchedDelay(const OMX_BUFFERHEADERTYPE*);
    void recordDisplayIP(const OMX_BUFFERHEADERTYPE*);
    void recordDisplayOP(const OMX_BUFFERHEADERTYPE*);
    void resetStats();

private:

    //decoder statistics
    uint64_t mVideoAverageDecodeTimeMs;
    uint64_t mAudioAverageDecodeTimeMs;
    uint64_t mPeakVideoDecodeTimeMs;
    uint64_t mPeakAudioDecodeTimeMs;
    uint64_t mMinVideoDecodeTimeMs;
    uint64_t mMinAudioDecodeTimeMs;
    uint64_t mNumberOfAudioFrames;
    uint64_t mVideoFramesDecoded;

    int64_t mSessionVideoFrames;
    mutable int64_t mCurrentVideoIPFrames;
    mutable int64_t mCurrentVideoOPFrames;
    int32_t mDecoderDrops;

    //render statistics

    int64_t mTotalVideoFrames;
    int64_t mTotalAudioFrames;
    int64_t mVideoArrivalTimeStamp;
    int64_t mAudioArrivalTimeStamp;

    int32_t mTotalVideoFramesDropped;
    int32_t mTotalAudioFramesDropped;

    int64_t mMaxAudioLateByMs;
    int64_t mMaxVideoLateByMs;

    int64_t mAverageAudioLateByMs;
    int64_t mAverageVideoLateByMs;

    int32_t mTotalVideoFramesDroppedAfterFiveSeconds;
    int32_t mTotalAudioFramesDroppedAfterFiveSeconds;

    int64_t mMaxDiffInArrivalOfSuccessiveAudioFrames;
    int64_t mMaxDiffInArrivalOfSuccessiveVideoFrames;

    int64_t mAverageVideoFrameArrival;
    int64_t mAverageAudioFrameArrival;

    int64_t mTotalVideoFramesArrived;
    int64_t mTotalAudioFramesArrived;

    bool mFirstVideoFrame;
    bool mFirstAudioFrame;
    uint64_t mFirstVideoFrameArrivalTime;
    uint64_t mFirstAudioFrameArrivalTime;

    uint32_t mAudioTrackLatency;
    uint32_t mAddedAudioTrackLatency;

    //Decrypt Statistics
    uint64_t mVideoAvgDecryptTimeMs;
    uint64_t mVideoMinDecryptTimeMs;
    uint64_t mVideoMaxDecryptTimeMs;
    uint64_t mNumberOfVideoFramesDecrypted;

    //FPS Statistics
    mutable uint64_t mLastPrintStatTime;

    /*--------------------------------------------------------------
     Member functions
    ----------------------------------------------------------------
    */

    void initData();
    void releaseResources();
    /*--------------------------------------------------------------
     Member variables
    ----------------------------------------------------------------
    */

    typedef struct statSlotInfo
    {
        OMX_TICKS nKey;
        OMX_TICKS nMediaSrcRcvTime;
        OMX_TICKS nDecryptTime;
        OMX_TICKS nDecDelTime;
        OMX_S64   nFrameNo;
        OMX_BOOL  bFrameDecoded;

        statSlotInfo()
        {
            nKey             = 0;
            nMediaSrcRcvTime = 0;
            nDecryptTime     = 0;
            nDecDelTime      = 0;
            nFrameNo         = 0;
            bFrameDecoded    = OMX_FALSE;
        }
    }statSlotInfo;

    statSlotInfo*                         mStatSlots;
    int                                   mStatSlotHead;
    int                                   mStatSlotTail;
    bool                                  mProfilingEnabled;
};
#endif /*__WFDMMSINKSTATISTICS_H*/
