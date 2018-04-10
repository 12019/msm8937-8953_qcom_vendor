/*==============================================================================
*       WFDMMSinkStatistics.cpp
*
*  DESCRIPTION:
*       Calculates and Prints the audio video frames and
        average statistics in the session
*
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
07/26/2013         Darshan R     InitialDraft
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
#include "WFDMMSinkCommon.h"
#include "WFDMMSinkStatistics.h"
#include "WFDMMLogs.h"
#include "MMMemory.h"
#include <cutils/properties.h>
#include "WFDUtils.h"

#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "WFDMMSINKSTATS"

#define MAX_STAT_SLOTS 100
#define ELAPSED_SESSION_TIME 5000000
#define WFD_SNK_STAT_ENABLE_PROFILING (1<<0)

/*==============================================================================

         FUNCTION:         getCurrentTimeUS

         DESCRIPTION:
*//**       @brief         Returns the current timestamp. Does not adjust
                           the value with respect to basetime. Used only for
                           statistics purposes!
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
                           None
*//*==========================================================================*/
uint64 getCurrentTimeUs()
{
    struct timespec sTime;
    clock_gettime(CLOCK_MONOTONIC, &sTime);
    uint64 currTS = ((OMX_U64)sTime.tv_sec * 1000000/*us*/)
           + ((OMX_U64)sTime.tv_nsec / 1000);
    return currTS /*- mnBaseMediaTime*/;
}


/*=============================================================================

         FUNCTION:          isProfilingEnabled

         DESCRIPTION:
*//**       @brief          Helper method to check if profiling is enabled
*//**
@par     DEPENDENCIES:
                            None
*//*
         PARAMETERS:
*//*         @param
                            None

*//*     RETURN VALUE:
*//**       @return
                            True if profiling is enabled, else False

@par     SIDE EFFECTS:
                            None
*//*=========================================================================*/

bool isProfilingEnabled()
{
    int ret = 0;
    char szTemp[PROPERTY_VALUE_MAX];
    const char* propString = "persist.debug.wfd.profile";
    ret = property_get(propString,szTemp,NULL);
    if (ret > 0)
    {
        int i = atoi(szTemp);
        if(i>0)
        {
            if(i&WFD_SNK_STAT_ENABLE_PROFILING)
            {
                WFDMMLOGE2("Found %s with value %d",propString, i);
            }
            return true;
        }
    }
    return false;
}

/*=============================================================================

         FUNCTION:          WFDMMSinkStatistics

         DESCRIPTION:
*//**       @brief          CTOR
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

WFDMMSinkStatistics::WFDMMSinkStatistics()
{
    initData();
}

/*=============================================================================

         FUNCTION:          ~WFDMMSinkStatistics

         DESCRIPTION:
*//**       @brief          DTOR
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

WFDMMSinkStatistics::~WFDMMSinkStatistics()
{
    PrintStatistics();
    releaseResources();
}

/*=============================================================================

         FUNCTION:          initData

         DESCRIPTION:
*//**       @brief          initializes data members of WFDMMSinkStatistics
                            instance
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

void WFDMMSinkStatistics::initData()
{
    mStatSlots    = NULL;
    /*-----------------------------------------------------------------
     Here's how the Q indexes work:

     mStatSlotHead --> points to the latest frame that has been found

     mStatSlotTail --> points to the location in the Q where a frame
                       is to be pushed.
    -------------------------------------------------------------------
    */
    mStatSlotHead = 0;
    mStatSlotTail = 0;

    //Video/Audio statistics
    mVideoAverageDecodeTimeMs = 0;
    mAudioAverageDecodeTimeMs = 0;
    mPeakVideoDecodeTimeMs = 0;
    mPeakAudioDecodeTimeMs = 0;
    mMinVideoDecodeTimeMs = 0;
    mMinAudioDecodeTimeMs = 0;
    mNumberOfAudioFrames = 0;
    mVideoFramesDecoded=0;

    mSessionVideoFrames = 0;
    mCurrentVideoIPFrames = 0;
    mCurrentVideoOPFrames = 0;
    mDecoderDrops = 0;

    //render statistics
    mTotalVideoFrames = 0;
    mTotalAudioFrames = 0;

    mAudioArrivalTimeStamp = 0;
    mVideoArrivalTimeStamp = 0;

    mTotalVideoFramesDropped = 0;
    mTotalAudioFramesDropped = 0;

    mMaxAudioLateByMs = 0;
    mMaxVideoLateByMs = 0;

    mAverageAudioLateByMs = 0;
    mAverageVideoLateByMs = 0;

    mTotalVideoFramesDroppedAfterFiveSeconds = 0;
    mTotalAudioFramesDroppedAfterFiveSeconds = 0;

    mMaxDiffInArrivalOfSuccessiveAudioFrames = 0;
    mMaxDiffInArrivalOfSuccessiveVideoFrames = 0;

    mAverageAudioFrameArrival = 0;
    mAverageVideoFrameArrival = 0;

    mTotalVideoFramesArrived = 0;
    mTotalAudioFramesArrived = 0;

    mFirstVideoFrame = 0;
    mFirstAudioFrame = 0;
    mFirstVideoFrameArrivalTime = 0;
    mFirstAudioFrameArrivalTime = 0;

    mAudioTrackLatency = 0;
    mAddedAudioTrackLatency = 0;

    mVideoAvgDecryptTimeMs = 0;
    mVideoMinDecryptTimeMs = 0;
    mVideoMaxDecryptTimeMs = 0;
    mNumberOfVideoFramesDecrypted = 0;

    mLastPrintStatTime = 0;
    mProfilingEnabled = isProfilingEnabled();

}

/*=============================================================================

         FUNCTION:          setup

         DESCRIPTION:
*//**       @brief          sets up the Statistics module
*//**
@par     DEPENDENCIES:
                            None
*//*
         PARAMETERS:
*//*         @param
                            None

*//*     RETURN VALUE:
*//**       @return
                            0 on success,else other value in case of failure

@par     SIDE EFFECTS:
                            None
*//*=========================================================================*/
int WFDMMSinkStatistics::setup()
{
    int nRet = 0;
    mStatSlots = MM_New_Array(statSlotInfo,MAX_STAT_SLOTS);
    if(!mStatSlots)
    {
        WFDMMLOGE("Failed to allocate memory for statistics array!!!");
        return 1;
    }
    return nRet;
}

/*=============================================================================

         FUNCTION:          releaseResources

         DESCRIPTION:
*//**       @brief          releases resources of WFDMMSinkStatistics
                            instance
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
void WFDMMSinkStatistics::releaseResources()
{
    if(mStatSlots)
    {
        MM_Delete_Array(mStatSlots);
        mStatSlots = NULL;
    }
}

/*=============================================================================

         FUNCTION:          recordMediaSrcIP

         DESCRIPTION:
*//**       @brief          records i/p time from media source
*//**
@par     DEPENDENCIES:
                            None
*//*
         PARAMETERS:
*//*         @param[in]     pDecIPBuffHdr buffer header received from parser

*//*     RETURN VALUE:
*//**       @return
                            None

@par     SIDE EFFECTS:
                            None
*//*=========================================================================*/

void WFDMMSinkStatistics::recordMediaSrcIP(
    const OMX_BUFFERHEADERTYPE* pDecIPBuffHdr)
{
    if(pDecIPBuffHdr)
    {
        buff_hdr_extra_info_sink* tempExtra =
        static_cast<buff_hdr_extra_info_sink*>
        (pDecIPBuffHdr->pOutputPortPrivate);
        tempExtra->nMediaSrcRcvTime = getCurrentTimeUs();
    }
}

/*==============================================================================

         FUNCTION:         recordDecryptStatistics

         DESCRIPTION:
*//**       @brief         record relevant decryption statistics
*//**
@par     DEPENDENCIES:
                           None
*//*
         PARAMETERS:
*//*         @param[in]     pDecIPBuffHdr buffer header being delivered to
                                          decoder

*//*     RETURN VALUE:
*//**       @return

@par     SIDE EFFECTS:
                           None
*//*==========================================================================*/
void WFDMMSinkStatistics::recordDecryptStatistics(
    const OMX_BUFFERHEADERTYPE* pDecIPBuffHdr)
{

    if(pDecIPBuffHdr)
    {
        buff_hdr_extra_info_sink* tempExtra =
         static_cast<buff_hdr_extra_info_sink*>(pDecIPBuffHdr->pOutputPortPrivate);

        uint64 currentTime = getCurrentTimeUs();
        /*----------------------------------------------------------------------
        After receiving buffer from parser, we decrypt the payload. Hence if
        we reach this block, then we can conclude that this is the decrypt time
        and take into account the thread scheduling dealys rather than just
        limiting it to the decrypt block since round trip time must account for
        this time as well
        ------------------------------------------------------------------------
        */
        uint64 nDecryptTime = currentTime - tempExtra->nMediaSrcRcvTime;
        tempExtra->nDecryptTime = nDecryptTime;

        mNumberOfVideoFramesDecrypted++;
        mVideoAvgDecryptTimeMs += nDecryptTime;

        if(WFD_UNLIKELY(mVideoMinDecryptTimeMs == 0))
        {
            mVideoMinDecryptTimeMs = nDecryptTime;
        }

        if(nDecryptTime > mVideoMaxDecryptTimeMs)
        {
            mVideoMaxDecryptTimeMs = nDecryptTime;
        }

        if(nDecryptTime < mVideoMinDecryptTimeMs)
        {
            mVideoMinDecryptTimeMs = nDecryptTime;
        }

    }

}

/*=============================================================================

         FUNCTION:          recordDecIP

         DESCRIPTION:
*//**       @brief          records relevant details from the buffer header
                            being delivered to decoder
*//**
@par     DEPENDENCIES:
                            None
*//*
         PARAMETERS:
*//*         @param[in]     pDecIPBuffHdr buffer header being delivered to
                                          decoder

*//*     RETURN VALUE:
*//**       @return
                            None

@par     SIDE EFFECTS:
                            None
*//*=========================================================================*/

void WFDMMSinkStatistics::recordDecIP(
    const OMX_BUFFERHEADERTYPE* pDecIPBuffHdr)
{
    if(pDecIPBuffHdr && mStatSlots)
    {
        uint64 currentTime = getCurrentTimeUs();

        mCurrentVideoIPFrames++;
        mSessionVideoFrames++;

        buff_hdr_extra_info_sink* tempExtra =
        static_cast<buff_hdr_extra_info_sink*>(pDecIPBuffHdr->pOutputPortPrivate);

        if(tempExtra)
        {

            tempExtra->nFrameNo = mSessionVideoFrames;
            tempExtra->nDecIPTime = currentTime;

            if(((mStatSlotTail + 1)% MAX_STAT_SLOTS) == mStatSlotHead)
            {

                /*----------------------------------------------------------------------
                !!!THIS BLOCK SHOULD NEVER BE HIT IDEALLY!!!

                If the current slot has a frame thats still undecoded, its an act
                of overwriting due to array being full. Hold off any more pushes to
                the statisticsQ till decoder gives outputs to make the situation
                amenable again.
                ------------------------------------------------------------------------
                */

                WFDMMLOGE("!!! Statistics Q is full !!!");
                return;
            }

            ALOGE_IF(mProfilingEnabled,
                "Sending Frame Number %lld with TS %lld at time %lld, slot %d",
                tempExtra->nFrameNo, pDecIPBuffHdr->nTimeStamp,
                tempExtra->nDecIPTime,mStatSlotTail);

            mStatSlots[mStatSlotTail].nKey          = pDecIPBuffHdr->nTimeStamp;
            mStatSlots[mStatSlotTail].nMediaSrcRcvTime =
                                                tempExtra->nMediaSrcRcvTime;
            mStatSlots[mStatSlotTail].nDecryptTime = tempExtra->nDecryptTime;
            //Decoder delivery time might not be same as the TS of Buffer Header
            mStatSlots[mStatSlotTail].nDecDelTime   = currentTime;
            mStatSlots[mStatSlotTail].nFrameNo      = tempExtra->nFrameNo;
            mStatSlots[mStatSlotTail].bFrameDecoded = OMX_FALSE;
            mStatSlotTail = (mStatSlotTail + 1) % MAX_STAT_SLOTS;

        }

    }

}

/*=============================================================================

         FUNCTION:          recordDecOP

         DESCRIPTION:
*//**       @brief          records relevant details from the buffer header
                            received from decoder
*//**
@par     DEPENDENCIES:
                            None
*//*
         PARAMETERS:
*//*         @param[in]     pDecOPBuffHdr buffer header received from encoder

*//*     RETURN VALUE:
*//**       @return
                            None

@par     SIDE EFFECTS:
                            None
*//*=========================================================================*/

void WFDMMSinkStatistics::recordDecOP(
const OMX_BUFFERHEADERTYPE* pDecOPBuffHdr)
{
    /*----------------------------------------------------------------------
    If component returns buffers after it has signalled an EOS, it will be
    giving ones with 0 filled length. Take care of that while recording
    statistics.
    ------------------------------------------------------------------------
    */

    if(pDecOPBuffHdr && pDecOPBuffHdr->nFilledLen && mStatSlots)
    {
        uint64 decTime = 0;

        mCurrentVideoOPFrames++;
        mVideoFramesDecoded++;

        buff_hdr_extra_info_sink* tempExtra =
        static_cast<buff_hdr_extra_info_sink*>(pDecOPBuffHdr->pInputPortPrivate);


        uint64 currentTime = getCurrentTimeUs();

        if(tempExtra)
        {
            tempExtra->nDecOPTime = currentTime;

            /*-----------------------------------------------------------------
            Retrieve the frame no from the stat array using the delivery TS as
            the key and use this to track the frame at output port
            -------------------------------------------------------------------
            */

            int i = mStatSlotHead;
            for(; i!= mStatSlotTail; i =(i+ 1) % MAX_STAT_SLOTS)
            {

                /*-------------------------------------------------------------
                The frame no at head should always match the TS of buffer header
                received from decoder, unless it actually drops frames timestamps
                should always be in order.

                Any concurrency concerns can be dismissed because the TS match
                should be a direct hit or else a few indexes ahead. One thread
                T1 will be updating the tail index, essentially pushing frames
                to be matched with later, and another thread T2 is going to
                search for a frame in this list. In the worst case T1 might be
                updating the tail index while T2 is iterating over the array,
                but this should not be a problem because the head index should
                always be lagging the tail index so it might just need to go a
                few slots ahead (that too when the frame isn't there at all in
                the array, which again is a case that shouldn't ever arise)
                ---------------------------------------------------------------
                */

                if(mStatSlots[i].nKey != pDecOPBuffHdr->nTimeStamp)
                {
                    WFDMMLOGE3("Frame Number %lld with TS %lld at %d "\
                        "dropped by decoder",
                        mStatSlots[i].nFrameNo, mStatSlots[i].nKey, i);
                    mDecoderDrops++;
                }
                else
                {
                    mStatSlots[i].bFrameDecoded = OMX_TRUE;

                    /*---------------------------------------------------------
                    Now populate the buffer header at output port with the info
                    received after hashing the stat array, to enable tracking
                    of the frame.
                    -----------------------------------------------------------
                    */

                    tempExtra->nFrameNo = mStatSlots[i].nFrameNo;
                    tempExtra->nMediaSrcRcvTime = mStatSlots[i].nMediaSrcRcvTime;
                    tempExtra->nDecryptTime = mStatSlots[i].nDecryptTime;
                    tempExtra->nDecIPTime= mStatSlots[i].nDecDelTime;
                    ALOGE_IF(mProfilingEnabled,
                        "Received Frame Number %lld with TS %lld at time %lld,"\
                        "at slot %d", tempExtra->nFrameNo,
                        pDecOPBuffHdr->nTimeStamp,tempExtra->nDecOPTime,i);

                    mStatSlotHead = (i+ 1) % MAX_STAT_SLOTS;
                    break;
                }
            }

            if(i == mStatSlotTail)
            {
                WFDMMLOGE1("Unable to find frame with TS %lld",
                    pDecOPBuffHdr->nTimeStamp);
                /*--------------------------------------------------------------
                 The entire list of frames doesn't have the buffer header with
                 the TS of the buffer header received from encoder. It's most
                 probably a spurious buffer header. No point in going ahead.
                ---------------------------------------------------------------
                */
                return;
            }

            decTime = tempExtra->nDecOPTime- tempExtra->nDecIPTime;

            mVideoAverageDecodeTimeMs += decTime;


            if(WFD_UNLIKELY(mMinVideoDecodeTimeMs == 0))
            {
                mMinVideoDecodeTimeMs = decTime;
            }

            if(WFD_UNLIKELY(mPeakVideoDecodeTimeMs == 0))
            {
                mPeakVideoDecodeTimeMs = decTime;
            }


            if(mMinVideoDecodeTimeMs > decTime)
            {
                mMinVideoDecodeTimeMs = decTime;
            }

            if(mPeakVideoDecodeTimeMs < decTime)
            {
                mPeakVideoDecodeTimeMs = decTime;
            }

        }

    }

}

/*=============================================================================

         FUNCTION:          recordVideoFrameDrop

         DESCRIPTION:
*//**       @brief          records a video frame drop in cases where either
                            the decoder itself gave a 0 filled len buffer or
                            the frame was dropped due to AV sync logic
*//**
@par     DEPENDENCIES:
                            None
*//*
         PARAMETERS:
*//*         @param[in]     pDecOPBuffHdr buffer header received from decoder

*//*     RETURN VALUE:
*//**       @return
                            None

@par     SIDE EFFECTS:
                            None
*//*=========================================================================*/

void WFDMMSinkStatistics::recordVideoFrameDrop(
const OMX_BUFFERHEADERTYPE* pDecOPBuffHdr, bool bDueToAVsync)
{

    /*------------------------------------------------------------------
     Ideally should have revamped UpdateDropsOrLateby() but it is common for
     audio frames as well, hence letting that stay as it is. This is just a
     diagnostic function, and accomodates no logic or statistic update.
    --------------------------------------------------------------------
    */
    if(pDecOPBuffHdr)
    {
        buff_hdr_extra_info_sink* extraInfo =
        static_cast<buff_hdr_extra_info_sink*>
        (pDecOPBuffHdr->pInputPortPrivate);

        if(extraInfo)
        {
            if(!bDueToAVsync)
            {
                WFDMMLOGE1("Dropping Frame Number %lld because of 0 nFilledLen",
                extraInfo->nFrameNo);
            }
            memset(extraInfo,0,sizeof(*extraInfo));//Reset for next run
        }
    }
}

/*=============================================================================

         FUNCTION:          recordSchedDelay

         DESCRIPTION:
*//**       @brief          records delay for scheduling video Buffer after
                            decoder has given the buffer
*//**
@par     DEPENDENCIES:
                            None
*//*
         PARAMETERS:
*//*         @param[in]     pDecOPBuffHdr buffer header received from decoder

*//*     RETURN VALUE:
*//**       @return
                            None

@par     SIDE EFFECTS:
                            None
*//*=========================================================================*/

void WFDMMSinkStatistics::recordSchedDelay(
    const OMX_BUFFERHEADERTYPE* pDecOPBuffHdr)
{
    if(pDecOPBuffHdr)
    {
        buff_hdr_extra_info_sink* extraInfo =
        static_cast<buff_hdr_extra_info_sink*>
        (pDecOPBuffHdr->pInputPortPrivate);

        if(extraInfo)
        {
            uint64 currentTime = getCurrentTimeUs();
            extraInfo->nRenderSchedTime = currentTime;
            extraInfo->nSchedDelayTime = extraInfo->nRenderSchedTime
                - extraInfo->nDecOPTime;
            ALOGE_IF(mProfilingEnabled,
                "Queue Frame Number %lld to Native window @ %lld with delay"\
                " %lld us", extraInfo->nFrameNo, extraInfo->nRenderSchedTime,
                extraInfo->nSchedDelayTime);
        }
    }
}

/*=============================================================================

         FUNCTION:          recordDisplayIP

         DESCRIPTION:
*//**       @brief          records details of buffer before submitting to
                            renderer
*//**
@par     DEPENDENCIES:
                            None
*//*
         PARAMETERS:
*//*         @param[in]     pDecOPBuffHdr buffer header received from decoder

*//*     RETURN VALUE:
*//**       @return
                            None

@par     SIDE EFFECTS:
                            None
*//*=========================================================================*/

void WFDMMSinkStatistics::recordDisplayIP(
    const OMX_BUFFERHEADERTYPE* pDecOPBuffHdr)
{
    if(pDecOPBuffHdr)
    {
        buff_hdr_extra_info_sink* extraInfo =
        static_cast<buff_hdr_extra_info_sink*>
        (pDecOPBuffHdr->pInputPortPrivate);

        if(extraInfo)
        {
            uint64 currentTime = getCurrentTimeUs();
            extraInfo->nRenderQTime = currentTime - extraInfo->nRenderSchedTime;
            ALOGE_IF(mProfilingEnabled,
                "Queue Frame Number %lld to Native window took %lld us",
                extraInfo->nFrameNo, extraInfo->nRenderQTime);
        }
    }

}

/*=============================================================================

         FUNCTION:          recordDisplayOP

         DESCRIPTION:
*//**       @brief          records details after receiving buffer from display
*//**
@par     DEPENDENCIES:
                            None
*//*
         PARAMETERS:
*//*         @param[in]     pDecOPBuffHdr buffer header received from decoder

*//*     RETURN VALUE:
*//**       @return
                            None

@par     SIDE EFFECTS:
                            None
*//*=========================================================================*/

void WFDMMSinkStatistics::recordDisplayOP(
    const OMX_BUFFERHEADERTYPE* pDecOPBuffHdr)
{
    if(pDecOPBuffHdr)
    {

        buff_hdr_extra_info_sink* extraInfo =
        static_cast<buff_hdr_extra_info_sink*>
        (pDecOPBuffHdr->pInputPortPrivate);

        if(extraInfo)
        {

            if(WFD_UNLIKELY(pDecOPBuffHdr->nFilledLen == 0))
            {
                /*------------------------------------------------------------------
                 There is some devious choreography in renderer module for initial
                 buffers which are pushed and popped off the videoQ without
                 actually delivering to renderer, but this API will be invoked in
                 any case, just log that and return.
                --------------------------------------------------------------------
                */
                WFDMMLOGE2("Ignoring Frame Number %lld with filled len %d",
                extraInfo->nFrameNo, pDecOPBuffHdr->nFilledLen);
                memset(extraInfo,0,sizeof(*extraInfo));//Reset for next run
                return;
            }

            uint64 currentTime = getCurrentTimeUs();
            uint64 decodeTime = extraInfo->nDecOPTime - extraInfo->nDecIPTime;
            extraInfo->nRenderTime = currentTime - extraInfo->nRenderSchedTime;
            uint64 rndTripTime = extraInfo->nDecryptTime + decodeTime +
                extraInfo->nSchedDelayTime + extraInfo->nRenderTime;
            ALOGE_IF(mProfilingEnabled,
                "DQueue Frame Number %lld Decrypt time = %lld us Decode time ="\
                "%lld us, Schedule Delay = %lld us, Renderer time = %lld us, "\
                "rndTrip time = %lld us", extraInfo->nFrameNo,
                extraInfo->nDecryptTime,decodeTime ,extraInfo->nSchedDelayTime,
                extraInfo->nRenderTime, rndTripTime);
            memset(extraInfo,0,sizeof(*extraInfo));//Reset for next run
        }
    }
}

/*=============================================================================

         FUNCTION:          resetStats

         DESCRIPTION:
*//**       @brief          resets the times & values for internal consistency
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
void WFDMMSinkStatistics::resetStats()
{
    mCurrentVideoIPFrames = mCurrentVideoOPFrames = mLastPrintStatTime = 0;
}

/*==============================================================================

         FUNCTION:         UpdateDropsOrLateby

         DESCRIPTION:
*//**       @brief         Updates the renderer specific members of the class
*//**
@par     DEPENDENCIES:
                           None
*//*
         PARAMETERS:
*//**       @param

*//*     RETURN VALUE:
*//**       @return

@par     SIDE EFFECTS:
                           None
*//*==========================================================================*/
void WFDMMSinkStatistics::UpdateDropsOrLateby(
    const rendererStatsType& nRendererObj)
{
    if(nRendererObj.nIsVideoFrame)
    {
        if(!mFirstVideoFrame)
        {
            mFirstVideoFrame = 1;
            mFirstVideoFrameArrivalTime = nRendererObj.nSessionTime;
        }

        mTotalVideoFramesArrived++;

        if(mVideoArrivalTimeStamp == 0)
            mVideoArrivalTimeStamp = nRendererObj.nArrivalTime;
        else if((nRendererObj.nArrivalTime - mVideoArrivalTimeStamp) >
                           (uint64)mMaxDiffInArrivalOfSuccessiveVideoFrames)
            mMaxDiffInArrivalOfSuccessiveVideoFrames =
                            nRendererObj.nArrivalTime - mVideoArrivalTimeStamp;

        mAverageVideoFrameArrival = mAverageVideoFrameArrival +
            (nRendererObj.nArrivalTime - mVideoArrivalTimeStamp);

        mVideoArrivalTimeStamp = nRendererObj.nArrivalTime;
        mAudioTrackLatency = nRendererObj.nAudioTrackLatency;
        mAddedAudioTrackLatency = nRendererObj.nAddedAudioTrackLatency;
    }
    else
    {
        if(!mFirstAudioFrame)
        {
            mFirstAudioFrame= 1;
            mFirstAudioFrameArrivalTime= nRendererObj.nSessionTime;
        }

        mTotalAudioFramesArrived++;

        if(mAudioArrivalTimeStamp == 0)
            mAudioArrivalTimeStamp = nRendererObj.nArrivalTime;
        else if((nRendererObj.nArrivalTime - mAudioArrivalTimeStamp) >
                           (uint64)mMaxDiffInArrivalOfSuccessiveAudioFrames)
            mMaxDiffInArrivalOfSuccessiveAudioFrames =
                            nRendererObj.nArrivalTime - mAudioArrivalTimeStamp;

        mAverageAudioFrameArrival = mAverageAudioFrameArrival +
            (nRendererObj.nArrivalTime - mAudioArrivalTimeStamp);

        mAudioArrivalTimeStamp = nRendererObj.nArrivalTime;
    }

    if(nRendererObj.nIsLate)
    {
        if(nRendererObj.nIsVideoFrame)
        {
            mTotalVideoFramesDropped++;
            if(nRendererObj.nSessionTime - mFirstVideoFrameArrivalTime
                >= ELAPSED_SESSION_TIME)
                mTotalVideoFramesDroppedAfterFiveSeconds++;
        }
        else
        {
            mTotalAudioFramesDropped++;
            if(nRendererObj.nSessionTime - mFirstAudioFrameArrivalTime
                >= ELAPSED_SESSION_TIME)
                mTotalAudioFramesDroppedAfterFiveSeconds++;
        }
    }
    else
    {
        if(nRendererObj.nIsVideoFrame)
        {
            if(nRendererObj.nTimeDecider < mMaxVideoLateByMs)
                mMaxVideoLateByMs = nRendererObj.nTimeDecider;

            mAverageVideoLateByMs += nRendererObj.nTimeDecider;
            mTotalVideoFrames++;
        }
        else
        {
            if(nRendererObj.nTimeDecider < mMaxAudioLateByMs)
                mMaxAudioLateByMs = nRendererObj.nTimeDecider;

            mAverageAudioLateByMs += nRendererObj.nTimeDecider;
            mTotalAudioFrames++;
        }
    }
}

/*==============================================================================

         FUNCTION:         PrintStatistics

         DESCRIPTION:
*//**       @brief         Prints the statistics of a session on WFD sink
*//**
@par     DEPENDENCIES:
                           None
*//*
         PARAMETERS:
*//**       @param

*//*     RETURN VALUE:
*//**       @return

@par     SIDE EFFECTS:
                           None
*//*==========================================================================*/
void  WFDMMSinkStatistics::PrintStatistics()const
{

#define FPS_PRINT_INTERVAL 1000000 // 1 second in us

    uint64 currTime = getCurrentTimeUs();

    if(WFD_UNLIKELY(mLastPrintStatTime == 0))
    {
        mLastPrintStatTime = currTime;
    }

    uint64 diffTime = currTime - mLastPrintStatTime;

    if(WFD_UNLIKELY(diffTime > FPS_PRINT_INTERVAL))
    {
        WFDMMLOGE("****************SINK STATISTICS******************************");
        WFDMMLOGE1("Minumim Decode Time = %llu",mMinVideoDecodeTimeMs);
        WFDMMLOGE1("Maximum Decode Time = %llu",mPeakVideoDecodeTimeMs);


        WFDMMLOGE1("I/P FPS for decoder is %0.2f",
            mCurrentVideoIPFrames*1000000.0/diffTime);

        WFDMMLOGE1("O/P FPS for decoder is %0.2f",
            mCurrentVideoOPFrames*1000000.0/diffTime);

        if(mVideoFramesDecoded)
        {
            WFDMMLOGE1("Average Decode Time = %llu",
                mVideoAverageDecodeTimeMs/mVideoFramesDecoded);
        }

        WFDMMLOGE1("Total Number of Video Frames dropped by decoder = %d",
            mDecoderDrops);
        WFDMMLOGE1("Total Number of Video Frames dropped in the session= %d",
            mTotalVideoFramesDropped);
        WFDMMLOGE1("Total Number of Audio Frames dropped in the session= %d",
            mTotalAudioFramesDropped);
        WFDMMLOGE1("Number of Video Frames dropped after 5 seconds= %d",
            mTotalVideoFramesDroppedAfterFiveSeconds);
        WFDMMLOGE1("Number of Audio Frames dropped after 5 seconds = %d",
            mTotalAudioFramesDroppedAfterFiveSeconds);

        WFDMMLOGE1("Max Video Lateby = %lld",mMaxVideoLateByMs);
        WFDMMLOGE1("Max Audio Lateby = %lld",mMaxAudioLateByMs);
        if(mTotalVideoFrames)
        {
            WFDMMLOGE1("Average Video Lateby = % lld",
                mAverageVideoLateByMs/mTotalVideoFrames);
        }
        if(mTotalAudioFrames)
        {
            WFDMMLOGE1("Average Audio Lateby = %lld",
                mAverageAudioLateByMs/mTotalAudioFrames);
        }

        WFDMMLOGE1("Max difference in arrival of successive Video Frames = %lld",
            mMaxDiffInArrivalOfSuccessiveVideoFrames);
        WFDMMLOGE1("Max difference in arrival of successive Audio Frames = %lld",
            mMaxDiffInArrivalOfSuccessiveAudioFrames);

        if(mTotalVideoFramesArrived)
        {
            WFDMMLOGE1("Avg diff in arrival of successive   Video Frames = %lld",
                mAverageVideoFrameArrival / mTotalVideoFramesArrived);
        }
        if(mTotalAudioFramesArrived)
        {
            WFDMMLOGE1("Avg diff in arrival of successive Audio Frames = %lld",
                mAverageAudioFrameArrival / mTotalAudioFramesArrived);
        }

        WFDMMLOGE2("Queried Audio track latency is %ld and added for AV sync "\
            "is %ld",mAudioTrackLatency,mAddedAudioTrackLatency);

        if(mNumberOfVideoFramesDecrypted)
        {
            WFDMMLOGE1("Number of video frames decrypted = %lld",
                mNumberOfVideoFramesDecrypted);
            WFDMMLOGE1("Maximum Decrypt time = %lld", mVideoMaxDecryptTimeMs);
            WFDMMLOGE1("Minimum Decrypt time = %lld", mVideoMinDecryptTimeMs);
            WFDMMLOGE1("Average Decrypt time = %lf ms",
                mVideoAvgDecryptTimeMs*1.0/mNumberOfVideoFramesDecrypted);
        }
        mCurrentVideoIPFrames = mCurrentVideoOPFrames =  mLastPrintStatTime = 0;
        WFDMMLOGE("**************************************************************");
    }
}
