/*===========================================================================
  Copyright (c) 2015-2016 Qualcomm Technologies, Inc.
  All Rights Reserved.
  Confidential and Proprietary - Qualcomm Technologies, Inc.
  ===========================================================================*/

/*
 * Iris Camera Implementation
 */

#include "common_log.h"
#include "iris_camera_source.h"
#include <cutils/native_handle.h>
#include <cutils/properties.h>
#include <sys/time.h>
#include <unistd.h>

#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "IrisCameraSource"
#define MODULE_NAME  "iriscamsource"

using namespace android;


static IrisCameraSource *g_CamSource = NULL;
/*-----------------------------------------------------------------------------
 * IrisCameraSource class
 *-----------------------------------------------------------------------------*/

 IrisCameraSource::IrisCameraSource(uint32_t camera) :
  mOpMode(0), mCameraNum(0), mCameraId(camera),
  mNumFramesReceived(0), mNumFramesProcessed(0), mNumFreeIonFrame(0), mNumActiveIonFrame(0),
  mSensorGain(DEFAULT_SENSOR_GAIN),mSensorExpTimeMs(DEFAULT_SENSOR_EXP_TIME_MS),  bStarted(false)
{
    ALOGD("create");
    g_CamSource = this;
}

IrisCameraSource::IrisCameraSource(uint32_t camera, uint32_t op_mode) :
  mOpMode(op_mode), mCameraNum(0), mCameraId(camera),
  mNumFramesReceived(0), mNumFramesProcessed(0), mNumFreeIonFrame(0), mNumActiveIonFrame(0),
  mSensorGain(DEFAULT_SENSOR_GAIN),mSensorExpTimeMs(DEFAULT_SENSOR_EXP_TIME_MS),  bStarted(false)
{
    ALOGD("create");
    g_CamSource = this;
}

 IrisCameraSource::~IrisCameraSource()
{

    ALOGD("destory");
    stop();
    close();
    g_CamSource = NULL;
}

status_t IrisCameraSource::get_source_info(struct iris_frame_info &info)
{
    info.format = IRIS_COLOR_FORMAT_Y_ONLY;
    info.width = mCurPrevSize.width;
    info.height = mCurPrevSize.height;
    info.stride = mCurPrevSize.width;

    return NO_ERROR;
}

status_t IrisCameraSource::get_supported_sizes(int32_t op_mode, Vector<Size>& sizes)
{
    return INVALID_OPERATION;
}

status_t IrisCameraSource::set_op_size(int32_t op_mode, Size size)
{
    if (op_mode != IRIS_CAMERA_OP_MODE_PREVIEW) {
        ALOGE("Only support preview op mode");
        return INVALID_OPERATION;
    }

    mCurPrevSize = size;
    mFrameLen = size.width * size.height;
    ALOGD("Set preview size %d x %d", size.width, size.height);

    return NO_ERROR;
}

status_t IrisCameraSource::set_op_mode(int32_t op_mode)
{
    status_t result = NO_ERROR;

    if (op_mode != IRIS_CAMERA_OP_MODE_PREVIEW) {
        ALOGE("Only support preview op mode");
        return INVALID_OPERATION;
    }

    mOpMode = op_mode;

    return NO_ERROR;
}

/*
 * Opens a connection to camera and registers self as a listener
 */
status_t IrisCameraSource::open(int32_t uid, String16 &package)
{
    status_t result = OK;
    int rc;
    Vector<Size> sizes;
    String16 packageName(MODULE_NAME);
    char pval[PROPERTY_VALUE_MAX];
    int property_val;

    ALOGD("open camera number %d", mCameraNum);

    mIonFd = ::open("/dev/ion", O_RDONLY);
    if (mIonFd <= 0) {
        ALOGE("Ion dev open failed %s\n", strerror(errno));
        return INVALID_OPERATION;
    }

    rc = iris_camera_open(&mCamHandle, mCameraId);
    if (rc != IRIS_CAMERA_OK) {
        ALOGE("mm_camera_lib_open() err=%d\n", rc);
        return INVALID_OPERATION;
    }

    mCameraNum = iris_camera_get_number_of_cameras(&mCamHandle);
    if ( mCameraId >= mCameraNum ) {
        ALOGE("Invalid camera index %d (total %d)!", mCameraId, mCameraNum);
        return INVALID_OPERATION;
    }

    /* set default parameters based on op_mode */
    mCurPrevSize.width = 640;
    mCurPrevSize.height = 480;

    property_get("persist.iris.camera.exposure", pval, "0");
    property_val = atoi(pval);
    if (property_val != 0) {
        mSensorExpTimeMs = property_val;
        ALOGD("Retrieve sensor exposure %dms from property", property_val);
    }

    property_get("persist.iris.camera.gain", pval, "0");
    property_val = atoi(pval);
    if (property_val != 0) {
        mSensorGain = property_val;
        ALOGD("Retrieve sensor gain %d from property", property_val);
    }

    return result;
}

status_t IrisCameraSource::add_frame(iris_camera_callback_buf_t *frame) {
    iris_camera_callback_buf_t *org_frame;
    iris_camera_callback_buf_t *new_frame = NULL;

    {
        Mutex::Autolock autoLock(mLock);

        if (((frame->exp_time_ns/1000000) != mSensorExpTimeMs) || (frame->sensitivity != mSensorGain)) {
            //exp_time and gain do not match, discard it
            ALOGD("add_frame: discard new frame: frame param is not match\n");
            release_frame(frame, false);
            return OK;
        }

        if ((mNumFramesReceived + mNumFramesProcessed) >= IRIS_CAMERA_MAX_FRAME_NUM) {
            if (mNumFramesReceived) {
                //discard the one in waiting list
                org_frame = *mFramesReceived.begin();
                mFramesReceived.erase(mFramesReceived.begin());
                mNumFramesReceived--;
                release_frame(org_frame, true);
            } else {
                //process queue is full, discard the new one
                ALOGD("add_frame: discard new frame: processing queue is full\n");
                release_frame(frame, false);
                return OK;
            }
        }

        // data structure will be reused by the camera lib, so duplicate it here
        new_frame = (iris_camera_callback_buf_t *) malloc(sizeof(*new_frame));
        *new_frame = *frame;
        mFramesReceived.push_back(new_frame);
        mNumFramesReceived++;
        ALOGD("add_frame: receive one new frame %d\n", mNumFramesReceived);
    }

    return OK;
}

void IrisCameraSource::preview_cb (iris_camera_callback_buf_t *frame)
{
    ALOGD("received one preview frame\n");

    g_CamSource->add_frame(frame);
}

status_t IrisCameraSource::set_flash_mode(bool on)
{
    cam_flash_mode_t mode = CAM_FLASH_MODE_OFF;

    if (on) {
           ALOGD("FLASH_MODE_TORCH\n");
            mode = CAM_FLASH_MODE_TORCH;
    }

    return iris_camera_send_command(&mCamHandle,
                                      IRIS_CAMERA_CMD_FLASH,
                                      &mode,
                                      NULL);
}

status_t IrisCameraSource::set_nr_mode(bool on)
{
    cam_noise_reduction_mode_t mode = CAM_NOISE_REDUCTION_MODE_OFF;

    if (on) {
           ALOGD("NR_MODE_HQ\n");
            mode = CAM_NOISE_REDUCTION_MODE_HIGH_QUALITY;
    }

    return iris_camera_send_command(&mCamHandle,
                                      IRIS_CAMERA_CMD_SET_NR_MODE,
                                      &mode,
                                      NULL);
}

status_t IrisCameraSource::set_sensor_gain(uint32_t gain)
{
    iris_camera_callback_buf_t *frame;

    if (gain == mSensorGain) {
        ALOGD("No gain update");
        return OK;
    }

    if (bStarted) {
        {
            Mutex::Autolock autoLock(mLock);

            /* flush received frame queue */
            while (mNumFramesReceived > 0) {
                frame = *mFramesReceived.begin();
                mFramesReceived.erase(mFramesReceived.begin());
                mNumFramesReceived--;
                release_frame(frame, true);
            }
        }
    }

    mSensorGain = gain;
    ALOGD("Set new sensor gain %d", gain);

    return iris_camera_send_command(&mCamHandle,
                                      IRIS_CAMERA_CMD_AEC_FORCE_GAIN,
                                      &gain,
                                      NULL);
}

status_t IrisCameraSource::set_sensor_exposure_time(uint32_t exp_ms)
{
    iris_camera_callback_buf_t *frame;

    if (exp_ms == mSensorExpTimeMs) {
        ALOGD("No exposure time update");
        return OK;
    }

    if (bStarted) {
        {
            Mutex::Autolock autoLock(mLock);

            /* flush received frame queue */
            while (mNumFramesReceived > 0) {
                frame = *mFramesReceived.begin();
                mFramesReceived.erase(mFramesReceived.begin());
                mNumFramesReceived--;
                release_frame(frame, true);
            }
        }
    }

    mSensorExpTimeMs = exp_ms;
    ALOGD("Set new sensor exposure time %dms", exp_ms);

    return iris_camera_send_command(&mCamHandle,
                                      IRIS_CAMERA_CMD_AEC_FORCE_EXP,
                                      &exp_ms,
                                      NULL);
}

/*
 * Start camera preview streaming, using RDI (Raw Dump Interface).
 * A previe surface is created before starting preview.
 */
status_t IrisCameraSource::start()
{
    status_t result = OK;
    int rc = 0, i;
    struct iris_cam_ion_buf *buf;

    ALOGD("start");

    for (i = 0; i < IRIS_CAMERA_MAX_FRAME_NUM; i++) {
        buf = allocIonBuf();
        if (!buf) {
            ALOGE("Fail to allocate ion buf");
            goto START_FAILED;
        }

        mFreeIonFrameQueue.push_back(buf);
        mNumFreeIonFrame++;
    }

    iris_camera_set_preview_resolution(&mCamHandle, mCurPrevSize.width, mCurPrevSize.height);
    iris_camera_set_preview_cb(&mCamHandle, preview_cb);
    iris_camera_send_command(&mCamHandle,
                                      IRIS_CAMERA_CMD_AEC_FORCE_EXP,
                                      &mSensorExpTimeMs,
                                      NULL);
    iris_camera_send_command(&mCamHandle,
                                      IRIS_CAMERA_CMD_AEC_FORCE_GAIN,
                                      &mSensorGain,
                                      NULL);

    ALOGD("ACTION_START_PREVIEW \n");
    rc = iris_camera_start_preview(&mCamHandle);
    if (rc != IRIS_CAMERA_OK) {
        ALOGE("iris_camera_start_stream() err=%d\n", rc);
        return INVALID_OPERATION;
    }

    ALOGD("Enable flash\n");
    set_flash_mode(true);

    bStarted = true;

    return result;

START_FAILED:
    stop();
    return INVALID_OPERATION;
}


/*
 * Stops camera preview streaming
 */
status_t IrisCameraSource::stop()
{
    int rc = 0;
    iris_camera_callback_buf_t *frame;
    struct iris_cam_ion_buf *buf;

    ALOGD("stop");

    if (bStarted) {
        {
            Mutex::Autolock autoLock(mLock);
            while (mNumFramesProcessed > 0) {
                frame = *mFramesProcessed.begin();
                mFramesProcessed.erase(mFramesProcessed.begin());
                mNumFramesProcessed--;
                release_frame(frame, true);
            }

            while (mNumFramesReceived > 0) {
                frame = *mFramesReceived.begin();
                mFramesReceived.erase(mFramesReceived.begin());
                mNumFramesReceived--;
                release_frame(frame, true);
            }

            while (mNumFreeIonFrame > 0) {
                buf = *mFreeIonFrameQueue.begin();
                mFreeIonFrameQueue.erase(mFreeIonFrameQueue.begin());
                mNumFreeIonFrame--;
                releaseIonBuf(buf);
            }

            while (mNumActiveIonFrame > 0) {
                buf = *mActiveIonFrameQueue.begin();
                mActiveIonFrameQueue.erase(mActiveIonFrameQueue.begin());
                mNumActiveIonFrame--;
                releaseIonBuf(buf);
            }
        }

        rc = iris_camera_stop_preview(&mCamHandle);
        if (rc != IRIS_CAMERA_OK) {
            ALOGE("iris_camera_stop_stream() err=%d\n", rc);
            return INVALID_OPERATION;
        }

        bStarted = false;
    }

    return OK;
}


/*
 * disconnect camera
 */
status_t IrisCameraSource::close()
{
    ALOGD("close");

    if (mIonFd > 0)
        ::close(mIonFd);

    iris_camera_close(&mCamHandle);

    return OK;
}

struct iris_cam_ion_buf *IrisCameraSource::allocIonBuf( )
{
    int rc = 0;
    struct ion_handle_data handle_data;
    struct ion_allocation_data alloc;
    struct ion_fd_data ion_info_fd;
    void *data = NULL;
    struct iris_cam_ion_buf *buf = NULL;

    buf = (struct iris_cam_ion_buf *)malloc(sizeof(*buf));
    if (!buf) {
        ALOGE("Failed to alloc cam_ion_buf");
        return NULL;
    }

    memset(&alloc, 0, sizeof(alloc));
    alloc.len = mCurPrevSize.width * mCurPrevSize.height;
    //to make it page size aligned
    alloc.len = (alloc.len + 4095U) & (~4095U);
    alloc.align = 4096;
    alloc.heap_id_mask = ION_HEAP(ION_QSECOM_HEAP_ID);
    rc = ioctl(mIonFd, ION_IOC_ALLOC, &alloc);
    if (rc < 0) {
        ALOGE("ION allocation failed %s with rc = %d \n",strerror(errno), rc);
        goto ION_ALLOC_FAILED;
    }

    memset(&ion_info_fd, 0, sizeof(ion_info_fd));
    ion_info_fd.handle = alloc.handle;
    rc = ioctl(mIonFd, ION_IOC_SHARE, &ion_info_fd);
    if (rc < 0) {
        ALOGE("ION map failed %s\n", strerror(errno));
        goto ION_MAP_FAILED;
    }

    data = mmap(NULL,
                alloc.len,
                PROT_READ  | PROT_WRITE,
                MAP_SHARED,
                ion_info_fd.fd,
                0);

    if (data == MAP_FAILED) {
        ALOGE("ION_MMAP_FAILED: %s (%d)\n", strerror(errno), errno);
        goto ION_MAP_FAILED;
    }

    buf->ion_dev_fd = mIonFd;
    buf->ion_buf_fd = ion_info_fd.fd;
    buf->ion_buf_handle = ion_info_fd.handle;
    buf->size = alloc.len;
    buf->data = data;

    return buf;

ION_MAP_FAILED:
    memset(&handle_data, 0, sizeof(handle_data));
    handle_data.handle = ion_info_fd.handle;
    ioctl(mIonFd, ION_IOC_FREE, &handle_data);
ION_ALLOC_FAILED:
    free(buf);
    ALOGE("Failed with error code %d", rc);

    return NULL;
}

int IrisCameraSource::cloneIonBuf(iris_camera_callback_buf_t *frame, struct iris_cam_ion_buf * buf)
{
    void *org_data = NULL;

    if (!buf || !frame) {
        ALOGE("No ION buffer is allocated\n");
        return -1;
    }

    org_data = mmap(NULL,
                buf->size,
                PROT_READ  | PROT_WRITE,
                MAP_SHARED,
                frame->frame->fd,
                0);
    if (org_data == MAP_FAILED) {
        ALOGE("ION_MMAP_FAILED: org buf %s (%d)\n", strerror(errno), errno);
        return -1;
    }

    memcpy(buf->data, org_data, buf->size);
    munmap(org_data, buf->size);

    return 0;
}

int IrisCameraSource::releaseIonBuf(struct iris_cam_ion_buf *buf)
{
    struct ion_handle_data handle_data;
    int rc = 0;

    if (!buf) {
        ALOGE("ION buffer is not valid\n");
        return -1;
    }

    rc = munmap(buf->data, buf->size);

    if (buf->ion_buf_fd >= 0) {
        ::close(buf->ion_buf_fd);
    }

    memset(&handle_data, 0, sizeof(handle_data));
    handle_data.handle = buf->ion_buf_handle;
    ioctl(buf->ion_dev_fd, ION_IOC_FREE, &handle_data);
    free(buf);
    ALOGD("Released one ION buf");

    return rc;
}


status_t IrisCameraSource::get_frame(iris_frame &frame) {
    iris_camera_callback_buf_t *org_frame;
    struct timeval tv_start, tv_end;
    struct iris_cam_ion_buf *buf;

    ALOGD("Enter get_frame");
    if (mFramesReceived.empty()) {
        ALOGE("No frame available");
        return NOT_ENOUGH_DATA;
    }

    if (mFreeIonFrameQueue.empty()) {
        ALOGE("No free ION buf available");
        return NOT_ENOUGH_DATA;
    }

    {
        Mutex::Autolock autoLock(mLock);

        org_frame = *mFramesReceived.begin();
        mFramesReceived.erase(mFramesReceived.begin());
        mNumFramesReceived--;

        buf = *mFreeIonFrameQueue.begin();
        mFreeIonFrameQueue.erase(mFreeIonFrameQueue.begin());
        mNumFreeIonFrame--;
    }

    ALOGD("get frame buffer fd %d\n", org_frame->frame->fd);
    if (mNumFramesProcessed >= IRIS_CAMERA_MAX_FRAME_NUM) {
        ALOGE("Warning: Too many frames are in processing\n");
    }

    gettimeofday (&tv_start, NULL);
    //copy to QSEECOM ion fd

    if (cloneIonBuf(org_frame, buf) < 0) {
        ALOGE("Failed to alloc ION buf");
        return -1;
    }
    gettimeofday (&tv_end, NULL);
    ALOGD("Clone time is %d ms", (tv_end.tv_usec - tv_start.tv_usec)/1000 + (tv_end.tv_sec - tv_start.tv_sec)*1000);

    frame.frame_handle = buf->ion_buf_fd;
    frame.frame_len = buf->size;
    frame.info.format = IRIS_COLOR_FORMAT_Y_ONLY;
    frame.info.width = mCurPrevSize.width;
    frame.info.height = mCurPrevSize.height;
    frame.info.stride = mCurPrevSize.width; //TODO: may need to align
    frame.info.sensitivity = org_frame->sensitivity;
    frame.info.exp_time_ms = org_frame->exp_time_ns/1000000;

    {
        Mutex::Autolock autoLock(mLock);
        mFramesProcessed.push_back(org_frame);
        mNumFramesProcessed++;

        mActiveIonFrameQueue.push_back(buf);
        mNumActiveIonFrame++;
    }

    ALOGD("get_frame: deliver one frame fd %d %d gain %d exp_time %dms\n",
            frame.frame_handle, mNumFramesReceived, frame.info.sensitivity, frame.info.exp_time_ms);

    return OK;
}

void IrisCameraSource::put_frame(iris_frame &frame) {
    iris_camera_callback_buf_t *org_frame;
    struct iris_cam_ion_buf *buf;

    {
        Mutex::Autolock autoLock(mLock);

        org_frame = *mFramesProcessed.begin();
        mFramesProcessed.erase(mFramesProcessed.begin());
        mNumFramesProcessed--;

        buf = *mActiveIonFrameQueue.begin();
        mActiveIonFrameQueue.erase(mActiveIonFrameQueue.begin());
        mNumActiveIonFrame--;
        mFreeIonFrameQueue.push_back(buf);
        mNumFreeIonFrame++;
    }

    if (frame.frame_handle != (uint32_t)buf->ion_buf_fd) {
        ALOGE("Released frame doesn't match with record\n");
    }

    ALOGD("put_frame: release one frame fd %d %d\n", frame.frame_handle, mNumFramesProcessed);
    release_frame(org_frame, true);
}

void IrisCameraSource::release_frame(iris_camera_callback_buf_t *frame, bool native)
{
    ALOGD("Release one frame");
    iris_camera_release_preview_buf(frame);

    //free the duplicated data structure
    if (native)
        free(frame);
}

