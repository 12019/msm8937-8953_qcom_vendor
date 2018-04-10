/*
 * Copyright (c) 2015-2016 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */

#ifndef __IRIS_TZ_API_H__
#define __IRIS_TZ_API_H__

#include <stdint.h>
#include <hardware/hw_auth_token.h>

#define IRIS_MAX_ENROLLMENT 5

enum iris_status_code {
	IRIS_STATUS_SUCCESS,
	IRIS_STATUS_CONTINUE,
	IRIS_STATUS_FRAME_COLLECTION_DONE,
	IRIS_STATUS_NO_FRAME,
	IRIS_STATUS_NOT_FOUND,
	IRIS_STATUS_BAD_IMAGE_QUALITY,
	IRIS_STATUS_FAIL = 0x100,
	IRIS_STATUS_BAD_PARAMETER,
	IRIS_STATUS_NOT_ENOUGH_MEMORY,
	IRIS_STATUS_NOT_INITED,
	IRIS_STATUS_NOT_SUPPORT,
	IRIS_STATUS_OVERFLOW,
	IRIS_STATUS_FILE_IO_FAIL,
	IRIS_STATUS_MAX,
};

typedef enum {
	IRIS_FRAME_STATUS_BAD_IMAGE_QUALITY = 1 << 0,
	IRIS_FRAME_STATUS_EYE_NOT_FOUND = 1 << 1,
	IRIS_FRAME_STATUS_EYE_TOO_CLOSE = 1 << 2,
	IRIS_FRAME_STATUS_EYE_TOO_FAR = 1 << 3,
	IRIS_FRAME_STATUS_EYE_TOO_UP = 1 << 4,
	IRIS_FRAME_STATUS_EYE_TOO_DOWN = 1 << 5,
	IRIS_FRAME_STATUS_EYE_TOO_LEFT = 1 << 6,
	IRIS_FRAME_STATUS_EYE_TOO_RIGHT = 1 << 7,
	IRIS_FRAME_STATUS_MOTION_BLUR = 1 << 8,
	IRIS_FRAME_STATUS_FOCUS_BLUR = 1 << 9,
	IRIS_FRAME_STATUS_BAD_EYE_OPENNESS = 1 << 10,
	IRIS_FRAME_STATUS_BAD_EYE_DISTANCE = 1 << 11,
	IRIS_FRAME_STATUS_EXPOSURE_UNDER = 1 << 12,
	IRIS_FRAME_STATUS_EXPOSURE_OVER = 1 << 13,
	IRIS_FRAME_STATUS_WITH_GLASS = 1 << 14,
} iris_frame_status;

#define IRIS_FRAME_STATUS_EYE_POSITION 0xFE

struct iris_version {
	uint32_t version;
};

struct iris_enroll_status {
	int status;
	uint32_t progress;
};

struct iris_enroll_record {
	uint32_t user_id;
	uint64_t enrollment_date;
};

struct iris_enroll_result {
	uint32_t iris_id;
};

struct iris_verify_result {
	int status;
	uint32_t iris_id;
	bool matched;
};

struct iris_identify_result {
	uint32_t user_id;
};

typedef enum {
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
	uint32_t format;
	uint32_t sensitivity;
	uint32_t exp_time_ms;
};

struct iris_frame {
	struct iris_frame_info info;
	uint32_t frame_handle;
	uint32_t frame_data;
	uint32_t frame_len;
};

struct iris_meta_data {
	struct iris_frame_info frame_info;
	uint32_t auto_exposure;
	uint32_t orientation;
};

struct iris_enrollment_entry {
	uint32_t user_id;
	uint32_t iris_id;
};

struct iris_enrollment_list {
	uint32_t count;
	iris_enrollment_entry data[IRIS_MAX_ENROLLMENT];
};

class iris_interface {
public:
	virtual int get_version(struct iris_version &version) = 0;

	virtual int pre_enroll(uint64_t &challenge) = 0;
	virtual int post_enroll(void) = 0;

	virtual int enroll_begin(uint32_t user_id) = 0;
	virtual int enroll_capture(struct iris_frame *frame,
							struct iris_enroll_status &enroll_status) = 0;
	virtual int enroll_commit(struct iris_enroll_result &enroll_result) = 0;
	virtual int enroll_cancel(void) = 0;

	virtual int identify_begin() = 0;
	virtual int identify_capture(struct iris_frame *frame,
							struct iris_identify_result &identify_result) = 0;
	virtual int identify_end() = 0;

	virtual int verify_begin(uint64_t operationId, uint32_t user_id) = 0;
	virtual int verify_capture(struct iris_frame *frame,
							struct iris_verify_result &verify_result) = 0;
	virtual int verify_end() = 0;

	virtual int retrieve_enrollee(uint32_t user_id,
							struct iris_enroll_record &enroll_record) = 0;
	virtual int delete_enrollee(uint32_t irisId, uint32_t user_id) = 0;
	virtual int delete_all_enrollee(void) = 0;
	virtual int enumerate_enrollment(struct iris_enrollment_list &enrollment_list) = 0;

	virtual int get_authenticator_id(uint64_t &id) = 0;

	virtual int verify_token(const hw_auth_token_t &token) = 0;

	virtual int get_auth_token(hw_auth_token_t &token) = 0;

	virtual int set_meta_data(struct iris_meta_data &meta) = 0;

	virtual int test(uint32_t num_input, char **buf) = 0;
	virtual ~iris_interface() {};
};


iris_interface *create_iris_tzee_obj(bool tz_comm);

#endif

