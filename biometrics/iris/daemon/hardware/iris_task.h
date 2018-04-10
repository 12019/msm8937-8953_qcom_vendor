/*
 * Copyright (c) 2015 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */

#ifndef __IRIS_TASK_H__
#define __IRIS_TASK_H__

#include <pthread.h>

enum iris_task_state {
	IRIS_TASK_STATE_INIT,
	IRIS_TASK_STATE_RUNNING,
	IRIS_TASK_STATE_STOP,
};

enum iris_task_event {
	IRIS_TASK_EVENT_ENROLL_PROGRESS,
	IRIS_TASK_EVENT_ENROLL_COLLECTION_DONE,
	IRIS_TASK_EVENT_ENROLL_DONE,

	IRIS_TASK_EVENT_AUTH_PROGRESS,
	IRIS_TASK_EVENT_AUTH_COLLECTION_DONE,
	IRIS_TASK_EVENT_AUTH_DONE,

	IRIS_TASK_EVENT_REMOVE_PROGRESS,
	IRIS_TASK_EVENT_REMOVE_DONE,
};

class iris_task_callback {
public:
	iris_task_callback() {};
	virtual ~iris_task_callback() {};
	virtual void on_event(int event, uint32_t data1, void *data2 = NULL) = 0;
	virtual void on_err(int err) = 0;
};

class iris_task {
public:
	iris_task();
	virtual ~iris_task();

	int start();
	void cancel();

	void set_state(iris_task_state state);
	bool is_running();

	virtual int run() = 0;

protected:
	int64_t getTimeMs();

private:
	static void* start_function(void* arg);

protected:
	volatile iris_task_state mState;
	pthread_mutex_t mMutex;
	pthread_t mThread;
	bool mThreadValid;
};

#endif

