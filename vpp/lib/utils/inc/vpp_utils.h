/*!
 * @file vpp_utils.h
 *
 * @cr
 * Copyright (c) 2015-2016 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.

 * @services
 */
#ifndef _VPP_UTILS_H_
#define _VPP_UTILS_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <fcntl.h>
#include <cutils/properties.h>
#include "vpp.h"
#include "vpp_ctx.h"

#define VPP_FLAG_SET(reg, flag)    (reg |= (flag))
#define VPP_FLAG_CLR(reg, flag)    (reg &= ~(flag))
#define VPP_FLAG_IS_SET(reg, flag) (reg & (flag))

/* Checks whether the device uses Qualcomm chipset */
#define IS_QCOM \
({ \
    uint32_t is_qcom = 0; \
    for (;;) { \
        char device[PROPERTY_VALUE_MAX]; \
        if ((property_get("ro.board.platform", device, "") >= 3) && \
            ((strncmp(device, "msm", 3) == 0) || (strncmp(device, "qsd", 3) == 0) || \
            (strncmp(device, "apq", 3) == 0))) { \
                is_qcom = 1; \
            } \
        break; \
    } \
    is_qcom; \
})

#define IS_QCOM_8956 \
({ \
    uint32_t is_8956 = 0; \
    if(IS_QCOM) { \
        for(;;){ \
            int fd; \
            if (!access("/sys/devices/soc0/soc_id", F_OK)) { \
                fd = open("/sys/devices/soc0/soc_id", O_RDONLY); \
            } else { \
                fd = open("/sys/devices/system/soc/soc0/id", O_RDONLY); \
            } \
            if (fd != -1) { \
                char raw_buf[5]; \
                int soc; \
                read(fd, raw_buf,4); \
                raw_buf[4] = 0; \
                soc = atoi(raw_buf); \
                close(fd); \
                if (soc == 266 || soc == 274) { \
                    /* DALCHIPINFO_ID_MSM8956 = 266 */ \
                    /* DALCHIPINFO_ID_APQ8056 = 274 */ \
                    is_8956 = 1; \
                } \
            } else { \
                close(fd); \
            } \
            break; \
        } \
    } \
    is_8956; \
})

 #define IS_QCOM_8976 \
({ \
    uint32_t is_8976 = 0; \
    if(IS_QCOM) { \
        for(;;){ \
            int fd; \
            if (!access("/sys/devices/soc0/soc_id", F_OK)) { \
                fd = open("/sys/devices/soc0/soc_id", O_RDONLY); \
            } else { \
                fd = open("/sys/devices/system/soc/soc0/id", O_RDONLY); \
            } \
            if (fd != -1) { \
                char raw_buf[5]; \
                int soc; \
                read(fd, raw_buf,4); \
                raw_buf[4] = 0; \
                soc = atoi(raw_buf); \
                close(fd); \
                if (soc == 278 || soc == 277) { \
                    /* DALCHIPINFO_ID_MSM8976 = 278 */ \
                    /* DALCHIPINFO_ID_APQ8076 = 277 */ \
                    is_8976 = 1; \
                } \
            } else { \
                close(fd); \
            } \
            break; \
        } \
    } \
    is_8976; \
})

typedef struct StVppVidPropCb {
    struct vid_prop_codec codec;
} t_StVppVidPropCb;

typedef struct StVppUtilsCb {
    uint32_t u32BufDumpEnable;
} t_StVppUtilsCb;

/***************************************************************************
 * Function Prototypes
 ***************************************************************************/
uint32_t u32VppUtils_Init(t_StVppCtx *pstCtx);
uint32_t u32VppUtils_Term(t_StVppCtx *pstCtx);
float fVppUtils_ScaleFloat(float old_min, float old_max,
                           float new_min, float new_max, float x);

uint32_t u32VppUtils_CalculateStride(uint32_t u32Width,
                                     enum vpp_color_format fmt);
uint32_t u32VppUtils_CalculateScanlines(uint32_t u32Height,
                                        enum vpp_color_format fmt);
uint32_t u32VppUtils_GetStride(struct vpp_port_param *pstPort);
uint32_t u32VppUtils_GetUVOffset(struct vpp_port_param *pstPort);
uint32_t u32VppUtils_GetBufferSize(struct vpp_port_param *pstPort);
uint32_t u32VppUtils_GetPxBufferSize(struct vpp_port_param *pstPort);
uint32_t u32VppUtils_GetExtraDataBufferSize(struct vpp_port_param *pstPort);
uint32_t u32VppUtils_GetTvUs(struct timeval *tv);
uint32_t u32VppUtils_GetTvDiff(struct timeval *pstTvEnd,
                               struct timeval *pstTvStart);
uint32_t u32VppUtils_GetMbiSize(uint32_t width, uint32_t height);
uint32_t u32VppUtils_ReadPropertyU32(char *pcProp, uint32_t *pu32Val,
                                     char *pcDefault);
uint32_t u32VppUtils_SetVidProp(t_StVppCtx *pstCtx, struct video_property stProp);
uint32_t u32VppUtils_GetVidProp(t_StVppCtx *pstCtx, struct video_property *pstProp);
uint32_t u32VppUtils_SetBufDumpEnable(t_StVppCtx *pstCtx, uint32_t u32BufDumpEnable);
uint32_t u32VppUtils_GetBufDumpEnable(t_StVppCtx *pstCtx, uint32_t *u32BufDumpEnable);
uint32_t u32VppUtils_GetVppResolution(struct vpp_port_param *pstPortParam);
#ifdef __cplusplus
 }
#endif

#endif /* _VPP_UTILS_H_ */
