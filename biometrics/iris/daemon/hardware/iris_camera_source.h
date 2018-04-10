/*===========================================================================
  Copyright (c) 2015-2016 Qualcomm Technologies, Inc.
  All Rights Reserved.
  Confidential and Proprietary - Qualcomm Technologies, Inc.
  ===========================================================================*/

/*
 * Part of Secure Camera HLOS listener
 */
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <semaphore.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <linux/msm_ion.h>
#include <utils/Log.h>
#include <utils/RefBase.h>
#include <utils/Mutex.h>
#include <utils/Condition.h>
#include <cutils/properties.h>
#include <cutils/memory.h>
#include "iris_tz_api.h"
#include "iris_frame_source.h"
#include "iris_camera.h"

using namespace android;

/**
 * Iris Camera OP mode
 */
#define IRIS_CAMERA_OP_MODE_PREVIEW     (1 << 0)
#define IRIS_CAMERA_OP_MODE_VIDEO       (1 << 1)
#define IRIS_CAMERA_OP_MODE_RDI         (1 << 2)
#define IRIS_CAMERA_OP_MODE_SECURE      (1 << 3)
#define IRIS_CAMERA_OP_MODE_SNAPSHOT      (1 << 4)

#define IRIS_CAMERA_MAX_FRAME_NUM   2

struct iris_cam_ion_buf {
    int ion_dev_fd;
    int ion_buf_fd;
    ion_user_handle_t ion_buf_handle;
    size_t size;
    void *data;
};

/**
 * IrisCamera
 * A common class for IR and RGB cameras
 */
class IrisCameraSource : public iris_frame_source {
public:
    IrisCameraSource(uint32_t camera);
    IrisCameraSource(uint32_t camera, uint32_t op_mode);
    virtual ~IrisCameraSource();

    status_t open(int32_t uid, String16 &package);
    status_t close();
    status_t start();
    status_t stop();

    /* retrive all supported resolutions for different mode */
    status_t get_supported_sizes(int32_t op_mode, Vector<Size>& sizes);

    /* set resolution for preview or video op mode */
    status_t set_op_size(int32_t op_mode, Size size);

    /* set op mode */
    status_t set_op_mode(int32_t op_mode);

    status_t get_source_info(struct iris_frame_info &info);

    /* add frame from callback */
    status_t add_frame (iris_camera_callback_buf_t *frame);

    /* get video frame */
    status_t get_frame(iris_frame &frame);

    /* release video frame */
    void put_frame(iris_frame &frame);

    inline void setPreviewDisplay(const sp<Surface>& surface) {
    }

    status_t set_sensor_gain(uint32_t gain);
    status_t set_sensor_exposure_time(uint32_t exp_ms);
    uint32_t get_sensor_gain() { return mSensorGain; }
    uint32_t get_sensor_exposure_time() { return mSensorExpTimeMs; }
    status_t set_nr_mode(bool on);
    status_t set_flash_mode(bool on);

    static void preview_cb (iris_camera_callback_buf_t *frame);

private:
    void release_frame(iris_camera_callback_buf_t *frame, bool native = false);
    status_t cloneIonBuf(iris_camera_callback_buf_t *frame, struct iris_cam_ion_buf * buf);
    status_t releaseIonBuf(struct iris_cam_ion_buf *buf);
    struct iris_cam_ion_buf * allocIonBuf();

private:
    Mutex mLock;
    uint32_t mOpMode;
    uint32_t mCameraNum;      /** Number of cameras available */
    uint32_t mCameraId;      /** camera id */
    Size mCurPrevSize;
    size_t mFrameLen;         /** Number of bytes in frame */
    List<iris_camera_callback_buf_t *> mFramesReceived;
    uint32_t mNumFramesReceived;
    List<iris_camera_callback_buf_t *> mFramesProcessed;
    uint32_t mNumFramesProcessed;
    List<struct iris_cam_ion_buf *> mFreeIonFrameQueue;
    uint32_t mNumFreeIonFrame;
    List<struct iris_cam_ion_buf *> mActiveIonFrameQueue;
    uint32_t mNumActiveIonFrame;

    iris_camera_handle mCamHandle;
    int mIonFd;

    /* sensor settings */
    uint32_t mSensorGain;
    uint32_t mSensorExpTimeMs;

    bool bStarted;
};

