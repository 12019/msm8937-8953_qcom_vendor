/*
 * Copyright (c) 2015 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */

#define LOG_TAG "IrisEnrollTask"
#include <utils/Log.h>
#include <unistd.h>
#include <hardware/hw_auth_token.h>

#include "iris_enroll_task.h"
#include "iris_frame_source.h"

#define IRIS_ENROLL_GET_FRAME_DELAY 10000 //in microseconds

iris_enroll_task::iris_enroll_task(iris_interface &intf, iris_task_callback &cb,
				const hw_auth_token_t &token, uint32_t gid, int timeout)
	:mIntf(intf), mCallback(cb), mToken(token),mGroupId(gid),
	mTimeout(timeout), mSource(NULL), mSourceStarted(false)
{
}

iris_enroll_task::~iris_enroll_task()
{
}

void iris_enroll_task::set_frame_source(iris_frame_source *source)
{
	mSource = source;
}

int iris_enroll_task::start_frame_source()
{
	int ret = 0;

	if (mSource && !mSourceStarted) {
		ret = mSource->start();
		if (!ret)
			mSourceStarted = true;
	}
	if (ret)
		return -EIO;
	else
		return ret;
}

void iris_enroll_task::stop_frame_source()
{
	if (mSource && mSourceStarted) {
		mSource->stop();
		mSourceStarted = false;
	}
}

int iris_enroll_task::verify_token()
{
	int ret;
	ret = mIntf.verify_token(mToken);
	return ret;
}

bool iris_enroll_task::continue_running(int status)
{
	return (status == IRIS_STATUS_CONTINUE ||
			status == IRIS_STATUS_NO_FRAME ||
			status == IRIS_STATUS_FRAME_COLLECTION_DONE);
}


int iris_enroll_task::run()
{
	int ret = 0;
	struct iris_enroll_status enroll_status;
	struct iris_enroll_result enroll_result;
	struct iris_frame buffer;
	bool buffer_valid = false;
	int64_t startTimeMs, loopTimeMs;

	startTimeMs = getTimeMs();

	ret = verify_token();
	if (ret) {
		ALOGE("fail to verify token for enroll ret=%d", ret);
		mCallback.on_err(ret);
		return ret;
	}

	ret = start_frame_source();
	if (ret) {
		mCallback.on_err(ret);
		return ret;
	}

	ret = mIntf.enroll_begin(mGroupId);
	if (ret != IRIS_STATUS_SUCCESS) {
		stop_frame_source();
		mCallback.on_err(ret);
		ALOGE("enroll_begin fail ret=%d", ret);
		return ret;
	}

	ret = IRIS_STATUS_NO_FRAME;
	while (mState == IRIS_TASK_STATE_RUNNING && continue_running(ret)) {
		loopTimeMs = getTimeMs() - startTimeMs;
		if (loopTimeMs >= mTimeout * 1000) {
			ret = -ETIME;
			break;
		}

		//get frame
		buffer_valid = false;
		if (mSourceStarted && ret == IRIS_STATUS_NO_FRAME) {
			if (mSource->get_frame(buffer)) {
				usleep(IRIS_ENROLL_GET_FRAME_DELAY);
				continue;
			} else {
				buffer_valid = true;
			}
		}

		//continuous enroll
		ret = mIntf.enroll_capture(buffer_valid ? &buffer : NULL, enroll_status);
		ALOGD("enroll_capture ret=%d, status=%d, progress=%d", ret, enroll_status.status, enroll_status.progress);

		//release frame
		if (buffer_valid)
			mSource->put_frame(buffer);

		switch (ret) {
			case IRIS_STATUS_SUCCESS:
				break;
			case IRIS_STATUS_CONTINUE:
			case IRIS_STATUS_NO_FRAME:
				mCallback.on_event(IRIS_TASK_EVENT_ENROLL_PROGRESS, enroll_status.progress, (void *)enroll_status.status);
				break;
			case IRIS_STATUS_FRAME_COLLECTION_DONE:
				stop_frame_source();
				mCallback.on_event(IRIS_TASK_EVENT_ENROLL_COLLECTION_DONE, enroll_status.progress, NULL);
				break;
			default:
				break;
		}
	}

	if (mState != IRIS_TASK_STATE_RUNNING)
		ret = -EINTR;

	if (ret == IRIS_STATUS_SUCCESS)
		ret = mIntf.enroll_commit(enroll_result);

	stop_frame_source();
	mState = IRIS_TASK_STATE_STOP;

	if (ret != IRIS_STATUS_SUCCESS) {
		mIntf.enroll_cancel();
		mCallback.on_err(ret);
		ALOGE("enroll fail ret=%d", ret);
	} else {
		mCallback.on_event(IRIS_TASK_EVENT_ENROLL_DONE, enroll_result.iris_id);
	}
	return ret;
}


