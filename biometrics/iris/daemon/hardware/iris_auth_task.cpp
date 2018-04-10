/*
 * Copyright (c) 2015 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */

#define LOG_TAG "IrisAuthTask"
#include <utils/Log.h>
#include <unistd.h>
#include <hardware/hw_auth_token.h>

#include "iris_auth_task.h"
#include "iris_frame_source.h"

#define IRIS_ENROLL_GET_FRAME_DELAY 10000 //in microseconds

iris_auth_task::iris_auth_task(iris_interface &intf, iris_task_callback &cb,
				uint64_t &operationId, uint32_t gid, int timeout)
	:mIntf(intf), mCallback(cb), mOperationId(operationId), mGroupId(gid),
	mTimeout(timeout), mSource(NULL), mSourceStarted(false)
{
}

iris_auth_task::~iris_auth_task()
{
}

void iris_auth_task::set_frame_source(iris_frame_source *source)
{
	mSource = source;
}

int iris_auth_task::start_frame_source()
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

void iris_auth_task::stop_frame_source()
{
	if (mSource && mSourceStarted) {
		mSource->stop();
		mSourceStarted = false;
	}
}

bool iris_auth_task::continue_running(int status)
{
	return (status == IRIS_STATUS_CONTINUE ||
			status == IRIS_STATUS_NO_FRAME ||
			status == IRIS_STATUS_FRAME_COLLECTION_DONE);
}

int iris_auth_task::run()
{
	int ret = 0;
	struct iris_verify_result verify_result;
	struct iris_frame buffer;
	int64_t startTimeMs, loopTimeMs;
	bool buffer_valid = false;

	startTimeMs = getTimeMs();

	ret = start_frame_source();
	if (ret) {
		mCallback.on_err(ret);
		return ret;
	}

	ret = mIntf.verify_begin(mOperationId, mGroupId);
	if (ret != IRIS_STATUS_SUCCESS) {
		stop_frame_source();
		mCallback.on_err(ret);
		return ret;
	}

	ret = IRIS_STATUS_NO_FRAME;
	while (mState == IRIS_TASK_STATE_RUNNING && continue_running(ret)) {
		loopTimeMs = getTimeMs() - startTimeMs;
		if (loopTimeMs >= mTimeout * 1000) {
			ret = -ETIME;
			ALOGE("auth time out");
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

		//continuous verity
		ret = mIntf.verify_capture(buffer_valid ? &buffer : NULL, verify_result);

		//release frame
		if (buffer_valid)
			mSource->put_frame(buffer);

		switch (ret) {
			case IRIS_STATUS_SUCCESS:
				break;
			case IRIS_STATUS_CONTINUE:
			case IRIS_STATUS_NO_FRAME:
				mCallback.on_event(IRIS_TASK_EVENT_AUTH_PROGRESS, verify_result.status);
				break;
			case IRIS_STATUS_FRAME_COLLECTION_DONE:
				stop_frame_source();
				mCallback.on_event(IRIS_TASK_EVENT_AUTH_COLLECTION_DONE, 0);
				break;
			default:
				break;
		}

	}

	if (mState != IRIS_TASK_STATE_RUNNING) {
		ret = -EINTR;
		ALOGD("auth canceled");
	}

	stop_frame_source();
	mState = IRIS_TASK_STATE_STOP;

	if (ret == IRIS_STATUS_SUCCESS && verify_result.matched)
		ret = mIntf.get_auth_token(mToken);
	mIntf.verify_end();

	if (ret != IRIS_STATUS_SUCCESS) {
		mCallback.on_err(ret);
	} else {
		struct iris_auth_result auth_result;
		auth_result.mIrisId = verify_result.iris_id;
		auth_result.mToken = &mToken;
		mCallback.on_event(IRIS_TASK_EVENT_AUTH_DONE, verify_result.matched, &auth_result);
	}

	return ret;
}


