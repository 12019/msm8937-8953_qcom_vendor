/*
 * Copyright (c) 2015 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */

#define LOG_TAG "IrisTask"
#include <utils/Log.h>
#include <time.h>
#include "iris_task.h"

void* iris_start_function(void *arg)
{
	iris_task* task = static_cast<iris_task *>(arg);
	task->run();
	task->set_state(IRIS_TASK_STATE_STOP);
	return 0;
}

iris_task::iris_task()
{
	pthread_mutexattr_t attr;
	pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
	pthread_mutex_init(&mMutex, &attr);

	mState = IRIS_TASK_STATE_INIT;
	mThreadValid = false;
}

iris_task::~iris_task()
{
	cancel();

	pthread_mutex_destroy(&mMutex);
}

int iris_task::start()
{
	int ret;

	pthread_mutex_lock(&mMutex);

	ret = pthread_create(&mThread, NULL, iris_start_function, this);
	if (!ret) {
		mState = IRIS_TASK_STATE_RUNNING;
		mThreadValid = true;
	}

	pthread_mutex_unlock(&mMutex);

	return ret;
}

void iris_task::cancel()
{
	pthread_mutex_lock(&mMutex);
	mState = IRIS_TASK_STATE_STOP;
	if (mThreadValid) {
		pthread_join(mThread, 0);
		mThreadValid = false;
	}
	pthread_mutex_unlock(&mMutex);
}

int64_t iris_task::getTimeMs()
{
	timespec ts;
	clock_gettime(CLOCK_REALTIME, &ts);
	return 1000LL * ts.tv_sec + ts.tv_nsec / 1000000;
}

void iris_task::set_state(iris_task_state state)
{
	mState = state;
}

bool iris_task::is_running()
{
	return (mState == IRIS_TASK_STATE_RUNNING);
}

