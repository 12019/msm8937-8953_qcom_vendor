/*
 * Copyright (c) 2015 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */

#ifndef __IRIS_AUTH_TASK_H__
#define __IRIS_AUTH_TASK_H__

#include "iris_task.h"
#include "iris_tz_api.h"

class iris_frame_source;

struct iris_auth_result {
	uint32_t mIrisId;
	hw_auth_token_t	*mToken;
};


class iris_auth_task : public iris_task {
public:
	iris_auth_task(iris_interface &intf, iris_task_callback &cb,
				uint64_t &operationId, uint32_t gid, int timeout);
	~iris_auth_task();

	void set_frame_source(iris_frame_source *source);

	virtual int run();

private:
	int start_frame_source();
	void stop_frame_source();
	bool continue_running(int status);

private:
	iris_interface 		&mIntf;
	iris_task_callback 	&mCallback;
	uint64_t			mOperationId;
	uint32_t			mGroupId;
	int					mTimeout;
	iris_frame_source 	*mSource;
	bool				mSourceStarted;
	hw_auth_token_t		mToken;
};

#endif

