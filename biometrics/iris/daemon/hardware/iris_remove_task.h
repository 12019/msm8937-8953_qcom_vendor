/*
 * Copyright (c) 2015 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */

#ifndef __IRIS_REMOVE_TASK_H__
#define __IRIS_REMOVE_TASK_H__

#include "iris_task.h"
#include "iris_tz_api.h"
#include "iris_dev.h"

class iris_remove_task : public iris_task  {
public:
	iris_remove_task(iris_interface &intf, iris_task_callback &cb,
					iris_identifier_t id,  int timeout);
	~iris_remove_task();

	virtual int run();

private:
	iris_interface 		&mIntf;
	iris_task_callback 	&mCallback;
	iris_identifier_t 	mId;
	int					mTimeout;
};

#endif

