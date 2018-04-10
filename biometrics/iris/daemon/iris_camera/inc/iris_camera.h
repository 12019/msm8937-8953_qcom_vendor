/*
 * Copyright (c) 2015-2016 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */

#ifndef __IRIS_CAMERA_H__
#define __IRIS_CAMERA_H__

#include <pthread.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <poll.h>
#include <stdbool.h>
#include <linux/fb.h>
#include <linux/msm_mdp.h>
#include <semaphore.h>
#include <pthread.h>
#include "mm_camera_interface.h"

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

//#define IRIS_CAMERA_HAL_V1

#ifdef IRIS_CAMERA_DEBUG
#define IRIS_CAMERA_LOG(fmt, args...) ALOGE("%s:%d "fmt"\n", __func__, __LINE__, ##args)
#else
#define IRIS_CAMERA_LOG(fmt, args...) do{}while(0)
#endif

#define PREVIEW_BUF_NUM 7

#define DEFAULT_PREVIEW_FORMAT    CAM_FORMAT_YUV_420_NV21
#define DEFAULT_PREVIEW_WIDTH     800
#define DEFAULT_PREVIEW_HEIGHT    480
#define DEFAULT_PREVIEW_PADDING   CAM_PAD_TO_WORD
#define DEFAULT_SNAPSHOT_FORMAT   CAM_FORMAT_YUV_420_NV21
#define DEFAULT_SNAPSHOT_WIDTH    1280
#define DEFAULT_SNAPSHOT_HEIGHT   960
#define DEFAULT_SNAPSHOT_PADDING  CAM_PAD_TO_WORD
#define DEFAULT_SENSOR_EXP_TIME_MS  14
#define DEFAULT_SENSOR_GAIN                 50

typedef struct {
    void *user_data;
    uint32_t camera_handle;
    uint32_t ch_id;
    uint32_t buf_frame_num;
    uint32_t sensitivity;
    uint64_t exp_time_ns;
    mm_camera_buf_def_t *frame;
} iris_camera_callback_buf_t;

typedef void (*cam_stream_user_cb) (iris_camera_callback_buf_t *cb_frame);

typedef enum {
    IRIS_CAMERA_OK,
    IRIS_CAMERA_E_GENERAL,
    IRIS_CAMERA_E_NO_MEMORY,
    IRIS_CAMERA_E_NOT_SUPPORTED,
    IRIS_CAMERA_E_INVALID_INPUT,
    IRIS_CAMERA_E_INVALID_OPERATION, /* 5 */
} iris_camera_status_type_t;

typedef enum {
    IRIS_CHANNEL_TYPE_PREVIEW,  /* preview only */
    IRIS_CHANNEL_TYPE_SNAPSHOT, /* snapshot main only */
    IRIS_CHANNEL_TYPE_VIDEO,    /* video only */
    IRIS_CHANNEL_TYPE_RDI,      /* rdi only */
    IRIS_CHANNEL_TYPE_MAX
} iris_camera_channel_type_t;

typedef enum {
    IRIS_BUF_STATE_FREE,
    IRIS_BUF_STATE_USED
} iris_buf_state_t;

typedef struct {
    int                     fd;
    int                     main_ion_fd;
    ion_user_handle_t       handle;
    size_t                  size;
    parm_buffer_t          *data;
} iris_camera_meminfo_t;

typedef struct {
    mm_camera_buf_def_t buf;
    iris_camera_meminfo_t mem_info;
    iris_buf_state_t buf_state;
    uint32_t buf_frame_num;
    uint32_t sensitivity;
    uint64_t exp_time_ns;
} iris_camera_buf_t;

typedef struct {
    uint32_t s_id;
    mm_camera_stream_config_t s_config;
    cam_frame_len_offset_t offset;
    uint8_t num_of_bufs;
    uint32_t multipleOf;
    iris_camera_buf_t s_bufs[MM_CAMERA_MAX_NUM_FRAMES];
    mm_camera_buf_def_t *s_mm_bufs;
    uint32_t next_frame_num;
    iris_camera_buf_t s_info_buf;
    cam_stream_info_t *s_info;
} iris_camera_stream_t;

typedef struct {
    uint32_t ch_id;
    uint8_t num_streams;
    iris_camera_stream_t streams[MAX_STREAM_NUM_IN_BUNDLE];
} iris_camera_channel_t;

typedef void (*release_data_fn)(void* data, void *user_data);

typedef struct {
    mm_camera_vtbl_t *cam;
    uint8_t num_channels;
    iris_camera_channel_t channels[IRIS_CHANNEL_TYPE_MAX];
    iris_camera_buf_t cap_buf;
    iris_camera_buf_t parm_buf;

    mm_camera_super_buf_t* current_job_frames;
    uint32_t current_job_id;

    uint32_t buffer_width, buffer_height;
    uint32_t buffer_size;
    cam_format_t buffer_format;
    uint32_t frame_size;
    uint32_t frame_count;
    bool focus_supported;
    bool need_release_preview_buf;
    cam_stream_user_cb user_preview_cb;
    cam_stream_user_cb user_metadata_cb;
    parm_buffer_t *params_buffer;
    metadata_buffer_t *metadata;

    uint16_t preview_width;
    uint16_t preview_height;
    cam_focus_mode_type af_mode;
    cam_ae_mode_type ae_mode;
    cam_noise_reduction_mode_t nr_mode;
    uint32_t sensitivity;
    uint64_t exp_time_ns;
    bool frame_param_update;

    pthread_t streaming_thread_id;
    bool stop_thread;

    pthread_mutex_t ctx_mutex;
} iris_camera_ctx_t;

typedef struct {
  void *ptr;

  uint8_t (*get_num_of_cameras) ();
  int32_t (*mm_camera_open) (uint8_t camera_idx, mm_camera_vtbl_t **camera_vtbl);
} camera_hal_interface_lib_t;

typedef struct {
    uint8_t num_cameras;
    camera_hal_interface_lib_t hal_lib;
} iris_camera_hal_info_t;

typedef enum {
    IRIS_CAMERA_CMD_NO_ACTION = 0,
    IRIS_CAMERA_CMD_SET_FOCUS_MODE,
    IRIS_CAMERA_CMD_SET_NR_MODE,
    IRIS_CAMERA_CMD_AF_ENABLE,
    IRIS_CAMERA_CMD_AF_DISABLE,
    IRIS_CAMERA_CMD_AWB_ENABLE,
    IRIS_CAMERA_CMD_AWB_DISABLE,
    IRIS_CAMERA_CMD_AEC_FORCE_LC,
    IRIS_CAMERA_CMD_AEC_FORCE_GAIN,
    IRIS_CAMERA_CMD_AEC_FORCE_EXP,
    IRIS_CAMERA_CMD_AEC_FORCE_SNAP_LC,
    IRIS_CAMERA_CMD_AEC_FORCE_SNAP_GAIN,
    IRIS_CAMERA_CMD_AEC_FORCE_SNAP_EXP,
    IRIS_CAMERA_CMD_WB,
    IRIS_CAMERA_CMD_EXPOSURE_METERING,
    IRIS_CAMERA_CMD_BRIGHTNESS,
    IRIS_CAMERA_CMD_CONTRAST,
    IRIS_CAMERA_CMD_SATURATION,
    IRIS_CAMERA_CMD_SHARPNESS,
    IRIS_CAMERA_CMD_ISO,
    IRIS_CAMERA_CMD_ZOOM,
    IRIS_CAMERA_CMD_FLASH,
    IRIS_CAMERA_CMD_FPS_RANGE,
} iris_camera_commands;

typedef struct {
    iris_camera_hal_info_t hal_info;
    iris_camera_ctx_t ctx;
    int stream_running;
} iris_camera_obj_t;

typedef iris_camera_obj_t iris_camera_handle;

#ifdef __cplusplus
extern "C" {
#endif

int iris_camera_open(iris_camera_handle *handle, int cam_id);
int iris_camera_close(iris_camera_handle *handle);
int iris_camera_get_number_of_cameras(iris_camera_handle *handle);
int iris_camera_send_command(iris_camera_handle *handle,
                               iris_camera_commands cmd,
                               void *data, void *out_data);
int iris_camera_start_preview(iris_camera_handle *handle);
int iris_camera_stop_preview(iris_camera_handle *handle);
int iris_camera_set_preview_cb(iris_camera_handle *handle, cam_stream_user_cb cb);
int iris_camera_set_preview_resolution(iris_camera_handle *handle, uint16_t width, uint16_t height);
int iris_camera_release_preview_buf(iris_camera_callback_buf_t *buf);

#ifdef __cplusplus
}
#endif

#endif /* __IRIS_CAMERA_H__ */

