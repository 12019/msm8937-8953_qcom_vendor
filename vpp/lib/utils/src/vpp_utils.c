/*!
 * @file vpp_utils.c
 *
 * @cr
 * Copyright (c) 2015-2016 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.

 * @services
 *
 *
 */

#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <media/msm_media_info.h>
#include <cutils/properties.h>
#include <stdlib.h>

#include "vpp_dbg.h"
#include "vpp.h"
#include "vpp_utils.h"
/************************************************************************
 * Local definitions
 ***********************************************************************/
const uint32_t u32ResolutionMaxWidth[VPP_RESOLUTION_MAX] = {RES_SD_MAX_W, RES_HD_MAX_W,
                                                            RES_FHD_MAX_W, RES_UHD_MAX_W};
const uint32_t u32ResolutionMaxHeight[VPP_RESOLUTION_MAX] = {RES_SD_MAX_H, RES_HD_MAX_H,
                                                             RES_FHD_MAX_H, RES_UHD_MAX_H};

/************************************************************************
 * Local static variables
 ***********************************************************************/

/************************************************************************
 * Forward Declarations
 ************************************************************************/

/************************************************************************
 * Local Functions
 ***********************************************************************/

/************************************************************************
 * Global Functions
 ***********************************************************************/
uint32_t u32VppUtils_GetMbiSize(uint32_t width, uint32_t height)
{
    uint32_t MBI_size;

    MBI_size= (((width+31)>>5) *((height+31)>>5) * 256)+512;
    return MBI_size;
}

uint32_t u32VppUtils_Init(t_StVppCtx *pstCtx)
{
    t_StVppVidPropCb *pstVidProp;
    t_StVppUtilsCb *pstUtils;

    if (!pstCtx)
    {
        return VPP_ERR_PARAM;
    }

    pstVidProp = calloc(1, sizeof(t_StVppVidPropCb));
    if (!pstVidProp)
        goto ERR_VID_PROP;

    pstUtils = calloc(1, sizeof(t_StVppUtilsCb));
    if (!pstUtils)
        goto ERR_UTILS;

    pstCtx->pstVidPropCb = pstVidProp;
    pstCtx->pstUtilsCb = pstUtils;

    return VPP_OK;

ERR_UTILS:
    if (pstVidProp)
        free(pstVidProp);
ERR_VID_PROP:
    return VPP_ERR_NO_MEM;
}

uint32_t u32VppUtils_Term(t_StVppCtx *pstCtx)
{
    if (!pstCtx)
        return VPP_ERR_PARAM;

    if (pstCtx->pstVidPropCb)
        free(pstCtx->pstVidPropCb);
    if (pstCtx->pstUtilsCb)
        free(pstCtx->pstUtilsCb);

    pstCtx->pstVidPropCb = NULL;
    pstCtx->pstUtilsCb = NULL;

    return VPP_OK;
}

float fVppUtils_ScaleFloat(float old_min, float old_max,
                           float new_min, float new_max, float x)
{
    return ((new_max - new_min) * (x - old_min) / (old_max - old_min)) + new_min;
}

uint32_t u32VppUtils_CalculateStride(uint32_t u32Width,
                                     enum vpp_color_format fmt)
{
    switch (fmt)
    {
        // Intentional fallthrough
        case VPP_COLOR_FORMAT_NV21_VENUS:
            return VENUS_Y_STRIDE(COLOR_FMT_NV21, u32Width);
        case VPP_COLOR_FORMAT_NV12_VENUS:
        default:
            return VENUS_Y_STRIDE(COLOR_FMT_NV12, u32Width);
    }
}

uint32_t u32VppUtils_CalculateScanlines(uint32_t u32Height,
                                        enum vpp_color_format fmt)
{
    switch (fmt)
    {
        // Intentional fallthrough
        case VPP_COLOR_FORMAT_NV21_VENUS:
            return VENUS_Y_SCANLINES(COLOR_FMT_NV21, u32Height);
        case VPP_COLOR_FORMAT_NV12_VENUS:
        default:
            return VENUS_Y_SCANLINES(COLOR_FMT_NV12, u32Height);
    }
}
uint32_t u32VppUtils_GetStride(struct vpp_port_param *pstPort)
{
    if (!pstPort->stride || !pstPort->scanlines)
        LOGE("%s: warning: str=%u, scl=%u",
             __func__, pstPort->stride, pstPort->scanlines);

    switch (pstPort->fmt)
    {
        // intentional fallthrough
        case VPP_COLOR_FORMAT_NV12_VENUS:
        case VPP_COLOR_FORMAT_NV21_VENUS:
        default:
            return pstPort->stride;
    }
}

uint32_t u32VppUtils_GetUVOffset(struct vpp_port_param *pstPort)
{
    if (!pstPort->stride || !pstPort->scanlines)
        LOGE("%s: warning: str=%u, scl=%u",
             __func__, pstPort->stride, pstPort->scanlines);

    switch (pstPort->fmt)
    {
        case VPP_COLOR_FORMAT_NV12_VENUS:
        case VPP_COLOR_FORMAT_NV21_VENUS:
        default:
            return pstPort->stride * pstPort->scanlines;
    }
}

uint32_t u32VppUtils_GetBufferSize(struct vpp_port_param *pstPort)
{
    if (!pstPort->stride || !pstPort->scanlines)
        LOGE("%s: warning: str=%u, scl=%u",
             __func__, pstPort->stride, pstPort->scanlines);

    switch (pstPort->fmt)
    {
        case VPP_COLOR_FORMAT_NV21_VENUS:
            return VENUS_BUFFER_SIZE(COLOR_FMT_NV12,
                                     pstPort->stride,
                                     pstPort->scanlines);
        case VPP_COLOR_FORMAT_NV12_VENUS:
        default:
            return VENUS_BUFFER_SIZE(COLOR_FMT_NV21,
                                     pstPort->stride,
                                     pstPort->scanlines);
    }
}

uint32_t u32VppUtils_GetPxBufferSize(struct vpp_port_param *pstPort)
{
    uint32_t u32YStr, u32UVStr, u32YScl, u32UVScl;

    switch (pstPort->fmt)
    {
        case VPP_COLOR_FORMAT_NV12_VENUS:
        case VPP_COLOR_FORMAT_NV21_VENUS:
        default:
            u32YStr = VENUS_Y_STRIDE(COLOR_FMT_NV12, pstPort->width);
            u32UVStr = VENUS_UV_STRIDE(COLOR_FMT_NV12, pstPort->width);
            u32YScl = VENUS_Y_SCANLINES(COLOR_FMT_NV12, pstPort->height);
            u32UVScl = VENUS_UV_SCANLINES(COLOR_FMT_NV12, pstPort->height);
            return (u32YStr * u32YScl) + (u32UVStr * u32UVScl);
    }
}

uint32_t u32VppUtils_GetExtraDataBufferSize(struct vpp_port_param *pstPort)
{
    uint32_t u32Size;

    switch (pstPort->fmt)
    {
        default:
            u32Size = VENUS_EXTRADATA_SIZE(pstPort->width, pstPort->height);
    }
    return u32Size;
}

uint32_t u32VppUtils_GetTvUs(struct timeval *pstTv)
{
    return (pstTv->tv_sec * 1000000UL) + pstTv->tv_usec;
}

uint32_t u32VppUtils_GetTvDiff(struct timeval *pstTvEnd,
                               struct timeval *pstTvStart)
{
    return u32VppUtils_GetTvUs(pstTvEnd) - u32VppUtils_GetTvUs(pstTvStart);
}

uint32_t u32VppUtils_ReadPropertyU32(char *pcProp, uint32_t *pu32Val,
                                     char *pcDefault)
{
    char property_value[PROPERTY_VALUE_MAX] = {0};

    if (!pcProp || !pu32Val || !pcDefault)
        return VPP_ERR;

    property_get(pcProp, property_value, pcDefault);
    *pu32Val = strtoul(property_value, NULL, 0);

    return VPP_OK;
}

uint32_t u32VppUtils_SetVidProp(t_StVppCtx *pstCtx, struct video_property stProp)
{
    t_StVppVidPropCb *pstVidProp;

    if (!pstCtx || !pstCtx->pstVidPropCb || stProp.property_type >= VID_PROP_MAX)
    {
        return VPP_ERR_PARAM;
    }

    pstVidProp = pstCtx->pstVidPropCb;

    switch (stProp.property_type)
    {
        case VID_PROP_CODEC:
            if(stProp.codec.eCodec >= VPP_CODEC_TYPE_MAX)
                stProp.codec.eCodec = VPP_CODEC_TYPE_UNKNOWN;
            pstVidProp->codec = stProp.codec;
            break;
        default:
            break;
    }

    return VPP_OK;
}

uint32_t u32VppUtils_GetVidProp(t_StVppCtx *pstCtx, struct video_property *pstProp)
{
    t_StVppVidPropCb *pstVidProp;

    if (!pstCtx || !pstCtx->pstVidPropCb || !pstProp || pstProp->property_type >= VID_PROP_MAX)
    {
        return VPP_ERR_PARAM;
    }

    pstVidProp = pstCtx->pstVidPropCb;

    switch (pstProp->property_type)
    {
        case VID_PROP_CODEC:
            pstProp->codec = pstVidProp->codec;
            break;
        default:
            break;
    }

    return VPP_OK;
}

uint32_t u32VppUtils_SetBufDumpEnable(t_StVppCtx *pstCtx, uint32_t u32BufDumpEnable)
{
    if (!pstCtx || !pstCtx->pstUtilsCb)
    {
        return VPP_ERR_PARAM;
    }

    pstCtx->pstUtilsCb->u32BufDumpEnable = u32BufDumpEnable;

    return VPP_OK;
}

uint32_t u32VppUtils_GetBufDumpEnable(t_StVppCtx *pstCtx, uint32_t *u32BufDumpEnable)
{
    if (!pstCtx || !pstCtx->pstUtilsCb || !u32BufDumpEnable)
    {
        return VPP_ERR_PARAM;
    }

    *u32BufDumpEnable = pstCtx->pstUtilsCb->u32BufDumpEnable;

    return VPP_OK;
}

uint32_t u32VppUtils_GetVppResolution(struct vpp_port_param *pstPortParam)
{
    if (!pstPortParam)
        return VPP_RESOLUTION_MAX;

    if ((pstPortParam->height > u32ResolutionMaxHeight[VPP_RESOLUTION_UHD]) ||
        (pstPortParam->width > u32ResolutionMaxWidth[VPP_RESOLUTION_UHD]))
        return VPP_RESOLUTION_MAX;
    else if ((pstPortParam->height > u32ResolutionMaxHeight[VPP_RESOLUTION_FHD]) ||
            (pstPortParam->width > u32ResolutionMaxWidth[VPP_RESOLUTION_FHD]))
        return VPP_RESOLUTION_UHD;
    else if ((pstPortParam->height > u32ResolutionMaxHeight[VPP_RESOLUTION_HD]) ||
            (pstPortParam->width > u32ResolutionMaxWidth[VPP_RESOLUTION_HD]))
        return VPP_RESOLUTION_FHD;
    else if ((pstPortParam->height > u32ResolutionMaxHeight[VPP_RESOLUTION_SD]) ||
            (pstPortParam->width > u32ResolutionMaxWidth[VPP_RESOLUTION_SD]))
        return VPP_RESOLUTION_HD;
    else
        return VPP_RESOLUTION_SD;
}
