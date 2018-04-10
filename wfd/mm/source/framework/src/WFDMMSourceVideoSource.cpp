/*=======================================================================
                              WFDMMSourceVideoSource.cpp
DESCRIPTION

This module is for WFD source implementation. Takes care of interacting
with Encoder (which in turn interacts with capture module).

Copyright (c) 2011-2016 Qualcomm Technologies, Inc.
All Rights Reserved.
Confidential and Proprietary - Qualcomm Technologies, Inc.
========================================================================== */

/*========================================================================
 *                             Include Files
 *==========================================================================*/
#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "WFDMMSourceVideoSource"

#include "WFDMMSourceSignalQueue.h"
#include "WFDMMSourceVideoSource.h"
#include "MMDebugMsg.h"
#include "WFDMMLogs.h"
#include "WFDMMThreads.h"
#include "WFDMMSourceVideoEncode.h"
#include "QOMX_VideoExtensions.h"
#include "WFD_HdcpCP.h"
#include "wfd_cfg_parser.h"
#include "MMMalloc.h"
#include "MMMemory.h"
#include <cutils/properties.h>
#include "WFDUtils.h"
#include "OMX_Video.h"
#include <fcntl.h>
#include "MMIonMemory.h"

#define VIDEO_SOURCE_DS_PTS_ADJUST_US 500000
#define VIDEO_SOURCE_DELAYED_RENDERING_TOLERANCE 1000000
#define VIDEO_SOURCE_DTS_DEFAULT_FRAME_DELTA 10000
#define VIDEO_SOURCE_SLOW_SINK_CONSUMPTION_THRESHOLD 10

#ifndef ABS
#define ABS(x) ((x) > 0 ? (x) : -(x))
#endif

#define WFD_VSRC_CRITICAL_SECT_ENTER(critSect) if(critSect)                 \
                                      MM_CriticalSection_Enter(critSect);
    /*      END CRITICAL_SECT_ENTER    */

#define WFD_VSRC_CRITICAL_SECT_LEAVE(critSect) if(critSect)                 \
                                      MM_CriticalSection_Leave(critSect);
    /*      END CRITICAL_SECT_LEAVE    */

#define STREAMING_WAIT_TIME_CNT  400

/* This file lists package names of all application
   which are allowed. */
#ifndef WFD_APPLIST_CFG_FILE
#define WFD_APPLIST_CFG_FILE "/system/etc/wfddswhitelist.xml"
#endif
#include "MMTimer.h"

#ifndef WFD_ICS
#include "common_log.h"
#endif
#include <threads.h>

#define WFDMM_VIDEO_SRC_ION_MEM_LIMIT 3145728
#define WFDMM_VIDEO_SRC_MAX_BITSTREAM_BUFS 4

#ifdef SYSTRACE_PROFILE
#define ATRACE_TAG ATRACE_TAG_VIDEO
#include <utils/Trace.h>
#endif

/*========================================================================
 *                          Defines/ Macro
 *==========================================================================*/
#define VIDEO_PVTDATA_TYPE_FILLERNALU           0
#define VIDEO_PVTDATA_TYPE_FILLERNALU_ENCRYPTED 1
#define VIDEO_PVTDATA_TYPE_DECODE_TIMESTAMP     2
#define PES_PVT_DATA_LEN 16
#define BUFFER_EXTRA_DATA 1024

#define WFD_H264_PROFILE_CONSTRAINED_BASE 1
#define WFD_H264_PROFILE_CONSTRAINED_HIGH 2
#define FRAME_SKIP_FPS_VARIANCE 20
#define PORT_INDEX_VIDEO 1

// Frame skipping delay
uint32 video_frame_skipping_start_delay = 0;

/**!
 * @brief Helper macro to set private/internal flag
 */
#define FLAG_SET(_pCtx_, _f_) (_pCtx_)->nFlags  |= (_f_)

/**!
 * @brief Helper macro to check if a flag is set or not
 */
#define FLAG_ISSET(_pCtx_, _f_) (((_pCtx_)->nFlags & (_f_)) ? OMX_TRUE : OMX_FALSE)

/**!
 * @brief Helper macro to clear a private/internal flag
 */
#define FLAG_CLEAR(_pCtx_, _f_) (_pCtx_)->

#ifndef OMX_SPEC_VERSION
#define OMX_SPEC_VERSION 0x00000101
#endif

struct output_metabuffer {
    OMX_U32 type;
    native_handle_t* nh;
};

enum
{
    CBP_VGA_BLANK_FRAME_SIZE = 1248,
    CHP_VGA_BLANK_FRAME_SIZE = 1250,
    MAX_BLANKING_PERIOD = 200,//in ms
};

OMX_U8 VideoSource::sFillerNALU[FILLER_NALU_SIZE] =
{0x00, 0x00, 0x00, 0x01, 0x0c, 0xff, 0xff, 0x80};

static const OMX_U8 sVGA_CBP_BkFm[CBP_VGA_BLANK_FRAME_SIZE]=
{
    0x00,0x00,0x00,0x01,0x09,0x10,0x00,0x00,0x00,0x01,0x67,0x42,0x80,0x1F,0xDA,0x02,
    0x80,0xF6,0x80,0x6D,0x0A,0x13,0x50,0x00,0x00,0x00,0x01,0x68,0xCE,0x06,0xE2,0x00,
    0x00,0x00,0x01,0x65,0xB8,0x41,0x5F,0xFF,0xF8,0x7A,0x28,0x00,0x08,0x18,0xFB,0x55,
    0x6B,0x6B,0x6B,0x6B,0x6B,0x6B,0x6B,0x6B,0x6B,0x6B,0x6B,0x6B,0x6B,0x6B,0x6B,0x6B,
    0x6B,0x6B,0x6B,0x6B,0x6B,0x6B,0x6B,0x6B,0x6B,0x6B,0x6B,0x6B,0x6B,0x6B,0x6B,0x6B,
    0x6B,0x6B,0x6B,0x6B,0x6B,0x6B,0x4D,0xAA,0x7A,0x7A,0x7A,0x7A,0x7A,0x7A,0x7A,0x7A,
    0x7A,0x7A,0x7A,0x7A,0x7A,0x7A,0x7A,0x7A,0x7A,0x7A,0x7A,0x7A,0x7A,0x7A,0x7A,0x7A,
    0x7A,0x7A,0x7A,0x7A,0x7A,0x7A,0x7A,0x7A,0x7A,0x7A,0x7A,0x7A,0x7A,0x7A,0x7A,0x6E,
    0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,
    0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,
    0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9B,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,
    0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,
    0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,
    0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,
    0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,
    0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,
    0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,
    0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,
    0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,
    0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,
    0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,
    0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,
    0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,
    0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,
    0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,
    0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,
    0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,
    0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,
    0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,
    0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,
    0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,
    0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,
    0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,
    0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,
    0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,
    0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,
    0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,
    0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,
    0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,
    0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,
    0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,
    0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,
    0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,
    0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,
    0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,
    0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,
    0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,
    0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,
    0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,
    0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,
    0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,
    0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,
    0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,
    0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,
    0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,
    0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,
    0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,
    0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,
    0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,
    0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,
    0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,
    0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,
    0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,
    0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,
    0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,
    0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,
    0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,
    0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,
    0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,
    0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,
    0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,
    0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,
    0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,
    0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,
    0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,
    0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,
    0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,
    0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xA7,0xC0
};

static const OMX_U8 sVGA_CHP_BkFm[CHP_VGA_BLANK_FRAME_SIZE] =
{
    0x00,0x00,0x00,0x01,0x09,0x10,0x00,0x00,0x00,0x01,0x67,0x64,0x00,0x1F,0xAC,0xD2,
    0x02,0x80,0xF6,0x80,0x6D,0x0A,0x13,0x50,0x00,0x00,0x00,0x01,0x68,0xCE,0x06,0xE2,
    0xC0,0x00,0x00,0x00,0x01,0x65,0xB8,0x40,0x0A,0xBF,0xFF,0xE1,0xE8,0xA0,0x00,0x20,
    0x63,0xED,0x55,0xAD,0xAD,0xAD,0xAD,0xAD,0xAD,0xAD,0xAD,0xAD,0xAD,0xAD,0xAD,0xAD,
    0xAD,0xAD,0xAD,0xAD,0xAD,0xAD,0xAD,0xAD,0xAD,0xAD,0xAD,0xAD,0xAD,0xAD,0xAD,0xAD,
    0xAD,0xAD,0xAD,0xAD,0xAD,0xAD,0xAD,0xAD,0xAD,0x36,0xA9,0xE9,0xE9,0xE9,0xE9,0xE9,
    0xE9,0xE9,0xE9,0xE9,0xE9,0xE9,0xE9,0xE9,0xE9,0xE9,0xE9,0xE9,0xE9,0xE9,0xE9,0xE9,
    0xE9,0xE9,0xE9,0xE9,0xE9,0xE9,0xE9,0xE9,0xE9,0xE9,0xE9,0xE9,0xE9,0xE9,0xE9,0xE9,
    0xE9,0xE9,0xBA,0x7A,0x7A,0x7A,0x7A,0x7A,0x7A,0x7A,0x7A,0x7A,0x7A,0x7A,0x7A,0x7A,
    0x7A,0x7A,0x7A,0x7A,0x7A,0x7A,0x7A,0x7A,0x7A,0x7A,0x7A,0x7A,0x7A,0x7A,0x7A,0x7A,
    0x7A,0x7A,0x7A,0x7A,0x7A,0x7A,0x7A,0x7A,0x7A,0x7A,0x6E,0x9E,0x9E,0x9E,0x9E,0x9E,
    0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,
    0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,
    0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,
    0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,
    0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,
    0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,
    0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,
    0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,
    0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,
    0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,
    0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,
    0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,
    0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,
    0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,
    0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,
    0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,
    0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,
    0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,
    0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,
    0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,
    0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,
    0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,
    0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,
    0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,
    0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,
    0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,
    0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,
    0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,
    0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,
    0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,
    0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,
    0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,
    0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,
    0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,
    0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,
    0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,
    0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,
    0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,
    0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,
    0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,
    0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,
    0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,
    0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,
    0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,
    0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,
    0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,
    0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,
    0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,
    0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,
    0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,
    0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,
    0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,
    0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,
    0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,
    0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,
    0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,
    0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,
    0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,
    0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,
    0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,
    0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,
    0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,
    0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,
    0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,
    0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,
    0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,
    0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,
    0x9E,0x9F
};

//This macro provides a single point exit from function on
//encountering any error
#define WFD_OMX_ERROR_CHECK(result,error) ({ \
    if(result!= OMX_ErrorNone) \
    {\
        WFDMMLOGE(error);\
        WFDMMLOGE1("due to %x", result);\
        goto EXIT;\
    }\
})
    /*--------------------------------------------------------------------------
    Setting the Static variable to flase at init
    ----------------------------------------------------------------------------
    */

bool VideoSource::m_bVideoSourceActive = false;
/*=============================================================================

         FUNCTION:          IS_DUMP_ENABLE

         DESCRIPTION:
*//**       @brief          helper function to enable h264 dumping
*//**
@par     DEPENDENCIES:
                            None
*//*
         PARAMETERS:
*//**       @param          None

*//*     RETURN VALUE:
*//**       @return
                            None
@par     SIFE EFFECTS:
                            None
*//*=========================================================================*/

inline OMX_BOOL IS_DUMP_ENABLE() {
    int ret = 0;
    char szTemp[PROPERTY_VALUE_MAX];
    // For legacy purposes leaving it as v4l2
    ret = property_get("persist.debug.wfd.strdump",szTemp,NULL);
    if (ret <= 0 )
    {
        WFDMMLOGE2("Failed: to read prop %d %s value", ret, szTemp);
        return OMX_FALSE;
    }
    if(strcmp(szTemp,"1")==0)
    {
        WFDMMLOGD("IS_DUMP_ENABLE OMX_TRUE");
        return OMX_TRUE;
    }
    else
    {
        WFDMMLOGD("IS_DUMP_ENABLE OMX_FALSE");
        return OMX_FALSE;
    }
}

/*!*************************************************************************
 * @brief     CTOR
 *
 * @param[in] NONE
 *
 * @return    NONE
 *
 * @note
 **************************************************************************/
VideoSource::VideoSource():
    m_nNumBuffers(0),
    m_pHdcpOutBuffers(NULL),
    m_pVideoSourceOutputBuffers(NULL),
    m_pHDCPOutputBufQ(NULL),
    m_nMuxBufferCount(0),
    m_bStarted(OMX_FALSE),
    m_bPause(OMX_FALSE),
    m_pFrameDeliverFn(NULL),
    ionfd(-1),
    m_secureSession(OMX_FALSE),
    m_nFillerInFd(-1),
    m_nFillerOutFd(-1),
    m_hFillerInHandle(0),
    m_hFillerOutHandle(0),
    m_pFillerDataInPtr(NULL),
    m_pFillerDataOutPtr(NULL),
    m_bFillerNaluEnabled(OMX_TRUE),
    m_pVencoder(NULL),
    m_pWFDMMSrcStats(NULL),
    m_bDropVideoFrame(OMX_FALSE),
    m_bBlankFrameSupport(OMX_TRUE),
    m_pBlankFrame(NULL),
    m_nBlankFrameSize(0),
    m_pBlankFrameQ(NULL),
    m_bHandleStandby(OMX_FALSE),
    m_nMaxBlankFrames(0),
    m_nBlankFramesDelivered(0),
    m_nBlankFrameInFd(-1),
    m_hBlankFrameInHandle(0),
    m_pBlankFrameDataInPtr(NULL),
    m_nLastFrameTS(0),
    m_eStreamingStatus(STREAMING_DEINIT)
{
    WFDMMLOGE("Creating VideoSource");
    m_eVideoSrcState    = WFDMM_VideoSource_STATE_INIT;
    m_eVideoStreamingSrcState = WFDMM_VideoSource_STATE_INIT;
    m_pHdcpHandle       = NULL;
    m_pStreamingBufArray = NULL;
    m_bHdcpSessionValid = OMX_FALSE;
    m_pHdcpOutBuffers   = NULL;
    m_nStreamingBufCnt = MAX_STR_BUFFER_ASSUME;
    m_hVideoSourceThread = NULL;
    m_pStreamingBufQ = NULL;
    m_pStreamingPendBufQ = NULL;
    m_bStreamingEnabled = OMX_FALSE;
    m_StreamingClosingReqSent = false;
    m_bVideoSourceActive = false;
    mbVideoSourceStopped = OMX_FALSE;
    mbStopWhileStreaming = false;
    m_bStreamingSupport = false;
    m_nRenderTSReadIdx = 0;
    m_nRenderTSWriteIdx = 0;
    m_nBaseRenderTimeStamp = 0;

#ifdef ENABLE_STREAMING_DUMP
    Str_dump_file = NULL;
    m_bEnableStrDump = OMX_FALSE;
#endif

    m_nBaseStreamTime = 0;
    m_bBaseStreamTimeTaken = false;
    m_nBaseSystemTime = 0;
    ntickIdx = 0;
    m_hCritSect = NULL;
    m_WFDMMService = NULL;
    m_pAppSupported = NULL;
    m_nCountAppSupported = 0;
    /*-------------------------------------------------------------------------
        Decision to encrypt non secure content or not is made by application
        or user based on the WFD config file
       ------------------------------------------------------------------------
    */
    int nVal;
    nVal = 0;
    // CHeck if Filler NALU is disabled
    getCfgItem(DISABLE_NALU_FILLER_KEY,&nVal);
    if(nVal == 1)
    {
        m_bFillerNaluEnabled = OMX_FALSE;
    }

    m_pWFDMMSrcStats =  MM_New(WFDMMSourceStatistics);
    if(!m_pWFDMMSrcStats)
    {
        WFDMMLOGE("Failed to create WFDMMSourceStatistics");
    }
    else
    {
        if(WFD_UNLIKELY(m_pWFDMMSrcStats->setup()!=0))
        {
            WFDMMLOGE("Failed to setup Source statistics!!!");
            MM_Delete(m_pWFDMMSrcStats);
            m_pWFDMMSrcStats = NULL;
        }
    }

    m_pVencoder = MM_New_Args(WFDMMSourceVideoEncode,(m_pWFDMMSrcStats));

    if(!m_pVencoder)
    {
        WFDMMLOGE("Failed to create Video Encoder");
        return;
    }

    nVal = 1;
    getCfgItem(BLANK_FRAME_SUPPORT_KEY,&nVal);
    if(0 == nVal)
    {
        m_bBlankFrameSupport = OMX_FALSE;
        WFDMMLOGH("No support for blank frame");
    }

    memset(&m_sConfig,0,sizeof(m_sConfig));

    /* Parse Supported APP List */
    parseAndPopulateWhiteListApp();
}

/*=============================================================================

         FUNCTION:          parseAndPopulateWhiteListApp

         DESCRIPTION:
*//**       @brief          This function reads from WFD_APPLIST_CFG_FILE file
                            and populates an array with package names mentioned
                            in this file. This prepares list of application
                            which are supported / allowed to do DS.
*//**
@par     DEPENDENCIES:      None
*//*
         PARAMETERS:
*//**       @param          None

*//*     RETURN VALUE:
*//**       @return         None

@par     SIDE EFFECTS:      None

*//*=========================================================================*/
void VideoSource::parseAndPopulateWhiteListApp()
{
    readConfigFile getWhiteList;
    getWhiteList.whiteListApp.count = 0;
    memset (&getWhiteList, 0, sizeof(readConfigFile));

    parseCfgforWhiteList(WFD_APPLIST_CFG_FILE,&getWhiteList);
    int count = getWhiteList.whiteListApp.count;
    bool err = false;
    if(count > 0)
    {
        m_pAppSupported = (char**)MM_Malloc((sizeof(char *))*count);
    }
    else
    {
        WFDMMService::updateWhiteListStatus(true);
        return;
    }

    if(m_pAppSupported != NULL && getWhiteList.whiteListApp.name != NULL)
    {
        for(int i = 0; i < count; i++)
        {
            m_pAppSupported[i] = (char *)MM_Malloc((sizeof(char))*WHITELIST_APP_NAME_LENGTH);
            if(m_pAppSupported[i] == NULL)
            {
                err = true;
                break;
            }

            ++m_nCountAppSupported;
            strlcpy(m_pAppSupported[i],getWhiteList.whiteListApp.name[i],
                    WHITELIST_APP_NAME_LENGTH);
            WFDMMLOGL1("parseAndPopulateWhiteListApp - app = %s",
                       m_pAppSupported[i]);
        }
        if(err)
        {
            WFDMMLOGE("parseAndPopulateWhiteListApp failed");
            DeleteAppSupported();
        }
    }
    /* Freeing Memory Allocated */
    if(count > 0)
    {
        if(getWhiteList.whiteListApp.name != NULL)
        {
            for(int i = 0; i < count ; i++)
            {
                if(getWhiteList.whiteListApp.name[i] != NULL)
                    MM_Free(getWhiteList.whiteListApp.name[i]);
            }
            MM_Free(getWhiteList.whiteListApp.name);
        }
    }
    WFDMMLOGE1("parseAndPopulateWhiteListApp - Count = %u",
                                                        m_nCountAppSupported);
}

/*!*************************************************************************
 * @brief     DTOR
 *
 * @param[in] NONE
 *
 * @return    NONE
 *
 * @note
 ***************************************************************************/
VideoSource::~VideoSource()
{
    OMX_ERRORTYPE result = OMX_ErrorNone;
    int timeoutCnt = 1000; /*timeout counter*/
    MM_MSG_PRIO(MM_GENERAL, MM_PRIO_MEDIUM,
                "WFDMMSourceVideoSource::~VideoSource()");

    if(m_hVideoSourceThread)
    {
        MM_MSG_PRIO(MM_GENERAL, MM_PRIO_MEDIUM, "WFDMMSourceVideoSource::~VideoSource Deleting m_hVideoSourceThread");
        MM_Delete(m_hVideoSourceThread);
        m_hVideoSourceThread = NULL;
        MM_MSG_PRIO(MM_GENERAL, MM_PRIO_MEDIUM, "WFDMMSourceVideoSource::~VideoSource m_hVideoSourceThread deleted");
    }

    // Delete m_pVencoder after deleting m_hVideoSourceThread, so that code in videosource thread signal processing
    // eg VIDEO_SOURCE_STOP that needs venc to be in scope completes first before
    if(m_pVencoder)
    {
        MM_Delete(m_pVencoder);
    }

    if(m_pWFDMMSrcStats)
    {
        MM_Delete(m_pWFDMMSrcStats);
        m_pWFDMMSrcStats = NULL;
    }

    ReleaseResources();
    if(m_WFDMMService)
    {
        //WFDMMService::RemoveService();
        //delete(m_WFDMMService);
        m_WFDMMService = NULL;
    }

    DeleteAppSupported();

    WFDMMLOGH("~WFDMMSourceVideoSource completed");
}

/*!*************************************************************************
 * @brief     Configures the source
 *
 * @param[in] nFrames The number of frames to to play.
 * @param[in] nBuffers The number of buffers allocated for the session.
 * @param[in] pFrameDeliverFn Frame delivery callback.
 * @param[in] bFrameSkipEnabled frame skipping enabled
 * @param[in] nFrameSkipLimitInterval frame skipping time interval
 *
 * @return    OMX_ERRORTYPE
 *
 * @note
 **************************************************************************/
OMX_ERRORTYPE VideoSource::Configure(
    VideoEncStaticConfigType* pConfig,
    OMX_S32 nBuffers,
    FrameDeliveryFnType pFrameDeliverFn,
    eventHandlerType pEventHandlerFn,
    OMX_BOOL bFrameSkipEnabled,
    OMX_U64 nFrameSkipLimitInterval,
    OMX_U32 nModuleId,
    void *appData)
{
    (void)nBuffers;
    (void)bFrameSkipEnabled;
    (void)nFrameSkipLimitInterval;
    OMX_ERRORTYPE result = OMX_ErrorNone;
    if(!pConfig)
    {
      WFDMMLOGE("WFDMMSourceVideoSource::bad params");
      return OMX_ErrorBadParameter;
    }// if pConfig
    memcpy(&m_sConfig, pConfig, sizeof(m_sConfig));
    m_pEventHandlerFn = pEventHandlerFn;
    m_pFrameDeliverFn = pFrameDeliverFn;
    m_nModuleId       = nModuleId;
    m_appData         = appData;
    if(m_pHdcpHandle &&
      (m_pHdcpHandle->m_eHdcpSessionStatus == HDCP_STATUS_SUCCESS))
    {
        m_bHdcpSessionValid = OMX_TRUE;
    }
    if(m_eVideoSrcState ==  WFDMM_VideoSource_STATE_INIT)
    {
        result = m_pVencoder->configure(pConfig,
                                        &VideoSourceEventHandlerCb,
                                        &VideoSourceFrameDelivery,
                                        m_bHdcpSessionValid,
                                        WFDMM_VENC_MODULE_ID,
                                        this);

        if(result!= OMX_ErrorNone)
        {
            WFDMMLOGE("Failed to configure encoder");
            return result;
        }

        result = CreateResources();

        if(result!= OMX_ErrorNone)
        {
            WFDMMLOGE("Failed to create Resources");
            return result;
        }

        if(!m_bHdcpSessionValid && m_bFillerNaluEnabled)
        {
            /*------------------------------------------------------------------
             For non HDCP session
            --------------------------------------------------------------------
            */
            m_pFillerDataOutPtr = (unsigned char*)sFillerNALU;
        }

        WFDMMLOGH("WFDMMSourceVideoSource::Allocated all resources");
        m_eVideoSrcState = WFDMM_VideoSource_STATE_IDLE;
    }
    else if(m_eVideoSrcState ==  WFDMM_VideoSource_STATE_PAUSE)
    {
        // call reconfigure to support DRC
        WFDMMLOGM("Calling reconfigure");
        result = m_pVencoder->reconfigure(pConfig);
     }

    if(m_eVideoStreamingSrcState ==  WFDMM_VideoSource_STATE_INIT)
    {
        // Start WFDMMService iff support from CFG file is present
        if(isCfgSupportForStreaming())
        {
            m_WFDMMService = WFDMMService::start();
            if(m_WFDMMService == NULL)
            {
                WFDMMLOGE("Failed: to create m_WFDMMService");
            }
            else
            {
                WFDMMLOGH1("Added m_WFDMMService = %p", m_WFDMMService);

                WFDMMService::addCallback(WFDBinderCallback, this);
                m_eVideoStreamingSrcState = WFDMM_VideoSource_STATE_IDLE;
            }
        }
        else
        {
            WFDMMLOGH("Not creating WFDMMService for this WFD session");
        }
    }
    return result;
}

/*=============================================================================

         FUNCTION:          AllocateHDCPResources

         DESCRIPTION:
*//**       @brief          Responisble for HDCP specific resource allocation
                            for VideoSource
*//**
@par     DEPENDENCIES:
                            Should be called once Encoder module is configured.
*//*
         PARAMETERS:
*//**       @param          None

*//*     RETURN VALUE:
*//**       @return
                            OMX_ErrorNone or other Error
@par     SIDE EFFECTS:
                            None
*//*=========================================================================*/

OMX_ERRORTYPE VideoSource::AllocateHDCPResources()
{
    OMX_ERRORTYPE result = OMX_ErrorNone;
    WFDMMLOGH("AllocateHDCPResources");
    if(!m_bHdcpSessionValid)
    {
        WFDMMLOGE("HDCP session is not established");
        return OMX_ErrorInvalidState;
    }

    if(!m_pVencoder)
    {
        WFDMMLOGE("Invalid encoder component");
        return OMX_ErrorBadParameter;
    }

    /*-------------------------------------------------------------------------
     Both output buffers and ION buffers for HDCP should be same in number
    ---------------------------------------------------------------------------
    */

    /*--------------------------------------------------------------------------
     Create Q for holding OMX BufferHeaders in case of HDCP session which will
     be delivered to MUX
    ---------------------------------------------------------------------------
    */

    OMX_S32 buffer_size = m_pVencoder->GetOutBufSize();
    m_pHDCPOutputBufQ = MM_New_Args(SignalQueue,
                                    (10, sizeof(OMX_BUFFERHEADERTYPE*)));
    if(!m_pHDCPOutputBufQ)
    {
        result = OMX_ErrorInsufficientResources;
        WFD_OMX_ERROR_CHECK(result,
            "Could not create HDCPOutputBufQ");
    }

    /*-----------------------------------------------------------------
     Create the array of OMX BufferHeaders in case of HDCP session which
     will be delivered to MUX.
    -------------------------------------------------------------------
    */

    m_pVideoSourceOutputBuffers = MM_New_Array(OMX_BUFFERHEADERTYPE*,
                                               m_nNumBuffers);
    if(!m_pVideoSourceOutputBuffers)
    {
        result = OMX_ErrorInsufficientResources;
        WFD_OMX_ERROR_CHECK(result,
            "Could not allocate VideoSourceOutputBufferHeaders");
    }

    /*-----------------------------------------------------------------
     Allocate ION buffers for HDCP output buffers
    -------------------------------------------------------------------
    */

    m_pHdcpOutBuffers = (struct buffer*)calloc(m_nNumBuffers,sizeof(buffer));
    if(!m_pHdcpOutBuffers)
    {
        result =  OMX_ErrorInsufficientResources;
        WFD_OMX_ERROR_CHECK(result,"Failed to allocate HDCP output buffers");
    }

    ionfd = open("/dev/ion",  O_RDONLY);
    if (ionfd < 0)
    {
        result = OMX_ErrorInsufficientResources;
        WFD_OMX_ERROR_CHECK(result,"Failed to open ion device");
    }

    WFDMMLOGE1("Opened ion device = %d\n", ionfd);

    for (int i = 0; i < m_nNumBuffers; i++)
    {

        buff_hdr_extra_info* pExtraInfo = NULL;
        /*-----------------------------------------------------------------
         Allocate OMX BufferHeaders
        -----------------------------------------------------------
        */
        m_pVideoSourceOutputBuffers[i] = (OMX_BUFFERHEADERTYPE* )\
                            MM_Malloc(sizeof(OMX_BUFFERHEADERTYPE));

        if(!m_pVideoSourceOutputBuffers[i])
        {
            result = OMX_ErrorInsufficientResources;
            WFD_OMX_ERROR_CHECK(result,
                            "Could not allocate VideoSourceOutputBuffers");
        }


        pExtraInfo = static_cast<buff_hdr_extra_info*>
                            (MM_Malloc(sizeof(*pExtraInfo)));

        if(!pExtraInfo)
        {
            m_pVideoSourceOutputBuffers[i]->pAppPrivate = NULL;
            result = OMX_ErrorInsufficientResources;
            WFD_OMX_ERROR_CHECK(result,
                               "Could not allocate ExtraInfo");
        }

        /*-----------------------------------------------------------------
         Populate AppPrivate with extra info to be populated later
         ------------------------------------------------------------------
        */

        m_pVideoSourceOutputBuffers[i]->pAppPrivate = pExtraInfo;

        /*-----------------------------------------------------------------
         Nullify PlatformPrivate because this will be populated with ION buffer
         ------------------------------------------------------------------
        */
        m_pVideoSourceOutputBuffers[i]->pPlatformPrivate = NULL;

        /*-----------------------------------------------------------------
         Nullify outputPortPrivate because this will be populated with OMX
         BufferHeader received from encoder/from the Blank Frame Q
         ------------------------------------------------------------------
        */
        m_pVideoSourceOutputBuffers[i]->pOutputPortPrivate = NULL;

        /*-----------------------------------------------------------------
         Allocate ION memory for the HDCP output buffers
         ------------------------------------------------------------------
        */

        int memfd = -1;

        if(buffer_size > WFDMM_VIDEO_SRC_ION_MEM_LIMIT)
        {
            buffer_size = WFDMM_VIDEO_SRC_ION_MEM_LIMIT;
        }
        memfd = allocate_ion_mem((unsigned int)buffer_size,
                                 &(m_pHdcpOutBuffers[i].handle), ionfd,
                                 ION_QSECOM_HEAP_ID,(OMX_BOOL) OMX_FALSE);
        if(memfd < 0)
        {
            WFDMMLOGE("Failed to allocate ion memory for HDCP buffers");
            return OMX_ErrorInsufficientResources;
        }

        WFDMMLOGE1("memfd = %d ", memfd);

        m_pHdcpOutBuffers[i].start = (unsigned char *)
        mmap(NULL, buffer_size, PROT_READ | PROT_WRITE, MAP_SHARED, memfd, 0);
        m_pHdcpOutBuffers[i].length = buffer_size;
        m_pHdcpOutBuffers[i].fd = memfd;
        m_pHdcpOutBuffers[i].offset = 0;
        m_pHdcpOutBuffers[i].index = i;
        WFDMMLOGH3("allocated buffer(%p) of size = %ld, fd = %d",
                m_pHdcpOutBuffers[i].start, buffer_size, memfd);
        if (m_pHdcpOutBuffers[i].start == MAP_FAILED)
        {
            WFDMMLOGE("Could not allocate ION buffers");
            return OMX_ErrorInsufficientResources;
        }

        m_pVideoSourceOutputBuffers[i]->nSize             =
                        sizeof(*(m_pVideoSourceOutputBuffers[i]));

        m_pVideoSourceOutputBuffers[i]->nVersion.nVersion =
                        static_cast<OMX_U32>(OMX_SPEC_VERSION);

        m_pVideoSourceOutputBuffers[i]->pBuffer           =
                        reinterpret_cast<OMX_U8*>(m_pHdcpOutBuffers[i].start);

        m_pVideoSourceOutputBuffers[i]->nAllocLen         = buffer_size;

        m_pVideoSourceOutputBuffers[i]->nInputPortIndex   = 0;

        m_pVideoSourceOutputBuffers[i]->nOutputPortIndex  = 1;

        m_pVideoSourceOutputBuffers[i]->pPlatformPrivate       =
                        reinterpret_cast<OMX_U8*>(&m_pHdcpOutBuffers[i]);

        /*-----------------------------------------------------------------
         Push the allocated buffer to the Q now that all its fields are
         properly polulated
        -----------------------------------------------------------------
        */

        m_pHDCPOutputBufQ->Push(&(m_pVideoSourceOutputBuffers[i]),
                                sizeof(m_pVideoSourceOutputBuffers[i]));
    }//for loop allocate buffer on ion memory

    if(m_bFillerNaluEnabled)
    {
      WFDMMLOGE("Allocate filler Nalu buffers");

      m_nFillerInFd = allocate_ion_mem(FILLER_ION_BUF_SIZE, &m_hFillerInHandle,
                                       ionfd, ION_QSECOM_HEAP_ID , OMX_FALSE);

      if(m_nFillerInFd <= 0)
      {
          WFDMMLOGE("Failed to allocate In FillerNalu ION buffer");
      }
      else
      {
          m_pFillerDataInPtr = (unsigned char*)
                         mmap(NULL, FILLER_ION_BUF_SIZE, PROT_READ | PROT_WRITE,
                              MAP_SHARED, m_nFillerInFd, 0);
          if(m_pFillerDataInPtr == NULL)
          {
            WFDMMLOGE("Failed to allocate In FillerNalu buffer mmap");
          }
          else
          {
            //Initialize the input buffer with fixed Filler NALU
            memcpy(m_pFillerDataInPtr, sFillerNALU, sizeof(sFillerNALU));
          }
      }

      m_nFillerOutFd = allocate_ion_mem(FILLER_ION_BUF_SIZE,
        &m_hFillerOutHandle, ionfd, ION_QSECOM_HEAP_ID, OMX_FALSE);

      if(m_nFillerOutFd <= 0)
      {
          WFDMMLOGE("Failed to allocate Out FillerNalu ION buffer");
      }
      else
      {
          m_pFillerDataOutPtr = (unsigned char*)
                       mmap(NULL, FILLER_ION_BUF_SIZE, PROT_READ | PROT_WRITE,
                                                MAP_SHARED, m_nFillerOutFd, 0);
          if(m_pFillerDataOutPtr == NULL)
          {
            WFDMMLOGE("Failed to allocate In FillerNalu buffer mmap");
          }
      }
    }

    if(m_bBlankFrameSupport)
    {
        WFDMMLOGH("Allocate Blank frame i/p ION handle");

        m_nBlankFrameInFd = allocate_ion_mem(FILLER_ION_BUF_SIZE,
            &m_hBlankFrameInHandle,ionfd, ION_QSECOM_HEAP_ID, OMX_FALSE);

        if(m_nBlankFrameInFd <= 0)
        {
            result = OMX_ErrorInsufficientResources;
            WFD_OMX_ERROR_CHECK(result,
                "Failed to allocate I/P BlankFrame ION buffer");
        }
        else
        {
            m_pBlankFrameDataInPtr = (unsigned char*)
                    mmap(NULL, FILLER_ION_BUF_SIZE, PROT_READ | PROT_WRITE,
                        MAP_SHARED, m_nBlankFrameInFd, 0);
            if(m_pBlankFrameDataInPtr == NULL)
            {
                result = OMX_ErrorInsufficientResources;
                WFD_OMX_ERROR_CHECK(result,
                    "Failed to allocate I/P BlankFrame buffer mmap");
            }
            else
            {
                //Initialize the input buffer with fixed Blank frame data
                memcpy(m_pBlankFrameDataInPtr, m_pBlankFrame,
                    static_cast<size_t>(m_nBlankFrameSize));
            }
        }

    }

EXIT:
    return result;
}

/*=============================================================================

         FUNCTION:          DeallocateHDCPResources

         DESCRIPTION:
*//**       @brief          Responisble for HDCP specific resource de-allocation
                            for VideoSource
*//**
@par     DEPENDENCIES:
                            Should be called once Encoder module is configured.
*//*
         PARAMETERS:
*//**       @param          None

*//*     RETURN VALUE:
*//**       @return
                            OMX_ErrorNone or other Error
@par     SIDE EFFECTS:
                            None
*//*=========================================================================*/

OMX_ERRORTYPE VideoSource::DeallocateHDCPResources()
{
    if(!m_bHdcpSessionValid)
    {
        return OMX_ErrorNone;
    }
    WFDMMLOGE("Deallocating HDCP resources");

    struct ion_handle_data handle_data;
    int ret = -1;

    if(m_pVideoSourceOutputBuffers)
    {
        for(unsigned int i = 0; i < (unsigned int)m_nNumBuffers; i++)
        {
            if(m_pVideoSourceOutputBuffers[i])
            {
                if((m_pHdcpHandle != NULL) &&(m_pHdcpOutBuffers != NULL))
                {
                    /*---------------------------------------------------------
                     Unmap the memory
                    -----------------------------------------------------------
                    */
                    if(m_pHdcpOutBuffers[i].start != NULL)
                    {
                        if(munmap(m_pHdcpOutBuffers[i].start,
                                  m_pHdcpOutBuffers[i].length) == -1)
                        {
                            WFDMMLOGE2("error in munmap at idx %d :%s",
                                       i, strerror(errno));
                        }
                        m_pHdcpOutBuffers[i].start = NULL;
                        m_pHdcpOutBuffers[i].length = 0;
                    }

                    /*---------------------------------------------------------
                     Free the ION handle
                    -----------------------------------------------------------
                    */

                    if(m_pHdcpOutBuffers[i].handle != 0)
                    {
                        handle_data.handle = m_pHdcpOutBuffers[i].handle;
                        ret = ioctl(ionfd, ION_IOC_FREE, &handle_data);
                        if(ret)
                        {
                            WFDMMLOGE2("Error in freeing handle at idx %d : %d",
                                i,ret);
                        }
                        m_pHdcpOutBuffers[i].handle = 0;
                    }

                    /*---------------------------------------------------------
                     Close the fd
                    -----------------------------------------------------------
                    */

                    if(m_pHdcpOutBuffers[i].fd > 0)
                    {
                        close(m_pHdcpOutBuffers[i].fd);
                        WFDMMLOGH2("closing hdcp ion fd = %d ret type = %d",
                                   m_pHdcpOutBuffers[i].fd, ret);
                        m_pHdcpOutBuffers[i].fd = -1;
                    }
                }

                if(m_pVideoSourceOutputBuffers[i]->pAppPrivate)
                {
                    MM_Free(m_pVideoSourceOutputBuffers[i]->pAppPrivate);
                    m_pVideoSourceOutputBuffers[i]->pAppPrivate = NULL;
                }

                MM_Free(m_pVideoSourceOutputBuffers[i]);
                m_pVideoSourceOutputBuffers[i] = NULL;

            }
        }

        if(m_nBlankFrameInFd> 0)
        {
            if(m_pBlankFrameDataInPtr)
            {
                munmap(m_pBlankFrameDataInPtr, FILLER_ION_BUF_SIZE);
                m_pBlankFrameDataInPtr = NULL;
            }

            if(m_hBlankFrameInHandle!= 0)
            {
                handle_data.handle = m_hBlankFrameInHandle;
                ret = ioctl(ionfd, ION_IOC_FREE, &handle_data);
                m_hBlankFrameInHandle = 0;
            }

            close(m_nBlankFrameInFd);
            WFDMMLOGH2("closing blank frame ion fd = %d ret type = %d \n",
                        m_nBlankFrameInFd, ret);
            m_nBlankFrameInFd = -1;
        }

        if(m_nFillerInFd > 0)
        {
            if(m_pFillerDataInPtr)
            {
                munmap(m_pFillerDataInPtr, FILLER_ION_BUF_SIZE);
                m_pFillerDataInPtr = NULL;
            }

            if(m_hFillerInHandle != 0)
            {
                handle_data.handle = m_hFillerInHandle;
                ret = ioctl(ionfd, ION_IOC_FREE, &handle_data);
                m_hFillerInHandle = 0;
            }

            close(m_nFillerInFd);
            WFDMMLOGH2("closing hdcp filler ion fd = %d ret type = %d \n",
                       m_nFillerInFd, ret);
            m_nFillerInFd = -1;
        }

        if(m_nFillerOutFd > 0)
        {
            if(m_pFillerDataOutPtr)
            {
                munmap(m_pFillerDataOutPtr, FILLER_ION_BUF_SIZE);
                m_pFillerDataOutPtr = NULL;
            }

            if(m_hFillerOutHandle != 0)
            {
                handle_data.handle = m_hFillerOutHandle;
                ret = ioctl(ionfd, ION_IOC_FREE, &handle_data);
                m_hFillerOutHandle = 0;
            }

            close(m_nFillerOutFd);
            WFDMMLOGH2("closing hdcp filler ion fd = %d ret type = %d \n",
                 m_nFillerOutFd, ret);
            m_nFillerOutFd = -1;
        }

        if(m_pHdcpOutBuffers)
        {
            MM_Free(m_pHdcpOutBuffers);
            m_pHdcpOutBuffers = NULL;
        }

        MM_Delete_Array(m_pVideoSourceOutputBuffers);
        m_pVideoSourceOutputBuffers = NULL;
    }

    if(m_pHDCPOutputBufQ)
    {
        MM_Delete(m_pHDCPOutputBufQ);
        m_pHDCPOutputBufQ = NULL;
    }

    if(ionfd > 0)
    {
        WFDMMLOGH1("closing ion fd = %d",ionfd);
        close(ionfd);
        ionfd = -1;
    }

    return OMX_ErrorNone;
}

/*!*************************************************************************
 * @brief     Start video source
 *
 * @param[in] NONE
 *
 * @return    OMX_ERRORTYPE
 *
 * @note      NONE
 **************************************************************************/
OMX_ERRORTYPE VideoSource::Start()
{
    OMX_ERRORTYPE result = OMX_ErrorNone;
    m_bHandleStandby = OMX_FALSE;
    m_nBlankFramesDelivered = 0;

    if(WFD_LIKELY(m_pWFDMMSrcStats))
    {
        m_pWFDMMSrcStats->resetStats();
    }

    // make sure we've been configured
    MM_MSG_PRIO(MM_GENERAL, MM_PRIO_HIGH,
                "WFDMMSourceVideoSource::SourceThread Start");
    if(!m_pVencoder)
    {
        result = OMX_ErrorUndefined;
        WFD_OMX_ERROR_CHECK(result,"Failed to start!");
    }
    if(m_hVideoSourceThread)
    {
        m_hVideoSourceThread->SetSignal(VIDEO_SOURCE_START);
    }

EXIT:
    return result;
}

/*==============================================================================
         FUNCTION:            ExecuteStart()

         DESCRIPTION:
*//**       @brief           call from thread to Execute Start
*//**
@par     DEPENDENCIES:
                            None
*//*
         PARAMETERS:
*//*         @param         None

*//*     RETURN VALUE:
*//**       @return
                            OMX_ErrorNone  or error
@par     SIDE EFFECTS:
                            None
*//*==========================================================================*/

OMX_ERRORTYPE VideoSource::ExecuteStart()
{
    OMX_ERRORTYPE result = OMX_ErrorNone;
    WFDMMLOGH("VideoSource::ExecuteStart");

    if(m_WFDMMService)
    {
        WFDMMLOGH("VideoSource:: In Start: Restore streaming support status");
        WFDMMService::updateStreamingFeatureStatus(m_bStreamingSupport);
    }

    if(m_bStreamingEnabled == OMX_TRUE)
    {
        WFDMMLOGH("VideoSource::ExecuteStreamingStart");

        /*----------------------------------------------------------------------
         1. Need  to fill what are the required conditions for the start of
          streaming after pause
        2. send callback message to binder  that start playback again
        ------------------------------------------------------------------------
       */
        if(m_eVideoStreamingSrcState == WFDMM_VideoSource_STATE_PLAY)
        {
            WFDMMLOGH("Already in Streaming, play Ignore");
        }
        else if(m_eVideoStreamingSrcState == WFDMM_VideoSource_STATE_PAUSE)
        {
            result = sendcallbackToBinder(
                                  (StreamingEventType)STREAMING_RESUME_PLAYBACK,
                                  (void*)NULL);
            /*------------------------------------------------------------------
               setting state of the mirror to streaming state to keep the state
               requested from Framework
            --------------------------------------------------------------------
            */
//            pauseWFD(0);
            if(result != OMX_ErrorNone)
            {
                result = sendcallbackToBinder(
                                (StreamingEventType)STREAMING_START_FAIL,
                                (void*)NULL);
                result = OMX_ErrorInvalidState;
                WFD_OMX_ERROR_CHECK(result,"Failed to start Streaming!");
            }
            m_eVideoStreamingSrcState = WFDMM_VideoSource_STATE_PLAY;
        }
        else
        {
            result = OMX_ErrorInvalidState;
            WFD_OMX_ERROR_CHECK(result,"Failed to start Streaming!");
        }
    }
    else
    {
        if(m_eVideoSrcState == WFDMM_VideoSource_STATE_PLAY)
        {
            WFDMMLOGH("Already in Playing, Ignore");
        }
        else if(m_eVideoSrcState == WFDMM_VideoSource_STATE_PAUSE)
        {
            result = m_pVencoder->Resume();
            WFD_OMX_ERROR_CHECK(result,"Failed to ExecuteMirroringStart!");
        }
        else if(m_eVideoSrcState == WFDMM_VideoSource_STATE_IDLE)
        {
            result = m_pVencoder->Start();
            WFD_OMX_ERROR_CHECK(result,"Failed to m_pVencoder->Start()!");
            if(m_pEventHandlerFn)
            {
                m_pEventHandlerFn( m_appData, m_nModuleId,
                                   WFDMMSRC_VIDEO_SESSION_START,
                                   OMX_ErrorNone, 0);
            }
        }
        else
        {
            result = OMX_ErrorInvalidState;
            WFD_OMX_ERROR_CHECK(result,"Failed to ExecuteMirroringStart!");
        }
        m_eVideoSrcState = WFDMM_VideoSource_STATE_PLAY;
    }
EXIT:
    return result;
}

/*!*************************************************************************
 * @brief     Pause video source
 *
 * @param[in] NONE
 *
 * @return    OMX_ERRORTYPE
 *
 * @note      NONE
 **************************************************************************/
OMX_ERRORTYPE VideoSource::Pause()
{
    OMX_ERRORTYPE result = OMX_ErrorInvalidState;
    m_bPauseSuccess = OMX_FALSE;

    if(m_hVideoSourceThread)
    {
        m_hVideoSourceThread->SetSignal(VIDEO_SOURCE_PAUSE);
    }

    int pausecnt = 10;
    while((m_bPauseSuccess == OMX_FALSE) && pausecnt--)
    {
       MM_Timer_Sleep(5);
    }

    result = OMX_ErrorNone;
    if(m_bPauseSuccess != OMX_TRUE)
    {
      result = OMX_ErrorInvalidState;
    }
    return result;
}

/*==============================================================================
         FUNCTION:            ExecutePause()

         DESCRIPTION:
*//**       @brief           pause implementation
*//**
@par     DEPENDENCIES:
                            None
*//*
         PARAMETERS:
*//*         @param         None

*//*     RETURN VALUE:
*//**       @return
                            OMX_ErrorNone  or error
@par     SIDE EFFECTS:
                            None
*//*==========================================================================*/

OMX_ERRORTYPE VideoSource::ExecutePause()
{
    WFDMMLOGH("VideoSource::ExecutePause()");
    OMX_ERRORTYPE result = OMX_ErrorInvalidState;

    if(m_WFDMMService)
    {
        WFDMMLOGH("VideoSource:: In Pause: Disable streaming support status");
        WFDMMService::updateStreamingFeatureStatus(false);
    }

    if(m_bStreamingEnabled == OMX_TRUE)
    {
        if(m_eVideoStreamingSrcState == WFDMM_VideoSource_STATE_PLAY)
        {
            //pauseWFD(1);
            result = sendcallbackToBinder(
                                (StreamingEventType)STREAMING_CLOSE_REQ,
                                (void*)NULL);
        }
        FlushStreamingQueue();
        MM_Timer_Sleep(2);
        m_eVideoStreamingSrcState = WFDMM_VideoSource_STATE_PAUSE;
        m_bPauseSuccess = OMX_TRUE;
    }
    else
    {
        if(m_eVideoSrcState == WFDMM_VideoSource_STATE_PLAY)
        {
            m_eVideoSrcState = WFDMM_VideoSource_STATE_PAUSING;
            if(m_pVencoder)
            {
                result = m_pVencoder->Pause();
                WFD_OMX_ERROR_CHECK(result,"Failed to pause!");
            }
            m_eVideoSrcState = WFDMM_VideoSource_STATE_PAUSE;
            m_bPauseSuccess = OMX_TRUE;
        }
        else
        {
            result = OMX_ErrorInvalidState;
            WFD_OMX_ERROR_CHECK(result,"Failed to pause!");
        }
    }
EXIT:
    return result;
}

/*!*************************************************************************
 * @brief     Finish video source
 *
 * @param[in] NONE
 *
 * @return    OMX_ERRORTYPE
 *
 * @note      NONE
 **************************************************************************/
OMX_ERRORTYPE VideoSource::Finish()
{
    OMX_ERRORTYPE result = OMX_ErrorNone;
    WFDMMLOGH("VideoSource Finish");

    if(m_hVideoSourceThread)
    {
        m_hVideoSourceThread->SetSignal(VIDEO_SOURCE_STOP);
    }

    return result;
}

OMX_ERRORTYPE VideoSource::ExecuteFinish()
{
    OMX_ERRORTYPE result = OMX_ErrorNone;
    WFDMMLOGH("VideoSource ExecuteFinish");

    if(m_eVideoSrcState == WFDMM_VideoSource_STATE_IDLE ||
       m_eVideoSrcState == WFDMM_VideoSource_STATE_INIT)
    {
        WFDMMLOGH("Already Stopped. Ignore");
        return OMX_ErrorNone;
    }

    if(m_WFDMMService)
    {
        WFDMMLOGH("WFDMMService is present. Reset flag. Adios");
        WFDMMService::updateStreamingFeatureStatus(false);
    }

    if((m_bStreamingEnabled == OMX_TRUE) &&
       (m_eVideoStreamingSrcState != WFDMM_VideoSource_STATE_STOPPING)
       && !mbStopWhileStreaming)
    {
        result = sendcallbackToBinder(
                 (StreamingEventType)STREAMING_CLOSE_REQ,
                              NULL);
        mbStopWhileStreaming = true;
        FlushStreamingQueue();
        while(m_pStreamingBufQ->GetSize() != m_nStreamingBufCnt)
        {
            WFDMMLOGH2("Waiting for %ld input buffers to be returned = %ld",
                       m_nStreamingBufCnt,m_pStreamingBufQ->GetSize());
            MM_Timer_Sleep(2);
//            ExecuteRenderFrame();
        }
        m_eVideoStreamingSrcState = WFDMM_VideoSource_STATE_STOPPING;
        return OMX_ErrorNone;
    }
    if(m_pVencoder)
    {
        WFDMMLOGH("Calling Encoder Stop");
        result = m_pVencoder->Stop();
        WFD_OMX_ERROR_CHECK(result,"Failed to m_pVencoder->Stop()");
    }
    m_eVideoSrcState = WFDMM_VideoSource_STATE_IDLE;
    mbStopWhileStreaming = false;
    mbVideoSourceStopped = OMX_TRUE;

EXIT:
    return OMX_ErrorNone;
}

/*!*************************************************************************
 * @brief     Get buffer header
 *
 * @param[in] NONE
 *
 * @return    OMX_BUFFERHEADERTYPE**
 *
 * @note      NONE
 **************************************************************************/
OMX_BUFFERHEADERTYPE** VideoSource::GetBuffers()
{
    OMX_BUFFERHEADERTYPE** ppBuffers;
    if(m_pVencoder)
    {
        ppBuffers = m_pVencoder->GetOutputBuffHdrs();
        MM_MSG_PRIO(MM_GENERAL, MM_PRIO_HIGH,
                    "WFDMMSourceVideoSource::GetBuffers success");
    }
    else
    {
        ppBuffers = NULL;
    }
    return ppBuffers;
}

/*!*************************************************************************
 * @brief     Get number of buffer
 *
 * @param[in] NONE
 * @param[out]pnBuffers
 *
 * @return    OMX_ERRORTYPE
 *
 * @note      NONE
 **************************************************************************/
OMX_ERRORTYPE VideoSource::GetOutNumBuffers(
    OMX_U32 nPortIndex, OMX_U32* pnBuffers)
{
    if(nPortIndex != 0)
    {
        return OMX_ErrorBadPortIndex;
    }
    if(!pnBuffers)
    {
        return OMX_ErrorBadParameter;
    }
    if(m_pVencoder)
    {
        *pnBuffers = m_pVencoder->GetNumOutBuf();
        MM_MSG_PRIO1(MM_GENERAL, MM_PRIO_HIGH,
                 "WFDMMSourceVideoSource::GetOutNumBuffers = %ld",
                 *pnBuffers);
    }
    return OMX_ErrorNone;
}

/*!*************************************************************************
 * @brief     Changes the frame rate
 *            The frame rate will take effect immediately.
 * @param[in] nFramerate New frame rate
 *
 * @return    OMX_ERRORTYPE
 *
 * @note
 **************************************************************************/
OMX_ERRORTYPE VideoSource::ChangeFrameRate(OMX_S32 nFramerate)
{
    OMX_ERRORTYPE result = OMX_ErrorNone;
    MM_MSG_PRIO1(MM_GENERAL, MM_PRIO_MEDIUM,
                 "WFDMMSourceVideoSource::ChangeFrameRate %ld", nFramerate);
    return result;
}

/*!*************************************************************************
 * @brief     Changes the bit rate
 *            The bit rate will take effect immediately.
 *
 * @param[in] nBitrate The new bit rate.
 *
 * @return    OMX_ERRORTYPE
 * @note      None
 **************************************************************************/
OMX_ERRORTYPE VideoSource::ChangeBitrate(OMX_S32 nBitrate)
{
    OMX_ERRORTYPE result = OMX_ErrorNone;
    MM_MSG_PRIO1(MM_GENERAL, MM_PRIO_MEDIUM,
                "WFDMMSourceVideoSource::ChangeBitrate %ld", nBitrate);
    if(m_bStreamingEnabled == OMX_TRUE)
    {
        return OMX_ErrorNotImplemented;
    }
    if(nBitrate > 0)
    {
        if(m_pVencoder)
        {
          m_pVencoder->ChangeBitrate(nBitrate);
        }
    } // if nBitRate
    return result;
}

/*!*************************************************************************
 * @brief     Request Intra VOP
 *
 * @param[in] NONE
 *
 * @return    OMX_ERRORTYPE
 *
 * @note      NONE
 **************************************************************************/
OMX_ERRORTYPE VideoSource::RequestIntraVOP()
{
    OMX_ERRORTYPE result = OMX_ErrorNone;
    MM_MSG_PRIO(MM_GENERAL, MM_PRIO_MEDIUM,
                "WFDMMSourceVideoSource::RequestIntraVOP");
    if(m_bStreamingEnabled == OMX_TRUE)
    {
        return OMX_ErrorNotImplemented;
    }
    if(m_pVencoder)
    {
        result = m_pVencoder->RequestIntraVOP();
    }
    return result;
}

/*=============================================================================

         FUNCTION:          HandleStandby

         DESCRIPTION:
*//**       @brief          Responisble for performing actions to handle
                            standby from upper layers
*//**
@par     DEPENDENCIES:
                            Should be called once Encoder module is configured.
*//*
         PARAMETERS:
*//**       @param          None

*//*     RETURN VALUE:
*//**       @return
                            OMX_ErrorNone or other Error
@par     SIDE EFFECTS:
                            None
*//*=========================================================================*/
OMX_ERRORTYPE VideoSource::HandleStandby()
{
    OMX_ERRORTYPE result = OMX_ErrorNone;
    WFDMMLOGH("VideoSource HandleStandby");

    //No point in going through the rigmarole in DS case
    if(m_eStreamingStatus == STREAMING_SUCCESS)
    {
        return OMX_ErrorNone;
    }

    if(OMX_TRUE == m_bBlankFrameSupport)
    {
        OMX_TICKS frameInterval = static_cast<OMX_TICKS>
                                  (1000000 / m_sConfig.nFramerate);
        unsigned long sleepInterval = static_cast<unsigned long>
                                      (frameInterval / 1000); //in ms

        WFDMMLOGH2("Blank Frame Interval = %lld us, Blank Frames sent = %d",
                   frameInterval, m_nBlankFramesDelivered);

        m_bHandleStandby = OMX_TRUE; //Force dropping of encoder ouptut
        /*----------------------------------------------------------------------
         After setting the flag there might be chances of a context switch while
         encoder was delivering a frame to MUX and fails to read the flag and
         proceeds to deliver the frame to MUX. So allow for a context switch to
         ensure that no more frames from encoder are sent to MUX in any case,
         else it might result in a blank frame interspersed with an actual
         encoded frame being rendered on sink (a catastrophe by any standard!).
         This is to avoid unecessary synchronization between HandleStandby() and
         FrameDeliveryFn().
         ------------------------------------------------------------------
        */

        MM_Timer_Sleep(10);

        if(m_eVideoSrcState == WFDMM_VideoSource_STATE_PLAY)
        {
            for(int i = 0; i < m_nMaxBlankFrames &&
                m_eVideoSrcState == WFDMM_VideoSource_STATE_PLAY; i++)
            {
                OMX_ERRORTYPE res = GenerateBlankFrame
                                    (m_nLastFrameTS + (i + 1) * frameInterval);
                if(res != OMX_ErrorNone)
                {
                    /*---------------------------------------------------------
                     Can't really do much here if it fails except logging. Also
                     we need to bail out from here since we don't want to be
                     stuck for MUX to deliver the frames because it never will,
                     a scenarios above layers need not tolerate.
                     ----------------------------------------------------------
                    */
                    WFDMMLOGE1("GenerateBlankFrame failed due to %x", res);
                    return OMX_ErrorUndefined;
                }
                MM_Timer_Sleep(sleepInterval);
            }
            while(m_nBlankFramesDelivered < m_nMaxBlankFrames &&
                  m_eVideoSrcState == WFDMM_VideoSource_STATE_PLAY)
            {
                MM_Timer_Sleep(10);
            }
            WFDMMLOGH1("Done Handling standby by sending %d blank frames",
                       m_nBlankFramesDelivered);
        }
        else
        {
            WFDMMLOGE("Can't generate blank frame in non-PLAY state");
            return OMX_ErrorIncorrectStateOperation;
        }
    }
    else
    {
        WFDMMLOGH("Blank frame generation not supported");
        result = OMX_ErrorUnsupportedSetting;
    }
    return result;
}

/*=============================================================================
  FUNCTION:            UpdateWFDMMStreamingQueryResponse

  DESCRIPTION:
*//**       @brief           status update from RTSP that Streaming request
                        has success or failure

*//**
@par     DEPENDENCIES:
                     None
*//*
  PARAMETERS:
*//**       @param[in]      bStatus   buffer fhaving encoded data

*//*     RETURN VALUE:
*//**       @return
                     None
@par     SIFE EFFECTS:
                     None
*//*=========================================================================*/
void VideoSource::UpdateWFDMMStreamingQueryResponse(bool bStatus)
{

    WFDMMLOGH1("UpdateWFDMMStreamingQueryResponse bStatus = %d", bStatus);
    if(m_eStreamingStatus == STREAMING_QUERY)
    {
        if(bStatus)
        {
            m_hVideoSourceThread->SetSignal(VIDEO_SOURCE_STREAMING_START);
            int pausecnt = STREAMING_WAIT_TIME_CNT;
            while((m_eStreamingStatus != STREAMING_SUCCESS) && pausecnt--)
            {
                if(m_eStreamingStatus == STREAMING_QUERY)
                {
                    MM_Timer_Sleep(5);
                }
                else
                {
                    WFDMMLOGH1("START_STREAMING  Failed pause enc= %d",
                               pausecnt);
                    m_eStreamingStatus = STREAMING_DEINIT;
                    break;
                }
                WFDMMLOGH1("START_STREAMING  Still wait to pause enc= %d",
                           pausecnt);
            }
            if(m_eStreamingStatus == STREAMING_SUCCESS)
            {
                m_hVideoSourceThread->SetSignal(
                    VIDEO_SOURCE_SET_START_STREAMING_SUCCESS);
            }
        }
        else
        {
            m_eStreamingStatus = STREAMING_FAILURE;
        }

        WFDMMLOGH1("UpdateWFDMMStreamingQueryResponse m_eStreamingStatus=%d",
                   m_eStreamingStatus);
    }
    else if(m_eStreamingStatus == STREAMING_STOPPING)
    {
        if(m_hVideoSourceThread)
        {
            m_hVideoSourceThread->SetSignal(
                            VIDEO_SOURCE_SET_STOP_STREAMING_SUCCESS);
        }
    }

}

/*=============================================================================
  FUNCTION:            directStreamingSupport

  DESCRIPTION:
*//**       @brief     update from GUI if DS support is enabled or disabled

*//**
@par     DEPENDENCIES:
                     None
*//*
  PARAMETERS:
*//**       @param[in]  enabled - boolean flag

*//*     RETURN VALUE:
*//**       @return
                     None
@par     SIFE EFFECTS:
                     None
*//*=========================================================================*/
OMX_ERRORTYPE VideoSource::DirectStreamingSupport(bool enabled)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    if((m_bStreamingEnabled == OMX_TRUE &&
        m_eVideoStreamingSrcState != WFDMM_VideoSource_STATE_PLAY) ||
        m_eVideoSrcState != WFDMM_VideoSource_STATE_PLAY)
    {
        WFDMMLOGE("Session (Mirror/Streaming) not in PLAY. Ignore DSSupport udpate");
        return OMX_ErrorInvalidState;
    }

    WFDMMLOGH1("DirectStreamingSupport enabled = %d", enabled);
    m_bStreamingSupport = enabled;

    if(m_WFDMMService)
    {
        WFDMMLOGH1("WFDMMService is present. Update the flag with status %d",
            enabled);
        WFDMMService::updateStreamingFeatureStatus(enabled);
    }
    else
    {
        WFDMMLOGE("WFDMMService is absent. UI support is ignored");
    }
    return ret;
}

/*=============================================================================

         FUNCTION:            VideoSourceFrameDelivery

         DESCRIPTION:
*//**       @brief           Function responsible for sending frames to MUX

*//**
@par     DEPENDENCIES:
                            None
*//*
         PARAMETERS:
*//**       @param[in]      pBufferHdr   buffer fhaving encoded data

     *         @param[in]      pThis          pointer to get current instance

*//*     RETURN VALUE:
*//**       @return
                            OMX_ErrorNone or other Error
@par     SIFE EFFECTS:
                            None
*//*=========================================================================*/

void VideoSource::VideoSourceFrameDelivery
(
    OMX_BUFFERHEADERTYPE* pEncBufferHdr,
    void* pThis
)
{

    WFDMMLOGH1("VideoSourceFrameDelivery to MUX %p",pEncBufferHdr);
    if(!pEncBufferHdr || !pThis)
    {
        WFDMMLOGE(" Invalid parameters to VideoSourceFrameDelivery");
        return;
    }
    VideoSource* pMe = (VideoSource*)pThis;

    if(pEncBufferHdr->nFlags & OMX_BUFFERFLAG_SYNCFRAME)
    {
        pMe->m_bDropVideoFrame = OMX_FALSE;
    }

    if((pMe->m_eVideoSrcState != WFDMM_VideoSource_STATE_PLAY
        || pMe->m_bDropVideoFrame
          || pMe->m_eStreamingStatus != STREAMING_DEINIT)
            && (pMe->m_bStreamingEnabled == OMX_FALSE))
    {
        WFDMMLOGH("Not delivering frame to MUX");
        pMe->VideoSourceFreeBuffer(pEncBufferHdr);
        return;
    }
    else if((pMe->m_eVideoStreamingSrcState != WFDMM_VideoSource_STATE_PLAY)
            && (pMe->m_bStreamingEnabled == OMX_TRUE))
    {
            WFDMMLOGH("Not delivering frame to MUX");
            pMe->VideoSourceFreeBuffer(pEncBufferHdr);
            return;
    }

    if(pMe->m_bBlankFrameSupport && pMe->m_bHandleStandby)
    {

        WFDMMLOGH("Ignoring encoder output for blank frame");
        pMe->m_pVencoder->SetFreeBuffer(pEncBufferHdr);
        return;
    }

    pMe->m_nLastFrameTS = pEncBufferHdr->nTimeStamp;

    if(pMe->m_bHdcpSessionValid)
    {
        //Adopt encryption path
        pMe->EncryptData(pEncBufferHdr);
    }
    else
    {
        if(pMe->m_bFillerNaluEnabled)
        {
            pMe->FillFillerBytesExtraData(pEncBufferHdr,NULL,1);
        }
        pMe->m_pFrameDeliverFn(pEncBufferHdr,pMe->m_appData);
    }

}

/*!*************************************************************************
 * @brief     Set Free buffer
 *
 * @param[in] pBufferHdr OMX_BUFFERHEADERTYPE passed in
 *
 * @return    OMX_ERRORTYPE
 *
 * @note      NONE
 **************************************************************************/
OMX_ERRORTYPE VideoSource::SetFreeBuffer(OMX_BUFFERHEADERTYPE* pBufferHdr)
{

#ifdef SYSTRACE_PROFILE
        ATRACE_CALL();
#endif

    OMX_ERRORTYPE result = OMX_ErrorNone;
    OMX_BUFFERHEADERTYPE* pEncOutputBuff = NULL, *pTempBufferHeader = NULL;

    if (pBufferHdr != NULL && pBufferHdr->pBuffer != NULL)
    {

#ifdef SYSTRACE_PROFILE
        ATRACE_ASYNC_END("Mux Frame", static_cast<int32_t>
                        (reinterpret_cast<uintptr_t>(pBufferHdr)));
#endif

        buff_hdr_extra_info* pExtraInfo = static_cast<buff_hdr_extra_info*>
                                          (pBufferHdr->pAppPrivate);

        bool bBlankFrame = false;

        if(pExtraInfo)
        {
            if(pExtraInfo->bBlackFrame == OMX_TRUE)
            {
                bBlankFrame = true;
            /*-------------------------------------------------------------
             NOTE: There can't be a race condition here since the same Buffer
             Header can't be worked upon by two different threads, so its
             safe to reset this flag here
            ---------------------------------------------------------------
            */
                pExtraInfo->bBlackFrame = OMX_FALSE;
            }
        }

        if(!bBlankFrame)
        {
            if(m_bHdcpSessionValid)
            {
                if(m_eStreamingStatus != STREAMING_SUCCESS)
                {
                   /*-------------------------------------------------------------
                     Extract the Buffer Header to be sent back to encoder from the
                     received one
                    ---------------------------------------------------------------
                   */
                    pEncOutputBuff = reinterpret_cast<OMX_BUFFERHEADERTYPE*>
                                 (pBufferHdr->pOutputPortPrivate);

                   /*-------------------------------------------------------------
                     Push back the buffer Header to the HDCP output Q
                    --------------------------------------------------------------
                   */
                    m_pHDCPOutputBufQ->Push(&pBufferHdr, sizeof(pBufferHdr));
                }
            }
            else
            {
            /*-------------------------------------------------------------
             Push back same buffer header to encoder as received back from
             Mux
             --------------------------------------------------------------
            */
                pEncOutputBuff = pBufferHdr;
            }
        }
        else //A few additional steps in case of a blank frame
        {
            m_nBlankFramesDelivered++;
            WFDMMLOGH2("Received Blank frame %d. %p from MUX",
                       m_nBlankFramesDelivered, pBufferHdr);
            if(m_bHdcpSessionValid)
            {
                /*----------------------------------------------------------
                  Extract the BlankFrame Buffer Header from the received one
                ------------------------------------------------------------
                */
                pTempBufferHeader = static_cast<OMX_BUFFERHEADERTYPE*>
                    (pBufferHdr->pOutputPortPrivate);
                /*----------------------------------------------------------
                  Push back the received buffer Header to the HDCP output Q
                ------------------------------------------------------------
                */
                m_pHDCPOutputBufQ->Push(&pBufferHdr, sizeof(pBufferHdr));
            }
            else
            {
                pTempBufferHeader = pBufferHdr;
            }
            /*--------------------------------------------------------------
              Push back the Blank frame buffer Header to Blank FrameQ
            ----------------------------------------------------------------
            */
            OMX_ERRORTYPE res = m_pBlankFrameQ->Push(
                &pTempBufferHeader, sizeof(pTempBufferHeader));
            /*--------------------------------------------------------------
              For blank frame case, the juggernaut stops here.
            ----------------------------------------------------------------
            */
            return result;
        }

        if(m_eVideoSrcState == WFDMM_VideoSource_STATE_PLAY)
        {
            if(WFD_LIKELY(m_pWFDMMSrcStats))
            {
                m_pWFDMMSrcStats->recordMuxStat(pEncOutputBuff);
            }
        }
        result = VideoSourceFreeBuffer(pEncOutputBuff);
    }
    else
    {
        WFDMMLOGE1("Bad params passed in to %s",__FUNCTION__);
        result = OMX_ErrorBadParameter;
    }
EXIT:
    return result;
}

/*=============================================================================

         FUNCTION:            EncryptData

         DESCRIPTION:
*//**       @brief           Function for encrypting encoded data
*//**
@par     DEPENDENCIES:
                            None
*//*
         PARAMETERS:
*//**       @param[in]      pBufferHdr   buffer fhaving encoded data

     *         @param[in]      pThis          pointer to get current instance

*//*     RETURN VALUE:
*//**       @return
                            OMX_ErrorNone or other Error
@par     SIFE EFFECTS:
                            None
*//*=========================================================================*/

void VideoSource::EncryptData(OMX_BUFFERHEADERTYPE* pEncBufferHdr)
{
    WFDMMLOGE("Received data to encrypt");
    if(!pEncBufferHdr)
    {
        WFDMMLOGE("Invalid argument to EncryptData");
        return;
    }
    OMX_BUFFERHEADERTYPE* pBuffHdr = NULL;
    OMX_ERRORTYPE eErr = m_pHDCPOutputBufQ->Pop(&pBuffHdr, sizeof(pBuffHdr),
                                                100);
    if(!pBuffHdr || eErr != OMX_ErrorNone)
    {
        WFDMMLOGE("Failed to POP from HDCP BufQ");
        m_pVencoder->SetFreeBuffer(pEncBufferHdr);
        m_pVencoder->RequestIntraVOP();
        return;
    }
    if(m_pHdcpOutBuffers[0].length < pEncBufferHdr->nFilledLen)
    {
        WFDMMLOGE("HDCP Input Size exceeded buffer size");
        m_pVencoder->SetFreeBuffer(pEncBufferHdr);
        m_pVencoder->RequestIntraVOP();
        return;
    }
    /*----------------------------------------------------------------
     Extract relevant details from the encoder output
     -----------------------------------------------------------------
    */
    pBuffHdr->nFlags           = pEncBufferHdr->nFlags;
    pBuffHdr->nTimeStamp       = pEncBufferHdr->nTimeStamp;

    /*-------------------------------------------------------------------------
      nFilledLen and nOffset shall be populated later based on encoder buffer
      -------------------------------------------------------------------------
    */

    buff_hdr_extra_info* tempExtra = static_cast<buff_hdr_extra_info*>
        (pEncBufferHdr->pAppPrivate);

    if(pEncBufferHdr->pBuffer)
    {
        uint8 ucPESPvtData[PES_PVT_DATA_LEN] = {0};
        output_metabuffer *meta_buf = (output_metabuffer *)(pEncBufferHdr->pBuffer);
        native_handle_t* handle = meta_buf->nh;
       /*----------------------------------------------------------------------
         !!!SHOULD BE IN SYNC WITH COMPONENT!!!
         data[0]-> fd
         data[1]-> offset
         data[2]-> size
         data[3]-> allocLen
         Hence the below check for 1 Minimum Fd and 3 Ints at [1],[2] and [3]
         NOTE: AllocLength need not be re-populated since this is already
         taken care when the ouput buffer was allocated
         ----------------------------------------------------------------------
       */
        if(!handle || !(handle->numInts >= 3 && handle->numFds >= 1))
        {
            WFDMMLOGE("Invalid parameters from encoder");
            if(handle)
            {
                WFDMMLOGE2("Handle has %d fds and %d ints",
                    handle->numFds,handle->numInts);
            }
            else
            {
                WFDMMLOGE("Native handle is null!!!");
            }
            /*-----------------------------------------------------------------
             Release back buffer to encoder, and push back buffer
             Header to Q and report a runtime error
             ------------------------------------------------------------------
            */
            m_pHDCPOutputBufQ->Push(&pBuffHdr, sizeof(pBuffHdr));
            m_pVencoder->SetFreeBuffer(pEncBufferHdr);
            if(m_pEventHandlerFn)
            {
                m_pEventHandlerFn( m_appData, m_nModuleId,WFDMMSRC_ERROR,
                  OMX_ErrorUndefined, 0);
            }
            return;
        }

        int ion_input_fd = handle->data[0];
        pBuffHdr->nOffset = handle->data[1];
       /*----------------------------------------------------------------------
         Encryption module has no way to propagate offsets since it maps fd and
         takes the entire data of nFilledLen. So encoder module should always
         provide 0 offset. And then MUX would read the data from the offset and
         there would be IV mismatch because encryption module would write data
         from the beginning, oblivious of offset.Right now adding an error log
         which should probably provide debugging hints in case encoder plays
         truant.
         -----------------------------------------------------------------
       */
        if(pBuffHdr->nOffset != 0)
        {
            WFDMMLOGE1("WARNING: Encoder provided offset %ld in secure session",
                        pBuffHdr->nOffset);
        }

        pBuffHdr->nFilledLen = handle->data[2];
        buffer* ion_buffer_out = (reinterpret_cast<buffer*>
                                    (pBuffHdr->pPlatformPrivate));
        //Reset in case encryption fails, to avoid spurious stats
        if(tempExtra)
        {
            tempExtra->nEncryptTime = 0;
        }

        unsigned ulStatus = 1;
        {

#ifdef SYSTRACE_PROFILE
            ATRACE_NAME("Encrypt frame");
#endif
            ulStatus =
            m_pHdcpHandle->WFD_HdcpDataEncrypt(STREAM_VIDEO ,
                              (unsigned char*)ucPESPvtData,
                              (unsigned char *) (uint64)(ion_input_fd),
                              (unsigned char *) (uint64)(ion_buffer_out->fd),
                               pBuffHdr->nFilledLen);
        }

        if( ulStatus != 0)
        {
            WFDMMLOGE1("Error in HDCP Encryption! %x", ulStatus);
            /*-----------------------------------------------------------------
             Release back buffer to encoder, and push back buffer
             Header to Q and report a runtime error
             ------------------------------------------------------------------
            */
            m_pHDCPOutputBufQ->Push(&pBuffHdr, sizeof(pBuffHdr));
            m_pVencoder->SetFreeBuffer(pEncBufferHdr);

            if(false == m_pHdcpHandle->proceedWithHDCP())
            {
                WFDMMLOGE("Cipher enablement wait timed out");
                if(m_pEventHandlerFn)
                {
                  m_pEventHandlerFn( m_appData, m_nModuleId,WFDMMSRC_ERROR,
                    OMX_ErrorUndefined, 0);
                }
            }
            else
            {
            /*-----------------------------------------------------------------
             In case a frame is dropped request an IDR from encoder to ensure
             that the sink always does receive an IDR, else we might end up in a
             scenario where an IDR frame is dropped due to CIPHER not being
             enabled and then once it's enabled, we end up sending only P frames
             until the sink explicitly requests for an IDR (not guaranteed) or
             the IDR interval expires.
             ------------------------------------------------------------------
            */
                m_pVencoder->RequestIntraVOP();
                m_bDropVideoFrame = OMX_TRUE;
            }
            return;
        }

        if(WFD_LIKELY(m_pWFDMMSrcStats))
        {
            m_pWFDMMSrcStats->recordVideoEncryptStat(pEncBufferHdr);
        }

        for ( int idx = 0; idx < PES_PVT_DATA_LEN; idx++)
        {
            WFDMMLOGL3("Encrypt PayloadLen[%lu] PES_PVTData[%d]:%x",
                        pEncBufferHdr->nFilledLen,
                        idx,
                        ucPESPvtData[idx]);
        }

        /*-----------------------------------------------------------------
             Fill PESPvtData at end of the encrypted buffer, as an extra data
            -----------------------------------------------------------
            */

        FillHdcpCpExtraData( pBuffHdr, ucPESPvtData, 1);

        if(m_bFillerNaluEnabled)
        {
          memset((void*)ucPESPvtData, 0, sizeof(ucPESPvtData));
          if(m_nFillerInFd > 0 && m_nFillerOutFd > 0)
          {
             unsigned long ulStatus = m_pHdcpHandle->WFD_HdcpDataEncrypt(
                                    STREAM_VIDEO ,
                                   (unsigned char*)ucPESPvtData,
                                   (unsigned char *)(uint64) (m_nFillerInFd),
                                   (unsigned char *) (uint64)(m_nFillerOutFd),
                                    FILLER_NALU_SIZE);
             if( ulStatus != 0)
             {
                 WFDMMLOGE("Error in Filler NALU HDCP Encryption");
             }
             else
             {
                 FillFillerBytesExtraData(pBuffHdr,
                                          ucPESPvtData, 1);
                 WFDMMLOGE("Filler NALU HDCP Encryption");
             }
          }
        }

        pBuffHdr->pOutputPortPrivate = reinterpret_cast<OMX_U8*>
                                    (pEncBufferHdr);
        if(m_pFrameDeliverFn)
        {
            m_pFrameDeliverFn(pBuffHdr,m_appData);
        }
    }
    else
    {
        WFDMMLOGE("Can't extract fd from encoded buffer!");
    }
}

/*=============================================================================

         FUNCTION:          GenerateBlankFrame

         DESCRIPTION:
*//**       @brief          Function for generating blank frame
*//**
@par     DEPENDENCIES:
                            None
*//*
         PARAMETERS:
*//**       @param[in]      nTimestamp   Timestamp to be attached with frame

*//*     RETURN VALUE:
*//**       @return
                            None
@par     SIFE EFFECTS:
                            None
*//*=========================================================================*/

OMX_ERRORTYPE VideoSource::GenerateBlankFrame(OMX_TICKS nTimestamp)
{

    if(OMX_FALSE == m_bBlankFrameSupport)
    {
        WFDMMLOGE("GenerateBlankFrame not supprted!");
        return OMX_ErrorUnsupportedSetting;
    }

    /*-----------------------------------------------------------------
     Things need to be done a bit differently here

     We aren't going to work on the encoder output, instead
     use pre-populated buffers we have and send it to MUX
     ------------------------------------------------------------------
    */

    /*-----------------------------------------------------------------
     1. Get an OMX_BUFFERHEADER from the Blank Frame Q
     ------------------------------------------------------------------
    */

    OMX_BUFFERHEADERTYPE* pOPBufferHdr = NULL;

    OMX_ERRORTYPE res = m_pBlankFrameQ->Pop(&pOPBufferHdr,
                                sizeof(pOPBufferHdr),100);

    if(res != OMX_ErrorNone)
    {
        WFDMMLOGE1("Failed to POP from Blank FrameQ due to %x",res);
        return OMX_ErrorInsufficientResources;
    }

    /*-----------------------------------------------------------------
     2. Fill in relevant details
     ------------------------------------------------------------------
    */

    pOPBufferHdr->nTimeStamp       = nTimestamp;

    /*----------------------------------------------------------------------
     Hard code some values for this OMX_BUFFERHEADER.

     NOTE: QCOM encoders supply a special flag of QOMX_VIDEO_PictureTypeIDR
     with an IDR frame, but since WFD's MUX stack doesn't care for it, just
     the flag OMX_BUFFERFLAG_SYNCFRAME should suffice here
     ----------------------------------------------------------------------
    */
    pOPBufferHdr->nFlags           = 0|OMX_BUFFERFLAG_SYNCFRAME;
    pOPBufferHdr->nFilledLen       = m_nBlankFrameSize;//Fixed from our end

    /*-------------------------------------------------------------------------
     The actual I/P buffer given to MUX depends on whether the session is
     secure or not.

     A) In non secure, this same buffer will be supplied

     B) In secure case, a buffer from the HDCP Q will be supplied and this
        blank frame will be tagged in it's outputportprivate for later recovery
     --------------------------------------------------------------------------
    */

    OMX_BUFFERHEADERTYPE* pMuxIPBufferHeader = NULL;//The actual MUX i/p buffer

    if(m_bHdcpSessionValid)
    {
        m_pHDCPOutputBufQ->Pop(&pMuxIPBufferHeader,
            sizeof(pMuxIPBufferHeader),100);

        if(!pMuxIPBufferHeader)
        {
            WFDMMLOGE("Failed to POP from HDCP BufQ");

            /*-----------------------------------------------------------------

             1. The OMX_BUFFERHEADER in Blank frame Q needs to be returned

             ------------------------------------------------------------------
            */

            m_pBlankFrameQ->Push(&pOPBufferHdr,sizeof(pOPBufferHdr));
            return OMX_ErrorInsufficientResources;
        }

        /*---------------------------------------------------------------------
         Extract relevant details from the output buffer
         ----------------------------------------------------------------------
        */

        pMuxIPBufferHeader->nFilledLen       = pOPBufferHdr->nFilledLen;
        pMuxIPBufferHeader->nFlags           = pOPBufferHdr->nFlags;
        pMuxIPBufferHeader->nOffset          = pOPBufferHdr->nOffset;
        pMuxIPBufferHeader->nTimeStamp       = pOPBufferHdr->nTimeStamp;

        uint8 ucPESPvtData[PES_PVT_DATA_LEN] = {0};

        buffer* ion_buffer_out = (reinterpret_cast<buffer*>
                                (pMuxIPBufferHeader->pPlatformPrivate));

        unsigned long ulStatus = m_pHdcpHandle->WFD_HdcpDataEncrypt(
                            STREAM_VIDEO ,
                           (unsigned char*)ucPESPvtData,
                           (unsigned char *)(uint64) (m_nBlankFrameInFd),
                           (unsigned char *) (uint64)(ion_buffer_out->fd),
                            pMuxIPBufferHeader->nFilledLen);

        if( ulStatus != 0)
        {
            WFDMMLOGE1("Blank frame encryption failed with %x",ulStatus);

            m_pBlankFrameQ->Push(&pOPBufferHdr,sizeof(pOPBufferHdr));
            return OMX_ErrorUndefined;
        }

        for ( int idx = 0; idx < PES_PVT_DATA_LEN; idx++)
        {
            WFDMMLOGL3("Encrypt PayloadLen[%lu] PES_PVTData[%d]:%x",
                        pMuxIPBufferHeader->nFilledLen,
                        idx,
                        ucPESPvtData[idx]);
        }

        /*---------------------------------------------------------------------
         Fill PESPvtData at end of the encrypted buffer, as an extra data
         ----------------------------------------------------------------------
        */

        FillHdcpCpExtraData( pMuxIPBufferHeader, ucPESPvtData, 1);

        /*---------------------------------------------------------------------
         Stuff in the frame from the BlankFrameQ to be extracted later
         once MUX returns the buffer
         ----------------------------------------------------------------------
        */

        pMuxIPBufferHeader->pOutputPortPrivate = static_cast<OMX_PTR>
                                    (pOPBufferHdr);

    }
    else
    {
        /*---------------------------------------------------------------------
         Same buffer header from blank frame Q needs to be sent to MUX
         ----------------------------------------------------------------------
        */
        pMuxIPBufferHeader = pOPBufferHdr;
    }

    /*---------------------------------------------------------------------
     Mark this buffer header as a blank frame to handle when MUX returns the
     buffer
     ----------------------------------------------------------------------
    */

    buff_hdr_extra_info* pMuxIPBuffHdrExtraInfo =
    static_cast<buff_hdr_extra_info*>(pMuxIPBufferHeader->pAppPrivate);

    if(pMuxIPBuffHdrExtraInfo)
    {
        pMuxIPBuffHdrExtraInfo->bBlackFrame = OMX_TRUE;
        WFDMMLOGH1("Sending Blank frame %p to MUX",pMuxIPBufferHeader);
    }

    if(m_pFrameDeliverFn)
    {
        m_pFrameDeliverFn(pMuxIPBufferHeader,m_appData);
    }

    return OMX_ErrorNone;
}

/******************************************************************************!
 * @brief      Fill Extra data in buffer
 * @details    Fill HDCP PES pvt extra data at end of buffer.
 * @param[in]  pBufHdr         Payload buffer header
 * @param[in]  pucPESPvtHeader PES Pvt data
 * @param[in]  nPortIndex      Port index
 * @return     RETURN 'OMX_ErrorNone' if SUCCESS
 *             OMX_ErrorBadParameter code in FAILURE
 ******************************************************************************/
OMX_ERRORTYPE  VideoSource::FillHdcpCpExtraData(
    OMX_BUFFERHEADERTYPE* pBufHdr,
                          OMX_U8* pucPESPvtHeader,
                          OMX_U32 nPortIndex)
{
  OMX_ERRORTYPE eError = OMX_ErrorNone;
  uint64 ulAddr;
    OMX_OTHER_EXTRADATATYPE* pHdcpCpExtraData;
    if((NULL != pBufHdr) && (NULL != pucPESPvtHeader))
  {
    MM_MSG_PRIO2(MM_GENERAL, MM_PRIO_MEDIUM,
                "WFDMMSourceVideoSource::pBufHdr->pBuffer[%p] FilledLen[%ld]",
                pBufHdr->pBuffer,
                pBufHdr->nFilledLen);
    /* Skip encoded frame payload length filled by V4L2 driver */
        ulAddr = (uint64)(pBufHdr->pBuffer) +  pBufHdr->nFilledLen;
        MM_MSG_PRIO1(MM_GENERAL, MM_PRIO_MEDIUM,
                 "WFDMMSourceVideoSource::ulAddr[%llu]", ulAddr);
    /* Aligned address to DWORD boundary */
    ulAddr = (ulAddr + 0x3) & (~0x3);
    MM_MSG_PRIO1(MM_GENERAL, MM_PRIO_MEDIUM,
                 "WFDMMSourceVideoSource::Aligned ulAddr[%llu]", ulAddr);
        pHdcpCpExtraData = (OMX_OTHER_EXTRADATATYPE*)ulAddr;
    /* Update pBufHdr flag, to indicate that it carry extra data */
    MM_MSG_PRIO1(MM_GENERAL, MM_PRIO_MEDIUM,
                 "WFDMMSourceVideoSource::pHdcpCpExtraData[%p]",
                  pHdcpCpExtraData);

    FLAG_SET(pBufHdr,OMX_BUFFERFLAG_EXTRADATA);
    /* Extra Data size = ExtraDataType*/
    pHdcpCpExtraData->nSize = sizeof(OMX_OTHER_EXTRADATATYPE)
                              + sizeof(OMX_U8)* PES_PVT_DATA_LEN -4;
    pHdcpCpExtraData->nVersion = pBufHdr->nVersion;
    pHdcpCpExtraData->nPortIndex = nPortIndex;
    pHdcpCpExtraData->nDataSize = PES_PVT_DATA_LEN;
    MM_MSG_PRIO3(MM_GENERAL, MM_PRIO_MEDIUM,
      "WFDMMSourceVideoSource::size[%ld] PortIndex[%ld] nDataSize[%ld]",
      pHdcpCpExtraData->nSize,
                     pHdcpCpExtraData->nPortIndex,pHdcpCpExtraData->nDataSize);
    pHdcpCpExtraData->eType = (OMX_EXTRADATATYPE)QOMX_ExtraDataHDCPEncryptionInfo;
    /* Fill PES_PVT_DATA into data*/
        memcpy(pHdcpCpExtraData->data,pucPESPvtHeader,PES_PVT_DATA_LEN);
    /* Append OMX_ExtraDataNone */
    ulAddr += pHdcpCpExtraData->nSize;
        pHdcpCpExtraData = (OMX_OTHER_EXTRADATATYPE*)ulAddr;
    pHdcpCpExtraData->nSize = sizeof(OMX_OTHER_EXTRADATATYPE);
    pHdcpCpExtraData->nVersion = pBufHdr->nVersion;
    pHdcpCpExtraData->nPortIndex = nPortIndex;
    pHdcpCpExtraData->eType = OMX_ExtraDataNone;
    pHdcpCpExtraData->nDataSize = 0;
  }
  else
  {
    eError = OMX_ErrorBadParameter;
  }
  return eError;
}


/******************************************************************************!
 * @brief      Fill DTS Extra data in buffer
 * @details    Fill Decode timestamps.
 * @param[in]  pBufHdr         Payload buffer header
 * @param[in]  pucPESPvtHeader PES Pvt data
 * @param[in]  nPortIndex      Port index
 * @return     RETURN 'OMX_ErrorNone' if SUCCESS
 *             OMX_ErrorBadParameter code in FAILURE
 ******************************************************************************/
bool  VideoSource::FillDTSExtraData(
    OMX_BUFFERHEADERTYPE* pBuffHdr,
    OMX_TICKS nDTS,
    OMX_U32 nPortIndex)
{
  OMX_OTHER_EXTRADATATYPE* pExtra;

  if(NULL != pBuffHdr)
  {
    WFDMMLOGE2("%s: DTS = %lld", __FUNCTION__, nDTS);
    OMX_U8 *pTmp = pBuffHdr->pBuffer +
                         pBuffHdr->nOffset + pBuffHdr->nFilledLen + 3;

    pExtra = (OMX_OTHER_EXTRADATATYPE *) ((reinterpret_cast<long>(pTmp)) & ~3);

    if(pBuffHdr->nFlags & OMX_BUFFERFLAG_EXTRADATA)
    {
      MM_MSG_PRIO(MM_GENERAL, MM_PRIO_ERROR, "ExtraData Already Present");
      //Extra Data already set. Find the end
      while(pExtra->eType != OMX_ExtraDataNone)
      {
        pExtra = (OMX_OTHER_EXTRADATATYPE *)
                      (((OMX_U8 *) pExtra) + pExtra->nSize);

        if(reinterpret_cast<long>(pExtra) + sizeof(OMX_OTHER_EXTRADATATYPE) >=
          reinterpret_cast<long>(pBuffHdr->pBuffer) + pBuffHdr->nAllocLen)
        {
          MM_MSG_PRIO(MM_GENERAL, MM_PRIO_ERROR,
                         "Fiiler Bytes Reached out of bounds");
          return false;
        }
      }
    }

    OMX_U32 nBytesToFill = sizeof(OMX_TICKS) + 1 /*Signalling Byte*/;


    WFDMMLOGE2("%s Fiiler Bytes nBytesToFill = %lu",
                  __FUNCTION__, nBytesToFill);

    /** Check out of bound access in the pBuffer */
    if(reinterpret_cast<long>(pExtra) + sizeof(OMX_OTHER_EXTRADATATYPE) +
                                     ((nBytesToFill +3) & (~3))  >
       reinterpret_cast<long>(pBuffHdr->pBuffer) + pBuffHdr->nAllocLen)
    {
      /*Can't fit in filler bytes*/
      MM_MSG_PRIO(MM_GENERAL, MM_PRIO_ERROR,
                      "VideoSource Can't fit in fillerNALU");
      return false;
    }

    FLAG_SET(pBuffHdr,OMX_BUFFERFLAG_EXTRADATA);

    /* Extra Data size = ExtraDataType*/
    pExtra->nVersion = pBuffHdr->nVersion;
    pExtra->nPortIndex = nPortIndex;
    pExtra->nDataSize = nBytesToFill;
    nBytesToFill += 3;
    nBytesToFill &= (~3);
    pExtra->nSize = sizeof(OMX_OTHER_EXTRADATATYPE) + nBytesToFill -4;

    WFDMMLOGE4("%s: Filler size[%ld] PortIndex[%ld] nDataSize[%ld]",
                __FUNCTION__,
                pExtra->nSize,
                pExtra->nPortIndex,pExtra->nDataSize );

    /* Using MAX to pass generic ExtraData Info using signalling byte to
       identify the type of data*/
    pExtra->eType = OMX_ExtraDataMax;

    /**------------------------------------------------------------------------
    Filler NALU format:= |SigByte|OMX_TICKS|
    ---------------------------------------------------------------------------
    */

    /**Fill the extra data bytes */
    uint32 nOffset = 0;

    /*Signal Encrypted Payload or non-encrypted Payload*/
    pExtra->data[nOffset] = VIDEO_PVTDATA_TYPE_DECODE_TIMESTAMP;

    nOffset++;

    for(int i = 0; i < sizeof(OMX_TICKS); i++)
    {
        pExtra->data[nOffset++] = (nDTS >> (i << 3)) & 0xFF;
    }

    /** Fill the extradataNone if there is space left. */
    pExtra = (OMX_OTHER_EXTRADATATYPE *)
           (reinterpret_cast<OMX_U8*>(pExtra) + pExtra->nSize);
    if(reinterpret_cast<long>(pExtra) + sizeof(OMX_OTHER_EXTRADATATYPE) >=
        reinterpret_cast<long>(pBuffHdr->pBuffer) + pBuffHdr->nAllocLen)
    {
      MM_MSG_PRIO(MM_GENERAL, MM_PRIO_ERROR,
                     "Fiiler Bytes: XtraNone Reached out of bounds");
      return true;
    }
    pExtra->nSize = sizeof(OMX_OTHER_EXTRADATATYPE);
    pExtra->nVersion = pBuffHdr->nVersion;
    pExtra->nPortIndex = nPortIndex;
    pExtra->eType = OMX_ExtraDataNone;
    pExtra->nDataSize = 0;
    return true;
  }
  return false;
}

/******************************************************************************!
 * @brief      Fill FillerBytes Extra data in buffer
 * @details    Fill Fillerbytes NALU extra data at end of buffer.
 * @param[in]  pBufHdr         Payload buffer header
 * @param[in]  pucPESPvtHeader PES Pvt data
 * @param[in]  nPortIndex      Port index
 * @return     RETURN 'OMX_ErrorNone' if SUCCESS
 *             OMX_ErrorBadParameter code in FAILURE
 ******************************************************************************/
bool  VideoSource::FillFillerBytesExtraData(
                          OMX_BUFFERHEADERTYPE *pBuffHdr,
                          OMX_U8* pucPESPvtHeader,
                          OMX_U32 nPortIndex)
{
  OMX_OTHER_EXTRADATATYPE *pExtra;

  if(NULL != pBuffHdr)
  {
    OMX_U8 *pTmp = pBuffHdr->pBuffer +
                         pBuffHdr->nOffset + pBuffHdr->nFilledLen + 3;

    pExtra = (OMX_OTHER_EXTRADATATYPE *) ((reinterpret_cast<long>(pTmp)) & ~3);

    if(pBuffHdr->nFlags & OMX_BUFFERFLAG_EXTRADATA)
    {
      //Extra Data already set. Find the end
      while(pExtra->eType != OMX_ExtraDataNone)
      {
        pExtra = (OMX_OTHER_EXTRADATATYPE *)
                      (((OMX_U8 *) pExtra) + pExtra->nSize);

        if(reinterpret_cast<long>(pExtra) + sizeof(OMX_OTHER_EXTRADATATYPE) >=
          reinterpret_cast<long>(pBuffHdr->pBuffer) + pBuffHdr->nAllocLen)
        {
          MM_MSG_PRIO(MM_GENERAL, MM_PRIO_ERROR,
                         "Fiiler Bytes Reached out of bounds");
          return false;
        }
      }
    }

    OMX_U32 nBytesToFill = FILLER_NALU_SIZE + 1 /*Signalling Byte*/
                                            + 1 /*Length */;

    if(pucPESPvtHeader != NULL)
    {
      /*Filler Nalu us encrypted*/
      nBytesToFill += PES_PVT_DATA_LEN + 1 /*Length */;
    }

    MM_MSG_PRIO1(MM_GENERAL, MM_PRIO_MEDIUM,
                 "VideoSource Fiiler Bytes nBytesToFill = %lu",
                  nBytesToFill);

    /** Check out of bound access in the pBuffer */
    if(reinterpret_cast<long>(pExtra) + sizeof(OMX_OTHER_EXTRADATATYPE) +
                                           ((nBytesToFill +3) & (~3))  >
       reinterpret_cast<long>(pBuffHdr->pBuffer) + pBuffHdr->nAllocLen)
    {
      /*Can't fit in filler bytes*/
      MM_MSG_PRIO(MM_GENERAL, MM_PRIO_ERROR,
                      "VideoSource Can't fit in fillerNALU");
      return false;
    }

    FLAG_SET(pBuffHdr,OMX_BUFFERFLAG_EXTRADATA);

    /* Extra Data size = ExtraDataType*/
    pExtra->nVersion = pBuffHdr->nVersion;
    pExtra->nPortIndex = nPortIndex;
    pExtra->nDataSize = nBytesToFill;
    nBytesToFill += 3;
    nBytesToFill &= (~3);
    pExtra->nSize = sizeof(OMX_OTHER_EXTRADATATYPE) + nBytesToFill -4;

    MM_MSG_PRIO3(MM_GENERAL, MM_PRIO_MEDIUM,
      "WFDMMSourceVideoSource:: Filler size[%ld] PortIndex[%ld] nDataSize[%ld]",
      pExtra->nSize,
      pExtra->nPortIndex,pExtra->nDataSize );

    /* Using MAX to pass generic ExtraData Info using signalling byte to
       identify the type of data*/
    pExtra->eType = OMX_ExtraDataMax;

    /**------------------------------------------------------------------------
    Filler NALU format:= |SigByte|Size|PES Pvt Data|Size|Filler NALU |
    ---------------------------------------------------------------------------
    */

    /**Fill the extra data bytes */
    uint32 nOffset = 0;

    /*Signal Encrypted Payload or non-encrypted Payload*/
    pExtra->data[nOffset] = VIDEO_PVTDATA_TYPE_FILLERNALU;

    if(pucPESPvtHeader != NULL)
    {
      pExtra->data[nOffset] = VIDEO_PVTDATA_TYPE_FILLERNALU_ENCRYPTED;
    }
    nOffset++;

    /**If encypted first add the PES private data */
    if(pucPESPvtHeader != NULL)
    {
      pExtra->data[nOffset] = PES_PVT_DATA_LEN;
      nOffset++;
      /* Fill PES_PVT_DATA into data*/
      memcpy(pExtra->data + nOffset,pucPESPvtHeader, PES_PVT_DATA_LEN );
      nOffset += PES_PVT_DATA_LEN;
    }

    /** FIll the filler NALU bytes */
    pExtra->data[nOffset] = FILLER_NALU_SIZE;
    nOffset++;
    memcpy(pExtra->data + nOffset, m_pFillerDataOutPtr, FILLER_NALU_SIZE);

    /** Fill the extradataNone if there is space left. Mux will
     *  check against AllocLen to prevent access beyond limit */
    pExtra = (OMX_OTHER_EXTRADATATYPE *)
           (reinterpret_cast<OMX_U8*>(pExtra) + pExtra->nSize);
    if(reinterpret_cast<long>(pExtra) + sizeof(OMX_OTHER_EXTRADATATYPE) >=
        reinterpret_cast<long>(pBuffHdr->pBuffer) + pBuffHdr->nAllocLen)
    {
      MM_MSG_PRIO(MM_GENERAL, MM_PRIO_ERROR,
                     "Fiiler Bytes: XtraNone Reached out of bounds");
      return true;
    }
    pExtra->nSize = sizeof(OMX_OTHER_EXTRADATATYPE);
    pExtra->nVersion = pBuffHdr->nVersion;
    pExtra->nPortIndex = nPortIndex;
    pExtra->eType = OMX_ExtraDataNone;
    pExtra->nDataSize = 0;
  }
  return false;
}

/*==============================================================================

         FUNCTION:         eventHandler

         DESCRIPTION:
*//**       @brief         Static function that handles events from encoder
                                modules


*//**

@par     DEPENDENCIES:      None

*//*
         PARAMETERS:
*//**       @param        pThis - this pointer
                          nModuleId - Id of the module reporting event
                          nEvent - Type of event
                          nStatus - status associated with event
                          nData  - More information about event


*//*     RETURN VALUE:
*//**       @return       None


@par     SIDE EFFECTS:

*//*==========================================================================*/
void VideoSource::VideoSourceEventHandlerCb(
                        void *pThis,
                        OMX_U32 nModuleId,
                        WFDMMSourceEventType nEvent,
                        OMX_ERRORTYPE nStatus, void* nData)
{
    (void) nData;
    (void) nStatus;
    if(!pThis)
    {
        WFDMMLOGE("Invalid Me, can't handle device callback");
        return;
    }
    VideoSource* pMe= static_cast<VideoSource*>(pThis);
    WFDMMLOGE1("Received callback from module %ld",nModuleId);
    switch(nEvent)
    {
        case WFDMMSRC_ERROR:
            pMe->m_pEventHandlerFn(pMe->m_appData, pMe->m_nModuleId,
                   WFDMMSRC_ERROR, OMX_ErrorNone, 0);
            break;
        default:
            WFDMMLOGE("Simply unreachable!");
            break;
    }
    return;
}

/*==============================================================================

         FUNCTION:          WFDBinderCallback

         DESCRIPTION:
*//**       @brief          Static function that receives events from Binder
*//**
@par     DEPENDENCIES:
                            None
*//*
         PARAMETERS:
*//**       @param          handle - this pointer
                            code - Id of the module reporting event
                            pData - Pointer to data to act on it


*//*     RETURN VALUE:
*//**       @return
                            Success/failure
@par     SIFE EFFECTS:
                            None
*//*==========================================================================*/

int VideoSource::WFDBinderCallback(void* handle, int code, void* pData)
{
    if(handle && VideoSource::m_bVideoSourceActive)
    {
        VideoSource* pMe = (VideoSource*)handle;
        return pMe->ProcessWFDBinderCallback(code, pData);
    }
    return -1;
}

/*==============================================================================

         FUNCTION:          ProcessWFDBinderCallback

         DESCRIPTION:
*//**       @brief          processing the events receivecd from server
*//**
@par     DEPENDENCIES:
                            None
*//*
         PARAMETERS:
*//**       @param          code - event type
                            pData - Pointer to data to act on it


*//*     RETURN VALUE:
*//**       @return
                            Success/failure
@par     SIFE EFFECTS:
                            None
*//*==========================================================================*/

int VideoSource:: ProcessWFDBinderCallback(int code, void* pData)
{
    int result = -1;
    WFDMMLOGH1("VideoSource::ProcessWFDBinderCallback %d", code);

    switch (code)
    {

        case WFDMMSERVICE_STREAMING_REQ:
            if(m_hVideoSourceThread)
            {
            /*------------------------------------------------------------------
                once get the Streaming support command from Miracast component
                waiting till response from Sink  otherwise timeout will happen
                and has to move to mirror Mode instructed by component
            --------------------------------------------------------------------
            */
                if(!m_hVideoSourceThread)
                {
                    WFDMMLOGE("Streaming Validity Failed m_hVideoSourceThread");
                    return -1;
                }

                m_eStreamingStatus = STREAMING_QUERY;
                StreamParamsType StreamParams;
                streamDataType* pInfo = (streamDataType*)pData;
                StreamParams.codecType = pInfo->streamParams.codecType;
                StreamParams.profile = pInfo->streamParams.profile;
                StreamParams.level = pInfo->streamParams.level;
                StreamParams.height = pInfo->streamParams.height;
                StreamParams.width = pInfo->streamParams.width;
                StreamParams.bSecure   = pInfo->streamParams.bSecure;
                StreamParams.bConstrainedProfile =
                    pInfo->streamParams.bConstrainedProfile;

                if(StreamParams.bSecure)
                {
                    if(!m_bHdcpSessionValid)
                    {
                        return  -1;
                    }
                }


                if(m_eStreamingStatus == STREAMING_SUCCESS)
                {
                    if(StreamParams.codecType == m_streamParams.codecType &&
                       StreamParams.profile == m_streamParams.profile &&
                       StreamParams.level == m_streamParams.level &&
                       StreamParams.height  == m_streamParams.height &&
                       StreamParams.width == m_streamParams.width)
                    {
                        return 0;
                    }
                }

                result = checkStreamingValidityOnCodec(StreamParams);

                if(result != 0)
                {
                    WFDMMLOGE("Streaming Validity failed ");
                    return -1;
                }
                memcpy(&m_streamParams, &StreamParams, sizeof(StreamParamsType));
                m_hVideoSourceThread->SetSignal(VIDEO_SOURCE_STREAMING_REQ);
                int StatusCnt = STREAMING_WAIT_TIME_CNT;
                while((m_eStreamingStatus != STREAMING_SUCCESS) && StatusCnt--)
                {
                    if(m_eStreamingStatus == STREAMING_QUERY)
                    {
                        MM_Timer_Sleep(5);
                    }
                    else
                    {
                        break;
                    }

                    WFDMMLOGH1("m_eStreamingStatus = %d", StatusCnt);
                }
                if((StatusCnt <= 0)||(m_eStreamingStatus == STREAMING_FAILURE))
                {
                    m_eStreamingStatus = STREAMING_DEINIT;
                    return -1;
                }
                return 0;
            }
            break;
    case WFDMMSERVICE_START_STREAMING:
        {
            if(m_hVideoSourceThread)
            {
                result = 0;
            }
            else
            {
                WFDMMLOGE("Failed START_STREAMINGdue to m_hVideoSourceThread");
                result = -1;
            }
        }
        break;

    case WFDMMSERVICE_WRITE_DATA:
        {
            result = WriteData(pData);
        }
        break;

    case WFDMMSERVICE_STOP_STREAMING:
        {
            if(m_hVideoSourceThread)
            {
                m_StreamingClosingReqSent = false;
                m_hVideoSourceThread->SetSignal(VIDEO_SOURCE_STREAMING_STOP);
                int StopCnt = STREAMING_WAIT_TIME_CNT;
                while((m_eStreamingStatus == STREAMING_SUCCESS) && StopCnt--)
                {
                    MM_Timer_Sleep(5);
                }
                if(StopCnt == 0)
                {
                    WFDMMLOGE2("Failed to Stop Streaming Cnt = %d,StopCnt = %d",
                               STREAMING_WAIT_TIME_CNT, StopCnt);
                }
            }
            else
            {
                WFDMMLOGE("Failed STOP_STREAMING due to m_hVideoSourceThread");
                result = -1;
            }
        }
        break;
    case WFDMMSERVICE_RENDER_DATA:
        {
            result = RenderFrame(pData);
        }
        break;
    case WFDMMSERVICE_FLUSH_STREAM:
        {
            if(m_hVideoSourceThread)
            {
                m_hVideoSourceThread->SetSignal(VIDEO_SOURCE_FLUSH);
            }
        }
        break;
    case WFDMMSERVICE_APP_INFO:
        {
            WhiteListType* wl = (WhiteListType*)pData;
            result = setAppInfo(wl->name,wl->operation);
        }
        break;
    }
    return result;
}
/*==============================================================================

         FUNCTION:         StartStreaming

         DESCRIPTION:
*//**       @brief         setting StartStreaming functionality

*//**

@par     DEPENDENCIES:      None

*//*
         PARAMETERS:
*//**       @param      None



*//*     RETURN VALUE:
*//**       @return       Success/failure


@par     SIDE EFFECTS:

*//*==========================================================================*/

OMX_ERRORTYPE VideoSource::StartStreaming()
{
    /*--------------------------------------------------------------------------
    1.check the resources needed for streaming functionality requirements
    2.create resources for stareaming functionality
    3.puase encoder and capture those are in mirror mode
    4.set as streaming mode
    ----------------------------------------------------------------------------
    */
    WFDMMLOGH("VideoSource::StartStreaming");
    OMX_ERRORTYPE result = OMX_ErrorNone;
    if((m_pStreamingBufQ == NULL))
    {
        WFDMMLOGE("StartStreaming failed due to m_pStreamingBufQ");
        return OMX_ErrorInsufficientResources;
    }

    if(m_pVencoder && (m_eVideoSrcState == WFDMM_VideoSource_STATE_PAUSE))
    {
        WFDMMLOGE("Streaming Cannot be started due to WFD in Pause state");
        m_eStreamingStatus = STREAMING_FAILURE;
        return OMX_ErrorInvalidState;
    }

    if(m_pVencoder && (m_eVideoSrcState == WFDMM_VideoSource_STATE_PLAY))
    {
        result = m_pVencoder->Pause();
        WFD_OMX_ERROR_CHECK(result,"Failed to pause!");
        m_eVideoSrcState = WFDMM_VideoSource_STATE_PAUSE;
        m_eStreamingStatus = STREAMING_SUCCESS;
    }
    m_bStreamingEnabled = OMX_TRUE;
    m_nRenderTSReadIdx = 0;
    m_nRenderTSWriteIdx = 0;
    m_bBaseStreamTimeTaken = false;
    m_nDSTotalTimeDrift = 0;
    m_nDSNumSamplesAvgDrift = 0;
    m_nPrevAdjTimeStamp = 0;
    m_nBaseStreamTime = 0;
    m_nBaseStreamTime = (int64)-1;
    m_nPrevPushedBufsOnRender = 0;

    m_eVideoStreamingSrcState = WFDMM_VideoSource_STATE_PLAY;
    WFDMMLOGH1("VideoSource::StartStreaming = %d",m_bStreamingEnabled);
EXIT:
    return result;
}

/*==============================================================================

         FUNCTION:         WriteData

         DESCRIPTION:
*//**       @brief         Passing encoded video data to either Mux or
                           HDCP Modules

*//**

@par     DEPENDENCIES:      None

*//*
         PARAMETERS:
*//**       @param      Pdata: OMX_BUFFERHEADERTYPE handle for video data



*//*     RETURN VALUE:
*//**       @return       Success/failure


@par     SIDE EFFECTS:

*//*==========================================================================*/
OMX_ERRORTYPE VideoSource::WriteData(void* pData)
{

    OMX_ERRORTYPE result = OMX_ErrorNone;
    OMX_BUFFERHEADERTYPE* pBufferHdr = NULL;
    OMX_BUFFERHEADERTYPE* pBufferHdrOp = NULL;
    StreamingEventType StrmEvent;
    void* pOutput = NULL;

    streamDataType* pInfo = (streamDataType*)pData;

    if(!pData)
    {
        result = OMX_ErrorBadParameter;
        WFD_OMX_ERROR_CHECK(result,"Failed in WriteData due to pData NULL!");
    }

    if(m_bStreamingEnabled != OMX_TRUE)
    {
        WFDMMLOGE1("Failed due to invalid state m_bStreamingEnabled = %d"
                   , m_bStreamingEnabled);
        WFDMMLOGE2("Failed Push pInfo->nLen= %d,nTimeUs = %lld",
                   (int)pInfo->WriteData.nLen, pInfo->WriteData.nTimeUs);
        if(pInfo->WriteData.bFd)
        {
            WFDMMLOGE2("Failed Push Fd = %d,pBuffer = %ld",
                       pInfo->WriteData.DataPtr.IonFdData.fd,
                       pInfo->WriteData.DataPtr.IonFdData.pvtData);
        }
        else
        {
            WFDMMLOGE1("Failed Push pData = %p",
                       pInfo->WriteData.DataPtr.pData);

        }
        result = OMX_ErrorComponentNotFound;
        WFD_OMX_ERROR_CHECK(result, "Failed in WriteData m_bStreamingEnabled!");
    }
    if(m_StreamingClosingReqSent)
    {
        result = OMX_ErrorBadParameter;
        WFD_OMX_ERROR_CHECK(result, "Failed in WriteData mStreamingClosed!");
    }

    /*--------------------------------------------------------------------------
      1.if Secure encrypt data and return buffer back to player to free
         encrypt has the details about the o/p buffers
      2.if non-secure case check the availability of o/p buffer,
        if available copy data to o/p buffer and return buffer to free.
        if o/p buffer is not available create buffer and update Node to NodeList
        if NodeList is grownUp to limit then delete one of the node to accomidate
        this node.
     3.
    ----------------------------------------------------------------------------
    */

    WFDMMLOGE2("WriteFrame Push Fd = %ld,Len = %ld",
               pInfo->WriteData.DataPtr.IonFdData.fd,
               pInfo->WriteData.nLen);

    if(pInfo->WriteData.bFd)
    {
        pBufferHdr = ProcessInputBuffer(pInfo->WriteData.nLen,
                                        pInfo->WriteData.DataPtr.IonFdData.fd,
                                        NULL,
                                        pInfo->WriteData.bSecure);
    }
    else
    {
        pBufferHdr = ProcessInputBuffer(pInfo->WriteData.nLen,
                                        -1,
                                        pInfo->WriteData.DataPtr.pData,
                                        pInfo->WriteData.bSecure);
    }

    if(pBufferHdr == NULL)
    {
        WFDMMLOGE("Failed to get pBuffHdr");
        result = OMX_ErrorInsufficientResources;
        WFD_OMX_ERROR_CHECK(result, "Failed in WriteData pBuffHdr is NULL!");
    }

    /*--------------------------------------------------------------------------
      1. check the data received is secure or non secure
      2. In case of non-secure get virtual address and pass to Mux
      3. in case of secure just pass the fd to HDCP module
      4. in both cases store the Pdata in to Buffer header and pass back
      sameto streamer to reuse the same Fd.
    ----------------------------------------------------------------------------
    */
    pBufferHdr->nInputPortIndex = PORT_INDEX_VIDEO;
    pBufferHdr->nFilledLen      = pInfo->WriteData.nLen;
    pBufferHdr->nFlags          |= pInfo->WriteData.nFlags;
    pBufferHdr->nTimeStamp      = pInfo->WriteData.nTimeUs;

    if(m_nBaseStreamTime > pInfo->WriteData.nTimeUs)
    {
        m_nBaseStreamTime = pInfo->WriteData.nTimeUs;
    }

    WFDMMLOGE4("WriteData pBufferHdr= %p,len= %d,data= %x ,msec = %d",
               pBufferHdr,(int)pBufferHdr->nFilledLen,
               pBufferHdr->pBuffer,pBufferHdr->nTimeStamp);
    /*--------------------------------------------------------------------------
        push the pBufferHeader to Queue to render frame when render got
        from binder
    ----------------------------------------------------------------------------
    */

    if(m_pStreamingPendBufQ && pBufferHdr
       && m_pStreamingPendBufQ->GetSize() < m_nStreamingBufCnt)
    {
        WFDMMLOGH4(" WriteData in pendBufQ Hdr=%p,len=%d,data=%x ,msec=%d",
                   pBufferHdr, (int)pBufferHdr->nFilledLen,
                   pBufferHdr->pBuffer, pBufferHdr->nTimeStamp);
        result = m_pStreamingPendBufQ->Push(&pBufferHdr,
                                            sizeof(OMX_BUFFERHEADERTYPE*));
        if(result != OMX_ErrorNone)
        {
            WFDMMLOGH4("Failed pendBufQ Hdr=%p,len=%d,data=%x,msec=%d",
                       pBufferHdr, (int)pBufferHdr->nFilledLen,
                       pBufferHdr->pBuffer, pBufferHdr->nTimeStamp);
            if(m_bHdcpSessionValid && pInfo->WriteData.bSecure)
            {
                m_pHDCPOutputBufQ->Push(&pBufferHdr, sizeof(pBufferHdr));
                pBufferHdr = reinterpret_cast<OMX_BUFFERHEADERTYPE*>
                    (pBufferHdr->pOutputPortPrivate);
            }
            VideoSourceFreeBuffer(pBufferHdr);
            result = (OMX_ERRORTYPE)STREAMING_QUEUE_FULL;
            WFD_OMX_ERROR_CHECK(result, "Failed: PUSH pendBufQ pBufferHdr");
        }
    }
    else
    {
        WFDMMLOGH4("Failed pendBufQ pBufferHdr=%p,len=%d,data=%x,msec = %d",
                   pBufferHdr, (int)pBufferHdr->nFilledLen,
                   pBufferHdr->pBuffer, pBufferHdr->nTimeStamp);
        VideoSourceFreeBuffer(pBufferHdr);
        result = (OMX_ERRORTYPE)STREAMING_QUEUE_FULL;
        WFD_OMX_ERROR_CHECK(result, "Failed: PendBufQ->GetSize");
    }
    WFDMMLOGE3("Success Writedata pInfo->nLen=%d,pData=%p nTimeUs = %lld",
               (int)pInfo->WriteData.nLen, pInfo->WriteData.DataPtr.pData,
               pInfo->WriteData.nTimeUs);
    return result;
EXIT:
    if(pInfo)
    {
        WFDMMLOGE3("Failed Writedata pInfo->nLen=%d,pData=%p nTimeUs = %d",
                   (int)pInfo->WriteData.nLen, pInfo->WriteData.DataPtr.pData,
                   pInfo->WriteData.nTimeUs);
    }

//    StrmEvent = STREAMING_FRAME_COMPLETE;
//    if(pInfo->WriteData.bFd)
//    {
//        StrmEvent = STREAMING_FD_FRAME_COMPLETE;
//        pOutput = pInfo->WriteData.DataPtr.pData;
//    }
//    void* pData = tempExtra->pData;
    return result;
}

/*==============================================================================
      FUNCTION:            RenderFrame()

      DESCRIPTION:
*//**       @brief           call from thread to perform rendring the frame
                          received in WriteData call from decoder/binder
*//**
@par     DEPENDENCIES:
                         None
*//*
      PARAMETERS:
*//*         @param         None

*//*     RETURN VALUE:
*//**       @return
                         OMX_ErrorNone  or error
@par     SIDE EFFECTS:
                         None
*//*==========================================================================*/
OMX_ERRORTYPE VideoSource::RenderFrame(void* pData)
{
    OMX_ERRORTYPE result = OMX_ErrorNone;
    if(!pData)
    {
       WFDMMLOGE("Failed in RenderFrame due to pData NULL!");
       return OMX_ErrorBadParameter;
    }
    streamDataType* pInfo = (streamDataType*)pData;
    WFDMMLOGH1("VideoSource::RenderFrame() TS = %lld",
               pInfo->RenderDataParam.nTimeUs);
    if(m_bStreamingEnabled != OMX_TRUE ||
       m_eVideoStreamingSrcState != WFDMM_VideoSource_STATE_PLAY)
    {
        WFDMMLOGE("RenderFrame is not expected in this state");
        return OMX_ErrorIncorrectStateOperation;
    }
    if(m_hVideoSourceThread)
    {
        m_nRenderTS[m_nRenderTSWriteIdx++] =
                         (OMX_TICKS)pInfo->RenderDataParam.nTimeUs;
        m_hVideoSourceThread->SetSignal(VIDEO_SOURCE_RENDER_FRAME);
    }
    return result;
}

/*==============================================================================

         FUNCTION:          ExecuteRenderFrame

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
                            OMX_ERRORTYPE
@par     SIFE EFFECTS:
                            None
*//*==========================================================================*/
OMX_ERRORTYPE VideoSource::ExecuteRenderFrame(bool bEOS)
{
    WFDDEBUGTRACE;

    OMX_ERRORTYPE result = OMX_ErrorNone;
    OMX_BUFFERHEADERTYPE *pBufferHdr = NULL;

    WFD_VSRC_CRITICAL_SECT_ENTER(m_hCritSect);
    uint64 nRenderTimeStamp = m_nRenderTS[m_nRenderTSReadIdx++];

    WFDMMLOGH1("ExecuteRenderFrame TS= %lld", nRenderTimeStamp);

    m_nPrevPushedBufsOnRender = 0;

    if(!bEOS)
    {
        /*----------------------------------------------------------------------
           Read current system Time
        ------------------------------------------------------------------------
        */
        struct timespec tempTime;
        clock_gettime(CLOCK_MONOTONIC, &tempTime);
        OMX_U64 nSysTimeUs = ((OMX_U64)tempTime.tv_sec * 1000000)
                           + ((OMX_U64)tempTime.tv_nsec / 1000);

        /*----------------------------------------------------------------------
           Tuning Params
        ------------------------------------------------------------------------
        */
        #define DS_VIDEO_DECODE_HEAD_START_US           300000
        #define DS_BASETIME_ADJUST_REPEAT_THRESHOLD_US  500000
        #define DS_BASETIME_ADJUST_THRESHOLD_US         200000
        #define DS_FORWARD_DATA_PUSH_INTERVAL_MS        5

        /*----------------------------------------------------------------------
           If base time not set take current time as base time
        ------------------------------------------------------------------------
        */
        if(!m_bBaseStreamTimeTaken)
        {
            if(nRenderTimeStamp < m_nBaseStreamTime ||
               nRenderTimeStamp - m_nBaseStreamTime >
                  VIDEO_SOURCE_DELAYED_RENDERING_TOLERANCE)
            {
                WFDMMLOGE1("SwitchToMirror: Too much data backed up %llu",
                          m_nBaseStreamTime);
                AbortStreaming();
                return OMX_ErrorNone;
            }
            m_nBaseStreamTime      = nRenderTimeStamp;
            m_bBaseStreamTimeTaken = true;
            m_nBaseRenderTimeStamp = nRenderTimeStamp;
            m_nBaseSystemTime      = nSysTimeUs
                                    - DS_VIDEO_DECODE_HEAD_START_US;
            m_nDSTotalTimeDrift    = 0;
            m_nDSNumSamplesAvgDrift = 0;
        }

        /*----------------------------------------------------------------------
           Compare local elapsed time against elapsed stream time
           if difference exceeds 200ms adjust basetime
        ------------------------------------------------------------------------
        */
        else
        {
            uint64 renderdiff    = (nRenderTimeStamp - m_nBaseRenderTimeStamp);
            uint64 nTimeDiff     = (nSysTimeUs - m_nBaseSystemTime);
            m_nDSTotalTimeDrift += (nTimeDiff - renderdiff);
            m_nDSNumSamplesAvgDrift++;



            if(nSysTimeUs - m_nPrevAdjTimeStamp >
                            DS_BASETIME_ADJUST_REPEAT_THRESHOLD_US &&
               m_nDSNumSamplesAvgDrift)
            {
                uint64 nDiff = (uint64)((int64)m_nDSTotalTimeDrift/
                                (int64)m_nDSNumSamplesAvgDrift);
                if(ABS((int64)nDiff) > DS_BASETIME_ADJUST_THRESHOLD_US)
                {
                    m_nBaseSystemTime += nDiff;
                    m_nPrevAdjTimeStamp = nSysTimeUs;
                    WFDMMLOGE1("Adjust DS BASETIME %lld",nDiff);
                    m_nDSTotalTimeDrift = 0;
                    m_nDSNumSamplesAvgDrift = 0;

                }
            }
        }
        if(m_nStreamingBufCnt -
                       (m_pStreamingPendBufQ->GetSize() +
                        m_pStreamingBufQ->GetSize()) >
                               VIDEO_SOURCE_SLOW_SINK_CONSUMPTION_THRESHOLD &&
           m_nPrevPushedBufsOnRender <
                               VIDEO_SOURCE_SLOW_SINK_CONSUMPTION_THRESHOLD &&
           /* Let catch up to happen initially*/
           nRenderTimeStamp - m_nBaseRenderTimeStamp >
                               VIDEO_SOURCE_DELAYED_RENDERING_TOLERANCE * 2)
        {
            WFDMMLOGE1("SwitchToMirror Num Bufs held at Mux %d",
                       m_nStreamingBufCnt -
                       (m_pStreamingPendBufQ->GetSize() +
                        m_pStreamingBufQ->GetSize()));
            AbortStreaming();
            return OMX_ErrorInsufficientResources;
        }
    }

    if(m_pStreamingPendBufQ )
    {
        OMX_U32 nLoopCnt = 0;
        while(m_pStreamingPendBufQ->GetSize())
        {
            pBufferHdr = NULL;
            result = m_pStreamingPendBufQ->Peek(&pBufferHdr,
                                               sizeof(OMX_BUFFERHEADERTYPE **));
            if(result == OMX_ErrorNone && pBufferHdr)
            {
                /*--------------------------------------------------------------
                  Pushing frames in advance up to VIDEO_SOURCE_DS_PTS_ADJUST_US
                  maximize bandwidth usage. When doing this we should be
                  careful not to send video frames with timestamps far away from
                  current rendering timestamp.
                ----------------------------------------------------------------
                */
                if(ABS(pBufferHdr->nTimeStamp - nRenderTimeStamp) >
                    VIDEO_SOURCE_DS_PTS_ADJUST_US && !bEOS)
                {
                    WFDMMLOGH("ExecuteRenderFrame Buffer TS > Render TS");
                    break;
                }
            }
            else
            {
                break;
            }
            result = m_pStreamingPendBufQ->Pop(&pBufferHdr,
                                           sizeof(OMX_BUFFERHEADERTYPE **), 10);
            if(result != OMX_ErrorNone || !pBufferHdr)
            {
                result = OMX_ErrorBadParameter;
                WFD_OMX_ERROR_CHECK(result, "Null input buffer");
            }

            if(m_pFrameDeliverFn && !m_StreamingClosingReqSent)
            {
                WFDMMLOGH2(
                     "RenderFrame pBufferHdr= %p ,pBufferHdr->pBuffer= %p",
                     pBufferHdr, pBufferHdr->pBuffer);
    #ifdef ENABLE_STREAMING_DUMP
                if((OMX_TRUE == m_bEnableStrDump) && (Str_dump_file))
                {
                    fwrite((void *)pBufferHdr->pBuffer,
                           1, pBufferHdr->nFilledLen, Str_dump_file);
                }
    #endif
                unsigned long nStreamTimeUs = pBufferHdr->nTimeStamp;

                pBufferHdr->nTimeStamp = (nStreamTimeUs - m_nBaseStreamTime)
                        + m_nBaseSystemTime + VIDEO_SOURCE_DS_PTS_ADJUST_US;

                OMX_TICKS nDTS = (OMX_TICKS)(
                               (nRenderTimeStamp - m_nBaseRenderTimeStamp) +
                                m_nBaseSystemTime + nLoopCnt++ *
                                VIDEO_SOURCE_DTS_DEFAULT_FRAME_DELTA);

                FillDTSExtraData(pBufferHdr,nDTS,pBufferHdr->nOutputPortIndex);

                WFDMMLOGH4(
                "TIMESTAMPS DTS=%llu, PTS=%lld BaseTime=%lld  BaseStream=%lld",
                                                        nDTS,
                                                        pBufferHdr->nTimeStamp,
                                                        m_nBaseSystemTime,
                                                        m_nBaseStreamTime);

                m_nPrevPushedBufsOnRender++;

                if(m_pFrameDeliverFn)
                {
                    m_pFrameDeliverFn(pBufferHdr, m_appData);
                }
                MM_Timer_Sleep(DS_FORWARD_DATA_PUSH_INTERVAL_MS);
            }
            else
            {
                WFDMMLOGH("ExecuteRenderFrame Push Buffer Back and Free");
                if(pBufferHdr)
                {
                    VideoSourceFreeBuffer(pBufferHdr);
                }
            }
        }
    }

EXIT:
    WFD_VSRC_CRITICAL_SECT_LEAVE(m_hCritSect);
    return result;
}

/*==============================================================================
 FUNCTION:            StopStreaming()

 DESCRIPTION:
*//**       @brief           received call to stopStreaming and
                     start mirroring depending on the mirring state
*//**
@par     DEPENDENCIES:
                    None
*//*
 PARAMETERS:
*//*         @param         None

*//*     RETURN VALUE:
*//**       @return
                    Success/failure
@par     SIDE EFFECTS:
                    None
*//*===========================================================================*/
OMX_ERRORTYPE VideoSource::StopStreaming()
{
    OMX_ERRORTYPE result = OMX_ErrorNone;
    WFDMMLOGH("StopStreaming()");
    if(m_bStreamingEnabled != OMX_TRUE)
    {
        WFDMMLOGH(" Already in Streaming StopState");
        return OMX_ErrorNone;
    }
    m_bStreamingEnabled = OMX_FALSE;

    DeleteStreamingBuffers();
    m_eStreamingStatus = STREAMING_STOPPING;

    if(mbStopWhileStreaming)
    {
        if(m_hVideoSourceThread)
        {
            m_hVideoSourceThread->SetSignal(VIDEO_SOURCE_STOP);
        }
        return OMX_ErrorNone;
    }
    else
    {
        ExecuteStopStreamingSuccess();
    }

    return result;

EXIT:
    if((result != OMX_ErrorNone) && (m_pEventHandlerFn))
    {
        m_pEventHandlerFn(m_appData, m_nModuleId, WFDMMSRC_ERROR,
                          OMX_ErrorUndefined, 0);
    }
    return result;
}

/*==============================================================================

         FUNCTION:         FlushStreamingQueue()

         DESCRIPTION:
*//**       @brief         freeing the buffer handles used for streaming

*//**

@par     DEPENDENCIES:      None

*//*
         PARAMETERS:
*//**       @param      : None



*//*     RETURN VALUE:
*//**       @return       Success/failure


@par     SIDE EFFECTS:

*//*==========================================================================*/

void  VideoSource::FlushStreamingQueue()
{
    WFDMMLOGM("FlushStreamingQueue() started");
    OMX_ERRORTYPE result = OMX_ErrorNone;
    buff_hdr_extra_info* tempExtra;
    void* pData = NULL;
    OMX_BUFFERHEADERTYPE* pBufferHdr = NULL;
    while(m_pStreamingPendBufQ->GetSize())
    {
        m_pStreamingPendBufQ->Pop(&pBufferHdr,sizeof(OMX_BUFFERHEADERTYPE**),0);
        VideoSourceFreeBuffer(pBufferHdr);
    }
}

/*==============================================================================
         FUNCTION:            FreeStreamingBuffer()

         DESCRIPTION:
*//**       @brief
*//**
@par     DEPENDENCIES:
                            None
*//*
         PARAMETERS:
*//*         @param
                            pBufferHdr to be freed

*//*     RETURN VALUE:
*//**       @return
                            OMX_ErrorNone  or error
@par     SIDE EFFECTS:
                            None
*//*==========================================================================*/
OMX_ERRORTYPE VideoSource::FreeStreamingBuffer(OMX_BUFFERHEADERTYPE* pBufferHdr)
{
    OMX_ERRORTYPE result = OMX_ErrorNone;
    buff_hdr_extra_info* tempExtra;

    if(!pBufferHdr)
    {
        WFDMMLOGE("Failed FreeStreamingBuffer pBufferHdr  NULL");
        return OMX_ErrorBadParameter;
    }

    WFDMMLOGH1("pushing back into Queue pBufferHdr = %p", pBufferHdr);

    /*--------------------------------------------------------------------------
     Construct streamDataType to pass it to Binder to free
     sendcallbackToBinder with STREAMING_FRAME_SENT_DONE
    ----------------------------------------------------------------------------
    */

    WFDMMLOGM3("VideoSource::FreeStreamingBuffer len=%d pBuffer=%p,data=%p",
               pBufferHdr->nFilledLen, pBufferHdr, pBufferHdr->pBuffer);

    result = m_pStreamingBufQ->Push(&pBufferHdr, sizeof(pBufferHdr));
    if(result != OMX_ErrorNone)
    {
        WFDMMLOGE1("Failed pushing back into Queue pBufferHdr =%p", pBufferHdr);
    }
    return result;
}
/*==============================================================================
         FUNCTION:            CreateResources

         DESCRIPTION:
*//**       @brief           creating the required resources for streaming
                             functionalities
*//**
@par     DEPENDENCIES:
                            None
*//*
         PARAMETERS:
*//*         @param
                            None

*//*     RETURN VALUE:
*//**       @return
                            OMX_ErrorNone  or error
@par     SIDE EFFECTS:
                            None
*//*==========================================================================*/

OMX_ERRORTYPE VideoSource::CreateResources()
{
    WFDMMLOGH("VideoSource::CreateResources()");
    OMX_ERRORTYPE result = OMX_ErrorNone;

    m_nNumBuffers = m_pVencoder->GetNumOutBuf();

    if(m_nNumBuffers > WFDMM_VIDEO_SRC_MAX_BITSTREAM_BUFS)
    {
        m_nNumBuffers = WFDMM_VIDEO_SRC_MAX_BITSTREAM_BUFS;
    }

#ifdef ENABLE_STREAMING_DUMP
    m_bEnableStrDump = OMX_FALSE;
    if(IS_DUMP_ENABLE())
    {
        Str_dump_file = fopen("/data/str_dump_user.bin","wb");
        if(!Str_dump_file)
        {
            WFDMMLOGE("WFDMMSourceVideoSource::file open failed");
            m_bEnableStrDump = OMX_FALSE;
        }
        else
        {
            m_bEnableStrDump = OMX_TRUE;
        }
    }
#endif
    if(m_bBlankFrameSupport)
    {

        m_nMaxBlankFrames = MAX_BLANKING_PERIOD / (1000 / m_sConfig.nFramerate);

        /*---------------------------------------------------------------------
         Select the blank frame to be sent based on negotiated profile to not
         irk rigid decoders
        -----------------------------------------------------------------------
        */

        if(!m_pBlankFrame)
        {

            if(m_sConfig.nProfile == WFD_H264_PROFILE_CONSTRAINED_BASE)
            {
                m_pBlankFrame = const_cast<OMX_U8*>(sVGA_CBP_BkFm);
                m_nBlankFrameSize = static_cast<OMX_U32>(sizeof(sVGA_CBP_BkFm));
            }
            else if(m_sConfig.nProfile == WFD_H264_PROFILE_CONSTRAINED_HIGH)
            {
                m_pBlankFrame = const_cast<OMX_U8*>(sVGA_CHP_BkFm);
                m_nBlankFrameSize = static_cast<OMX_U32>(sizeof(sVGA_CHP_BkFm));
            }

            WFDMMLOGH2("Done with blank frame of size %ld for profile %ld",
                       m_nBlankFrameSize, m_sConfig.nProfile);
        }

        /*-------------------------------------------------------------
         Maintain a queue for supplying blank frames to MUX
        ---------------------------------------------------------------
        */
        if(!m_pBlankFrameQ)
        {
            m_pBlankFrameQ = MM_New_Args(SignalQueue,
                                         (m_nMaxBlankFrames,
                                         sizeof(OMX_BUFFERHEADERTYPE*)));

            if(!m_pBlankFrameQ)
            {
                WFDMMLOGE("Failed to allocate blank frame Q!");
                return OMX_ErrorInsufficientResources;
            }

            for(OMX_S32 i = 0; i < m_nMaxBlankFrames; i++)
            {
                OMX_BUFFERHEADERTYPE* pBufferHdr = NULL;
                buff_hdr_extra_info* pExtraInfo = NULL;

                pBufferHdr = static_cast<OMX_BUFFERHEADERTYPE*>
                             (MM_Malloc(sizeof(*pBufferHdr)));

                if(!pBufferHdr)
                {
                    WFDMMLOGE("Failed to allocate blank frame buffers!");
                    return OMX_ErrorInsufficientResources;
                }

                memset(pBufferHdr,0,sizeof(*pBufferHdr));

                pExtraInfo = static_cast<buff_hdr_extra_info*>
                             (MM_Malloc(sizeof(*pExtraInfo)));

                if(!pExtraInfo)
                {
                    MM_Free(pBufferHdr);
                    WFDMMLOGE("Failed to allocate extaInfo!");
                    return OMX_ErrorInsufficientResources;
                }

                pExtraInfo->bBlackFrame         = OMX_FALSE;

                pBufferHdr->pAppPrivate    = static_cast<OMX_PTR>
                                                  (pExtraInfo);

                /*-------------------------------------------------------------
                 Fixed data from our end using the reference blank frames
                ---------------------------------------------------------------
                */
                pBufferHdr->nSize               = sizeof(*pBufferHdr);
                pBufferHdr->pBuffer             = m_pBlankFrame;
                pBufferHdr->nInputPortIndex     = 0;
                pBufferHdr->nOffset             = 0;
                pBufferHdr->nOutputPortIndex    = 1;
                pBufferHdr->nVersion.nVersion   = static_cast<OMX_U32>
                                                  (OMX_SPEC_VERSION);
                pBufferHdr->nAllocLen           = m_nBlankFrameSize;

                /*-------------------------------------------------------------
                 Done with this OMX_BUFFERHEADER, add it to the Blank Frame Q
                ---------------------------------------------------------------
                */
                WFDMMLOGH1("Pushing %p to Blank Frame Q", pBufferHdr);

                m_pBlankFrameQ->Push(&pBufferHdr, sizeof(pBufferHdr));
            }
        }

    }

    buff_hdr_extra_info* tempExtra;
    m_hVideoSourceThread = MM_New_Args(WFDMMThreads, (VIDEO_SOURCE_MAX_EVENT));
    if(!m_hVideoSourceThread)
    {
        WFDMMLOGE("Failed to create m_hVideoSourceThread");
        return OMX_ErrorInsufficientResources;
    }
    m_hVideoSourceThread->Start(VideoSourceThreadEntry,
                                -2, 32768,this, "VideoSourceThread");
    m_bVideoSourceActive = true;

    if(m_bHdcpSessionValid)
    {
        /*----------------------------------------------------------------------
          HDCP handle is valid which implies HDCP connection has
          gone through. Now that encoder module has been configured,
          go ahead and allocate the resources required for HDCP.
        ------------------------------------------------------------------------
        */
        result = AllocateHDCPResources();
        if(result != OMX_ErrorNone)
        {
            WFDMMLOGE("Failed to allocate HDCP resources");
            return result;
        }
    }

    if(!m_bHdcpSessionValid && m_bFillerNaluEnabled)
    {
        /*----------------------------------------------------------------------
          For non HDCP session
        ------------------------------------------------------------------------
        */
        m_pFillerDataOutPtr = (unsigned char*)sFillerNALU;
    }

    /*--------------------------------------------------------------------------
      Adding Steraming binder callback to Video source
      Create Q for holding OMX BufferHeaders in for streaming session which
      will be delivered to MUX or HDCP
    ----------------------------------------------------------------------------
    */

    m_pStreamingBufQ = MM_New_Args(SignalQueue,
                                   (m_nStreamingBufCnt,
                                    sizeof(OMX_BUFFERHEADERTYPE*)));
    if(m_pStreamingBufQ == NULL)
    {
        WFDMMLOGE("Failed to create m_pStreamingBufQ ");
        return OMX_ErrorInsufficientResources;
    }

    m_pStreamingPendBufQ = MM_New_Args(SignalQueue,
                                       (m_nStreamingBufCnt,
                                        sizeof(OMX_BUFFERHEADERTYPE*)));
    if(m_pStreamingPendBufQ == NULL)
    {
        WFDMMLOGE("Failed to create m_pStreamingBufQ ");
        return OMX_ErrorInsufficientResources;
    }

    m_pStreamingBufArray = MM_New_Array(OMX_BUFFERHEADERTYPE*,
                                        m_nStreamingBufCnt);
    if(m_pStreamingBufArray == NULL)
    {
        WFDMMLOGE("Failed to create m_pStreamingBufArray ");
        return OMX_ErrorInsufficientResources;
    }

    for(int ii = 0; ii < m_nStreamingBufCnt; ii++)
    {
        OMX_BUFFERHEADERTYPE* pBufferHdr = (OMX_BUFFERHEADERTYPE*)
                                        MM_Malloc(sizeof(OMX_BUFFERHEADERTYPE));
        if(pBufferHdr != NULL && m_pStreamingBufQ != NULL)
        {
            m_pStreamingBufArray[ii] = pBufferHdr;
            memset(pBufferHdr, 0, sizeof(OMX_BUFFERHEADERTYPE));
            tempExtra = static_cast<buff_hdr_extra_info*>
                        (calloc(1, sizeof(buff_hdr_extra_info)));
            if(tempExtra)
            {
                tempExtra->eMode = WFD_OPERATION_STREAMING;
                pBufferHdr->pAppPrivate = static_cast<OMX_PTR>(tempExtra);
            }
            m_pStreamingBufQ->Push(&pBufferHdr, sizeof(pBufferHdr));
        }
        else
        {
            break;
            WFDMMLOGE("Failed to create pBufferHdr CreateResources()");
            return OMX_ErrorInsufficientResources;
        }
    }
    int nRet = MM_CriticalSection_Create(&m_hCritSect);
    if(nRet)
    {
        WFDMMLOGE("Failed to create Critical Sections");
        return OMX_ErrorInsufficientResources;
    }
    return OMX_ErrorNone;
}


/*=============================================================================

        FUNCTION:          ReleaseResources

        DESCRIPTION:
*//**       @brief         Responisble for resource deallocation for VideoSource
*//**
@par     DEPENDENCIES:
                           Should be called once Encoder module is deleted.
*//*
        PARAMETERS:
*//**       @param          None

*//*     RETURN VALUE:
*//**       @return
                           OMX_ErrorNone or other Error
@par     SIDE EFFECTS:
                           None
*//*=========================================================================*/


OMX_ERRORTYPE VideoSource::ReleaseResources()
{
    WFDMMLOGH("VideoSource::releaseResources()");
    OMX_ERRORTYPE result = OMX_ErrorNone;
    buff_hdr_extra_info* tempExtra;

    if(m_pBlankFrameQ)
    {

        /*---------------------------------------------------------------------
          Wait for all blank frames to return from MUX, if any
        -----------------------------------------------------------------------
        */
        while(m_pBlankFrameQ->GetSize() != m_nMaxBlankFrames)
        {
            WFDMMLOGH1("Waiting for %ld blank frames from MUX",
                       m_nNumBuffers - m_pBlankFrameQ->GetSize());
            MM_Timer_Sleep(2);
        }

        while(m_pBlankFrameQ->GetSize())
        {
            OMX_BUFFERHEADERTYPE* pBuffHdr = NULL;
            m_pBlankFrameQ->Pop(&pBuffHdr, sizeof(pBuffHdr), 100);

            if(pBuffHdr)
            {
                if(pBuffHdr->pAppPrivate)
                {
                    MM_Free(pBuffHdr->pAppPrivate);
                }
                MM_Free(pBuffHdr);
                WFDMMLOGH1("Deleted %p from Blank Frame Q", pBuffHdr);
            }
        }

        WFDMMLOGH("Done with buffers for Blank Frame Q");

        MM_Delete(m_pBlankFrameQ);
        m_pBlankFrameQ = NULL;
    }

    if(m_pBlankFrame)
    {
        m_pBlankFrame = NULL;
    }
    /*--------------------------------------------------------------------------
      Releasing OMX BufferHeaders and resources created in for streaming session
    ----------------------------------------------------------------------------
    */
    if((m_pStreamingBufArray))
    {
        for(int ii = 0; ii < m_nStreamingBufCnt; ii++)
        {
            if(m_pStreamingBufArray[ii] != NULL)
            {
                OMX_BUFFERHEADERTYPE* pBufferHdr = m_pStreamingBufArray[ii];
                if(pBufferHdr)
                {
                    tempExtra = static_cast<buff_hdr_extra_info*>
                        (pBufferHdr->pAppPrivate);
                    if(tempExtra)
                    {
                        MM_Free(tempExtra);
                    }
                    MM_Free(pBufferHdr);
                }
                pBufferHdr = NULL;
            }
        }
        MM_Delete_Array(m_pStreamingBufArray);
        m_pStreamingBufArray = NULL;
    }

    if(m_pStreamingBufQ != NULL)
    {
        delete m_pStreamingBufQ;
        m_pStreamingBufQ = NULL;
    }
    if(m_pStreamingPendBufQ)
    {
        delete m_pStreamingPendBufQ;
        m_pStreamingPendBufQ = NULL;
    }

    if(m_bHdcpSessionValid)
    {
        DeallocateHDCPResources();
    }

    m_bVideoSourceActive = false;

    if(m_hCritSect)
    {
        MM_CriticalSection_Release(m_hCritSect);
        m_hCritSect = NULL;
    }

#ifdef ENABLE_STREAMING_DUMP
    if(Str_dump_file)
    {
        fclose(Str_dump_file);
        Str_dump_file = NULL;
    }
#endif
    return result;
}

/*==============================================================================
         FUNCTION:            VideoSourceThreadEntry

         DESCRIPTION:
*//**       @brief           main body for VideoSourceThreadEntry execution
*//**
@par     DEPENDENCIES:
                            None
*//*
         PARAMETERS:
*//*         @param         pThreadData Context for VideoSource class this ptr
                            nSignal  signal rised to perforam specified action
*//*     RETURN VALUE:
*//**       @return
                            None
@par     SIDE EFFECTS:
                            None
*//*==========================================================================*/

void VideoSource::VideoSourceThreadEntry
(
    void *pThreadData,
    unsigned int signal
)
{
    if(!pThreadData)
    {
        return ;
    }
    WFDMMLOGH1("VideoSourceThreadEntry %d",signal);
    VideoSource *pMe = (VideoSource*)pThreadData;
    pMe->VideoSourceThread(signal);

}

/*==============================================================================
         FUNCTION:            VideoSourceThread

         DESCRIPTION:
*//**       @brief           performing action depending on the signal received
*//**
@par     DEPENDENCIES:
                            None
*//*
         PARAMETERS:
*//*         @param
                            nSignal

*//*     RETURN VALUE:
*//**       @return
                            OMX_ErrorNone  or error
@par     SIDE EFFECTS:
                            None
*//*==========================================================================*/
OMX_ERRORTYPE VideoSource::VideoSourceThread(int nSignal)
{
    OMX_ERRORTYPE result = OMX_ErrorNone;
    int ret ;

    WFDMMLOGH("WFDMMSourceVideoSource::VideoSourceThread Entry");

    /*--------------------------------------------------------------------------
    //If ever rechristening of this thread is implemented bear in mind that
    //there is an upper limit to the name (as of now 16)
    ----------------------------------------------------------------------------
    */
    ret = pthread_setname_np(pthread_self(),"WFD_VSRC_Thread");
    if(ret)
    {
        WFDMMLOGE2("Failed to set thread name due to %d %s",
                   ret, strerror(errno));
    }

    switch (nSignal)
    {
        case VIDEO_SOURCE_STREAMING_REQ:
        {
            if((m_streamParams.bSecure && !m_bHdcpSessionValid) ||
               (m_eVideoSrcState != WFDMM_VideoSource_STATE_PLAY &&
                m_eVideoSrcState != WFDMM_VideoSource_STATE_PLAYING))
            {
                m_eStreamingStatus = STREAMING_FAILURE;
            }
            else
            {
                if(m_pEventHandlerFn)
                {
                    m_pEventHandlerFn(m_appData, m_nModuleId,
                                      WFDMMSRC_STREAMING_REQ,
                                      OMX_ErrorNone, &m_CodecInfo);
                }
            }
        }
            break;

        case VIDEO_SOURCE_START:
        {
            result = ExecuteStart();
        }
            break;

        case  VIDEO_SOURCE_STREAMING_START:
        {
            result = StartStreaming();
        }
            break;

        case VIDEO_SOURCE_SET_START_STREAMING_SUCCESS:
        {
            if(m_pEventHandlerFn)
            {
                OMX_TICKS nTimeDelay = VIDEO_SOURCE_DS_PTS_ADJUST_US;
                m_pEventHandlerFn(m_appData, m_nModuleId,
                                  WFDMMSRC_STREAMING_START_SUCCESS,
                                  OMX_ErrorNone, &nTimeDelay);
                WFDMMLOGH("VIDEO_SOURCE_SET_START_STREAMING_SUCCESS \n");
            }
        }
            break;

        case  VIDEO_SOURCE_STREAMING_STOP:
        {
            result = StopStreaming();
        }
        break;

        case VIDEO_SOURCE_PAUSE:
        {
            ExecutePause();
        }
            break;

        case VIDEO_SOURCE_RENDER_FRAME:
        {
            ExecuteRenderFrame();
        }
            break;

        case VIDEO_SOURCE_STOP:
        {
            ExecuteFinish();
        }
            break;

        case VIDEO_SOURCE_SET_STOP_STREAMING_SUCCESS:
        {
            m_eStreamingStatus = STREAMING_DEINIT;

            /* Clean up streaming resources before switching to mirroring*/
            if(m_pEventHandlerFn)
            {
                m_pEventHandlerFn(m_appData, m_nModuleId,
                                  WFDMMSRC_STREAMING_STOP_DONE_SUCCESS,
                                  OMX_ErrorNone, 0);
                WFDMMLOGH("WFDMMSRC_STREAMING_STOP_DONE_SUCCESS");
            }

            if(m_pVencoder && (m_eVideoSrcState == WFDMM_VideoSource_STATE_PAUSE)
               && m_eVideoStreamingSrcState != WFDMM_VideoSource_STATE_PAUSE)
            {
                WFDMMLOGM("Mirroring Resume");
                result = m_pVencoder->Resume();
                WFD_OMX_ERROR_CHECK(result,"Failed m_pVencoder->Resume()StopStreaming");
                m_eVideoSrcState = WFDMM_VideoSource_STATE_PLAY;
                m_eVideoStreamingSrcState = WFDMM_VideoSource_STATE_IDLE;
            }
            else
            {
                m_eVideoStreamingSrcState = WFDMM_VideoSource_STATE_IDLE;
                WFDMMLOGE2("Failed to Mirror m_pVencoder = %p/m_eVideoSrcState = %d",
                           m_pVencoder, m_eVideoSrcState);
            }

        }
        break;

        case VIDEO_SOURCE_FLUSH:
        {
            result = ExecuteFlush();
        }
            break;
    default:
        {
            WFDMMLOGE("Failure VideoSourceThread default");
        }
        break;
    }
EXIT:
    return result;
}

/*==============================================================================
         FUNCTION:            sendcallbackToBinder

         DESCRIPTION:
*//**       @brief           sending callback to Binder
*//**
@par     DEPENDENCIES:
                            None
*//*
         PARAMETERS:
*//*         @param
                            StrmError : provides error information

*//*     RETURN VALUE:
*//**       @return
                            OMX_ErrorNone  or error
@par     SIDE EFFECTS:
                            None
*//*==========================================================================*/


OMX_ERRORTYPE VideoSource::sendcallbackToBinder
(
    StreamingEventType StrmEvent,
    void* pData
)
{
    UNUSED_VAR(pData);
    OMX_ERRORTYPE result = OMX_ErrorNone;
    switch (StrmEvent)
    {
        case STREAMING_PAUSE_PLAYBACK:
        {
            WFDMMLOGH("sendcallbackToBinder STREAMING_PAUSE_PLAYBACK \n");
        }
        break;
        case STREAMING_RESUME_PLAYBACK:
        {
            WFDMMLOGH("sendcallbackToBinder STREAMING_RESUME_PLAYBACK \n");
        }
        break;
        case STREAMING_FRAME_COMPLETE:
        {
            WFDMMLOGH("sendcallbackToBinder STREAMING_FRAME_COMPLETE \n");
        }
        break;

        case STREAMING_FD_FRAME_COMPLETE:
        {
            WFDMMLOGH("sendcallbackToBinder STREAMING_FD_FRAME_COMPLETE \n");
        }
        break;
    case STREAMING_CLOSE_REQ:
        {
            WFDMMLOGH("sendcallbackToBinder STREAMING_CLOSE_REQ \n");
            m_StreamingClosingReqSent = true;
        }
        break;
    case STREAMING_START_FAIL:
        {
            WFDMMLOGH(" Failed: sendcallbackToBinder STREAMING_START_FAIL \n");
        }
        break;
    case STREAMING_FRAME_REC_FAIL:
        {
            WFDMMLOGH1(" Failed: sendcallbackToBinder %s \n",
                       "STREAMING_FRAME_REC_FAIL");
        }
        break;
    case STREAMING_STOP_FAIL:
        {
            WFDMMLOGH(" Failed: sendcallbackToBinder STREAMING_STOP_FAIL \n");
        }
        break;
    case STREAMING_QUEUE_FULL:
        {
            WFDMMLOGH("Failed:sendcallbackToBinder STREAMING_QUEUE_FULL\n");
        }
        break;
    case STREAMING_MEM_ALLOC_FAIL:
        {
            WFDMMLOGH("Failed:sendcallbackToBinder STREAMING_MEM_ALLOC_FAIL\n");
        }
        break;
    case STREAMING_ERROR_NONE:
        {
            WFDMMLOGH(" Failed: sendcallbackToBinder STREAMING_ERROR_NONE \n");
        }
        break;
    case STREAMING_PAUSE_FAIL:
        {
            WFDMMLOGH(" Failed: sendcallbackToBinder STREAMING_PAUSE_FAIL \n");
        }
        break;
    }
    int event = (int)StrmEvent;
    WFDMMService::notifyClient(m_WFDMMService,event,(void*)pData);
    return result;
}

/*==============================================================================
         FUNCTION:            VideoSourceFreeBuffer

         DESCRIPTION:
*//**       @brief           depending the Streaming or mirroring queueing the
                            buffer header to queue
*//**
@par     DEPENDENCIES:
                            None
*//*
         PARAMETERS:
*//*         @param
                            pBufferHdr

*//*     RETURN VALUE:
*//**       @return
                            OMX_ErrorNone  or error
@par     SIDE EFFECTS:
                            None
*//*==========================================================================*/

OMX_ERRORTYPE VideoSource::VideoSourceFreeBuffer
(
   OMX_BUFFERHEADERTYPE* pBufferHdr
)
{
    OMX_ERRORTYPE result = OMX_ErrorNone;
    buff_hdr_extra_info* tempExtra;
    WFDMMLOGH1("VideoSource::VideoSourceFreeBuffer = %p ",pBufferHdr);

    if(pBufferHdr != NULL && pBufferHdr->pBuffer != NULL)
    {
        tempExtra = static_cast<buff_hdr_extra_info*>
                    (pBufferHdr->pAppPrivate);

        if(tempExtra &&
           tempExtra->eMode == (OperationType)WFD_OPERATION_STREAMING)
        {
           result = FreeStreamingBuffer(pBufferHdr);
        }
        else
        {
            if(m_pVencoder)
            {
                result = m_pVencoder->SetFreeBuffer(pBufferHdr);
            }
        }
    }
    else
    {
        WFDMMLOGE1("VideoSource::VideoSourceFreeBuffer bad params = %p "
                   , pBufferHdr);
        result = OMX_ErrorBadParameter;
    }
    return result;
}

int VideoSource::isCfgSupportForStreaming()
{
    int result = -1;

    /*Read from cfg file and return the value for streaming support*/
    int nVal;
    nVal = 0;

    getCfgItem(STREAMING_FEATURE_SUPPORT_KEY, &nVal);
    result = nVal;
    WFDMMLOGH1("VideoSource:: isStreamingFeatureEnabled - Cfg File value = %d",
        result);

    return result;/*0 or 1*/
}

/*==============================================================================
         FUNCTION:          setAppInfo

         DESCRIPTION:
*//**       @brief          This function receives name of application active
                            and operation performed on that application.
                            It also checks if this application allows DS or not.
                            And same is updated to WFDMMService.
                            And this variable is used in taking decision to
                            start DS.
*//**
@par     DEPENDENCIES:
                            None
*//*
         PARAMETERS:
*//*         @param
                            [in]name
                            [in]operation

*//*     RETURN VALUE:
*//**       @return
                            SUCCESS
@par     SIDE EFFECTS:
                            None
*//*==========================================================================*/
int VideoSource::setAppInfo(char *name,char *operation)
{
    bool err = false;

    if(!m_pAppSupported)
    {
        return 0;
    }

    if(name != NULL && operation != NULL)
    {
        /* Extracting Package Name */
        char *pTmpPtr = NULL;
        char *packageName = strtok_r(name,"/", &pTmpPtr);
        err = true;
        if(packageName != NULL)
        {
            strlcpy(m_AppWhiteList.name,packageName,WHITELIST_APP_NAME_LENGTH);
            strlcpy(m_AppWhiteList.operation,operation,WHITELIST_OPERATION_LENGTH);
            WFDMMLOGM2("VideoSource:: setAppInfo - app name = %s opera = %s",
                                  m_AppWhiteList.name,m_AppWhiteList.operation);
            bool allowed = isCurrentAppAllowed();
            WFDMMService::updateWhiteListStatus(allowed);
            err = false;
        }
    }
    else
    {
        err = true;
    }
    if(err)
    {
        WFDMMLOGE1("VideoSource:: setAppInfo - Fails - app name = %s",name);
        WFDMMService::updateWhiteListStatus(false);
    }
    return 0;
}

/*==============================================================================
         FUNCTION:          isCurrentAppAllowed

         DESCRIPTION:
*//**       @brief          Checks and returns whether current application is
                            present in allowed list of application or not.
*//**
@par     DEPENDENCIES:
                            None
*//*
         PARAMETERS:
*//*         @param
                            None

*//*     RETURN VALUE:
*//**       @return
                            Bool
@par     SIDE EFFECTS:
                            None
*//*==========================================================================*/
bool VideoSource::isCurrentAppAllowed()
{
    if(m_nCountAppSupported <= 0)
        return true;
    else
    {
        for(int i = 0; i < m_nCountAppSupported; i++)
        {
            if(!strncmp(m_AppWhiteList.name,m_pAppSupported[i],
                                                WHITELIST_APP_NAME_LENGTH))
                return true;
        }
    }
    return false;
}

int  VideoSource::checkStreamingValidityOnCodec(StreamParamsType streamParams)
{
    int result = -1;
    switch ((OMX_VIDEO_CODINGTYPE)streamParams.codecType)
    {
        case OMX_VIDEO_CodingAVC:
        {
            char codec[] = "H.264";
            memset(&m_CodecInfo, 0, sizeof(m_CodecInfo));
            strlcpy(&m_CodecInfo.codecType[0], &codec[0], sizeof(m_CodecInfo.codecType));
            /*------------------------------------------------------------------
              OMX_VIDEO_AVCProfileBaseline = 0x01,   < Baseline profile >
              OMX_VIDEO_AVCProfileMain     = 0x02,   < Main profile
              OMX_VIDEO_AVCProfileExtended = 0x04,   < Extended profile
              OMX_VIDEO_AVCProfileHigh     = 0x08,   < High profile
              OMX_VIDEO_AVCProfileHigh10   = 0x10,   < High 10 profile
              OMX_VIDEO_AVCProfileHigh422  = 0x20,   < High 4:2:2 profile
              OMX_VIDEO_AVCProfileHigh444  = 0x40,   < High 4:4:4 profile
              OMX_VIDEO_AVCProfileKhronosExtensions = 0x6F000000,
                    Reserved region for introducing Khronos Standard Extensions
              OMX_VIDEO_AVCProfileVendorStartUnused = 0x7F000000,
                       < Reserved region for introducing Vendor Extensions
              OMX_VIDEO_AVCProfileMax      = 0x7FFFFFFF
            --------------------------------------------------------------------
            */
            switch (streamParams.profile)
            {
                case OMX_VIDEO_AVCProfileBaseline:
                    m_CodecInfo.profile = 0x2;
                    if(streamParams.bConstrainedProfile)
                    {
                        m_CodecInfo.profile = 0x0;
                    }
                    break;

                case OMX_VIDEO_AVCProfileMain:
                    m_CodecInfo.profile = 3;
                    break;

                case OMX_VIDEO_AVCProfileHigh:
                    m_CodecInfo.profile = 4;
                    if(streamParams.bConstrainedProfile)
                    {
                        m_CodecInfo.profile = 1;
                    }
                    break;

                default:
                    WFDMMLOGE1("Error: Mapping Profiles in H.264 = %d "
                               , streamParams.profile);
                    break;
            }

            /*------------------------------------------------------------------
                  OMX_VIDEO_AVCLevel1   = 0x01,     < Level 1
                  OMX_VIDEO_AVCLevel1b  = 0x02,     < Level 1b
                  OMX_VIDEO_AVCLevel11  = 0x04,     < Level 1.1
                  OMX_VIDEO_AVCLevel12  = 0x08,     < Level 1.2
                  OMX_VIDEO_AVCLevel13  = 0x10,     < Level 1.3
                  OMX_VIDEO_AVCLevel2   = 0x20,     < Level 2
                  OMX_VIDEO_AVCLevel21  = 0x40,     < Level 2.1
                  OMX_VIDEO_AVCLevel22  = 0x80,     < Level 2.2
                  OMX_VIDEO_AVCLevel3   = 0x100,    < Level 3
                  OMX_VIDEO_AVCLevel31  = 0x200,    < Level 3.1
                  OMX_VIDEO_AVCLevel32  = 0x400,    < Level 3.2
                  OMX_VIDEO_AVCLevel4   = 0x800,    < Level 4
                  OMX_VIDEO_AVCLevel41  = 0x1000,   < Level 4.1
                  OMX_VIDEO_AVCLevel42  = 0x2000,   < Level 4.2
                  OMX_VIDEO_AVCLevel5   = 0x4000,   < Level 5
                  OMX_VIDEO_AVCLevel51  = 0x8000,   < Level 5.1
                  OMX_VIDEO_AVCLevel52  = 0x10000,  < Level 5.2
                  OMX_VIDEO_AVCLevelKhronosExtensions = 0x6F000000,
                  < Reserved region for introducing Khronos Standard Extensions
                  OMX_VIDEO_AVCLevelVendorStartUnused = 0x7F000000,
                  < Reserved region for introducing Vendor Extensions
                  OMX_VIDEO_AVCLevelMax = 0x7FFFFFFF
            --------------------------------------------------------------------
            */
            switch (streamParams.level)
            {
                case OMX_VIDEO_AVCLevel31:
                    m_CodecInfo.level = 0x1 << 0;
                    break;
                case OMX_VIDEO_AVCLevel32:
                    m_CodecInfo.level = 0x1 << 1;
                    break;
                case OMX_VIDEO_AVCLevel4:
                    m_CodecInfo.level = 0x1 << 2;
                    break;
                case OMX_VIDEO_AVCLevel41:
                    m_CodecInfo.level = 0x1 << 3;
                    break;
                case OMX_VIDEO_AVCLevel42:
                    m_CodecInfo.level = 0x1 << 4;
                    break;
                case OMX_VIDEO_AVCLevel5:
                    m_CodecInfo.level = 0x1 << 5;
                    break;
                case OMX_VIDEO_AVCLevel51:
                    m_CodecInfo.level = 0x1 << 6;
                    break;
                case OMX_VIDEO_AVCLevel52:
                    m_CodecInfo.level = 0x1 << 7;
                    break;
                default:
                    WFDMMLOGE1("Error: Mapping level in H.264 = %d "
                               , streamParams.level);
                    break;
            }
            m_CodecInfo.width = (uint16)streamParams.width;
            m_CodecInfo.height = (uint16)streamParams.height;
            result = 0;

            WFDMMLOGE3("checkStreamingValidityOnCodec = %s, %d, %d"
                       , m_CodecInfo.codecType, m_CodecInfo.profile,
                       m_CodecInfo.level);
            WFDMMLOGE2("checkStreamingValidityOnCodec = %d,%d "
                       , m_CodecInfo.width, m_CodecInfo.height);
        }
            break;
        case OMX_VIDEO_CodingMPEG4:
        {
            char codec[] = "MPEG4";
            strlcpy(&m_CodecInfo.codecType[0], &codec[0], sizeof(m_CodecInfo.codecType));
            m_CodecInfo.profile = (uint8)streamParams.profile;
            m_CodecInfo.level = (uint8)streamParams.level;
            m_CodecInfo.width = (uint16)streamParams.width;
            m_CodecInfo.height = (uint16)streamParams.height;
            result = 0;
        }
        break;
    default:
        WFDMMLOGE("Failed checkStreamingValidityOnCodec default");
        break;
    }
    return result;
}


/*==============================================================================

         FUNCTION:          ProcessInputBuffer

         DESCRIPTION:       Process Buffer received through binder
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
@par     SIFE EFFECTS:
                            None
*//*==========================================================================*/
OMX_BUFFERHEADERTYPE* VideoSource::ProcessInputBuffer
(
    unsigned int nLen,
    int          fd,
    char        *pData,
    bool         bSecure
)
{
    OMX_BUFFERHEADERTYPE* pBuffHdr = NULL;
    bool bMatchFound               = false;
    OMX_ERRORTYPE result           = OMX_ErrorBadParameter;
    buff_hdr_extra_info* tempExtra = NULL;
    buffer* ion_buffer_out         = NULL;
    unsigned int length            = 0;
    unsigned int ReqLen            = nLen;
    int QAvlSize                   = 0;
    OMX_U8*  pVirtaddr             = NULL;

    if(fd < 0 && pData == NULL)
    {
        WFD_OMX_ERROR_CHECK(result, "Invalid Args!");
    }

    if(m_pStreamingBufQ)
    {
        QAvlSize = m_pStreamingBufQ->GetSize();
    }

    WFDMMLOGM4("%s QAvlSize= %d nLen = %d, fd = %d \n",
               __FUNCTION__, QAvlSize, nLen, fd);

    if(m_pStreamingBufQ && QAvlSize != 0)
    {
        if(m_bHdcpSessionValid)
        {
            /*------------------------------------------------------------------
             To accomodate Extra data
            --------------------------------------------------------------------
            */
            ReqLen = ReqLen + 256;
        }


        /*----------------------------------------------------------------------
         Search if a buffer that can carry the incoming data is available
        ------------------------------------------------------------------------
        */
        for(int ii = 0; ii < QAvlSize; ii++)
        {
            result = m_pStreamingBufQ->Pop(&pBuffHdr, sizeof(pBuffHdr), 100);
            if(result == OMX_ErrorNone)
            {
                if(pBuffHdr->nAllocLen >= ReqLen && pBuffHdr->pBuffer)
                {
                    bMatchFound = true;
                    break;
                }
                else
                {
                    m_pStreamingBufQ->Push(&pBuffHdr, sizeof(pBuffHdr));
                }
            }
        }

        /*----------------------------------------------------------------------
         If no existing buffer can carry incoming data, realloc existing one
         with larger capacity
        ------------------------------------------------------------------------
        */
        if(!bMatchFound)
        {
            result = m_pStreamingBufQ->Pop(&pBuffHdr, sizeof(pBuffHdr), 100);

            if(result == OMX_ErrorNone)
            {
                tempExtra = (static_cast<buff_hdr_extra_info*>
                             (pBuffHdr->pAppPrivate));

                if(!tempExtra)
                {
                    m_pStreamingBufQ->Push(&pBuffHdr, sizeof(pBuffHdr));
                    result = OMX_ErrorInsufficientResources;
                    WFD_OMX_ERROR_CHECK(result,
                                        "Invalid tempExtra in !bMatchFound");
                }

                if(pBuffHdr->pBuffer != NULL)
                {
                    MM_Free(pBuffHdr->pBuffer);
                }

                memset(tempExtra, 0, sizeof(buff_hdr_extra_info));

                tempExtra->eMode    = WFD_OPERATION_STREAMING;
                pBuffHdr->pBuffer   = NULL;
                pBuffHdr->nAllocLen = 0;

                length = ((nLen + QUART_MEGA_BYTES - 1) >> 8) << 8;

                pBuffHdr->pBuffer = (OMX_U8*)MM_Malloc(length);
                if(pBuffHdr->pBuffer == NULL)
                {
                    m_pStreamingBufQ->Push(&pBuffHdr, sizeof(pBuffHdr));
                    pBuffHdr = NULL;
                    result = OMX_ErrorBadParameter;
                    WFD_OMX_ERROR_CHECK(result,
                                        "Failed to MM_Malloc in !bMatchFound!");
                }
                pBuffHdr->nAllocLen = length;
            }
            else
            {
                result = OMX_ErrorBadParameter;
                WFD_OMX_ERROR_CHECK(result,
                                    "Failed Pop pBuffHdr in !bMatchFound");
            }
        }

        pBuffHdr->nFilledLen = nLen;
        pBuffHdr->nFlags     = 0;

        if(m_bHdcpSessionValid)
        {
            WFDMMLOGL("Encryption started in DS");
            result = EncryptData(pBuffHdr, fd, pData, nLen, bSecure);

            if(result != OMX_ErrorNone)
            {
                m_pStreamingBufQ->Push(&pBuffHdr, sizeof(pBuffHdr));
                pBuffHdr = NULL;
                result   = OMX_ErrorInsufficientResources;
                WFD_OMX_ERROR_CHECK(result, "EncryptData is Failed in DS");
            }
        }
        else
        {
            pVirtaddr = fd >= 0 ?
                       (OMX_U8*)GetVirtualAddressForIonFd(false, nLen, fd):
                       (OMX_U8*)pData;
            if(pVirtaddr == NULL)
            {
                WFDMMLOGE2("pVirtaddr is NULLfor Fd = %d,Len=%d,\n",
                           fd, nLen);
                m_pStreamingBufQ->Push(&pBuffHdr, sizeof(pBuffHdr));
                pBuffHdr = NULL;
                result = OMX_ErrorInsufficientResources;
                WFD_OMX_ERROR_CHECK(result,
                                    "pVirtaddr is NULL in !bMatchFound");
            }
            memcpy(pBuffHdr->pBuffer, pVirtaddr, nLen);
            if(fd >= 0)
            {
                MMIonMemoryFree(-1, nLen, (char*)pVirtaddr);
            }
        }
    }
    else
    {
        WFDMMLOGE2("QAvlSize is Zero for Fd = %d,Len=%d,\n",
                   fd, nLen);
        WFDMMLOGE("SwitchToMirror: Out of Streaming Buffers");
        AbortStreaming();
        return NULL;
    }
    return pBuffHdr;
EXIT:
    return NULL;
}

/*==============================================================================

         FUNCTION:          DeleteStreamingBuffers

         DESCRIPTION:       DeleteStreamingBuffers is what it does
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
@par     SIFE EFFECTS:
                            None
*//*==========================================================================*/
void VideoSource::DeleteStreamingBuffers()
{
    OMX_BUFFERHEADERTYPE* pBuffHdr = NULL;
    OMX_ERRORTYPE result = OMX_ErrorBadParameter;
    buff_hdr_extra_info* tempExtra = NULL;
    WFDMMLOGE("DeleteStreamingBuffers");

    if(!m_pStreamingBufQ)
    {
        return;
    }

    while(m_pStreamingBufQ->GetSize() != m_nStreamingBufCnt)
    {
        WFDMMLOGH2("Waiting for %ld input buffers to be returned returned= %ld",
                   m_nStreamingBufCnt, m_pStreamingBufQ->GetSize());
        ExecuteRenderFrame(/*bEOS =*/true);
        MM_Timer_Sleep(2);
    }


    for(int i = 0; i < m_nStreamingBufCnt; i++)
    {
        result = m_pStreamingBufQ->Pop(&pBuffHdr, sizeof(pBuffHdr), 100);
        WFDMMLOGE1("Buffer Popped = %p", pBuffHdr);
        if(result == OMX_ErrorNone)
        {
            if(pBuffHdr->pBuffer != NULL)
            {
                MM_Free(pBuffHdr->pBuffer);
            }
            pBuffHdr->pBuffer = NULL;
            pBuffHdr->nAllocLen = 0;
            pBuffHdr->nFilledLen = 0;

            buff_hdr_extra_info* tempExtra = static_cast<buff_hdr_extra_info*>
                                            (pBuffHdr->pAppPrivate);
            if(tempExtra)
            {
                memset(tempExtra, 0, sizeof(tempExtra));
            }

            m_pStreamingBufQ->Push(&pBuffHdr, sizeof(pBuffHdr));
        }
        else
        {
            WFDMMLOGE("Failed to pop pBuffHdr in deleteStreamingBuffers \n");
        }
    }
}

/*==============================================================================

         FUNCTION:          DeleteAppSupported

         DESCRIPTION:       Deletes m_pAppSupported
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
@par     SIFE EFFECTS:
                            None
*//*==========================================================================*/
void VideoSource::DeleteAppSupported()
{
    WFDMMLOGE("DeleteAppSupported");

    if(m_pAppSupported)
    {
        for(int i = 0; i < m_nCountAppSupported; i++)
        {
            if(m_pAppSupported[i] != NULL)
            {
                MM_Free(m_pAppSupported[i]);
                m_pAppSupported[i] = NULL;
            }
        }
        MM_Free(m_pAppSupported);
        m_pAppSupported = NULL;
        m_nCountAppSupported = 0;
    }
}

/*==============================================================================

         FUNCTION:          EncryptData

         DESCRIPTION:       Encrypt Incoming data in ion fd or buffer
*//**       @brief
*//**
@par     DEPENDENCIES:
                            None
*//*
         PARAMETERS:
*//**       @param          None

*//*     RETURN VALUE:
*//**       @return
                            OMX_ERRORTYPE
@par     SIFE EFFECTS:
                            None
*//*==========================================================================*/
OMX_ERRORTYPE VideoSource::EncryptData
(
    OMX_BUFFERHEADERTYPE *pEncBufferHdr,
    int                   fd,
    char                 *pData,
    unsigned long         nLen,
    bool                  bSecure
)
{
    OMX_ERRORTYPE result = OMX_ErrorBadParameter;
    WFDMMLOGM3("Received data to encrypt fd =%ld,Len = %ld, bSecure = %d",
               fd, nLen,bSecure);

    if(pData == NULL && fd < 0)
    {
        return OMX_ErrorBadParameter;
    }

    OMX_BUFFERHEADERTYPE* pBuffHdr = NULL;

    if(fd < 0 || nLen == 0 || (pEncBufferHdr == NULL))
    {
        WFDMMLOGE("Invalid argument to EncryptData");
        return result;
    }
    m_pHDCPOutputBufQ->Pop(&pBuffHdr, sizeof(pBuffHdr), 100);

    if(!pBuffHdr)
    {
        WFDMMLOGE("Failed to POP from HDCP BufQ");
        return result;
    }


    if(m_pHdcpOutBuffers[0].length < nLen)
    {
        WFDMMLOGE("HDCP Input Size exceeded buffer size");
        return OMX_ErrorInsufficientResources;
    }

    /*--------------------------------------------------------------------------
    1. When only WFD is secure and Data is non secure encryption is in-place
    2. When WFD and Content is secure need to pass both fd's to encrption
    ----------------------------------------------------------------------------
    */

    buffer* ion_buffer_out =(reinterpret_cast<buffer*>(pBuffHdr->pPlatformPrivate));


    uint8 ucPESPvtData[PES_PVT_DATA_LEN] = { 0 };
    if(!bSecure)
    {
        /*----------------------------------------------------------------------
          1. copy the content to Secure buffer
          2. pass the input to output to same buffer to encoder

          Note that secure always will be an fd
        ------------------------------------------------------------------------
        */
        char* pVirtaddr = fd >=0 ? GetVirtualAddressForIonFd(false,nLen,fd)
                                   : pData;
        if(pVirtaddr == NULL)
        {
            WFDMMLOGE2("pVirtaddr is NULL in Sec for Fd = %d,Len=%d,\n",
                       fd, nLen);
            m_pHDCPOutputBufQ->Push(&pBuffHdr, sizeof(pBuffHdr));
            return result;
        }

        fd = ion_buffer_out->fd;
        WFDMMLOGE3("Encryption buffer Non-Secure I/P= %p VirtAddr= %p Len= %d",
                   ion_buffer_out->start, pVirtaddr, nLen);
        memcpy(ion_buffer_out->start, pVirtaddr, nLen);
        if(fd >= 0)
        {
            MMIonMemoryFree(-1, nLen, (char*)pVirtaddr);
        }
    }


    /*--------------------------------------------------------------------------
     Reset in case encryption fails, to avoid spurious stats
    ----------------------------------------------------------------------------
    */

    WFDMMLOGE2("WFD_HdcpDataEncrypt inputFd = %d, output fd= %d",
               fd, ion_buffer_out->fd);

    unsigned ulStatus = m_pHdcpHandle->WFD_HdcpDataEncrypt(
                        STREAM_VIDEO,
                        (unsigned char*)ucPESPvtData,
                        (unsigned char*)(uint64)(fd),
                        (unsigned char*)(uint64)(ion_buffer_out->fd),nLen);
    if(ulStatus != 0)
    {
        WFDMMLOGE1("Error in HDCP Encryption! %x", ulStatus);
        /*----------------------------------------------------------------------
             Release back buffer to encoder, and push back buffer
             Header to Q and report a runtime error
        ------------------------------------------------------------------------
        */
        m_pHDCPOutputBufQ->Push(&pBuffHdr, sizeof(pBuffHdr));
        if(false == m_pHdcpHandle->proceedWithHDCP())
        {
            WFDMMLOGE("Cipher enablement wait timed out");
            if(m_pEventHandlerFn)
            {
                m_pEventHandlerFn(m_appData, m_nModuleId, WFDMMSRC_ERROR,
                                  OMX_ErrorUndefined, 0);
            }
        }
        else
        {
            /*------------------------------------------------------------------
              In case a frame is dropped request an IDR from encoder to ensure
              that the sink always does receive an IDR, else we might end up in
              a scenario where an IDR frame is dropped due to CIPHER not being
              enabled and then once it's enabled, we end up sending only
              Pframes until the sink explicitly requests for an IDR
              (not guaranteed) or the IDR interval expires.
            --------------------------------------------------------------------
            */
            m_bDropVideoFrame = OMX_TRUE;

            // send message to streamer to start Mirroring
            //STREAMING_CLOSE_REQ
        }

        return result;
    }
    else
    {
        memcpy(pEncBufferHdr->pBuffer, ion_buffer_out->start, nLen);
        pEncBufferHdr->nFilledLen = nLen;
        WFDMMLOGM2("Encryption Success pEncBufferHdr= %p,pBuffer =%p",
                   pEncBufferHdr, pEncBufferHdr->pBuffer);
        WFDMMLOGM2("Encryption Success ion_buffer_out->start= %p,nLen =%ld",
                   ion_buffer_out->start, nLen);
    }

    for(int idx = 0; idx < PES_PVT_DATA_LEN; idx++)
    {
        WFDMMLOGM3("Encrypt PayloadLen[%lu] PES_PVTData[%d]:%x",
                   nLen,idx,ucPESPvtData[idx]);
    }

    /*--------------------------------------------------------------------------
             Fill PESPvtData at end of the encrypted buffer, as an extra data
    ----------------------------------------------------------------------------
    */

    FillHdcpCpExtraData(pEncBufferHdr, ucPESPvtData, 1);
    m_pHDCPOutputBufQ->Push(&pBuffHdr, sizeof(pBuffHdr));
    WFDMMLOGH1("Encryption Success m_pHDCPOutputBufQ Push pBuffHdr = %p",
               pBuffHdr);
    return OMX_ErrorNone;
}

OMX_ERRORTYPE VideoSource::ExecuteStopStreamingSuccess()
{
    OMX_ERRORTYPE result = OMX_ErrorNone;
    WFDMMLOGH("ExecuteStopStreamingSuccess");

    if(m_pEventHandlerFn)
    {
        if(mbVideoSourceStopped)
        {
            WFDMMLOGE(" do not take action Received STOP after WFD is closed");
            result = OMX_ErrorIncorrectStateOperation;
        }
        else
        {
            m_pEventHandlerFn(m_appData, m_nModuleId,
                              WFDMMSRC_STREAMING_STOP_SUCCESS,
                              OMX_ErrorNone, 0);
            WFDMMLOGH("VIDEO_SOURCE_SET_STOP_STREAMING_SUCCESS \n");
        }
    }
    else
    {
        WFDMMLOGE("Error ExecuteStopStreamingSuccess m_pEventHandlerFn NULL");
        result = OMX_ErrorBadParameter;
    }
    return result;
}
OMX_ERRORTYPE VideoSource::ExecuteFlush()
{
    WFDDEBUGTRACE;

    /*--------------------------------------------------------------------------
                flush input frames received
                reset base time flag
    ----------------------------------------------------------------------------
    */
    FlushStreamingQueue();

    m_nRenderTSReadIdx = 0;
    m_nRenderTSWriteIdx = 0;
    m_bBaseStreamTimeTaken = false;
    m_nDSTotalTimeDrift = 0;
    m_nDSNumSamplesAvgDrift = 0;
    m_nPrevAdjTimeStamp = 0;
    m_nBaseStreamTime = (int64)-1;
    m_nPrevPushedBufsOnRender = 0;

    return OMX_ErrorNone;
}

void VideoSource::AbortStreaming()
{
    WFDDEBUGTRACE
    WFDMMLOGE("Direct Streaming Aborted");

    if(m_StreamingClosingReqSent)
    {
        WFDMMLOGH("Already Closing. Ignore");
        return;
    }

    m_pEventHandlerFn(m_appData, m_nModuleId,
                      WFDMMSRC_STREAMING_VIDEO_STREAM_FLUSH,
                      OMX_ErrorNone, 0);
    sendcallbackToBinder(
                    (StreamingEventType)STREAMING_CLOSE_REQ,
                    (void*)NULL);

    FlushStreamingQueue();
}

