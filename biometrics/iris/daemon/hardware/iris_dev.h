/*
 * Copyright (c) 2015-2016 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */

#ifndef IRIS_DEV_H
#define IRIS_DEV_H

#include "iris_tz_api.h"

#include <stdint.h>
#include <utils/String16.h>
#include <hardware/hw_auth_token.h>
#include <utils/RefBase.h>

#define IRIS_MODULE_API_VERSION_1_0 HARDWARE_MODULE_API_VERSION(1, 0)
#define IRIS_HARDWARE_MODULE_ID "iris"

using namespace android;

typedef enum iris_msg_type {
	IRIS_ERROR = -1,
	IRIS_ENROLLING = 1,
	IRIS_REMOVED = 2,
	IRIS_AUTHENTICATED = 3,
	IRIS_AUTHENTICATING = 4,
} iris_msg_type_t;

typedef enum iris_error {
	IRIS_ERROR_HW_UNAVAILABLE = 1,
	IRIS_ERROR_TIMEOUT = 2,
	IRIS_ERROR_CANCELED = 3,
	IRIS_ERROR_NO_SPACE = 4,
	IRIS_ERROR_NO_USER = 5,
	IRIS_ERROR_NO_IRIS = 6,
	IRIS_ERROR_GENERIC = 7,
	IRIS_ERROR_FATAL = 8,
	IRIS_ERROR_IR_SENSOR = 9,
	IRIS_ERROR_BAD_QUALITY = 10,

	IRIS_ERROR_VENDOR_BASE = 1000,
} iris_error_t;

typedef enum iris_quality {
	IRIS_QUALITY_GOOD = 0,
	IRIS_QUALITY_BAD_IMAGE_QUALITY,
	IRIS_QUALITY_EYE_NOT_FOUND,
	IRIS_QUALITY_EYE_TOO_CLOSE,
	IRIS_QUALITY_EYE_TOO_FAR,
	IRIS_QUALITY_EYE_TOO_UP,
	IRIS_QUALITY_EYE_TOO_DOWN,
	IRIS_QUALITY_EYE_TOO_LEFT,
	IRIS_QUALITY_EYE_TOO_RIGHT,
	IRIS_QUALITY_MOTION_BLUR,
	IRIS_QUALITY_FOCUS_BLUR,
	IRIS_QUALITY_BAD_EYE_OPENNESS,
	IRIS_QUALITY_BAD_EYE_DISTANCE,
	IRIS_QUALITY_WITH_GLASS,
	IRIS_QUALITY_IR_SENSOR_DONE,
} iris_quality_t;

typedef enum iris_camera_param {
	IRIS_CAMERA_PARAM_GAIN = 0,
	IRIS_CAMERA_PARAM_EXP_TIME
} iris_camera_param_t;

typedef struct iris_identifier {
	uint32_t irisId;
	uint32_t gid;
} iris_identifier_t;

typedef struct iris_enroll {
	iris_identifier_t id;
	uint32_t progress;
	iris_quality_t quality;
} iris_enroll_t;

typedef struct iris_removed {
	iris_identifier_t id;
} iris_removed_t;

typedef struct iris_authenticated {
	iris_identifier_t id;
	hw_auth_token_t *hat;
	iris_quality_t quality;
} iris_authenticated_t;

typedef struct iris_authenticating {
	uint32_t progress;
	iris_quality_t quality;
} iris_authenticating_t;

typedef struct iris_msg {
	iris_msg_type_t type;
	union {
		int error;
		iris_enroll_t enroll;
		iris_removed_t removed;
		iris_authenticated_t authenticated;
		iris_authenticating_t authenticating;
	} data;
} iris_msg_t;

/* Callback function type */
typedef void (*iris_notify_t)(const iris_msg_t *msg, void *data);

//forward declaration
class iris_frame_source;
class iris_device_callback;
class iris_interface;
class iris_task;

class iris_device {
public:
	iris_device();
	virtual ~iris_device();

	int open(bool tz_comm);
	void close();
	uint64_t pre_enroll();
	int enroll( const hw_auth_token_t *hat, uint32_t gid, uint32_t timeout_sec, int32_t uid, String16 &package);
	int post_enroll();
	uint64_t get_authenticator_id();
	int cancel();
	int enumerate(iris_identifier_t *results, uint32_t *max_size);
	int remove(iris_identifier_t id);
	int set_active_group(uint32_t gid, const char *store_path);
	int authenticate(uint64_t operation_id, uint32_t gid, int32_t uid, String16 &package);

	int set_notify(iris_notify_t notify, void *data);

	void destroy_camera();

	int set_meta_data(iris_meta_data &meta);

	int adjust_camera_parameter(iris_camera_param_t param, bool toIncrease);

private:
	bool is_busy();

	int create_camera(uint32_t cam_id, int32_t uid, String16 &package);

private:
	iris_interface *mTZIntf;
	iris_task *mTask;
	iris_device_callback *mTaskCallback;

	iris_frame_source *mFrameSource;
};

#endif  /* IRIS_DEV_H */

