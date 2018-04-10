/*
 * Copyright (c) 2015 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */

#define LOG_TAG "IrisRemoveTask"
#include <utils/Log.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include "iris_remove_task.h"

iris_remove_task::iris_remove_task(iris_interface &intf, iris_task_callback &cb,
				iris_identifier_t id, int timeout)
	:mIntf(intf), mCallback(cb),  mId(id), mTimeout(timeout)
{
}

iris_remove_task::~iris_remove_task()
{
}

int iris_remove_task::run()
{
	int ret = 0;;
	int64_t startTimeMs, loopTimeMs;

	startTimeMs = getTimeMs();

	ret = IRIS_STATUS_CONTINUE;
	while (mState == IRIS_TASK_STATE_RUNNING && ret == IRIS_STATUS_CONTINUE) {
		loopTimeMs = getTimeMs() - startTimeMs;
		if (loopTimeMs >= mTimeout * 1000) {
			ret = -ETIME;
			break;
		}

		//continuous verity
		ret = mIntf.delete_enrollee(mId.irisId, mId.gid);
	}

	if (mState != IRIS_TASK_STATE_RUNNING)
		ret = -EINTR;

	mState = IRIS_TASK_STATE_STOP;

	if (ret != IRIS_STATUS_SUCCESS) {
		mCallback.on_err(ret);
	} else {
		mCallback.on_event(IRIS_TASK_EVENT_REMOVE_DONE, mId.irisId, NULL);
	}

	return ret;
}


