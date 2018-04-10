/*==============================================================================
*        @file MMCapability.cpp
*
*  @par DESCRIPTION:
*        Thread class.
*
*
*Copyright (c) 2011-2016 Qualcomm Technologies, Inc.
*All Rights Reserved.
*Confidential and Proprietary - Qualcomm Technologies, Inc.
==============================================================================*/


#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_NDEBUG  0
#define LOG_NDDEBUG 0


/* EDID related Macro Definition*/
#define EDID_PAYLOAD_LENGTH 32768
#define MAX_EDID_BLOCK_SIZE  128 //As binary data is packed in 4 bits
#define MAX_EDID_BLOCK_COUNT_SIZE  6 // includes space

#include <utils/Log.h>
#ifndef WFD_ICS
#include "common_log.h"
#endif
#include "SessionManager.h"
#include "MMCapability.h"
#include <string.h>
#include "rtsp_wfd.h"
#include "wdsm_mm_interface.h"
#include <fcntl.h>
#include "qd_utils.h"
#include <math.h>

using namespace qdutils;

MMCapability::MMCapability() {
    MM_MSG_PRIO(MM_GENERAL,MM_PRIO_HIGH,"MMCapability:Constructor");
    pCapability = (WFD_MM_capability_t*)malloc(sizeof(WFD_MM_capability_t));
    if(pCapability !=NULL)
    {
        memset(pCapability, 0, sizeof(WFD_MM_capability_t));
        pCapability->video_config.video_config.h264_codec = (WFD_h264_codec_config_t*)MM_Malloc
                                                     (sizeof(WFD_h264_codec_config_t)*WFD_MAX_NUM_H264_PROFILES);
       if(pCapability->video_config.video_config.h264_codec !=NULL)
       {
          memset(pCapability->video_config.video_config.h264_codec, 0,
                                                       sizeof(WFD_h264_codec_config_t)*WFD_MAX_NUM_H264_PROFILES);
       }

       pCapability->edid.pEdidData = NULL;
       pCapability->edid.pEdidData = (uint8*)MM_Malloc(EDID_PAYLOAD_LENGTH);
       if(pCapability->edid.pEdidData)
       {
         memset(pCapability->edid.pEdidData, 0, EDID_PAYLOAD_LENGTH);
       }
       pCapability->transport_capability_config.eBeforeDSPortType = RTP_PORT_UDP;
       /* Set default values for link properties */
       pCapability->link_properties.nss = 2;
       pCapability->link_properties.rateflags = 0;
       pCapability->link_properties.freq = 5100;
       pCapability->link_properties.enable_4k = 1; //4K Enabled
    }
    pUibcCapability = (WFD_uibc_capability_t*)MM_Malloc(sizeof(WFD_uibc_capability_t));
    if(pUibcCapability !=NULL)
    {
        memset(pUibcCapability, 0, sizeof(WFD_uibc_capability_t));
    }
    m_bEdidValidity = false;
    //pExtendedCapability = (WFD_MM_Extended_capability_t*)(malloc(sizeof(WFD_MM_Extended_capability_t)*WFD_MAX_NUM_Extended_Codec));
    bool exValid = true;
    pExtendedCapability = (WFD_MM_Extended_capability_t *)MM_Malloc(sizeof(WFD_MM_Extended_capability_t));
    if(pExtendedCapability == NULL)
    {
        MM_MSG_PRIO(MM_GENERAL,MM_PRIO_HIGH,"MMCapability_CPP: wfd_video_formats_extension , memory alloc failed");
        exValid = false;
    }
    if(exValid)
    {
        memset(pExtendedCapability,0,sizeof(WFD_MM_Extended_capability_t));

        WFD_extended_video_codec_config_t *tempExVideoCfg =
           &(pExtendedCapability->extended_video_config.extended_video_config);

        if(tempExVideoCfg->codec != NULL)
        {
            MM_Free(tempExVideoCfg->codec);
            tempExVideoCfg->codec = NULL;
        }
        tempExVideoCfg->codec =
            (WFD_extended_codec_config_t *)MM_Malloc(WFD_MAX_NUM_Extended_Codec * sizeof(WFD_extended_codec_config_t));

        if(tempExVideoCfg->codec == NULL)
        {
            MM_MSG_PRIO(MM_GENERAL,MM_PRIO_HIGH,
                "MMCapability_CPP:WFD_extended_codec_config,memory alloc failed");
            exValid = false;
        }
        if(exValid)
        {
            memset(tempExVideoCfg->codec, 0,
               sizeof(WFD_extended_codec_config_t)*WFD_MAX_NUM_Extended_Codec);
            tempExVideoCfg->num_codec = 0;
        }
    }
    exValid = true;

    plocalExCapability = (WFD_MM_Extended_capability_t*)MM_Malloc(sizeof(WFD_MM_Extended_capability_t));
    if(plocalExCapability == NULL)
    {
        MM_MSG_PRIO(MM_GENERAL,MM_PRIO_HIGH,"MMCapability: localExCapability creation failed");
        exValid = false;
    }
    if(exValid)
    {
        memset(plocalExCapability, 0, sizeof(WFD_MM_Extended_capability_t));
        WFD_extended_video_codec_config_t *tempExVideoCfg =
           &(plocalExCapability->extended_video_config.extended_video_config);

        if(tempExVideoCfg->codec != NULL)
        {
            MM_Free(tempExVideoCfg->codec);
            tempExVideoCfg->codec = NULL;
        }
        tempExVideoCfg->codec =
            (WFD_extended_codec_config_t *)MM_Malloc(sizeof(WFD_extended_codec_config_t));
        if(tempExVideoCfg->codec == NULL)
        {
            MM_MSG_PRIO(MM_GENERAL,MM_PRIO_HIGH,
                "MMCapability_CPP:Local WFD_extended_codec_config,memory alloc failed");
            exValid = false;
        }
        if(exValid)
        {
            memset(tempExVideoCfg->codec, 0, sizeof(WFD_extended_codec_config_t));
            tempExVideoCfg->num_codec = 0;
        }
    }

    pAuxCapability = NULL;
    pAuxCapability = (WFD_MM_Aux_capability_t*)MM_Malloc(sizeof(WFD_MM_Aux_capability_t));
    if(pAuxCapability == NULL)
    {
        MM_MSG_PRIO(MM_GENERAL,MM_PRIO_HIGH,"MMCapability: Aux Stream capability creation failed");
    }
    else
    {
        memset(pAuxCapability, 0, sizeof(WFD_MM_Aux_capability_t));
        pAuxCapability->count = 0;
    }
}

MMCapability::~MMCapability() {
    MM_MSG_PRIO(MM_GENERAL,MM_PRIO_HIGH,"MMCapability:Destructor");
    if(pCapability)
    {
      if(pCapability->video_config.video_config.h264_codec)
      {
        MM_Free(pCapability->video_config.video_config.h264_codec);
      }
      //note:This handle is being freed in mm
      pCapability->HdcpCtx.hHDCPSession = NULL;
      if (pCapability->edid.pEdidData)
      {
        MM_Free(pCapability->edid.pEdidData);
      }
      MM_Free(pCapability);
    }

    if(pExtendedCapability)
    {
        if(pExtendedCapability->extended_video_config.extended_video_config.codec)
        {
            MM_Free(pExtendedCapability->extended_video_config.extended_video_config.codec);
        }
        MM_Free(pExtendedCapability);
    }

    if(plocalExCapability)
    {
        if(plocalExCapability->extended_video_config.extended_video_config.codec)
        {
            MM_Free(plocalExCapability->extended_video_config.extended_video_config.codec);
        }
        MM_Free(plocalExCapability);
    }

    if(pAuxCapability)
    {
        MM_Free(pAuxCapability);
    }

    if(pUibcCapability)
    {
      MM_Free(pUibcCapability);
    }
    m_bEdidValidity = false;
}

void MMCapability::getResolutionFromBitmap(WFD_h264_codec_config_t* pCapability_h264_codec, uint32 numH264)
{
    uint32 cea_mode = pCapability_h264_codec[numH264].supported_cea_mode;
    uint64 vesa_mode = pCapability_h264_codec[numH264].supported_vesa_mode;
    uint32 hh_mode = pCapability_h264_codec[numH264].supported_hh_mode;
    MM_MSG_PRIO3(MM_GENERAL,MM_PRIO_HIGH,"MMCapability::getResolutionRefreshRate %u %u %u",cea_mode, vesa_mode, hh_mode);
    uint32 NegotiatedWidth, NegotiatedHeight;
    if(cea_mode)
    {
        switch (cea_mode)
        {
        case 1:
            NegotiatedWidth = 640;
            NegotiatedHeight= 480;
            break;
        case 2:
            NegotiatedWidth = 720;
            NegotiatedHeight = 480;
            break;
        case 4:
            //480i
            NegotiatedWidth = 720;
            NegotiatedHeight = 480;
            break;
        case 8:
            NegotiatedWidth = 720;
            NegotiatedHeight = 576;
            break;
        case 16:
            //576i
            NegotiatedWidth = 720;
            NegotiatedHeight = 576;
            break;
        case 32:
            NegotiatedWidth = 1280;
            NegotiatedHeight = 720;
            break;
        case 64:
            NegotiatedWidth = 1280;
            NegotiatedHeight = 720;
            break;
        case 128:
            NegotiatedWidth = 1920;
            NegotiatedHeight = 1080;
            break;
        case 256:
            NegotiatedWidth = 1920;
            NegotiatedHeight = 1080;
            break;
        case 512:
            //1080i 60
            NegotiatedWidth = 1920;
            NegotiatedHeight = 1080;
            break;
        case 1024:
            NegotiatedWidth = 1280;
            NegotiatedHeight = 720;
            break;
        case 2048:
            NegotiatedWidth = 1280;
            NegotiatedHeight = 720;
            break;
        case 4096:
            NegotiatedWidth = 1920;
            NegotiatedHeight = 1080;
            break;
        case 8192:
            NegotiatedWidth = 1920;
            NegotiatedHeight = 1080;
            break;
        case 16384:
            NegotiatedWidth = 1920;
            NegotiatedHeight = 1080;
            break;
        case 32768:
            NegotiatedWidth = 1280;
            NegotiatedHeight = 720;
            break;
        case 65536:
            NegotiatedWidth = 1920;
            NegotiatedHeight = 1080;
            break;
#if 0 // Currently 4K is not enabled for Video Formats
        case   131072: // BIT 17
        case   262144: // BIT 18
        case  2097152: // BIT 21
        case  4194304: // BIT 22
        case 33554432: // BIT 25
            NegotiatedWidth = 3840;
            NegotiatedHeight = 2160;
            break;
        case   524288: // BIT 19
        case  1048576: // BIT 20
        case  8388608: // BIT 23
        case 16777216: // BIT 24
        case 67108864: // BIT 26
            NegotiatedWidth = 4096;
            NegotiatedHeight = 2160;
            break;
#endif
            /** Test cases proprietary for debug purpose */
        case 100:
            NegotiatedWidth = 800;
            NegotiatedHeight = 480;
            break;
        case 200:
            NegotiatedWidth = 800;
            NegotiatedHeight = 480;
            break;
        case 300:
            NegotiatedWidth = 800;
            NegotiatedHeight = 480;
            break;
        case 400:
            NegotiatedWidth = 800;
            NegotiatedHeight = 480;
            break;
        case 500:
            NegotiatedWidth = 800;
            NegotiatedHeight = 480;
            break;

        default:
            NegotiatedWidth = 800;
            NegotiatedHeight = 480;
            break;
        }
    }

    else if(hh_mode)
    {
        switch (hh_mode)
        {
        case 1:
            NegotiatedWidth = 800;
            NegotiatedHeight = 480;
            break;
        case 2:
            NegotiatedWidth = 800;
            NegotiatedHeight = 480;
            break;
        case 4:
            NegotiatedWidth = 854;
            NegotiatedHeight = 480;
            break;
        case 8:
            NegotiatedWidth = 854;
            NegotiatedHeight = 480;
            break;
        case 16:
            NegotiatedWidth = 864;
            NegotiatedHeight = 480;
            break;
        case 32:
            NegotiatedWidth = 864;
            NegotiatedHeight = 480;
            break;
        case 64:
            NegotiatedWidth = 640;
            NegotiatedHeight = 360;
            break;
        case 128:
            NegotiatedWidth = 640;
            NegotiatedHeight = 360;
            break;
        case 256:
            NegotiatedWidth = 960;
            NegotiatedHeight = 540;
            break;
        case 512:
            NegotiatedWidth = 960;
            NegotiatedHeight = 540;
            break;
        case 1024:
            NegotiatedWidth = 848;
            NegotiatedHeight = 480;
            break;
        case 2048:
            NegotiatedWidth = 848;
            NegotiatedHeight = 480;
            break;

        default:
            NegotiatedWidth = 800;
            NegotiatedHeight = 480;
            break;
        }
    }

    else if(vesa_mode)
    {
        switch (vesa_mode)
        {
        case 1:
            NegotiatedWidth = 800;
            NegotiatedHeight = 600;
            break;
        case 2:
            NegotiatedWidth = 800;
            NegotiatedHeight = 600;
            break;
        case 4:
            NegotiatedWidth = 1024;
            NegotiatedHeight = 768;
            break;
        case 8:
            NegotiatedWidth = 1024;
            NegotiatedHeight = 768;
            break;
        case 16:
            NegotiatedWidth = 1152;
            NegotiatedHeight = 864;
            break;
        case 32:
            NegotiatedWidth = 1152;
            NegotiatedHeight = 864;
            break;
        case 64:
            NegotiatedWidth = 1280;
            NegotiatedHeight = 768;
            break;
        case 128:
            NegotiatedWidth = 1280;
            NegotiatedHeight = 768;
            break;
        case 256:
            NegotiatedWidth = 1280;
            NegotiatedHeight = 800;
            break;

        case 512:
            NegotiatedWidth = 1280;
            NegotiatedHeight = 800;
            break;
        case 1024:
            NegotiatedWidth = 1360;
            NegotiatedHeight = 768;
            break;
        case 2048:
            NegotiatedWidth = 1360;
            NegotiatedHeight = 768;
            break;
        case 4096:
            NegotiatedWidth = 1366;
            NegotiatedHeight = 768;
            break;
        case 8192:
            NegotiatedWidth = 1366;
            NegotiatedHeight = 768;
            break;
        case 16384:
            NegotiatedWidth = 1280;
            NegotiatedHeight = 1024;
            break;
        case 32768:
            NegotiatedWidth = 1280;
            NegotiatedHeight = 1024;
            break;
        case 65536:
            NegotiatedWidth = 1400;
            NegotiatedHeight = 1050;
            break;

        case 131072:
            NegotiatedWidth = 1400;
            NegotiatedHeight = 1050;
            break;
        case 262144:
            NegotiatedWidth = 1440;
            NegotiatedHeight = 900;
            break;

        case 524288:
            NegotiatedWidth = 1440;
            NegotiatedHeight = 900;
            break;
        case 1048576:
            NegotiatedWidth = 1600;
            NegotiatedHeight = 900;
            break;
        case 2097152:
            NegotiatedWidth = 1600;
            NegotiatedHeight = 900;
            break;
        case 4194304:
            NegotiatedWidth = 1600;
            NegotiatedHeight = 1200;
            break;
        case 8388608:
            NegotiatedWidth = 1600;
            NegotiatedHeight = 1200;
            break;
        case 16777216:
            NegotiatedWidth = 1680;
            NegotiatedHeight = 1024;
            break;
        case 33554432:
            NegotiatedWidth = 1680;
            NegotiatedHeight = 1024;
            break;
        case 67108864:
            NegotiatedWidth = 1680;
            NegotiatedHeight = 1050;
            break;
        case 134217728:
            NegotiatedWidth = 1680;
            NegotiatedHeight = 1050;
            break;
        case 268435456:
            NegotiatedWidth = 1920;
            NegotiatedHeight = 1200;
            break;
        case 536870912:
            NegotiatedWidth = 1920;
            NegotiatedHeight = 1200;
            break;
#if 0 // Currently 4K is not enabled for Video Formats
        case 1073741824: // BIT 30
        case 2147483648: // BIT 31
            NegotiatedWidth = 2560;
            NegotiatedHeight = 1600;
            break;
        case 4294967296: // BIT 32
        case 8589934592: // BIT 33
            NegotiatedWidth = 4096;
            NegotiatedHeight = 2160;
            break;
#endif
        default :
            NegotiatedWidth = 800;
            NegotiatedHeight = 600;
            break;
        }
    }
    else
    {
        MM_MSG_PRIO(MM_GENERAL,MM_PRIO_HIGH,"MMCapability::No Modes are available setting  default values");
        NegotiatedWidth = 640;
        NegotiatedHeight = 480;
    }
    pCapability_h264_codec[numH264].max_hres =NegotiatedWidth;
    pCapability_h264_codec[numH264].max_vres =NegotiatedHeight;

}

void MMCapability::getResolutionFromBitmap
(
    WFD_extended_codec_config_t* pCapability_codec,
    uint32 numCodec
)
{
    uint32 cea_mode = pCapability_codec[numCodec].supported_cea_mode;
    uint64 vesa_mode = pCapability_codec[numCodec].supported_vesa_mode;
    uint32 hh_mode = pCapability_codec[numCodec].supported_hh_mode;

    MM_MSG_PRIO3(MM_GENERAL,MM_PRIO_HIGH,
        "MMCapability::getResolutionFromBitmap Extended %u %u %u",
        cea_mode,vesa_mode,hh_mode);

    uint32 NegotiatedWidth, NegotiatedHeight;

    if(cea_mode)
    {
        uint16 MaxHeight =
                           pCapability_codec[numCodec].max_mode_list[WFD_VIDEO_CEA_MODE].height;
        uint16 MaxWidth =
                          pCapability_codec[numCodec].max_mode_list[WFD_VIDEO_CEA_MODE].width;

        for(int i = 0;i < MAX_MODE_LIST; i++)
        {
            uint32 index = pow(2,i);
            uint32 tempCea = (cea_mode & index);
            switch (tempCea)
            {
            case 0:
                NegotiatedWidth = 0;
                NegotiatedHeight = 0;
                break;
            case 1:
                NegotiatedWidth = 640;
                NegotiatedHeight= 480;
                break;
            case 2:
                NegotiatedWidth = 720;
                NegotiatedHeight = 480;
                break;
            case 4:
                //480i
                NegotiatedWidth = 720;
                NegotiatedHeight = 480;
                break;
            case 8:
                NegotiatedWidth = 720;
                NegotiatedHeight = 576;
                break;
            case 16:
                //576i
                NegotiatedWidth = 720;
                NegotiatedHeight = 576;
                break;
            case 32:
                NegotiatedWidth = 1280;
                NegotiatedHeight = 720;
                break;
            case 64:
                NegotiatedWidth = 1280;
                NegotiatedHeight = 720;
                break;
            case 128:
                NegotiatedWidth = 1920;
                NegotiatedHeight = 1080;
                break;
            case 256:
                NegotiatedWidth = 1920;
                NegotiatedHeight = 1080;
                break;
            case 512:
                //1080i 60
                NegotiatedWidth = 1920;
                NegotiatedHeight = 1080;
                break;
            case 1024:
                NegotiatedWidth = 1280;
                NegotiatedHeight = 720;
                break;
            case 2048:
                NegotiatedWidth = 1280;
                NegotiatedHeight = 720;
                break;
            case 4096:
                NegotiatedWidth = 1920;
                NegotiatedHeight = 1080;
                break;
            case 8192:
                NegotiatedWidth = 1920;
                NegotiatedHeight = 1080;
                break;
            case 16384:
                NegotiatedWidth = 1920;
                NegotiatedHeight = 1080;
                break;
            case 32768:
                NegotiatedWidth = 1280;
                NegotiatedHeight = 720;
                break;
            case 65536:
                NegotiatedWidth = 1920;
                NegotiatedHeight = 1080;
                break;
            case   131072: // BIT 17
            case   262144: // BIT 18
            case  2097152: // BIT 21
            case  4194304: // BIT 22
            case 33554432: // BIT 25
                NegotiatedWidth = 3840;
                NegotiatedHeight = 2160;
                break;
            case   524288: // BIT 19
            case  1048576: // BIT 20
            case  8388608: // BIT 23
            case 16777216: // BIT 24
            case 67108864: // BIT 26
                NegotiatedWidth = 4096;
                NegotiatedHeight = 2160;
                break;
            default:
                NegotiatedWidth = 800;
                NegotiatedHeight = 480;
                break;
            }
            pCapability_codec[numCodec].cea_mode_list[i].width = NegotiatedWidth;
            pCapability_codec[numCodec].cea_mode_list[i].height = NegotiatedHeight;
            if(MaxHeight < NegotiatedHeight || MaxWidth < NegotiatedWidth)
            {
                MaxHeight = NegotiatedHeight;
                MaxWidth = NegotiatedWidth;
            }
        }
        pCapability_codec[numCodec].max_mode_list[WFD_VIDEO_CEA_MODE].height = MaxHeight;
        pCapability_codec[numCodec].max_mode_list[WFD_VIDEO_CEA_MODE].width = MaxWidth;

        MM_MSG_PRIO2(MM_GENERAL, MM_PRIO_HIGH,
                     "MMCapability::getResolutionFromBitmap CEA Maximum H= %u W= %u ",
                     MaxHeight, MaxWidth);
    }

    if(vesa_mode)
    {
        uint16 MaxHeight =
                           pCapability_codec[numCodec].max_mode_list[WFD_VIDEO_VESA_MODE].height;
        uint16 MaxWidth =
                          pCapability_codec[numCodec].max_mode_list[WFD_VIDEO_VESA_MODE].width;

        for(int i = 0;i < MAX_MODE_LIST; i++)
        {
            uint32 index = pow(2,i);
            uint64 tempVesa = (vesa_mode & index);
            switch (tempVesa)
            {
            case 0:
                NegotiatedWidth = 0;
                NegotiatedHeight = 0;
                break;
            case 1:
                NegotiatedWidth = 800;
                NegotiatedHeight = 600;
                break;
            case 2:
                NegotiatedWidth = 800;
                NegotiatedHeight = 600;
                break;
            case 4:
                NegotiatedWidth = 1024;
                NegotiatedHeight = 768;
                break;
            case 8:
                NegotiatedWidth = 1024;
                NegotiatedHeight = 768;
                break;
            case 16:
                NegotiatedWidth = 1152;
                NegotiatedHeight = 864;
                break;
            case 32:
                NegotiatedWidth = 1152;
                NegotiatedHeight = 864;
                break;
            case 64:
                NegotiatedWidth = 1280;
                NegotiatedHeight = 768;
                break;
            case 128:
                NegotiatedWidth = 1280;
                NegotiatedHeight = 768;
                break;
            case 256:
                NegotiatedWidth = 1280;
                NegotiatedHeight = 800;
                break;
            case 512:
                NegotiatedWidth = 1280;
                NegotiatedHeight = 800;
                break;
            case 1024:
                NegotiatedWidth = 1360;
                NegotiatedHeight = 768;
                break;
            case 2048:
                NegotiatedWidth = 1360;
                NegotiatedHeight = 768;
                break;
            case 4096:
                NegotiatedWidth = 1366;
                NegotiatedHeight = 768;
                break;
            case 8192:
                NegotiatedWidth = 1366;
                NegotiatedHeight = 768;
                break;
            case 16384:
                NegotiatedWidth = 1280;
                NegotiatedHeight = 1024;
                break;
            case 32768:
                NegotiatedWidth = 1280;
                NegotiatedHeight = 1024;
                break;
            case 65536:
                NegotiatedWidth = 1400;
                NegotiatedHeight = 1050;
                break;
            case 131072:
                NegotiatedWidth = 1400;
                NegotiatedHeight = 1050;
                break;
            case 262144:
                NegotiatedWidth = 1440;
                NegotiatedHeight = 900;
                break;
            case 524288:
                NegotiatedWidth = 1440;
                NegotiatedHeight = 900;
                break;
            case 1048576:
                NegotiatedWidth = 1600;
                NegotiatedHeight = 900;
                break;
            case 2097152:
                NegotiatedWidth = 1600;
                NegotiatedHeight = 900;
                break;
            case 4194304:
                NegotiatedWidth = 1600;
                NegotiatedHeight = 1200;
                break;
            case 8388608:
                NegotiatedWidth = 1600;
                NegotiatedHeight = 1200;
                break;
            case 16777216:
                NegotiatedWidth = 1680;
                NegotiatedHeight = 1024;
                break;
            case 33554432:
                NegotiatedWidth = 1680;
                NegotiatedHeight = 1024;
                break;
            case 67108864:
                NegotiatedWidth = 1680;
                NegotiatedHeight = 1050;
                break;
            case 134217728:
                NegotiatedWidth = 1680;
                NegotiatedHeight = 1050;
                break;
            case 268435456:
                NegotiatedWidth = 1920;
                NegotiatedHeight = 1200;
                break;
            case 536870912:
                NegotiatedWidth = 1920;
                NegotiatedHeight = 1200;
                break;
            case 1073741824: // BIT 30
            case 2147483648: // BIT 31
                NegotiatedWidth = 2560;
                NegotiatedHeight = 1600;
                break;
            case 4294967296: // BIT 32
            case 8589934592: // BIT 33
                NegotiatedWidth = 4096;
                NegotiatedHeight = 2160;
                break;
            default:
                NegotiatedWidth = 800;
                NegotiatedHeight = 600;
                break;
            }
            pCapability_codec[numCodec].vesa_mode_list[i].width = NegotiatedWidth;
            pCapability_codec[numCodec].vesa_mode_list[i].height = NegotiatedHeight;
            if(MaxHeight < NegotiatedHeight || MaxWidth < NegotiatedWidth)
            {
                MaxHeight = NegotiatedHeight;
                MaxWidth = NegotiatedWidth;
            }
        }
        pCapability_codec[numCodec].max_mode_list[WFD_VIDEO_VESA_MODE].height = MaxHeight;
        pCapability_codec[numCodec].max_mode_list[WFD_VIDEO_VESA_MODE].width = MaxWidth;
        MM_MSG_PRIO2(MM_GENERAL, MM_PRIO_HIGH,
                     "MMCapability::getResolutionFromBitmap VESA Maximum H= %u W= %u ",
                     MaxHeight, MaxWidth);
    }

    if(hh_mode)
    {
        uint16 MaxHeight =
                           pCapability_codec[numCodec].max_mode_list[WFD_VIDEO_HH_MODE].height;
        uint16 MaxWidth =
                          pCapability_codec[numCodec].max_mode_list[WFD_VIDEO_HH_MODE].width;

        for(int i = 0; i < MAX_MODE_LIST; i++)
        {
            uint32 index = pow(2, i);
            uint32 temphh = (hh_mode & index);
            switch (temphh)
            {
                case 0:
                    NegotiatedWidth = 0;
                    NegotiatedHeight = 0;
                    break;
                case 1:
                    NegotiatedWidth = 800;
                    NegotiatedHeight = 480;
                    break;
                case 2:
                    NegotiatedWidth = 800;
                    NegotiatedHeight = 480;
                    break;
                case 4:
                    NegotiatedWidth = 854;
                    NegotiatedHeight = 480;
                    break;
                case 8:
                    NegotiatedWidth = 854;
                    NegotiatedHeight = 480;
                    break;
                case 16:
                    NegotiatedWidth = 864;
                    NegotiatedHeight = 480;
                    break;
                case 32:
                    NegotiatedWidth = 864;
                    NegotiatedHeight = 480;
                    break;
                case 64:
                    NegotiatedWidth = 640;
                    NegotiatedHeight = 360;
                    break;
                case 128:
                    NegotiatedWidth = 640;
                    NegotiatedHeight = 360;
                    break;
                case 256:
                    NegotiatedWidth = 960;
                    NegotiatedHeight = 540;
                    break;
                case 512:
                    NegotiatedWidth = 960;
                    NegotiatedHeight = 540;
                    break;
                case 1024:
                    NegotiatedWidth = 848;
                    NegotiatedHeight = 480;
                    break;
                case 2048:
                    NegotiatedWidth = 848;
                    NegotiatedHeight = 480;
                    break;
                default:
                    NegotiatedWidth = 800;
                    NegotiatedHeight = 480;
                    break;
            }
            pCapability_codec[numCodec].hh_mode_list[i].width = NegotiatedWidth;
            pCapability_codec[numCodec].hh_mode_list[i].height = NegotiatedHeight;
            if(MaxHeight < NegotiatedHeight || MaxWidth < NegotiatedWidth)
            {
                MaxHeight = NegotiatedHeight;
                MaxWidth = NegotiatedWidth;
            }
        }
        pCapability_codec[numCodec].max_mode_list[WFD_VIDEO_HH_MODE].height =
            MaxHeight;
        pCapability_codec[numCodec].max_mode_list[WFD_VIDEO_HH_MODE].width =
            MaxWidth;
        MM_MSG_PRIO2(MM_GENERAL, MM_PRIO_HIGH,
                     "MMCapability::getResolutionFromBitmap HH Maximum H= %u W= %u ",
                     MaxHeight, MaxWidth);
    }
}

char* MMCapability::getKeyValue(char* pKey) {

    memset(strABNF, 0, sizeof(strABNF));

    if (strncmp(pKey, "wfd_audio_codecs", sizeof("wfd_audio_codecs")) == 0) {
        char audioStr[256];

        if (pCapability->audio_config.lpcm_codec.supported_modes_bitmap != 0) {
            snprintf(audioStr,256, "LPCM %.8x %.2x",
                     pCapability->audio_config.lpcm_codec.supported_modes_bitmap,
                     pCapability->audio_config.lpcm_codec.decoder_latency);
            strlcat(strABNF, audioStr, sizeof(strABNF));
        }
        if (pCapability->audio_config.aac_codec.supported_modes_bitmap != 0) {
            if (strlen(strABNF) != 0) {
                strlcat(strABNF, ", ", sizeof(strABNF));
            }
            snprintf(audioStr, 256, "AAC %.8x %.2x",
                     pCapability->audio_config.aac_codec.supported_modes_bitmap,
                     pCapability->audio_config.aac_codec.decoder_latency);
            strlcat(strABNF, audioStr, sizeof(strABNF));
        }
        if (pCapability->audio_config.dolby_digital_codec.supported_modes_bitmap != 0) {
            if (strlen(strABNF) != 0) {
                strlcat(strABNF, ", ", sizeof(strABNF));
            }
            snprintf(audioStr,256, "AC3 %.8x %.2x",
                     pCapability->audio_config.dolby_digital_codec.supported_modes_bitmap,
                     pCapability->audio_config.dolby_digital_codec.decoder_latency);
            strlcat(strABNF, audioStr, sizeof(strABNF));
        }
        return strABNF;
    }

    if (strncmp(pKey, "wfd_video_formats", sizeof("wfd_video_formats")) == 0) {
        char videoStr[256];
        WFD_h264_codec_config_t* pH264Config;

        snprintf(strABNF,sizeof(strABNF), "%.2x %.2x ",
                 pCapability->video_config.video_config.native_bitmap,
                 pCapability->video_config.video_config.preferred_display_mode_supported);

        for (int i=0; i<pCapability->video_config.video_config.num_h264_profiles; i++) {
            if (i != 0) {
                strlcat(strABNF, ", ", sizeof(strABNF));
            }

            pH264Config = &(pCapability->video_config.video_config.h264_codec[i]);
            snprintf(videoStr, 256, "%.2x %.2x %.8x %.8x %.8x %.2x %.4x %.4x %.2x ",
                    pH264Config->h264_profile,
                    pH264Config->h264_level,
                    pH264Config->supported_cea_mode,
                    pH264Config->supported_vesa_mode,
                    pH264Config->supported_hh_mode,
                    pH264Config->decoder_latency,
                    pH264Config->min_slice_size,
                    pH264Config->slice_enc_params,
                    pH264Config->frame_rate_control_support);
            if (pCapability->video_config.video_config.preferred_display_mode_supported == 0) {
                strlcat(videoStr, "none none", 256);
            } else {
                char buffer[20];
                snprintf(buffer,20, "%.4x %.4x", pH264Config->max_hres, pH264Config->max_vres);
                strlcat(videoStr, buffer, 256);
            }
            strlcat(strABNF, videoStr, sizeof(strABNF));
        }

        return strABNF;
    }

    if ((strncmp(pKey, "wfd_video_formats_extension", sizeof("wfd_video_formats_extension")) == 0) &&
        (pExtendedCapability != NULL))
    {
        char videoFormatExtStr[1024];

        WFD_extended_video_codec_config_t *tempExVideoCfg =
           &(pExtendedCapability->extended_video_config.extended_video_config);
        if(tempExVideoCfg == NULL)
        {
            MM_MSG_PRIO(MM_GENERAL,MM_PRIO_ERROR,"getKeyValue: \
                    video_formats_extension : extended_video_config NULL");
            return strABNF;
        }
        MM_MSG_PRIO1(MM_GENERAL,MM_PRIO_ERROR,
                     "getKeyValue : wfd_video_formats_extension : count %d",
                     tempExVideoCfg->num_codec);
        if(tempExVideoCfg->num_codec > 0)
        {
            WFD_extended_codec_config_t* pExtCodecConfig;

            snprintf(strABNF,sizeof(strABNF), "%.2x %.2x ",
                    tempExVideoCfg->native_bitmap,
                    tempExVideoCfg->preferred_display_mode_supported);

            for (int i=0; i < tempExVideoCfg->num_codec; i++)
            {
                pExtCodecConfig = &(tempExVideoCfg->codec[i]);
                if(pExtCodecConfig != NULL)
                {
                    if (i != 0)
                    {
                        strlcat(strABNF, ", ", sizeof(strABNF));
                    }
                    MM_MSG_PRIO3(MM_GENERAL,MM_PRIO_ERROR,
                          "CEA = %x , Vesa = %x , hh = %x",
                          pExtCodecConfig->supported_cea_mode,
                          pExtCodecConfig->supported_vesa_mode,
                          pExtCodecConfig->supported_hh_mode);
                    snprintf(videoFormatExtStr, 1024,
                        "%s %.2x %.2x %.10x %.10x %.10x %.2x %.4x %.4x %.2x ",
                        pExtCodecConfig->name,
                        pExtCodecConfig->profile,
                        pExtCodecConfig->level,
                        pExtCodecConfig->supported_cea_mode,
                        pExtCodecConfig->supported_vesa_mode,
                        pExtCodecConfig->supported_hh_mode,
                        pExtCodecConfig->decoder_latency,
                        pExtCodecConfig->min_slice_size,
                        pExtCodecConfig->slice_enc_params,
                        pExtCodecConfig->frame_rate_control_support);
                    strlcat(strABNF, videoFormatExtStr, sizeof(strABNF));
                }
            }
        }

        return strABNF;
    }

    if (strncmp(pKey, "wfd_aux_stream_info", sizeof("wfd_aux_stream_info")) == 0 && pAuxCapability != NULL) {
        char auxFormatStr[1024];

        if(pAuxCapability->count)
        {
            for (int i=0; i < pAuxCapability->count; i++)
            {
                if (i != 0)
                {
                    strlcat(strABNF, "; ", sizeof(strABNF));
                }
                snprintf(auxFormatStr, 1024, "%s ",pAuxCapability->name[i]);
                strlcat(strABNF,auxFormatStr,sizeof(strABNF));
            }
            snprintf(auxFormatStr, 1024, "%.2x %.2x %.2x",
                                    pAuxCapability->maxOverlay,
                                    pAuxCapability->blendMode,
                                    pAuxCapability->supportedBitMap);
            strlcat(strABNF,auxFormatStr,sizeof(strABNF));
        }

        return strABNF;
    }

    if (strncmp(pKey, "wfd_3d_video_formats",sizeof("wfd_3d_video_formats")) == 0) {
        // no 3d video support
    strlcat(strABNF, "00 00 00 00 00000000 00 0000 0000 00 none none", sizeof(strABNF));
        return strABNF;
    }

    if (strncmp(pKey, "wfd_content_protection", sizeof("wfd_content_protection")) == 0) {
        // no HDCP support
      strlcat(strABNF, (char*)getHDCPCapStr().c_str(),sizeof(strABNF));
        return strABNF;
    }

    if (strncmp(pKey, "wfd_display_edid", sizeof("wfd_display_edid")) == 0) {
        char sEdidData[EDID_RAW_DATA_SIZE + 1];
        int nSize = 0;
        memset (sEdidData, '\0', EDID_RAW_DATA_SIZE + 1);
        // get the EDID information from Display
        nSize = getEdidRawData(sEdidData);

        int i = nSize;
        if((nSize > 0) && (i < (EDID_RAW_DATA_SIZE + 1)))
        {
            while(--i && !sEdidData[i]);
        }


        if (nSize == 0 || i == 0) {
          // no display EDID support
          m_bEdidValidity = false;
          MM_MSG_PRIO(MM_GENERAL,MM_PRIO_ERROR,"edid none");
          strlcat(strABNF, "none", sizeof(strABNF));
        }
        else
        {
          int nBlockCount =
              ((nSize % MAX_EDID_BLOCK_SIZE) ? (nSize / MAX_EDID_BLOCK_SIZE) + 1
                                             : (nSize / MAX_EDID_BLOCK_SIZE));
          if (nBlockCount > 2)
          {
             nBlockCount = 2;
          }
          char sBlock[5];
          char sEdidlBuffer[(EDID_RAW_DATA_SIZE * 2) + 1];
          char sEdidFinallBuffer[(EDID_RAW_DATA_SIZE * 2) + MAX_EDID_BLOCK_COUNT_SIZE + 1];
          memset (sEdidlBuffer, '\0', (EDID_RAW_DATA_SIZE * 2) + 1);
          memset (sBlock, '\0', 5);
          memset (sEdidFinallBuffer, '\0',
                 (EDID_RAW_DATA_SIZE * 2) + MAX_EDID_BLOCK_COUNT_SIZE + 1);
          snprintf(sBlock,5,"000%d",nBlockCount);
          int i = 0;
          int j = 0;
          /* Convert the Raw data to hex stream */
          for (i = 0; i < nSize; i++)
          {
              /*Copy each nibble from source buffer to edid final hex stream buffer */
              int len = snprintf (&sEdidlBuffer[j++],(EDID_RAW_DATA_SIZE * 2) + 1,"%x",(char)(sEdidData[i] >> 4));
              len = snprintf (&sEdidlBuffer[j++],(EDID_RAW_DATA_SIZE * 2) + 1,"%x",(char)(sEdidData[i] & 0x0F));
          }

        /*Preparing Final string with number of block in EDID*/
          snprintf (sEdidFinallBuffer,
                    (EDID_RAW_DATA_SIZE * 2)+ MAX_EDID_BLOCK_COUNT_SIZE + 1,
                    "%s %s",
                    sBlock,sEdidlBuffer);

          m_bEdidValidity = true;
          strlcat(strABNF, sEdidFinallBuffer, sizeof(strABNF));

        }

       return strABNF;
    }

    if (strncmp(pKey, "wfd_coupled_sink", sizeof("wfd_coupled_sink")) == 0) {
        strlcat(strABNF, "00 none", sizeof(strABNF));
        return strABNF;
    }

    //if (strcmp(pKey, "wfd_trigger_method") == 0) {
    //}

    if (strncmp(pKey, "wfd_client_rtp_ports", sizeof("wfd_client_rtp_ports")) == 0) {
        snprintf(strABNF, sizeof(strABNF), "RTP/AVP/UDP;unicast %d %d mode=play", pCapability->transport_capability_config.port1_id, pCapability->transport_capability_config.port2_id);
        return strABNF;
    }

    if (strncmp(pKey, "wfd_I2C", sizeof("wfd_I2C")) == 0) {
        snprintf(strABNF, sizeof(strABNF), "00 0");
        return strABNF;
    }

    if (strncmp(pKey, "wfd_uibc_capability", sizeof("wfd_uibc_capability")) == 0) {
        strlcpy(strABNF, (char*)getUibcCapStr().c_str(), sizeof(strABNF));
        return strABNF;
    }

    if (strncmp(pKey, "wfd_connector_type", sizeof("wfd_connector_type")) == 0) {
        // no connector type support
        if (m_bEdidValidity)
        {
          snprintf(strABNF, sizeof(strABNF), "05");
        }
        else
        {
          snprintf(strABNF, sizeof(strABNF), "07");
        }
        return strABNF;
    }

    SessionManager *pSM = SessionManager::Instance();

    if (strncmp(pKey, "wfd_presentation_URL", sizeof("wfd_presentation_URL")) == 0) {
        snprintf(strABNF,sizeof(strABNF), "rtsp://%s/wfd1.0/streamid=0 none", pSM->pMyDevice->ipAddr.c_str());
        return strABNF;
    }

    if (strncmp(pKey, "wfd_standby_resume_capability", sizeof("wfd_standby_resume_capability")) == 0) {
        if((pSM !=NULL) && (pSM->pMyDevice != NULL)  && (pSM->pMyDevice->pMMCapability != NULL) && (pSM->pMyDevice->pMMCapability->pCapability != NULL))
        {
            if (pSM->pMyDevice->pMMCapability->pCapability->standby_resume_support){
                snprintf(strABNF, sizeof(strABNF), "supported");
            }else {
                snprintf(strABNF, sizeof(strABNF), "none");
            }
        }
        return strABNF;
    }

    return strABNF;
}


bool MMCapability::setKeyValue(char* pKey, char* pValue) {

    if (strncmp(pKey, "wfd_audio_codecs", sizeof("wfd_audio_codecs")) == 0) {
        memset(&(pCapability->audio_config), 0, sizeof(WFD_audio_config));

        char* pch;
        if ((pch = strstr(pValue, "LPCM")) != NULL) {
            pCapability->audio_method = WFD_AUDIO_LPCM;
            pCapability->audio_config.lpcm_codec.supported_modes_bitmap = hex2Long(pch+5, 8);
            pCapability->audio_config.lpcm_codec.decoder_latency = hex2Long(pch+14, 2);
        }
        if ((pch = strstr(pValue, "AAC")) != NULL) {
            pCapability->audio_method = WFD_AUDIO_AAC;
            pCapability->audio_config.aac_codec.supported_modes_bitmap = hex2Long(pch+4, 8);
            pCapability->audio_config.aac_codec.decoder_latency = hex2Long(pch+13, 2);
        }
        if ((pch=strstr(pValue, "AC3")) != NULL) {
            pCapability->audio_method = WFD_AUDIO_DOLBY_DIGITAL;
            pCapability->audio_config.dolby_digital_codec.supported_modes_bitmap = hex2Long(pch+4, 8);
            pCapability->audio_config.dolby_digital_codec.decoder_latency = hex2Long(pch+13, 2);
        }
        return true;
    }

    if (strncmp(pKey, "wfd_video_formats", sizeof("wfd_video_formats")) == 0) {
        memset(pCapability->video_config.video_config.h264_codec, 0,
               sizeof(WFD_h264_codec_config_t)*WFD_MAX_NUM_H264_PROFILES);

        char* pch;

        pCapability->video_config.video_config.native_bitmap = hex2Long(pValue, 2);
        pCapability->video_config.video_config.preferred_display_mode_supported = hex2Long(pValue+3, 2);

        pch = pValue+6;

        int numH264 = 0;

        while (pch != NULL) {
            // parse the H264 params
            pCapability->video_config.video_config.h264_codec[numH264].h264_profile = hex2Long(pch, 2);
            pCapability->video_config.video_config.h264_codec[numH264].h264_level = hex2Long(pch+3, 2);
            pCapability->video_config.video_config.h264_codec[numH264].supported_cea_mode = hex2Long(pch+6, 8);
            pCapability->video_config.video_config.h264_codec[numH264].supported_vesa_mode = hex2Long(pch+15, 8);
            pCapability->video_config.video_config.h264_codec[numH264].supported_hh_mode = hex2Long(pch+24, 8);
            pCapability->video_config.video_config.h264_codec[numH264].decoder_latency = hex2Long(pch+33, 2);
            pCapability->video_config.video_config.h264_codec[numH264].min_slice_size = hex2Long(pch+36, 4);
            pCapability->video_config.video_config.h264_codec[numH264].slice_enc_params = hex2Long(pch+41, 4);
            pCapability->video_config.video_config.h264_codec[numH264].frame_rate_control_support = hex2Long(pch+46, 2);
            pCapability->video_config.video_config.h264_codec[numH264].max_hres = hex2Long(pch+49, 4);
            pCapability->video_config.video_config.h264_codec[numH264].max_vres = hex2Long(pch+54, 4);
            //As resolution values are not populated in negotiated capabilities anywhere, we have to derive the same from modes bitmap
            getResolutionFromBitmap(pCapability->video_config.video_config.h264_codec,numH264);

            pch = strchr(pch, ',');
            if (pch != NULL) {
                pch += 2;
            }

            MM_MSG_PRIO1(MM_GENERAL,MM_PRIO_HIGH,"MMCapability_CPP: setKeyValue h264_profile %d",pCapability->video_config.video_config.h264_codec[numH264].h264_profile);
            MM_MSG_PRIO1(MM_GENERAL,MM_PRIO_HIGH,"MMCapability_CPP: setKeyValue h264_level %d",pCapability->video_config.video_config.h264_codec[numH264].h264_level);
            MM_MSG_PRIO1(MM_GENERAL,MM_PRIO_HIGH,"MMCapability_CPP: setKeyValue cea %u",pCapability->video_config.video_config.h264_codec[numH264].supported_cea_mode);
            MM_MSG_PRIO1(MM_GENERAL,MM_PRIO_HIGH,"MMCapability_CPP: setKeyValue hh  %u",pCapability->video_config.video_config.h264_codec[numH264].supported_hh_mode);
            MM_MSG_PRIO1(MM_GENERAL,MM_PRIO_HIGH,"MMCapability_CPP: setKeyValue vesa %u",pCapability->video_config.video_config.h264_codec[numH264].supported_vesa_mode);
            MM_MSG_PRIO1(MM_GENERAL,MM_PRIO_HIGH,"MMCapability_CPP: setKeyValue max_hres %d",pCapability->video_config.video_config.h264_codec[numH264].max_hres);
            MM_MSG_PRIO1(MM_GENERAL,MM_PRIO_HIGH,"MMCapability_CPP: setKeyValue max_vres %d",pCapability->video_config.video_config.h264_codec[numH264].max_vres);
            numH264++;
        }
        pCapability->video_config.video_config.num_h264_profiles = numH264;
        if (pCapability->video_config.video_config.num_h264_profiles != 0) {
           pCapability->video_method = WFD_VIDEO_H264;
        }
        return true;
    }

    if (strncmp(pKey, "wfd_video_formats_extension", sizeof("wfd_video_formats_extension")) == 0) {
        /*TODO: Make it proper for codec count > 9*/
        int count = hex2Long(pValue, 1);
        MM_MSG_PRIO1(MM_GENERAL,MM_PRIO_HIGH,"MMCapability_CPP: setKeyValue codecCount %d",count);

        if(pExtendedCapability != NULL)
        {
            MM_Free(pExtendedCapability);
            pExtendedCapability = NULL;
        }
        pExtendedCapability = (WFD_MM_Extended_capability_t *)MM_Malloc(sizeof(WFD_MM_Extended_capability_t));
        if(pExtendedCapability == NULL)
        {
            MM_MSG_PRIO(MM_GENERAL,MM_PRIO_HIGH,"MMCapability_CPP: wfd_video_formats_extension , memory alloc failed");
            return false;
        }
        memset(pExtendedCapability,0,sizeof(WFD_MM_Extended_capability_t));

        WFD_extended_video_codec_config_t *tempExVideoCfg =
           &(pExtendedCapability->extended_video_config.extended_video_config);

        if(tempExVideoCfg->codec != NULL)
        {
            MM_Free(tempExVideoCfg->codec);
            tempExVideoCfg->codec = NULL;
        }
        tempExVideoCfg->codec =
            (WFD_extended_codec_config_t *)MM_Malloc(count * sizeof(WFD_extended_codec_config_t));

        if(tempExVideoCfg->codec == NULL)
        {
            MM_MSG_PRIO(MM_GENERAL,MM_PRIO_HIGH,
                "MMCapability_CPP:WFD_extended_codec_config,memory alloc failed");
            return false;
        }
        memset(tempExVideoCfg->codec, 0,
               sizeof(WFD_extended_codec_config_t)*count);

        char* pch;
        tempExVideoCfg->native_bitmap = hex2Long(pValue+2, 2);
        tempExVideoCfg->preferred_display_mode_supported = hex2Long(pValue+5, 2);
        MM_MSG_PRIO2(MM_GENERAL,MM_PRIO_HIGH,"MMCapability_CPP: setKeyValue \
            native_bitmap %d preferred_display_mode_supported %d",
            tempExVideoCfg->native_bitmap,
            tempExVideoCfg->preferred_display_mode_supported);

        pch = pValue+9;

        int numCodec = 0;

        while (pch != NULL && numCodec < count)
        {
            WFD_extended_codec_config_t *tempCodec =
                                &(tempExVideoCfg->codec[numCodec]);
            /* Parse the Codec params */
            /* Start Parsing Name of Codec*/
            char *pName = pch;
            int nSize = 0;
            /* 'pch' will point to space after name string */
            pch = strchr(pch,' ');
            if(pch != NULL)
            {
                /* Get the size of name String */
                nSize = pch - pName;
                /* Move 'pch' to point to next data after space for
                  further processing */
                pch+=1;
            }
            else
            {
                break;
            }
            /* If    string is with in limit of name array which can store
                     6 char as of now then only go ahead and upadte it and
                     mark the 6th char as '\0'.
               Else  populate name as '\0'. Currently not discarding this
                     data here, if needed it can be discarded when trying
                     intersect as name info is not present.
            */
            if(nSize < (int)sizeof(tempCodec->name))
            {
                strlcpy(tempCodec->name,pName,nSize+1);
            }
            else
            {
                tempCodec->name[0]='\0';
            }
            /* Parse other codec params */
            tempCodec->profile = hex2Long(pch, 2);
            tempCodec->level = hex2Long(pch+3, 2);
            tempCodec->supported_cea_mode = hex2Long(pch+6, 10);
            tempCodec->supported_vesa_mode = hex2Long(pch+17, 10);
            tempCodec->supported_hh_mode = hex2Long(pch+28, 10);
            tempCodec->decoder_latency = hex2Long(pch+39, 2);
            tempCodec->min_slice_size = hex2Long(pch+42, 4);
            tempCodec->slice_enc_params = hex2Long(pch+47, 4);
            tempCodec->frame_rate_control_support = hex2Long(pch+52, 2);

            /* Create list of supported resolution for different Modes,
               specific to this codec */
            getResolutionFromBitmap(tempExVideoCfg->codec,numCodec);

            /* Move to next codec element for parsing */
            pch = strchr(pch, ',');
            if (pch != NULL)
            {
                pch += 2;
            }

            MM_MSG_PRIO1(MM_GENERAL,MM_PRIO_HIGH,
                "MMCapability_CPP: setKeyValue name %s",tempCodec->name);
            MM_MSG_PRIO1(MM_GENERAL,MM_PRIO_HIGH,
                "MMCapability_CPP: setKeyValue profile %d",tempCodec->profile);
            MM_MSG_PRIO1(MM_GENERAL,MM_PRIO_HIGH,
                "MMCapability_CPP: setKeyValue level %d",tempCodec->level);
            MM_MSG_PRIO1(MM_GENERAL,MM_PRIO_HIGH,
                "MMCapability_CPP: setKeyValue cea %u",tempCodec->supported_cea_mode);
            MM_MSG_PRIO1(MM_GENERAL,MM_PRIO_HIGH,
                "MMCapability_CPP: setKeyValue hh  %u",tempCodec->supported_hh_mode);
            MM_MSG_PRIO1(MM_GENERAL,MM_PRIO_HIGH,
                "MMCapability_CPP: setKeyValue vesa %u",tempCodec->supported_vesa_mode);

            MM_MSG_PRIO(MM_GENERAL,MM_PRIO_HIGH,
                "MMCapability_CPP: setKeyValue cea_list");
            for(int i = 0; i < MAX_MODE_LIST; i++)
            {
                /*MM_MSG_PRIO3(MM_GENERAL,MM_PRIO_HIGH,
                    "cea_list[%d] width = %u , height = %u",
                    i,
                    tempCodec->cea_mode_list[i].width,
                    tempCodec->cea_mode_list[i].height);*/
            }
            MM_MSG_PRIO(MM_GENERAL,MM_PRIO_HIGH,
                "MMCapability_CPP: setKeyValue hh_list");
            for(int i = 0; i < MAX_MODE_LIST; i++)
            {
                /*MM_MSG_PRIO3(MM_GENERAL,MM_PRIO_HIGH,
                    "hh_list[%d] width = %u , height = %u",
                    i,
                    tempCodec->hh_mode_list[i].width,
                    tempCodec->hh_mode_list[i].height);*/
            }
            MM_MSG_PRIO(MM_GENERAL,MM_PRIO_HIGH,
                "MMCapability_CPP: setKeyValue vesa_list");
            for(int i = 0; i < MAX_MODE_LIST; i++)
            {
                /*MM_MSG_PRIO3(MM_GENERAL,MM_PRIO_HIGH,
                    "vesa_list[%d] width = %u , height = %u",
                    i,
                    tempCodec->vesa_mode_list[i].width,
                    tempCodec->vesa_mode_list[i].height);*/
            }

            numCodec++;
        }
        tempExVideoCfg->num_codec= numCodec;
        return true;
    }

    if (strncmp(pKey, "wfd_aux_stream_info", sizeof("wfd_aux_stream_info")) == 0) {
        if(pValue == NULL)
            return false;
        if(!pAuxCapability)
        {
            pAuxCapability = (WFD_MM_Aux_capability_t*)MM_Malloc(sizeof(WFD_MM_Aux_capability_t));
            if(!pAuxCapability)
                return false;
            memset(pAuxCapability, 0, sizeof(WFD_MM_Aux_capability_t));
        }
        pAuxCapability->count = hex2Long(pValue, 1);
        if(pAuxCapability->count <= 0 ||
           pAuxCapability->count > WFD_GENERIC_INVALID)
        {
            return false;
        }
        char* pch = pValue+2;
        int numCodec = 0;
        while (pch != NULL  && numCodec < pAuxCapability->count)
        {
            /* Parse the Codec params */
            /* Start Parsing Name of Codec*/
            char *pName = pch;
            int nSize = 0;
            /* 'pch' will point to space after name string */
            pch = strchr(pch,' ');
            if(pch != NULL)
            {
                /* Get the size of name String.*/
                nSize = pch - pName;
                /* Move 'pch' to point to next data after space for
                  further processing */
                pch+=1;
            }
            /* If    string is with in limit of name array which can store
                     10 char as of now then only go ahead and upadte it and
                     mark the 10th char as '\0'.
               Else  populate name as '\0'. Currently not discarding this
                     data here, if needed it can be discarded when trying
                     intersect as name info is not present.
            */
            if(nSize < (int)sizeof(pAuxCapability->name[0]))
            {
                strlcpy(pAuxCapability->name[numCodec],pName,nSize+1);
            }
            else
            {
                pAuxCapability->name[numCodec][0]='\0';
            }
            numCodec++;
        }

        if(pch != NULL) {
            pAuxCapability->maxOverlay = hex2Long(pch, 2);
            pAuxCapability->blendMode = hex2Long(pch+3, 2);
            pAuxCapability->supportedBitMap = hex2Long(pch+6, 2);
        }
        MM_MSG_PRIO1(MM_GENERAL,MM_PRIO_HIGH,
            "MMCapability_CPP: wfd_aux_stream_info setKeyValue count %d",pAuxCapability->count);
        MM_MSG_PRIO1(MM_GENERAL,MM_PRIO_HIGH,
            "MMCapability_CPP: wfd_aux_stream_info setKeyValue name %s",pAuxCapability->name[0]);
        MM_MSG_PRIO1(MM_GENERAL,MM_PRIO_HIGH,
            "MMCapability_CPP: wfd_aux_stream_info setKeyValue overlay %d",pAuxCapability->maxOverlay);
        MM_MSG_PRIO1(MM_GENERAL,MM_PRIO_HIGH,
            "MMCapability_CPP: wfd_aux_stream_info setKeyValue mode %d",pAuxCapability->blendMode);
        MM_MSG_PRIO1(MM_GENERAL,MM_PRIO_HIGH,
            "MMCapability_CPP: wfd_aux_stream_info setKeyValue bitmap %d",pAuxCapability->supportedBitMap);
    }

    if (strncmp(pKey, "wfd_3d_video_formats", sizeof("wfd_3d_video_formats")) == 0) {
        // no 3d video support
        return true;
    }

    if (strncmp(pKey, "wfd_content_protection", sizeof("wfd_content_protection")) == 0)
    {
      //getting HDCP version
      if(strstr(pValue,"HDCP2.") != NULL) {
          pCapability->content_protection_config.content_protection_capability =
                                                 atoi(pValue + strlen("HDCP2.")) + 1;
          MM_MSG_PRIO1(MM_GENERAL, MM_PRIO_HIGH, "wfd_cfg_parser->getSupportedHDCPVersion() HDCP2.%d",
                       (pCapability->content_protection_config.content_protection_capability - 1));
      }

      // update HDCP port information
      char* pch;
      if ((pch = strstr(pValue, "port=")) != NULL)
      {
          int port;
          sscanf(pch+5, "%d", &port);
          pCapability->content_protection_config.content_protection_ake_port = port;
      }
      return true;
    }

    if (strncmp(pKey, "wfd_display_edid", sizeof("wfd_display_edid")) == 0) {
        // no display EDID support
        return true;
    }

    if (strncmp(pKey, "wfd_coupled_sink", sizeof("wfd_coupled_sink")) == 0) {
        // ToDo:
        return true;
    }

    if (strncmp(pKey, "wfd_client_rtp_ports", sizeof("wfd_client_rtp_ports")) == 0) {
        // ToDo:
        return true;
    }

    if (strncmp(pKey, "wfd_I2C", sizeof("wfd_I2C")) == 0) {
        return true;
    }

    if (strncmp(pKey, "wfd_uibc_capability", sizeof("wfd_uibc_capability")) == 0) {
        memset(pUibcCapability, 0, sizeof(WFD_uibc_capability_t));
        char *pTmpPtr = NULL;
        char* pch;
        pch = strtok_r(pValue, ";", &pTmpPtr);
        while (pch != NULL) {
            if (strstr(pch, "input_category_list=")) {
                if (strstr(pch, "GENERIC") != NULL) {
                    pUibcCapability->config.category |= GENERIC;
                }
                if (strstr(pch, "HIDC") != NULL) {
                    pUibcCapability->config.category |= HIDC;
                }
            } else if (strstr(pch, "generic_cap_list=")) {
                if (strstr(pch, "Keyboard") != NULL) {
                    pUibcCapability->config.generic_input_type |= KEYBOARD;
                }
                if (strstr(pch, "Mouse") != NULL) {
                    pUibcCapability->config.generic_input_type |= MOUSE;
                }
                if (strstr(pch, "SingleTouch") != NULL) {
                    pUibcCapability->config.generic_input_type |= SINGLETOUCH;
                }
                if (strstr(pch, "MultiTouch") != NULL) {
                    pUibcCapability->config.generic_input_type |= MULTITOUCH;
                }
                if (strstr(pch, "Joystick") != NULL) {
                    pUibcCapability->config.generic_input_type |= JOYSTICK;
                }
                if (strstr(pch, "Camera") != NULL) {
                    pUibcCapability->config.generic_input_type |= CAMERA;
                }
                if (strstr(pch, "Gesture") != NULL) {
                    pUibcCapability->config.generic_input_type |= GESTURE;
                }
                if (strstr(pch, "RemoteControl") != NULL) {
                    pUibcCapability->config.generic_input_type |= REMOTECONTROL;
                }
            } else if (strstr(pch, "hidc_cap_list=")) {
                char* pch2 = NULL;
                char* pTmpPtr2 = NULL;
                int idx = -1;
                pch2 = strtok_r(pch, ",", &pTmpPtr2);
                while (pch2 != NULL) {
                    if (strstr(pch2, "Infrared") != NULL) {
                        idx = INFRARED;
                    } else if (strstr(pch2, "USB") != NULL) {
                        idx = USB;
                    } else if (strstr(pch2, "BT") != NULL) {
                        idx = BT;
                    } else if (strstr(pch2, "Zigbee") != NULL) {
                        idx = ZIGBEE;
                    } else if (strstr(pch2, "Wi-Fi") != NULL) {
                        idx = WIFI;
                    } else if (strstr(pch2, "No-SP") != NULL) {
                        idx = NOSP;
                    }

                    if (idx != -1) {
                        if (strstr(pch2, "Keyboard") != NULL) {
                            pUibcCapability->config.hid_input_type_path[idx] |= KEYBOARD;
                        } else if (strstr(pch2, "Mouse") != NULL) {
                            pUibcCapability->config.hid_input_type_path[idx] |= MOUSE;
                        } else if (strstr(pch2, "SingleTouch") != NULL) {
                            pUibcCapability->config.hid_input_type_path[idx] |= SINGLETOUCH;
                        } else if (strstr(pch2, "MultiTouch") != NULL) {
                            pUibcCapability->config.hid_input_type_path[idx] |= MULTITOUCH;
                        } else if (strstr(pch2, "Joystick") != NULL) {
                            pUibcCapability->config.hid_input_type_path[idx] |= JOYSTICK;
                        } else if (strstr(pch2, "Camera") != NULL) {
                            pUibcCapability->config.hid_input_type_path[idx] |= CAMERA;
                        } else if (strstr(pch2, "Gesture") != NULL) {
                            pUibcCapability->config.hid_input_type_path[idx] |= GESTURE;
                        } else if (strstr(pch2, "RemoteControl") != NULL) {
                            pUibcCapability->config.hid_input_type_path[idx] |= REMOTECONTROL;
                        }
                    }

                    pch2 = strtok_r(NULL, ",", &pTmpPtr2);
                }
            } else if (strstr(pch, "port=")) {
                int port;
                sscanf(pch+5, "%d", &port);
                pUibcCapability->port_id = port;
            }

            pch = strtok_r(NULL, ";", &pTmpPtr);
        }


        return true;
    }

    if (strncmp(pKey, "wfd_connector_type", sizeof("wfd_connector_type")) == 0) {
        return true;
    }

    if (strncmp(pKey, "wfd_standby_resume_capability", sizeof("wfd_standby_resume_capability")) == 0) {
        if (strstr(pValue, "supported") != NULL) {
            pCapability->standby_resume_support = true;
        } else {
            pCapability->standby_resume_support = false;
        }
    }

    return true;
}


/**
 * Dump the content of MMCapability to log output
 */
void MMCapability::dump() {

    MM_MSG_PRIO(MM_GENERAL,MM_PRIO_HIGH,"=========== MMCapability dump BEGIN ===============");

    #define numParams 12
    string params[numParams] = {
        "wfd_audio_codecs",
        "wfd_video_formats",
        "wfd_video_formats_extension",
        "wfd_3d_video_formats",
        "wfd_content_protection",
        "wfd_display_edid",
        "wfd_coupled_sink",
        "wfd_client_rtp_ports",
        "wfd_I2C",
        "wfd_uibc_capability",
        "wfd_connector_type",
        "wfd_aux_stream_info"
    };

    char buffer[2048];
    for (int i=0; i<numParams; i++) {
        memset(buffer, 0, sizeof(buffer));
        strlcat(buffer, params[i].c_str(), sizeof(buffer));
        strlcat(buffer, ": ", sizeof(buffer));
        strlcat(buffer, getKeyValue((char*)params[i].c_str()), sizeof(buffer));
        MM_MSG_PRIO1(MM_GENERAL,MM_PRIO_HIGH,"%s", buffer);
    }

    MM_MSG_PRIO(MM_GENERAL,MM_PRIO_HIGH,"=========== MMCapability dump END ===============");
}


/**
 * Configure MMCapability using the input from rtsp library
 * @param pRtspWfd
 */
void MMCapability::configure(rtspWfd &RtspWfd) {

    MM_MSG_PRIO(MM_GENERAL,MM_PRIO_HIGH,"Configure Peer MM Capability using received RTSP wfd");

    RTSPStringStream ss;

    /* audio_codecs */
    string audioStr;
    if (RtspWfd.audioLpcm.getValid()) {
        ss.str("");
        ss << RtspWfd.audioLpcm;
        audioStr = ss.str();
    }
    if (RtspWfd.audioAac.getValid()) {
        ss.str("");
        ss << RtspWfd.audioAac;
        if (audioStr.length() > 0) {
            audioStr += ", ";
        }
        audioStr += ss.str();
    }
    if (RtspWfd.audioEac.getValid()) {
        ss.str("");
        ss << RtspWfd.audioEac;
        if (audioStr.length() > 0) {
            audioStr += ", ";
        }
        audioStr += ss.str();
    }

    if (RtspWfd.audioAc3.getValid()) {
        ss.str("");
        ss << RtspWfd.audioAc3;
        if (audioStr.length() > 0) {
            audioStr += ", ";
        }
        audioStr += ss.str();
    }

    if (RtspWfd.audioDts.getValid()) {
        ss.str("");
        ss << RtspWfd.audioDts;
        if (audioStr.length() > 0) {
            audioStr += ", ";
        }
        audioStr += ss.str();
    }

    if (audioStr.length() > 0) {
        MM_MSG_PRIO1(MM_GENERAL,MM_PRIO_HIGH,"Configure wfd_audio_codecs...  %s", audioStr.c_str());
        setKeyValue((char *)"wfd_audio_codecs", (char*)audioStr.c_str());
    }

    /* video_formats */
    string videoStr;
    string videoHdr, h264Str;
    if (RtspWfd.videoHeader.getValid()) {
        ss.str("");
        ss << RtspWfd.videoHeader;
        videoHdr = ss.str();
    }
    if (RtspWfd.h264Cbp.getValid()) {
        ss.str("");
        ss << RtspWfd.h264Cbp;
        h264Str = ss.str();
    }
    if (RtspWfd.h264Chp.getValid()) {
        ss.str("");
        ss << RtspWfd.h264Chp;

        if (h264Str.length() > 0) {
            h264Str = h264Str + ", " + ss.str();
        }
        else
        {
            h264Str = ss.str();
        }
    }
    if (RtspWfd.h264Chi444p.getValid()) {
        ss.str("");
        ss << RtspWfd.h264Chi444p;
        if (h264Str.length() > 0) {
            h264Str = h264Str + ", " + ss.str();
        }
    }

    videoStr = videoHdr + " " + h264Str;
    if (videoStr.length() > 1) {
        MM_MSG_PRIO1(MM_GENERAL,MM_PRIO_HIGH,"Configure wfd_video_formats...  %s", videoStr.c_str());
        setKeyValue((char *)"wfd_video_formats", (char*)videoStr.c_str());
    }

    /* wfd_uibc_capability */
    string uibcStr;
    if (RtspWfd.uibcCap.getValid()) {
        ss.str("");
        ss << RtspWfd.uibcCap;
        //MM_MSG_PRIO2(MM_GENERAL,MM_PRIO_HIGH,"configure %s: %s", RtspWfd.uibcCap.getName().c_str(), ss.str().c_str());
        uibcStr = ss.str();
    }

    if (uibcStr.length() > 0) {
        MM_MSG_PRIO1(MM_GENERAL,MM_PRIO_HIGH,"Configure wfd_uibc_capability...  %s", uibcStr.c_str());
        setKeyValue((char *)"wfd_uibc_capability", (char*)uibcStr.c_str());
    }

    /* wfd_standby_resume_capability */
    string standby_capStr;
    if (RtspWfd.standbyCap.getValid()) {
        ss.str("");
        ss << RtspWfd.standbyCap;
        standby_capStr = ss.str();
    }

    if (standby_capStr.length() > 0){
        MM_MSG_PRIO1(MM_GENERAL,MM_PRIO_HIGH,"Configure wfd_standby_resume_capability...  %s", standby_capStr.c_str());
        setKeyValue((char *)"wfd_standby_resume_capability",(char*)standby_capStr.c_str());
    }

    /* wfd_content_protection capability */
    string hdcp_content_protection;
    if( RtspWfd.contentProtection.getValid())
    {
        ss.str("");
        ss << RtspWfd.contentProtection;
        hdcp_content_protection = ss.str();
    }

    if( hdcp_content_protection.length() > 0 )
    {
        MM_MSG_PRIO1(MM_GENERAL,MM_PRIO_HIGH,"Configure wfd_content_protection  %s", hdcp_content_protection.c_str());
        setKeyValue((char *)"wfd_content_protection",(char*) hdcp_content_protection.c_str());
    }

    /*Extended Video Codecs*/
    string codecStr,headerStr;
    int availableCount = 0;
    char countS[10];
    if(RtspWfd.codecInfo != NULL)
    {
        ss.str("");
        if(RtspWfd.videoHeaderCodecInfo.getValid())
        {
            ss<<RtspWfd.videoHeaderCodecInfo;
            headerStr = ss.str();
        }

        availableCount = RtspWfd.codecInfoCount;
        snprintf(countS,10,"%d",availableCount);
        string countStr(countS);
        codecStr = countStr+" " + headerStr;

        for(int count = 0 ; count < availableCount ; count++)
        {
            ss.str("");
            ss << RtspWfd.codecInfo[count];
            codecStr = codecStr + ", "+ss.str();
        }
    }
    if( codecStr.length() > 0 )
    {
        MM_MSG_PRIO1(MM_GENERAL,MM_PRIO_HIGH,"Configure wfd_video_formats_extension: [%s]", codecStr.c_str());
        setKeyValue((char *)"wfd_video_formats_extension",(char*) codecStr.c_str());
    }

    /*wfd_aux_stream_info*/
    string auxCodecStr;
    int codecCount = 0;
    char auxS[5];
    if(RtspWfd.auxCodecInfo.getValid())
    {
        ss.str("");
        codecCount = RtspWfd.auxCodecInfo.getCodecCount();
        ss << RtspWfd.auxCodecInfo;
        auxCodecStr = ss.str();

        snprintf(auxS,5,"%d",codecCount);
        string auxStr(auxS);
        auxCodecStr = auxStr+" " + auxCodecStr;
    }

    if( auxCodecStr.length() > 0 )
    {
        MM_MSG_PRIO1(MM_GENERAL,MM_PRIO_HIGH,"Configure wfd_aux_stream_info: [%s]", auxCodecStr.c_str());
        setKeyValue((char *)"wfd_aux_stream_info",(char*) auxCodecStr.c_str());
    }

    if(!strncmp(RtspWfd.dirStreamMode.getMode().c_str(),"active",sizeof("active")))
    {
        pCapability->direct_streaming_mode = true;
    }
    else if(!strncmp(RtspWfd.dirStreamMode.getMode().c_str(),"inactive",sizeof("inactive")))
    {
        pCapability->direct_streaming_mode = false;
    }
}


long MMCapability::hex2Long(char* pHex, int hexLength) {
    long val = 0;

    int cnt = 0;
    char* c = pHex;
    while (*c != '\0' && cnt<hexLength) {
        val <<= 4;
        if( (*c >= '0') && (*c <= '9') ) {
            val += (*c - '0');
        } else if ( (*c >= 'A') && (*c <= 'F') ) {
            val += (*c - 'A' + 10);
        } else if ( (*c >= 'a') && (*c <= 'f') ) {
            val += (*c - 'a' + 10);
        } else {
            return 0;
        }
        c++;
        cnt++;
    }
    return val;
}


string MMCapability::getHDCPCapStr() {
  WFD_content_protection_capability_config_t content_protection_config = pCapability->content_protection_config;
  if (content_protection_config.content_protection_capability == 0)
  {
    return "none";
  }

  string hdcpStr;

  char verDe[24];
  memset(verDe, '\0', 24);
  //Miracast spec asks the string to be 2.1 if the HDCP version is 2.1 or higher
  if (content_protection_config.content_protection_capability <= WFD_HDCP_VERSION_2_1)
      snprintf(verDe,24, "HDCP2.%d ",(content_protection_config.content_protection_capability - 1));
  else
      snprintf(verDe,24, "HDCP2.%d ",(WFD_HDCP_VERSION_2_1 - 1));

  hdcpStr += verDe;

  char port_value[256];
  snprintf(port_value,256,"port=%d",content_protection_config.content_protection_ake_port);
  string port_str(port_value);

  hdcpStr += port_str;
  return hdcpStr;
}

/**
 * Function to convert UIBC capability into string
 *
 * @return string
 */
string MMCapability::getUibcCapStr() {
    if ((pUibcCapability->config.category & (BIT0|BIT1)) == 0) {
        return "none";
    }

    string uibcStr;

    //input-category-val
    uibcStr = "input_category_list=";
    switch (pUibcCapability->config.category & (GENERIC|HIDC)) {
    case 0:
        uibcStr += "none;";
        break;
    case GENERIC:
        uibcStr += "GENERIC;";
        break;
    case HIDC:
        uibcStr += "HIDC;";
        break;
    case (GENERIC|HIDC):
        uibcStr += "GENERIC, HIDC;";
        break;
    default:
        break;
    }
    //generic-cap-val
    uibcStr += "generic_cap_list=";
    string gi_value;
    if (pUibcCapability->config.generic_input_type == 0) {
        gi_value = "none;";
    } else {
        if (pUibcCapability->config.generic_input_type & KEYBOARD) {
            gi_value += "Keyboard, ";
        }
        if (pUibcCapability->config.generic_input_type & MOUSE) {
            gi_value += "Mouse, ";
        }
        if (pUibcCapability->config.generic_input_type & SINGLETOUCH) {
            gi_value += "SingleTouch, ";
        }
        if (pUibcCapability->config.generic_input_type & MULTITOUCH) {
            gi_value += "MultiTouch, ";
        }
        if (pUibcCapability->config.generic_input_type & JOYSTICK) {
            gi_value += "Joystick, ";
        }
        if (pUibcCapability->config.generic_input_type & CAMERA) {
            gi_value += "Camera, ";
        }
        if (pUibcCapability->config.generic_input_type & GESTURE) {
            gi_value += "Gesture, ";
        }
        if (pUibcCapability->config.generic_input_type & REMOTECONTROL) {
            gi_value += "RemoteControl, ";
        }
        gi_value.replace(gi_value.rfind(", "), 2, ";");
    }
    uibcStr += gi_value;
    //hidc-cap-val
    uibcStr += "hidc_cap_list=";
    string hi_value;
    for (int i=0; i<UIBC_NUM_INPUT_PATHS; i++) {
        if (pUibcCapability->config.hid_input_type_path[i] != 0) {
            string type, path;
            switch (i) {
            case INFRARED:
                path = "Infrared";
                break;
            case USB:
                path = "USB";
                break;
            case BT:
                path = "BT";
                break;
            case ZIGBEE:
                path = "Zigbee";
                break;
            case WIFI:
                path = "Wi-Fi";
                break;
            case NOSP:
                path = "No-SP";
                break;
            }
            if (pUibcCapability->config.hid_input_type_path[i] & KEYBOARD) {
                hi_value += "Keyboard/"+path+", ";
            }
            if (pUibcCapability->config.hid_input_type_path[i] & MOUSE) {
                hi_value += "Mouse/"+path+", ";
            }
            if (pUibcCapability->config.hid_input_type_path[i] & SINGLETOUCH) {
                hi_value += "SingleTouch/"+path+", ";
            }
            if (pUibcCapability->config.hid_input_type_path[i] & MULTITOUCH) {
                hi_value += "MultiTouch/"+path+", ";
            }
            if (pUibcCapability->config.hid_input_type_path[i] & JOYSTICK) {
                hi_value += "Joystick/"+path+", ";
            }
            if (pUibcCapability->config.hid_input_type_path[i] & CAMERA) {
                hi_value += "Camera/"+path+", ";
            }
            if (pUibcCapability->config.hid_input_type_path[i] & GESTURE) {
                hi_value += "Gesture/"+path+", ";
            }
            if (pUibcCapability->config.hid_input_type_path[i] & REMOTECONTROL) {
                hi_value += "RemoteControl/"+path+", ";
            }
        }
    }
    if (hi_value.length() == 0) {
        hi_value = "none;";
    } else {
        hi_value.replace(hi_value.rfind(", "), 2, ";");
    }
    uibcStr += hi_value;

    char port_value[256];
    snprintf(port_value, 256, "port=%d",pUibcCapability->port_id);
    string port_str(port_value);
    //tcp-port
    uibcStr += port_str;

    return uibcStr;
}

bool MMCapability::isHDCPVersionSupported(int nCapVersion)
{
  if ( ((WFD_HDCP_version_t)nCapVersion >= WFD_HDCP_VERSION_2_0) &&
       ((WFD_HDCP_version_t)nCapVersion <= WFD_HDCP_VERSION_MAX))
  {
       MM_MSG_PRIO1(MM_GENERAL,MM_PRIO_HIGH,"MMCapability::isHDCPVersionSupported() Version: 2.%d is supported", (nCapVersion - 1));
       return true;
  }
  MM_MSG_PRIO1(MM_GENERAL,MM_PRIO_HIGH,"MMCapability::isHDCPVersionSupported() Version: %d not supported", (nCapVersion - 1));

  return false;
}

