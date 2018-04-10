/*
 * Copyright (c) 2015 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */

#ifndef __IRIS_COMMON_H__
#define __IRIS_COMMON_H__

#ifdef WIN32
#define IRIS_API __declspec(dllexport)
#else
#define IRIS_API
#endif

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif


#ifndef TRUE
#define TRUE   1   /* Boolean true value. */
#endif

#ifndef FALSE
#define FALSE  0   /* Boolean false value. */
#endif

#ifndef NULL
#define NULL   0
#endif

typedef void* iris_handle;

/*===== enum and structures =====*/

typedef enum {
	IRIS_STATUS_SUCCESS,
	IRIS_STATUS_CONTINUE,
	IRIS_STATUS_FRAME_COLLECTION_DONE,
	IRIS_STATUS_NO_FRAME,
	IRIS_STATUS_NOT_FOUND,
	IRIS_STATUS_BAD_EYE_DISTANCE,
	IRIS_STATUS_BAD_IMAGE_QUALITY,
	IRIS_STATUS_BAD_EYE_OPENNESS,
	IRIS_STATUS_SEGMENTATION_FAIL,
	IRIS_STATUS_FAIL = 0x100,
	IRIS_STATUS_BAD_PARAMETER,
	IRIS_STATUS_NOT_ENOUGH_MEMORY,
	IRIS_STATUS_NOT_INITED,
	IRIS_STATUS_NOT_SUPPORT,
	IRIS_STATUS_OVERFLOW,
	IRIS_STATUS_FILE_IO_FAIL,
	IRIS_STATUS_MAX,
} iris_status;

typedef enum  {
	IRIS_COLOR_FORMAT_RGB888,
	IRIS_COLOR_FORMAT_YCBCR_420SP,
	IRIS_COLOR_FORMAT_YCRCB_420SP,
	IRIS_COLOR_FORMAT_Y_ONLY,
	IRIS_COLOR_FORMAT_MAX,
} iris_color_format;

struct iris_frame_info {
	uint32_t width;
	uint32_t height;
	uint32_t stride;
	uint32_t color_format;
	uint32_t sensitivity;
	uint32_t exp_time_ms;
};

struct iris_frame_buffer {
	struct iris_frame_info meta;
	uint32_t buffer_length;
	uint8_t *buffer;
};

struct iris_rect {
	int x;
	int y;
	int width;
	int height;
};

struct iris_meta_data {
	struct iris_frame_info frame_info;
	uint32_t auto_exposure;
	uint32_t orientation;
};

#ifdef __cplusplus
}
#endif

#endif

