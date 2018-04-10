/*
 * Copyright (c) 2015-2016 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */

#define LOG_TAG "IrisDev"
#include <utils/Log.h>
#include <errno.h>
#include <hardware/hw_auth_token.h>

#include "iris_dev.h"
#include "iris_task.h"
#include "iris_enroll_task.h"
#include "iris_auth_task.h"
#include "iris_remove_task.h"
#include "iris_camera_source.h"
#include "iris_tz_api.h"

static const int IRIS_AUTHENTICATE_TASK_TIME_OUT = 10;
static const int IRIS_REMOVE_TASK_TIME_OUT = 3;

class iris_device_callback : public iris_task_callback {
public:
	iris_device_callback(iris_device *device);
	virtual ~iris_device_callback();

	virtual void on_event(int event, uint32_t data1, void *data2 = NULL);
	virtual void on_err(int err);

	void set_notify(iris_notify_t notify, void *data);
	void set_groupId(uint32_t groupId);

private:
	iris_quality_t translate_frame_status(uint32_t status);
	uint32_t handle_frame_status(uint32_t status);
private:
	iris_device *mDevice;
	iris_notify_t mNotify;
	void *mData;
	uint32_t mGroupId;
};

iris_device_callback::iris_device_callback(iris_device *device)
	:mDevice(device), mNotify(NULL), mData(NULL), mGroupId(0)
{
}

iris_device_callback::~iris_device_callback()
{
}

void iris_device_callback::set_notify(iris_notify_t notify, void *data)
{
	mNotify = notify;
	mData = data;
}

void iris_device_callback::set_groupId(uint32_t groupId)
{
	mGroupId = groupId;
}

uint32_t iris_device_callback::handle_frame_status(uint32_t status)
{
	uint32_t filtered_status = status;

	if (status & IRIS_FRAME_STATUS_EXPOSURE_UNDER) {
		filtered_status &= ~IRIS_FRAME_STATUS_EXPOSURE_UNDER;
		ALOGD("Under Exposure");
		mDevice->adjust_camera_parameter(IRIS_CAMERA_PARAM_EXP_TIME, true);
	} else if (status & IRIS_FRAME_STATUS_EXPOSURE_OVER) {
		filtered_status &= ~IRIS_FRAME_STATUS_EXPOSURE_OVER;
		ALOGD("Over Exposure");
		mDevice->adjust_camera_parameter(IRIS_CAMERA_PARAM_EXP_TIME, false);
	}

	if (status & IRIS_FRAME_STATUS_FOCUS_BLUR) {
		ALOGD("filter focus blur code");
		filtered_status &= ~IRIS_FRAME_STATUS_FOCUS_BLUR;
	}

	if (status & IRIS_FRAME_STATUS_BAD_EYE_DISTANCE) {
		ALOGD("filter bad eye distance code");
		filtered_status &= ~IRIS_FRAME_STATUS_BAD_EYE_DISTANCE;
	}

	ALOGE("frame status=%x, filtered status=%x", status, filtered_status);
	return filtered_status;
}

iris_quality_t iris_device_callback::translate_frame_status(uint32_t status)
{
	iris_quality_t quality;

	if (status & IRIS_FRAME_STATUS_EYE_POSITION) {
		uint32_t eye_position_code  = status & IRIS_FRAME_STATUS_EYE_POSITION;
		switch (eye_position_code) {
			case IRIS_FRAME_STATUS_EYE_NOT_FOUND:
				quality = IRIS_QUALITY_EYE_NOT_FOUND;
				break;
			case IRIS_FRAME_STATUS_EYE_TOO_CLOSE:
				quality = IRIS_QUALITY_EYE_TOO_CLOSE;
				break;
			case IRIS_FRAME_STATUS_EYE_TOO_FAR:
				quality = IRIS_QUALITY_EYE_TOO_FAR;
				break;
			case IRIS_FRAME_STATUS_EYE_TOO_UP:
				quality = IRIS_QUALITY_EYE_TOO_UP;
				break;
			case IRIS_FRAME_STATUS_EYE_TOO_DOWN:
				quality = IRIS_QUALITY_EYE_TOO_DOWN;
				break;
			case IRIS_FRAME_STATUS_EYE_TOO_LEFT:
				quality = IRIS_QUALITY_EYE_TOO_LEFT;
				break;
			case IRIS_FRAME_STATUS_EYE_TOO_RIGHT:
				quality = IRIS_QUALITY_EYE_TOO_RIGHT;
				break;
			default:
				quality = IRIS_QUALITY_BAD_IMAGE_QUALITY;
		}
		return quality;
	}

	if (status & IRIS_FRAME_STATUS_BAD_EYE_OPENNESS)
		return IRIS_QUALITY_BAD_EYE_OPENNESS;

	if (status & IRIS_FRAME_STATUS_MOTION_BLUR)
		return IRIS_QUALITY_MOTION_BLUR;

	if (status & IRIS_FRAME_STATUS_WITH_GLASS)
		return IRIS_QUALITY_WITH_GLASS;

	if (status & IRIS_FRAME_STATUS_BAD_IMAGE_QUALITY)
		return IRIS_QUALITY_BAD_IMAGE_QUALITY;

	return IRIS_QUALITY_GOOD;
}

void iris_device_callback::on_event(int event, uint32_t data1, void *data2)
{
	iris_msg_t msg;
	bool ignore = false;
	struct iris_auth_result *result;
	uint32_t filtered_status;

	if (!mNotify)
		return;

	switch (event) {
		case IRIS_TASK_EVENT_ENROLL_PROGRESS:
			msg.type = IRIS_ENROLLING;
			msg.data.enroll.id.irisId = 0;
			msg.data.enroll.id.gid = mGroupId;
			msg.data.enroll.progress = data1;
			filtered_status = handle_frame_status((uint32_t)data2);
			msg.data.enroll.quality = translate_frame_status(filtered_status);
			break;

		case IRIS_TASK_EVENT_ENROLL_COLLECTION_DONE:
			msg.type = IRIS_ENROLLING;
			msg.data.enroll.id.irisId = 0;
			msg.data.enroll.id.gid = mGroupId;
			msg.data.enroll.progress = data1;
			msg.data.enroll.quality = IRIS_QUALITY_IR_SENSOR_DONE;
			break;

		case IRIS_TASK_EVENT_ENROLL_DONE:
			msg.type = IRIS_ENROLLING;
			msg.data.enroll.id.irisId = data1;
			msg.data.enroll.id.gid = mGroupId;
			msg.data.enroll.progress = 100;
			msg.data.enroll.quality = IRIS_QUALITY_GOOD;
			mDevice->destroy_camera();
			break;

		case IRIS_TASK_EVENT_AUTH_PROGRESS:
			msg.type = IRIS_AUTHENTICATING;
			filtered_status = handle_frame_status(data1);
			msg.data.authenticating.quality = translate_frame_status(filtered_status);
			break;

		case IRIS_TASK_EVENT_AUTH_COLLECTION_DONE:
			msg.type = IRIS_AUTHENTICATING;
			msg.data.authenticating.quality = IRIS_QUALITY_IR_SENSOR_DONE;
			break;

		case IRIS_TASK_EVENT_AUTH_DONE:
			result = (struct iris_auth_result *)data2;
			msg.type = IRIS_AUTHENTICATED;
			if (data1) {
				msg.data.authenticated.id.irisId = result->mIrisId;
				msg.data.authenticated.id.gid = mGroupId;
				msg.data.authenticated.hat = (hw_auth_token_t *)result->mToken;
			} else {
				msg.data.authenticated.id.irisId = 0;
				msg.data.authenticated.id.gid = 0;
				msg.data.authenticated.hat = NULL;
			}
			msg.data.authenticated.quality = IRIS_QUALITY_GOOD;
			mDevice->destroy_camera();
			break;

		case IRIS_TASK_EVENT_REMOVE_PROGRESS:
			ignore = true;
			break;

		case IRIS_TASK_EVENT_REMOVE_DONE:
			msg.type = IRIS_REMOVED;
			msg.data.removed.id.irisId = data1;
			msg.data.removed.id.gid = mGroupId;
			break;

		default:
			ALOGW("Invalid event %d", event);
			ignore = true;
			break;
	}

	if (!ignore)
		mNotify(&msg, mData);
}

void iris_device_callback::on_err(int err)
{
	iris_msg_t msg;

	ALOGE("Iris device error=%d\n", err);

	if (!mNotify)
		return;

	msg.type = IRIS_ERROR;
	msg.data.error = IRIS_ERROR_GENERIC;

	if (err < 0) {
		switch (err) {
			case -ETIME:
				msg.data.error = IRIS_ERROR_TIMEOUT;
				break;
			case -EINTR:
				msg.data.error = IRIS_ERROR_CANCELED;
				break;
			case -EIO:
				msg.data.error = IRIS_ERROR_IR_SENSOR;
				break;
			default:
				msg.data.error = IRIS_ERROR_FATAL;
		}
	} else {
		switch (err) {
			case IRIS_STATUS_CONTINUE:
			case IRIS_STATUS_FRAME_COLLECTION_DONE:
			case IRIS_STATUS_NO_FRAME:
				msg.data.error = IRIS_ERROR_BAD_QUALITY;
				break;
			case IRIS_STATUS_NOT_FOUND:
				msg.data.error = IRIS_ERROR_NO_USER;
				break;
			case IRIS_STATUS_BAD_IMAGE_QUALITY:
				msg.data.error = IRIS_ERROR_BAD_QUALITY;
				break;
			case IRIS_STATUS_FAIL:
			case IRIS_STATUS_BAD_PARAMETER:
			case IRIS_STATUS_NOT_ENOUGH_MEMORY:
			case IRIS_STATUS_NOT_INITED:
			case IRIS_STATUS_NOT_SUPPORT:
			case IRIS_STATUS_OVERFLOW:
			case IRIS_STATUS_FILE_IO_FAIL:
				msg.data.error = IRIS_ERROR_GENERIC;
				break;
			default:
				msg.data.error = err;
		}
	}
	mNotify(&msg, mData);
	mDevice->destroy_camera();
}

/* iris_device */
iris_device::iris_device()
	:mTZIntf(NULL), mTask(NULL), mTaskCallback(NULL), mFrameSource(NULL)
{
}


iris_device::~iris_device()
{
	close();
}

int iris_device::open(bool tz_comm)
{
	mTZIntf = create_iris_tzee_obj(tz_comm);
	if (!mTZIntf) {
		ALOGE("Iris device open fail to create tzee object");
		return -ENOMEM;
	}

	mTaskCallback = new iris_device_callback(this);
	if (!mTaskCallback) {
		delete mTZIntf;
		mTZIntf = NULL;
		ALOGE("Iris device open fail to create iris_device_callback object");
		return -ENOMEM;
	}

	return 0;
}

void iris_device::close()
{
	if (mTask) {
		mTask->cancel();
		delete mTask;
		mTask = NULL;
	}

	if (mTZIntf) {
		delete mTZIntf;
		mTZIntf = NULL;
	}

	if (mTaskCallback) {
		delete mTaskCallback;
		mTaskCallback = NULL;
	}

	if (mFrameSource) {
		delete mFrameSource;
		mFrameSource = NULL;
	}
}

int iris_device::set_notify(iris_notify_t notify, void *data)
{
	if (!mTaskCallback)
		return -1;

	mTaskCallback->set_notify(notify, data);
	return 0;
}

uint64_t iris_device::pre_enroll()
{
	uint64_t challenge = 0;

	if (!mTZIntf) {
		ALOGE("Iris device not ready");
		return -ENODEV;
	}

	if (is_busy()) {
		ALOGE("Iris device is busy");
	}
	cancel();

	mTZIntf->pre_enroll(challenge);
	return challenge;
}

int iris_device::enroll(const hw_auth_token_t *hat,  uint32_t gid, uint32_t timeout_sec,
	int32_t uid, String16 &package)
{
	int ret = 0;
	iris_enroll_task *enroll_task = NULL;

	if (!hat) {
		ALOGE("invalid auth token");
		return -EINVAL;
	}

	if (!mTZIntf) {
		ALOGE("Iris device not ready");
		return -ENODEV;
	}

	if (is_busy()) {
		ALOGE("Iris device is busy");
	}
	cancel();

	ret = create_camera(0, uid, package);
	if ( ret < 0) {
		ALOGE("Create iris camera failed");
		return ret;
	}

	mTaskCallback->set_groupId(gid);
	enroll_task = new iris_enroll_task(*mTZIntf, *mTaskCallback, *hat, gid, timeout_sec);
	if (!enroll_task) {
		ALOGE("Create enroll task failed");
		delete mFrameSource;
		mFrameSource = NULL;
		return -ENOMEM;
	}

	enroll_task->set_frame_source(mFrameSource);
	mTask = enroll_task;
	return mTask->start();
}

int iris_device::post_enroll()
{
	int ret = 0;

	if (!mTZIntf) {
		ALOGE("Iris device not ready");
		return -ENODEV;
	}

	if (is_busy()) {
		ALOGE("Iris post enroll device is busy");
	}
	cancel();

	ret = mTZIntf->post_enroll();

	return ret;
}

uint64_t iris_device::get_authenticator_id()
{
	uint64_t id = 0;

	if (!mTZIntf) {
		ALOGE("Iris device not ready");
		return id;
	}

	if (is_busy()) {
		ALOGE("Iris device is busy");
	}
	cancel();

	mTZIntf->get_authenticator_id(id);
	return id;
}

int iris_device::cancel()
{
	if (mTask) {
		mTask->cancel();
		delete mTask;
		mTask = NULL;
	}

	destroy_camera();
	return 0;
}

int iris_device::enumerate(iris_identifier_t *results,uint32_t *max_size)
{
	int ret = 0;
	uint32_t i, count;
	struct iris_enrollment_list enrollment_list;

	if (!mTZIntf) {
		ALOGE("Iris device not ready");
		return -ENODEV;
	}

	if (is_busy()) {
		ALOGE("Iris device is busy");
	}
	cancel();

	ret =  mTZIntf->enumerate_enrollment(enrollment_list);
	if (ret) {
		ALOGE("fail to enumerate ret=%d", ret);
		return ret;
	}

	count = *max_size;
	if (enrollment_list.count < count)
		count = enrollment_list.count;

	*max_size = count;
	for (i = 0; i < count; i++) {
		results[i].gid = enrollment_list.data[i].user_id;
		results[i].irisId = enrollment_list.data[i].iris_id;
	}

	return ret;
}

int iris_device::remove(iris_identifier_t id)
{
	iris_remove_task *remove_task = NULL;

	if (!mTZIntf) {
		ALOGE("Iris device not ready");
		return -ENODEV;
	}

	if (is_busy()) {
		ALOGE("Iris device is busy");
	}

	cancel();

	mTaskCallback->set_groupId(id.gid);
	remove_task = new iris_remove_task(*mTZIntf, *mTaskCallback,
						id, IRIS_REMOVE_TASK_TIME_OUT);
	if (!remove_task) {
		ALOGE("Create remove task failed");
		return -ENOMEM;
	}

	mTask = remove_task;
	return mTask->start();
}

int iris_device::set_active_group(uint32_t gid, const char *store_path)
{
	if (!mTZIntf) {
		ALOGE("Iris device not ready");
		return -ENODEV;
	}

	if (is_busy()) {
		ALOGE("Iris device is busy");
	}

	cancel();

	return -EINVAL;
}

int iris_device::authenticate(uint64_t operation_id, uint32_t gid,
	int32_t uid, String16 &package)
{
	int ret = 0;
	iris_auth_task *auth_task = NULL;

	if (!mTZIntf) {
		ALOGE("Iris device not ready");
		return -ENODEV;
	}

	if (is_busy()) {
		ALOGE("Iris device is busy");
	}

	cancel();

	ret = create_camera(0, uid, package);
	if ( ret < 0) {
		ALOGE("Create iris camera failed");
		return ret;
	}

	mTaskCallback->set_groupId(gid);
	auth_task = new iris_auth_task(*mTZIntf, *mTaskCallback,
					operation_id, gid, IRIS_AUTHENTICATE_TASK_TIME_OUT);
	if (!auth_task) {
		ALOGE("Create enroll task failed");
		delete mFrameSource;
		mFrameSource = NULL;
		return -ENOMEM;
	}
	
	auth_task->set_frame_source(mFrameSource);
	mTask = auth_task;
	return mTask->start();
}

bool iris_device::is_busy()
{
	if (mTask && mTask->is_running())
		return true;

	return false;
}

int iris_device::create_camera(uint32_t cam_id, int32_t uid, String16 &package)
{
	IrisCameraSource  *camera = NULL;
	Size size = {1920, 1920};

	camera = new IrisCameraSource(cam_id);
	if (camera == NULL) {
		ALOGE("Camera init failed\n");
		return -ENOMEM;
	}

    // Start camera data flow
    if (camera->open(uid, package) != OK) {
		ALOGE("Cannot open camera!");
		delete camera;
		return  -ENOMEM;
	}

	camera->set_op_mode(IRIS_CAMERA_OP_MODE_PREVIEW);
	camera->set_op_size(IRIS_CAMERA_OP_MODE_PREVIEW, size);
	mFrameSource = camera;
	return 0;
}

void iris_device::destroy_camera()
{
    ALOGD("End camera test\n");
	if (mFrameSource) {
		delete mFrameSource;
		mFrameSource = NULL;
	}
}

int iris_device::adjust_camera_parameter(iris_camera_param_t param, bool toIncrease)
{
    uint32_t val = 0;

    ALOGD("Adjust camera param %d", param);

    switch(param) {
    case IRIS_CAMERA_PARAM_GAIN:
        val = mFrameSource->get_sensor_gain();
        if (toIncrease) {
            val++;
        } else {
            if (val > 1)
                val--;
        }
        mFrameSource->set_sensor_gain(val);
        break;
    case IRIS_CAMERA_PARAM_EXP_TIME:
        val = mFrameSource->get_sensor_exposure_time();
        if (toIncrease) {
            val++;
        } else {
            if (val > 1)
                val--;
        }
        mFrameSource->set_sensor_exposure_time(val);
        break;
    default:
        ALOGE("Invalid camera parameter");
        return -EINVAL;
    }

    return 0;
}

int iris_device::set_meta_data(iris_meta_data &meta)
{
	int ret = 0;

	if (is_busy()) {
		ALOGE("Iris device is busy");
		return -EBUSY;
	}

	if (!mTZIntf) {
		ALOGE("Iris device not ready");
		return -ENODEV;
	}

	ret = mTZIntf->set_meta_data(meta);
	return ret;
}


