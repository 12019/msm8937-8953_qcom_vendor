/*
 * Copyright (c) 2015 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */

#ifndef __IRIS_FRAME_SOURCE_H__
#define __IRIS_FRAME_SOURCE_H__

#include <stdint.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <semaphore.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <camera/Camera.h>
#include <camera/CameraParameters.h>
#include <gui/Surface.h>
#include <gui/SurfaceComposerClient.h>
#include <gui/ISurfaceComposer.h>
#include <binder/ProcessState.h>
#include <linux/msm_ion.h>
#include <utils/Log.h>
#include <ui/DisplayInfo.h>
#include <system/camera.h>
#include <camera/ICamera.h>
#include <media/mediarecorder.h>
#include <utils/RefBase.h>
#include <utils/Mutex.h>
#include <utils/Condition.h>
#include <binder/IPCThreadState.h>
#include <binder/IServiceManager.h>
#include <cutils/properties.h>
#include <cutils/memory.h>
#include "iris_tz_api.h"
#include "iris_frame_source.h"
using namespace android;

class iris_frame;
class iris_frame_info;
typedef int status_t;
class iris_frame_source {
public:
	virtual status_t open(int32_t uid, String16 &package) = 0;
	virtual status_t close() = 0;
	virtual status_t start() = 0;
	virtual status_t stop() = 0;

	virtual status_t get_source_info(struct iris_frame_info &info) = 0;

	/* get video frame */
	virtual status_t get_frame(iris_frame &frame) = 0;
	/* release video frame */
	virtual void put_frame(iris_frame &frame) = 0;

	/* Camera only */
	virtual void setPreviewDisplay(const sp<Surface>& surface) = 0;

	virtual status_t set_sensor_gain(uint32_t gain) = 0;
	virtual status_t set_sensor_exposure_time(uint32_t exp_ms) = 0;
	virtual uint32_t get_sensor_gain() = 0;
	virtual uint32_t get_sensor_exposure_time() = 0;

	virtual ~iris_frame_source() {};
};


#endif

