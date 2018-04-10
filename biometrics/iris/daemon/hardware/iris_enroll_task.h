/*
 * Copyright (c) 2015 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */

#ifndef __IRIS_ENROLL_TASK_H__
#define __IRIS_ENROLL_TASK_H__

#include "iris_task.h"
#include "iris_tz_api.h"

class iris_frame_source;

class iris_enroll_task : public iris_task  {
public:
	iris_enroll_task(iris_interface &intf, iris_task_callback &cb,
				const hw_auth_token_t &token, uint32_t gid, int timeout);
	~iris_enroll_task();

	void set_frame_source(iris_frame_source *source);

	virtual int run();

private:
	int verify_token();
	int start_frame_source();
	void stop_frame_source();
	bool continue_running(int status);

private:
	iris_interface 		&mIntf;
	iris_task_callback 	&mCallback;
	hw_auth_token_t		mToken;
	uint32_t			mGroupId;
	int					mTimeout;
	iris_frame_source 	*mSource;
	bool				mSourceStarted;
};

#endif

