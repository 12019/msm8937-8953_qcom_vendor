/*
 * Copyright (c) 2015-2016 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */

#include <ctype.h>
#include <cutils/properties.h>
#include <fcntl.h>
#include <dlfcn.h>
#include <linux/msm_ion.h>
#include <sys/mman.h>
#include <utils/Log.h>
#include "iris_camera.h"

#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "IrisCameraLib"

static int iris_camera_load_hal(iris_camera_hal_info_t *hal_info)
{
    memset(&hal_info->hal_lib, 0, sizeof(iris_camera_hal_info_t));
    hal_info->hal_lib.ptr = dlopen("libmmcamera_interface.so", RTLD_NOW);
    if (!hal_info->hal_lib.ptr) {
        IRIS_CAMERA_LOG("%s Error opening HAL library %s\n", __func__, dlerror());
        return -IRIS_CAMERA_E_GENERAL;
    }
    *(void **)&(hal_info->hal_lib.get_num_of_cameras) =
        dlsym(hal_info->hal_lib.ptr, "get_num_of_cameras");
    *(void **)&(hal_info->hal_lib.mm_camera_open) =
        dlsym(hal_info->hal_lib.ptr, "camera_open");

    if (hal_info->hal_lib.get_num_of_cameras == NULL ||
        hal_info->hal_lib.mm_camera_open == NULL) {
        IRIS_CAMERA_LOG("%s Error loading HAL sym %s\n", __func__, dlerror());
        return -IRIS_CAMERA_E_GENERAL;
    }

    hal_info->num_cameras = hal_info->hal_lib.get_num_of_cameras();
    IRIS_CAMERA_LOG("%s: num_cameras = %d\n", __func__, hal_info->num_cameras);

    return IRIS_CAMERA_OK;
}

static int iris_camera_allocate_ion_memory(iris_camera_buf_t *buf, unsigned int ion_type)
{
    int rc = IRIS_CAMERA_OK;
    struct ion_handle_data handle_data;
    struct ion_allocation_data alloc;
    struct ion_fd_data ion_info_fd;
    int main_ion_fd = -1;
    void *data = NULL;

    main_ion_fd = open("/dev/ion", O_RDONLY);
    if (main_ion_fd <= 0) {
        IRIS_CAMERA_LOG("Ion dev open failed %s\n", strerror(errno));
        goto ION_OPEN_FAILED;
    }

    memset(&alloc, 0, sizeof(alloc));
    alloc.len = buf->mem_info.size;
    /* to make it page size aligned */
    alloc.len = (alloc.len + 4095U) & (~4095U);
    alloc.align = 4096;
    alloc.flags = ION_FLAG_CACHED;
    alloc.heap_id_mask = ION_HEAP(ION_SYSTEM_HEAP_ID);
    rc = ioctl(main_ion_fd, ION_IOC_ALLOC, &alloc);
    if (rc < 0) {
        IRIS_CAMERA_LOG("ION allocation failed %s with rc = %d \n",strerror(errno), rc);
        goto ION_ALLOC_FAILED;
    }

    memset(&ion_info_fd, 0, sizeof(ion_info_fd));
    ion_info_fd.handle = alloc.handle;
    rc = ioctl(main_ion_fd, ION_IOC_SHARE, &ion_info_fd);
    if (rc < 0) {
        IRIS_CAMERA_LOG("ION map failed %s\n", strerror(errno));
        goto ION_MAP_FAILED;
    }

    data = mmap(NULL,
                alloc.len,
                PROT_READ  | PROT_WRITE,
                MAP_SHARED,
                ion_info_fd.fd,
                0);

    if (data == MAP_FAILED) {
        IRIS_CAMERA_LOG("ION_MMAP_FAILED: %s (%d)\n", strerror(errno), errno);
        goto ION_MAP_FAILED;
    }
    buf->mem_info.main_ion_fd = main_ion_fd;
    buf->mem_info.fd = ion_info_fd.fd;
    buf->mem_info.handle = ion_info_fd.handle;
    buf->mem_info.size = alloc.len;
    buf->mem_info.data = data;
    return IRIS_CAMERA_OK;

ION_MAP_FAILED:
    memset(&handle_data, 0, sizeof(handle_data));
    handle_data.handle = ion_info_fd.handle;
    ioctl(main_ion_fd, ION_IOC_FREE, &handle_data);
ION_ALLOC_FAILED:
    close(main_ion_fd);
ION_OPEN_FAILED:
    return -IRIS_CAMERA_E_GENERAL;
}

static int iris_camera_deallocate_ion_memory(iris_camera_buf_t *buf)
{
    struct ion_handle_data handle_data;

    munmap(buf->mem_info.data, buf->mem_info.size);

    if (buf->mem_info.fd >= 0) {
        close(buf->mem_info.fd);
        buf->mem_info.fd = -1;
    }

    if (buf->mem_info.main_ion_fd >= 0) {
        memset(&handle_data, 0, sizeof(handle_data));
        handle_data.handle = buf->mem_info.handle;
        ioctl(buf->mem_info.main_ion_fd, ION_IOC_FREE, &handle_data);
        close(buf->mem_info.main_ion_fd);
        buf->mem_info.main_ion_fd = -1;
    }

    return IRIS_CAMERA_OK;
}

/* cmd = ION_IOC_CLEAN_CACHES, ION_IOC_INV_CACHES, ION_IOC_CLEAN_INV_CACHES */
static int iris_camera_cache_ops(iris_camera_meminfo_t *mem_info,
                     int cmd)
{
    struct ion_flush_data cache_inv_data;
    struct ion_custom_data custom_data;
    int rc = IRIS_CAMERA_OK;

    if (NULL == mem_info) {
        IRIS_CAMERA_LOG("%s: mem_info is NULL, return here", __func__);
        return -IRIS_CAMERA_E_GENERAL;
    }

    memset(&cache_inv_data, 0, sizeof(cache_inv_data));
    memset(&custom_data, 0, sizeof(custom_data));
    cache_inv_data.vaddr = mem_info->data;
    cache_inv_data.fd = mem_info->fd;
    cache_inv_data.handle = mem_info->handle;
    cache_inv_data.length = (unsigned int)mem_info->size;
    custom_data.cmd = (unsigned int)cmd;
    custom_data.arg = (unsigned long)&cache_inv_data;

    IRIS_CAMERA_LOG("addr = %p, fd = %d, handle = %lx length = %d, ION Fd = %d",
         cache_inv_data.vaddr, cache_inv_data.fd,
         (unsigned long)cache_inv_data.handle, cache_inv_data.length,
         mem_info->main_ion_fd);
    if(mem_info->main_ion_fd >= 0) {
        if(ioctl(mem_info->main_ion_fd, ION_IOC_CUSTOM, &custom_data) < 0) {
            IRIS_CAMERA_LOG("%s: Cache Invalidate failed\n", __func__);
            rc = -IRIS_CAMERA_E_GENERAL;
        }
    }

    return rc;
}

static int iris_camera_alloc_bufs(iris_camera_buf_t* app_bufs,
                      cam_frame_len_offset_t *frame_offset_info,
                      uint8_t num_bufs,
                      uint8_t is_streambuf,
                      size_t multipleOf)
{
    uint32_t i, j;
    unsigned int ion_type = 0x1 << CAMERA_ION_HEAP_ID;

    for (i = 0; i < num_bufs ; i++) {
        if ( 0 < multipleOf ) {
            size_t m = frame_offset_info->frame_len / multipleOf;
            if ( ( frame_offset_info->frame_len % multipleOf ) != 0 ) {
                m++;
            }
            app_bufs[i].mem_info.size = m * multipleOf;
        } else {
            app_bufs[i].mem_info.size = frame_offset_info->frame_len;
        }
        iris_camera_allocate_ion_memory(&app_bufs[i], ion_type);

        app_bufs[i].buf.buf_idx = i;
        app_bufs[i].buf.planes_buf.num_planes = (int8_t)frame_offset_info->num_planes;
        app_bufs[i].buf.fd = app_bufs[i].mem_info.fd;
        app_bufs[i].buf.frame_len = app_bufs[i].mem_info.size;
        app_bufs[i].buf.buffer = app_bufs[i].mem_info.data;
        app_bufs[i].buf.mem_info = (void *)&app_bufs[i].mem_info;

        /* Plane 0 needs to be set seperately. Set other planes
             * in a loop. */
        app_bufs[i].buf.planes_buf.planes[0].length = frame_offset_info->mp[0].len;
        app_bufs[i].buf.planes_buf.planes[0].m.userptr =
            (long unsigned int)app_bufs[i].buf.fd;
        app_bufs[i].buf.planes_buf.planes[0].data_offset = frame_offset_info->mp[0].offset;
        app_bufs[i].buf.planes_buf.planes[0].reserved[0] = 0;
        for (j = 1; j < (uint8_t)frame_offset_info->num_planes; j++) {
            app_bufs[i].buf.planes_buf.planes[j].length = frame_offset_info->mp[j].len;
            app_bufs[i].buf.planes_buf.planes[j].m.userptr =
                (long unsigned int)app_bufs[i].buf.fd;
            app_bufs[i].buf.planes_buf.planes[j].data_offset = frame_offset_info->mp[j].offset;
            app_bufs[i].buf.planes_buf.planes[j].reserved[0] =
                app_bufs[i].buf.planes_buf.planes[j-1].reserved[0] +
                app_bufs[i].buf.planes_buf.planes[j-1].length;
        }
    }
    IRIS_CAMERA_LOG("%s: X", __func__);

    return IRIS_CAMERA_OK;
}

static int iris_camera_release_bufs(uint8_t num_bufs,
                        iris_camera_buf_t* app_bufs)
{
    int i, rc = IRIS_CAMERA_OK;

    IRIS_CAMERA_LOG("%s: E", __func__);

    for (i = 0; i < num_bufs; i++) {
        rc = iris_camera_deallocate_ion_memory(&app_bufs[i]);
    }
    memset(app_bufs, 0, num_bufs * sizeof(iris_camera_buf_t));
    IRIS_CAMERA_LOG("%s: X", __func__);

    return rc;
}

static int iris_camera_stream_init_buf_internal(cam_frame_len_offset_t *frame_offset_info,
                          uint8_t *num_bufs,
                          uint8_t **initial_reg_flag,
                          mm_camera_buf_def_t **bufs,
                          mm_camera_map_unmap_ops_tbl_t *ops_tbl,
                          void *user_data,
                          int b_qbuf_at_reg)
{
    iris_camera_stream_t *stream = (iris_camera_stream_t *)user_data;
    mm_camera_buf_def_t *pBufs = NULL;
    uint8_t *reg_flags = NULL;
    int i, rc;

    stream->offset = *frame_offset_info;

    IRIS_CAMERA_LOG("%s: alloc buf for stream_id %d, len=%d, num planes: %d, offset: %d",
         __func__,
         stream->s_id,
         frame_offset_info->frame_len,
         frame_offset_info->num_planes,
         frame_offset_info->mp[1].offset);

    if (stream->num_of_bufs > CAM_MAX_NUM_BUFS_PER_STREAM)
        stream->num_of_bufs = CAM_MAX_NUM_BUFS_PER_STREAM;

    pBufs = stream->s_mm_bufs = (mm_camera_buf_def_t *)malloc(sizeof(mm_camera_buf_def_t) * stream->num_of_bufs);
    if (pBufs == NULL) {
        IRIS_CAMERA_LOG("%s: No mem for s_mm_bufs", __func__);
        return -1;
    }

    reg_flags = (uint8_t *)malloc(sizeof(uint8_t) * stream->num_of_bufs);
    if (reg_flags == NULL) {
        IRIS_CAMERA_LOG("%s: No mem for bufs", __func__);
        free(pBufs);
        return -1;
    }

    rc = iris_camera_alloc_bufs(&stream->s_bufs[0],
                           frame_offset_info,
                           stream->num_of_bufs,
                           1,
                           stream->multipleOf);

    if (rc != IRIS_CAMERA_OK) {
        IRIS_CAMERA_LOG("%s: mm_stream_alloc_bufs err = %d", __func__, rc);
        free(reg_flags);
        free(pBufs);
        return rc;
    }

    for (i = 0; i < stream->num_of_bufs; i++) {
        /* mapping stream bufs first */
        pBufs[i] = stream->s_bufs[i].buf;
        if (b_qbuf_at_reg)
            reg_flags[i] = 1;
        else
            reg_flags[i] = 0;

        stream->s_bufs[i].buf_frame_num = 0;
        stream->s_bufs[i].buf_state = b_qbuf_at_reg ? IRIS_BUF_STATE_USED: IRIS_BUF_STATE_FREE;
        rc = ops_tbl->map_ops(pBufs[i].buf_idx,
                              -1,
                              pBufs[i].fd,
                              (uint32_t)pBufs[i].frame_len,
                              CAM_MAPPING_BUF_TYPE_STREAM_BUF, ops_tbl->userdata);
        if (rc != IRIS_CAMERA_OK) {
            IRIS_CAMERA_LOG("%s: mapping buf[%d] err = %d", __func__, i, rc);
            break;
        }
    }

    if (rc != IRIS_CAMERA_OK) {
        int j;
        for (j=0; j>i; j++) {
            ops_tbl->unmap_ops(pBufs[j].buf_idx, -1,
                    CAM_MAPPING_BUF_TYPE_STREAM_BUF, ops_tbl->userdata);
        }
        iris_camera_release_bufs(stream->num_of_bufs, &stream->s_bufs[0]);
        free(reg_flags);
        free(pBufs);
        return rc;
    }

    *num_bufs = stream->num_of_bufs;
    *bufs = pBufs;
    *initial_reg_flag = reg_flags;

    IRIS_CAMERA_LOG("%s: X",__func__);

    return rc;
}

static int iris_camera_stream_init_preview_buf(cam_frame_len_offset_t *frame_offset_info,
                          uint8_t *num_bufs,
                          uint8_t **initial_reg_flag,
                          mm_camera_buf_def_t **bufs,
                          mm_camera_map_unmap_ops_tbl_t *ops_tbl,
                          void *user_data)
{
#ifdef IRIS_CAMERA_HAL_V1
    return iris_camera_stream_init_buf_internal(frame_offset_info, num_bufs, initial_reg_flag,
                        bufs, ops_tbl, user_data, 1);
#else
    return iris_camera_stream_init_buf_internal(frame_offset_info, num_bufs, initial_reg_flag,
                        bufs, ops_tbl, user_data, 0);
#endif
}

static int iris_camera_stream_init_meta_buf(cam_frame_len_offset_t *frame_offset_info,
                          uint8_t *num_bufs,
                          uint8_t **initial_reg_flag,
                          mm_camera_buf_def_t **bufs,
                          mm_camera_map_unmap_ops_tbl_t *ops_tbl,
                          void *user_data)
{
    return iris_camera_stream_init_buf_internal(frame_offset_info, num_bufs, initial_reg_flag,
                        bufs, ops_tbl, user_data, 1);
}

static int32_t iris_camera_stream_deinit_buf(mm_camera_map_unmap_ops_tbl_t *ops_tbl,
                                void *user_data)
{
    iris_camera_stream_t *stream = (iris_camera_stream_t *)user_data;
    int i;

    for (i = 0; i < stream->num_of_bufs ; i++) {
        /* mapping stream bufs first */
        ops_tbl->unmap_ops(stream->s_bufs[i].buf.buf_idx, -1,
                CAM_MAPPING_BUF_TYPE_STREAM_BUF, ops_tbl->userdata);
    }

    iris_camera_release_bufs(stream->num_of_bufs, &stream->s_bufs[0]);

    IRIS_CAMERA_LOG("%s: X",__func__);

    return 0;
}

static int32_t iris_camera_stream_clean_invalidate_buf(uint32_t index, void *user_data)
{
    iris_camera_stream_t *stream = (iris_camera_stream_t *)user_data;

    return iris_camera_cache_ops(&stream->s_bufs[index].mem_info,
      ION_IOC_CLEAN_INV_CACHES);
}

static int32_t iris_camera_stream_invalidate_buf(uint32_t index, void *user_data)
{
    iris_camera_stream_t *stream = (iris_camera_stream_t *)user_data;

    return iris_camera_cache_ops(&stream->s_bufs[index].mem_info, ION_IOC_INV_CACHES);
}

static int iris_camera_init_batch_update(iris_camera_ctx_t *ctx)
{
#ifdef IRIS_CAMERA_HAL_V1
    int32_t hal_version = CAM_HAL_V1;
#else
    int32_t hal_version = CAM_HAL_V3;
#endif

    parm_buffer_t *parm_buf = ( parm_buffer_t * ) ctx->parm_buf.mem_info.data;
    memset(parm_buf, 0, sizeof(parm_buffer_t));
    ADD_SET_PARAM_ENTRY_TO_BATCH(ctx->parm_buf.mem_info.data,
            CAM_INTF_PARM_HAL_VERSION, hal_version);

    return IRIS_CAMERA_OK;
}

static int iris_camera_commit_set_batch(iris_camera_ctx_t *ctx)
{
    int rc = IRIS_CAMERA_OK;
    int i = 0;

    parm_buffer_t *p_table = ( parm_buffer_t * ) ctx->parm_buf.mem_info.data;
    for(i = 0; i < CAM_INTF_PARM_MAX; i++){
        if(p_table->is_valid[i])
            break;
    }

    if (i < CAM_INTF_PARM_MAX) {
        rc = ctx->cam->ops->set_parms(ctx->cam->camera_handle, p_table);
    }

    return rc;
}


static int iris_camera_commit_get_batch(iris_camera_ctx_t *ctx)
{
    int rc = IRIS_CAMERA_OK;
    int i = 0;
    parm_buffer_t *p_table = ( parm_buffer_t * ) ctx->parm_buf.mem_info.data;

    for(i = 0; i < CAM_INTF_PARM_MAX; i++){
        if(p_table->is_valid[i])
            break;
    }
    if (i < CAM_INTF_PARM_MAX) {
        rc = ctx->cam->ops->get_parms(ctx->cam->camera_handle, p_table);
    }
    return rc;
}

static int iris_camera_set_focus_mode(iris_camera_ctx_t *ctx, cam_focus_mode_type mode)
{
    int rc = IRIS_CAMERA_OK;
    uint32_t value = mode;

    rc = iris_camera_init_batch_update(ctx);
    if (rc != IRIS_CAMERA_OK) {
        IRIS_CAMERA_LOG("%s: Batch camera parameter update failed\n", __func__);
        return IRIS_CAMERA_E_GENERAL;
    }


    if (ADD_SET_PARAM_ENTRY_TO_BATCH(ctx->parm_buf.mem_info.data,
            CAM_INTF_PARM_FOCUS_MODE, value)) {
        IRIS_CAMERA_LOG("%s: Focus mode parameter not added to batch\n", __func__);
        return IRIS_CAMERA_E_GENERAL;
    }

    rc = iris_camera_commit_set_batch(ctx);
    if (rc != IRIS_CAMERA_OK) {
        IRIS_CAMERA_LOG("%s: Batch parameters commit failed\n", __func__);
    }

    return rc;
}

static int iris_camera_get_caps(iris_camera_handle *handle,
                           cam_capability_t *caps)
{
    int rc = IRIS_CAMERA_OK;

    if ( NULL == handle ) {
        IRIS_CAMERA_LOG(" %s : Invalid handle", __func__);
        return IRIS_CAMERA_E_INVALID_INPUT;
    }

    if ( NULL == caps ) {
        IRIS_CAMERA_LOG(" %s : Invalid capabilities structure", __func__);
        return IRIS_CAMERA_E_INVALID_INPUT;
    }

    *caps = *( (cam_capability_t *) handle->ctx.cap_buf.mem_info.data );

    return rc;
}

static int iris_camera_set_metainfo(iris_camera_ctx_t *ctx,
        cam_stream_size_info_t *value)
{
    int rc = IRIS_CAMERA_OK;

    rc = iris_camera_init_batch_update(ctx);
    if (rc != IRIS_CAMERA_OK) {
        IRIS_CAMERA_LOG("%s: Batch camera parameter update failed\n", __func__);
        return rc;
    }

    if (ADD_SET_PARAM_ENTRY_TO_BATCH(ctx->parm_buf.mem_info.data,
            CAM_INTF_META_STREAM_INFO, *value)) {
        IRIS_CAMERA_LOG("%s: PP Command not added to batch\n", __func__);
        return IRIS_CAMERA_E_GENERAL;
    }

    //hardcode to preview intent for now
    ADD_SET_PARAM_ENTRY_TO_BATCH(ctx->parm_buf.mem_info.data, CAM_INTF_META_CAPTURE_INTENT,
        CAM_INTENT_PREVIEW);
    ADD_SET_PARAM_ENTRY_TO_BATCH(ctx->parm_buf.mem_info.data, CAM_INTF_PARM_DIS_ENABLE,
        DIS_DISABLE);

    rc = iris_camera_commit_set_batch(ctx);
    if (rc != IRIS_CAMERA_OK) {
        IRIS_CAMERA_LOG("%s: Batch parameters commit failed\n", __func__);
    }

    return rc;
}

static int iris_camera_set_frame_param(
                    iris_camera_ctx_t *ctx,
                    cam_stream_ID_t streamID,
                    uint32_t frame_number,
                    cam_ae_mode_type ae_mode,
                    uint64_t exp_time_ns,
                    uint32_t sensitivity,
                    cam_noise_reduction_mode_t nr_mode,
                    bool param_update)
{
    /*translate from camera_metadata_t type to parm_type_t*/
    int rc = IRIS_CAMERA_OK;

    if (ctx == NULL) {
        IRIS_CAMERA_LOG("%s, invalid params!", __func__);
        return IRIS_CAMERA_E_INVALID_INPUT;
    }

    IRIS_CAMERA_LOG("%s: framenumber = %d", __func__, (int)frame_number);

    rc = iris_camera_init_batch_update(ctx);
    if (rc != IRIS_CAMERA_OK) {
        IRIS_CAMERA_LOG("%s: Batch camera parameter update failed\n", __func__);
        return rc;
    }

    /*we need to update the frame number in the parameters*/
    if (ADD_SET_PARAM_ENTRY_TO_BATCH(ctx->parm_buf.mem_info.data,
            CAM_INTF_META_FRAME_NUMBER,
            frame_number)) {
        IRIS_CAMERA_LOG("%s: Failed to set the frame number in the parameters", __func__);
        return IRIS_CAMERA_E_GENERAL;
    }

    /* Update stream id of all the requested buffers */
    if (ADD_SET_PARAM_ENTRY_TO_BATCH(ctx->parm_buf.mem_info.data,
            CAM_INTF_META_STREAM_ID, streamID)) {
        IRIS_CAMERA_LOG("%s: Failed to set stream type mask in the parameters", __func__);
        return IRIS_CAMERA_E_GENERAL;
    }

    if (param_update) {
        if (ADD_SET_PARAM_ENTRY_TO_BATCH(ctx->parm_buf.mem_info.data,
                CAM_INTF_META_AEC_MODE, ae_mode)) {
            IRIS_CAMERA_LOG("%s: Failed to set sensor ae mode in the parameters", __func__);
            return IRIS_CAMERA_E_GENERAL;
        }

        if (ADD_SET_PARAM_ENTRY_TO_BATCH(ctx->parm_buf.mem_info.data,
                CAM_INTF_META_SENSOR_EXPOSURE_TIME, exp_time_ns)) {
            IRIS_CAMERA_LOG("%s: Failed to set sensor exposure time in the parameters", __func__);
            return IRIS_CAMERA_E_GENERAL;
        }
        IRIS_CAMERA_LOG("Update sensor exp time %lld ns", exp_time_ns);

        if (ADD_SET_PARAM_ENTRY_TO_BATCH(ctx->parm_buf.mem_info.data,
                CAM_INTF_META_SENSOR_SENSITIVITY,
                sensitivity)) {
            IRIS_CAMERA_LOG("%s: Failed to set sensor sensitivity in the parameters", __func__);
            return IRIS_CAMERA_E_GENERAL;
        }

        if (ADD_SET_PARAM_ENTRY_TO_BATCH(ctx->parm_buf.mem_info.data,
                CAM_INTF_META_NOISE_REDUCTION_MODE,
                nr_mode)) {
            IRIS_CAMERA_LOG("%s: Failed to set noise reduction mode in the parameters", __func__);
            return IRIS_CAMERA_E_GENERAL;
        }

        IRIS_CAMERA_LOG("Update sensor sensitivity %d", sensitivity);
    }

    rc = iris_camera_commit_set_batch(ctx);
    if (rc != IRIS_CAMERA_OK) {
        IRIS_CAMERA_LOG("%s: Batch parameters commit failed\n", __func__);
    }

    return rc;
}


static int iris_camera_set_flash_mode(iris_camera_ctx_t *ctx,
        cam_flash_mode_t flashMode)
{
    int rc = IRIS_CAMERA_OK;

    if (ctx == NULL) {
        IRIS_CAMERA_LOG("%s, invalid params!", __func__);
        return IRIS_CAMERA_E_INVALID_INPUT;
    }

    IRIS_CAMERA_LOG("%s: mode = %d", __func__, (int)flashMode);

    rc = iris_camera_init_batch_update(ctx);
    if (rc != IRIS_CAMERA_OK) {
        IRIS_CAMERA_LOG("%s: Batch camera parameter update failed\n", __func__);
        return rc;
    }

    if (ADD_SET_PARAM_ENTRY_TO_BATCH(ctx->parm_buf.mem_info.data,
        CAM_INTF_PARM_LED_MODE, flashMode)) {
        IRIS_CAMERA_LOG("%s: Flash mode parameter not added to batch\n", __func__);
        return IRIS_CAMERA_E_GENERAL;
    }

    rc = iris_camera_commit_set_batch(ctx);
    if (rc != IRIS_CAMERA_OK) {
        IRIS_CAMERA_LOG("%s: Batch parameters commit failed\n", __func__);
    }

    return rc;
}


static mm_camera_buf_def_t *iris_camera_get_next_free_preview_buf(iris_camera_ctx_t *ctx, uint32_t *next_frame_num)
{
    int i;
    iris_camera_stream_t *stream;

    stream = &ctx->channels[IRIS_CHANNEL_TYPE_PREVIEW].streams[0];

    pthread_mutex_lock(&ctx->ctx_mutex);
    for (i = 0; i < stream->num_of_bufs; i++) {
        if (stream->s_bufs[i].buf_state == IRIS_BUF_STATE_FREE)
            break;
    }

    if (i == stream->num_of_bufs) {
        IRIS_CAMERA_LOG("No free preview buffers");
        pthread_mutex_unlock(&ctx->ctx_mutex);
        return NULL;
    }

    *next_frame_num = stream->s_bufs[i].buf_frame_num = stream->next_frame_num;
    stream->s_bufs[i].buf_state = IRIS_BUF_STATE_USED;
    stream->next_frame_num++;
    pthread_mutex_unlock(&ctx->ctx_mutex);

    return &stream->s_mm_bufs[i];
}

static int iris_camera_put_preview_buf(iris_camera_ctx_t *ctx, uint32_t camera_handle,
        uint32_t ch_id, mm_camera_buf_def_t *buf)
{
    int rc = IRIS_CAMERA_OK;

#ifdef IRIS_CAMERA_HAL_V1
    rc = ctx->cam->ops->qbuf(camera_handle, ch_id, buf);
    if (rc != IRIS_CAMERA_OK) {
        IRIS_CAMERA_LOG("%s: Failed in Preview Qbuf\n", __func__);
        return rc;
    }
    iris_camera_cache_ops((iris_camera_meminfo_t *)buf->mem_info,
            ION_IOC_INV_CACHES);
#else
    iris_camera_stream_t *stream;

    stream = &ctx->channels[IRIS_CHANNEL_TYPE_PREVIEW].streams[0];

    pthread_mutex_lock(&ctx->ctx_mutex);
    if (stream->s_bufs[buf->buf_idx].buf_state == IRIS_BUF_STATE_FREE) {
        IRIS_CAMERA_LOG("Invalid buffer state");
        pthread_mutex_unlock(&ctx->ctx_mutex);
        return IRIS_CAMERA_E_GENERAL;
    }

    iris_camera_cache_ops((iris_camera_meminfo_t *)buf->mem_info,
            ION_IOC_INV_CACHES);
    stream->s_bufs[buf->buf_idx].buf_state = IRIS_BUF_STATE_FREE;
    pthread_mutex_unlock(&ctx->ctx_mutex);
#endif

    return IRIS_CAMERA_OK;
}


static void iris_camera_process_pull_req(iris_camera_ctx_t *ctx)
{
    mm_camera_buf_def_t *new_buf;
    uint32_t next_frame_num = 0;
    cam_stream_ID_t stream_id;
    iris_camera_stream_t *stream;
    int rc;

    new_buf = iris_camera_get_next_free_preview_buf(ctx, &next_frame_num);
    if (new_buf) {
        /* queue frame buffer */
        rc = ctx->cam->ops->qbuf(ctx->cam->camera_handle, ctx->channels[IRIS_CHANNEL_TYPE_PREVIEW].ch_id, new_buf);
        if (rc != IRIS_CAMERA_OK) {
            IRIS_CAMERA_LOG("queue new buffer failed");
        }

        IRIS_CAMERA_LOG("queued a new buffer");
    } else {
        IRIS_CAMERA_LOG("No free frame");
        return;
    }

    stream = &ctx->channels[IRIS_CHANNEL_TYPE_PREVIEW].streams[0];
    /* send notification to backend */
    stream_id.num_streams = 1;
    stream_id.streamID[0] = stream->s_info->stream_svr_id;
    pthread_mutex_lock(&ctx->ctx_mutex);
    iris_camera_set_frame_param(ctx, stream_id, next_frame_num, ctx->ae_mode, ctx->exp_time_ns,
            ctx->sensitivity, ctx->nr_mode, ctx->frame_param_update);
    ctx->frame_param_update = false;
    stream->s_bufs[new_buf->buf_idx].sensitivity = ctx->sensitivity;
    stream->s_bufs[new_buf->buf_idx].exp_time_ns = ctx->exp_time_ns;
    pthread_mutex_unlock(&ctx->ctx_mutex);
}

void *iris_camera_streaming_thread_func(void *arg)
{
    iris_camera_ctx_t *ctx = (iris_camera_ctx_t *)arg;

    while(!ctx->stop_thread) {
        usleep(30000);
        iris_camera_process_pull_req(ctx);
    }

    return NULL;
}

int32_t iris_camera_streaming_thread_start(iris_camera_ctx_t *ctx)
{
    /* launch the thread */
    ctx->stop_thread = false;
    pthread_create(&ctx->streaming_thread_id,
                   NULL,
                   &iris_camera_streaming_thread_func,
                   (void *)ctx);

    return IRIS_CAMERA_OK;
}

int32_t iris_camera_streaming_thread_stop(iris_camera_ctx_t *ctx)
{
    ctx->stop_thread = true;

    /* wait until cmd thread exits */
    if (pthread_join(ctx->streaming_thread_id, NULL) != 0) {
        IRIS_CAMERA_LOG("%s: pthread dead already\n", __func__);
    }
    ctx->stop_thread = false;

    return IRIS_CAMERA_OK;
}

static void iris_camera_notify_evt_cb(uint32_t camera_handle,
                          mm_camera_event_t *evt,
                          void *user_data)
{
    iris_camera_ctx_t *ctx =
        (iris_camera_ctx_t *)user_data;
    if (ctx == NULL || ctx->cam->camera_handle != camera_handle) {
        IRIS_CAMERA_LOG("%s: Not a valid test obj", __func__);
        return;
    }

    IRIS_CAMERA_LOG("%s:E evt = %d", __func__, evt->server_event_type);
    switch (evt->server_event_type) {
       case CAM_EVENT_TYPE_AUTO_FOCUS_DONE:
           IRIS_CAMERA_LOG("%s: rcvd auto focus done evt", __func__);
           break;
       case CAM_EVENT_TYPE_ZOOM_DONE:
           IRIS_CAMERA_LOG("%s: rcvd zoom done evt", __func__);
           break;
       case CAM_EVENT_TYPE_DAEMON_PULL_REQ:
           IRIS_CAMERA_LOG("%s: rcvd pull request evt", __func__);
           //iris_camera_process_pull_req(ctx);
           break;
       default:
           break;
    }

    IRIS_CAMERA_LOG("%s:X", __func__);
}

static int iris_camera_open_internal(iris_camera_hal_info_t *hal_info,
                int cam_id,
                iris_camera_ctx_t *ctx)
{
    int32_t rc = IRIS_CAMERA_OK;
    cam_frame_len_offset_t offset_info;

    IRIS_CAMERA_LOG("%s:BEGIN\n", __func__);

    rc = hal_info->hal_lib.mm_camera_open((uint8_t)cam_id, &(ctx->cam));
    if(rc || !ctx->cam) {
        IRIS_CAMERA_LOG("%s:dev open error. rc = %d, vtbl = %p\n", __func__, rc, ctx->cam);
        return -IRIS_CAMERA_E_GENERAL;
    }

    IRIS_CAMERA_LOG("Open Camera id = %d handle = %d", cam_id, ctx->cam->camera_handle);

    /* alloc ion mem for capability buf */
    memset(&offset_info, 0, sizeof(offset_info));
    offset_info.frame_len = sizeof(cam_capability_t);

    rc = iris_camera_alloc_bufs(&ctx->cap_buf,
                           &offset_info,
                           1,
                           0,
                           0);
    if (rc != IRIS_CAMERA_OK) {
        IRIS_CAMERA_LOG("%s:alloc buf for capability error\n", __func__);
        goto error_after_cam_open;
    }

    /* mapping capability buf */
    rc = ctx->cam->ops->map_buf(ctx->cam->camera_handle,
                                     CAM_MAPPING_BUF_TYPE_CAPABILITY,
                                     ctx->cap_buf.mem_info.fd,
                                     ctx->cap_buf.mem_info.size);
    if (rc != IRIS_CAMERA_OK) {
        IRIS_CAMERA_LOG("%s:map for capability error\n", __func__);
        goto error_after_cap_buf_alloc;
    }

    /* alloc ion mem for getparm buf */
    memset(&offset_info, 0, sizeof(offset_info));
    offset_info.frame_len = sizeof(parm_buffer_t);
    rc = iris_camera_alloc_bufs(&ctx->parm_buf,
                           &offset_info,
                           1,
                           0,
                           0);
    if (rc != IRIS_CAMERA_OK) {
        IRIS_CAMERA_LOG("%s:alloc buf for getparm_buf error\n", __func__);
        goto error_after_cap_buf_map;
    }

    /* mapping getparm buf */
    rc = ctx->cam->ops->map_buf(ctx->cam->camera_handle,
                                     CAM_MAPPING_BUF_TYPE_PARM_BUF,
                                     ctx->parm_buf.mem_info.fd,
                                     ctx->parm_buf.mem_info.size);
    if (rc != IRIS_CAMERA_OK) {
        IRIS_CAMERA_LOG("%s:map getparm_buf error\n", __func__);
        goto error_after_getparm_buf_alloc;
    }
    ctx->params_buffer = (parm_buffer_t*) ctx->parm_buf.mem_info.data;
    IRIS_CAMERA_LOG("\n%s params_buffer=%p\n",__func__,ctx->params_buffer);

    rc = ctx->cam->ops->register_event_notify(ctx->cam->camera_handle,
                                                   iris_camera_notify_evt_cb,
                                                   ctx);
    if (rc != IRIS_CAMERA_OK) {
        IRIS_CAMERA_LOG("%s: failed register_event_notify", __func__);
        rc = -IRIS_CAMERA_E_GENERAL;
        goto error_after_getparm_buf_map;
    }

    rc = ctx->cam->ops->query_capability(ctx->cam->camera_handle);
    if (rc != IRIS_CAMERA_OK) {
        IRIS_CAMERA_LOG("%s: failed query_capability", __func__);
        rc = -IRIS_CAMERA_E_GENERAL;
        goto error_after_getparm_buf_map;
    }

    //need to release preview buffer manually
    ctx->need_release_preview_buf = true;
    // Default to auto focus mode
    ctx->af_mode = CAM_FOCUS_MODE_AUTO;
#ifdef IRIS_CAMERA_HAL_V1
    ctx->ae_mode = CAM_AE_MODE_ON;
#else
    ctx->ae_mode = CAM_AE_MODE_OFF;
#endif
    ctx->exp_time_ns = DEFAULT_SENSOR_EXP_TIME_MS * 1000000;
    ctx->sensitivity = DEFAULT_SENSOR_GAIN;
    ctx->nr_mode = CAM_NOISE_REDUCTION_MODE_OFF;
    ctx->frame_param_update = true;
    pthread_mutex_init(&ctx->ctx_mutex, NULL);

    return rc;

error_after_getparm_buf_map:
    ctx->cam->ops->unmap_buf(ctx->cam->camera_handle,
                                  CAM_MAPPING_BUF_TYPE_PARM_BUF);
error_after_getparm_buf_alloc:
    iris_camera_release_bufs(1, &ctx->parm_buf);
error_after_cap_buf_map:
    ctx->cam->ops->unmap_buf(ctx->cam->camera_handle,
                                  CAM_MAPPING_BUF_TYPE_CAPABILITY);
error_after_cap_buf_alloc:
    iris_camera_release_bufs(1, &ctx->cap_buf);
error_after_cam_open:
    ctx->cam->ops->close_camera(ctx->cam->camera_handle);
    ctx->cam = NULL;
    return rc;
}

static int iris_camera_close_internal(iris_camera_ctx_t *ctx)
{
    int32_t rc = IRIS_CAMERA_OK;

    if (ctx == NULL || ctx->cam ==NULL) {
        IRIS_CAMERA_LOG("%s: cam not opened", __func__);
        return -IRIS_CAMERA_E_GENERAL;
    }

    /* unmap capability buf */
    rc = ctx->cam->ops->unmap_buf(ctx->cam->camera_handle,
                                       CAM_MAPPING_BUF_TYPE_CAPABILITY);
    if (rc != IRIS_CAMERA_OK) {
        IRIS_CAMERA_LOG("%s: unmap capability buf failed, rc=%d", __func__, rc);
    }

    /* unmap parm buf */
    rc = ctx->cam->ops->unmap_buf(ctx->cam->camera_handle,
                                       CAM_MAPPING_BUF_TYPE_PARM_BUF);
    if (rc != IRIS_CAMERA_OK) {
        IRIS_CAMERA_LOG("%s: unmap setparm buf failed, rc=%d", __func__, rc);
    }

    rc = ctx->cam->ops->close_camera(ctx->cam->camera_handle);
    if (rc != IRIS_CAMERA_OK) {
        IRIS_CAMERA_LOG("%s: close camera failed, rc=%d", __func__, rc);
    }
    ctx->cam = NULL;

    /* dealloc capability buf */
    rc = iris_camera_release_bufs(1, &ctx->cap_buf);
    if (rc != IRIS_CAMERA_OK) {
        IRIS_CAMERA_LOG("%s: release capability buf failed, rc=%d", __func__, rc);
    }

    /* dealloc parm buf */
    rc = iris_camera_release_bufs(1, &ctx->parm_buf);
    if (rc != IRIS_CAMERA_OK) {
        IRIS_CAMERA_LOG("%s: release setparm buf failed, rc=%d", __func__, rc);
    }
    pthread_mutex_destroy(&ctx->ctx_mutex);

    return IRIS_CAMERA_OK;
}

static iris_camera_channel_t * iris_camera_add_channel(iris_camera_ctx_t *ctx,
                                         iris_camera_channel_type_t ch_type,
                                         mm_camera_channel_attr_t *attr,
                                         mm_camera_buf_notify_t channel_cb,
                                         void *userdata)
{
    uint32_t ch_id = 0;
    iris_camera_channel_t *channel = NULL;

    ch_id = ctx->cam->ops->add_channel(ctx->cam->camera_handle,
                                            attr,
                                            channel_cb,
                                            userdata);
    if (ch_id == 0) {
        IRIS_CAMERA_LOG("%s: add channel failed", __func__);
        return NULL;
    }
    channel = &ctx->channels[ch_type];
    channel->ch_id = ch_id;

    return channel;
}

static int iris_camera_del_channel(iris_camera_ctx_t *ctx,
                       iris_camera_channel_t *channel)
{
    ctx->cam->ops->delete_channel(ctx->cam->camera_handle,
                                       channel->ch_id);
    memset(channel, 0, sizeof(iris_camera_channel_t));

    return IRIS_CAMERA_OK;
}

static iris_camera_stream_t * iris_camera_add_stream(iris_camera_ctx_t *ctx,
                                       iris_camera_channel_t *channel)
{
    int rc = IRIS_CAMERA_OK;
    iris_camera_stream_t *stream = NULL;
    cam_frame_len_offset_t offset_info;

    stream = &(channel->streams[channel->num_streams++]);
    stream->s_id = ctx->cam->ops->add_stream(ctx->cam->camera_handle,
                                                  channel->ch_id);
    if (stream->s_id == 0) {
        IRIS_CAMERA_LOG("%s: add stream failed", __func__);
        return NULL;
    }

    /* alloc ion mem for stream_info buf */
    memset(&offset_info, 0, sizeof(offset_info));
    offset_info.frame_len = sizeof(cam_stream_info_t);

    rc = iris_camera_alloc_bufs(&stream->s_info_buf,
                           &offset_info,
                           1,
                           0,
                           0);
    if (rc != IRIS_CAMERA_OK) {
        IRIS_CAMERA_LOG("%s:alloc buf for stream_info error\n", __func__);
        ctx->cam->ops->delete_stream(ctx->cam->camera_handle,
                                          channel->ch_id,
                                          stream->s_id);
        stream->s_id = 0;
        return NULL;
    }

	stream->s_info = (cam_stream_info_t *)stream->s_info_buf.mem_info.data;
    /* mapping streaminfo buf */
    rc = ctx->cam->ops->map_stream_buf(ctx->cam->camera_handle,
                                            channel->ch_id,
                                            stream->s_id,
                                            CAM_MAPPING_BUF_TYPE_STREAM_INFO,
                                            0,
                                            -1,
                                            stream->s_info_buf.mem_info.fd,
                                            (uint32_t)stream->s_info_buf.mem_info.size);
    if (rc != IRIS_CAMERA_OK) {
        IRIS_CAMERA_LOG("%s:map setparm_buf error\n", __func__);
        iris_camera_deallocate_ion_memory(&stream->s_info_buf);
        ctx->cam->ops->delete_stream(ctx->cam->camera_handle,
                                          channel->ch_id,
                                          stream->s_id);
        stream->s_id = 0;
        return NULL;
    }

    return stream;
}

static int iris_camera_del_stream(iris_camera_ctx_t *ctx,
                      iris_camera_channel_t *channel,
                      iris_camera_stream_t *stream)
{
    ctx->cam->ops->unmap_stream_buf(ctx->cam->camera_handle,
                                         channel->ch_id,
                                         stream->s_id,
                                         CAM_MAPPING_BUF_TYPE_STREAM_INFO,
                                         0,
                                         -1);
    iris_camera_deallocate_ion_memory(&stream->s_info_buf);
    ctx->cam->ops->delete_stream(ctx->cam->camera_handle,
                                      channel->ch_id,
                                      stream->s_id);
    memset(stream, 0, sizeof(iris_camera_stream_t));

    return IRIS_CAMERA_OK;
}

static iris_camera_channel_t *iris_camera_get_channel_by_type(iris_camera_ctx_t *ctx,
                                                iris_camera_channel_type_t ch_type)
{
    return &ctx->channels[ch_type];
}

static int iris_camera_config_stream(iris_camera_ctx_t *ctx,
                         iris_camera_channel_t *channel,
                         iris_camera_stream_t *stream,
                         mm_camera_stream_config_t *config)
{
    return ctx->cam->ops->config_stream(ctx->cam->camera_handle,
                                             channel->ch_id,
                                             stream->s_id,
                                             config);
}

static int iris_camera_start_channel(iris_camera_ctx_t *ctx,
                         iris_camera_channel_t *channel)
{
    return ctx->cam->ops->start_channel(ctx->cam->camera_handle,
                                             channel->ch_id);
}

static int iris_camera_stop_channel(iris_camera_ctx_t *ctx,
                        iris_camera_channel_t *channel)
{
    return ctx->cam->ops->stop_channel(ctx->cam->camera_handle,
                                            channel->ch_id);
}

static void iris_camera_metadata_notify_cb(mm_camera_super_buf_t *bufs,
                                     void *user_data)
{
    uint32_t i = 0;
    iris_camera_channel_t *channel = NULL;
    iris_camera_stream_t *p_stream = NULL;
    iris_camera_ctx_t *pme = (iris_camera_ctx_t *)user_data;
    mm_camera_buf_def_t *frame = NULL;
    iris_camera_callback_buf_t cb_frame;
    metadata_buffer_t *pMetadata;

    if (NULL == bufs || NULL == user_data) {
        IRIS_CAMERA_LOG("%s: bufs or user_data are not valid ", __func__);
        return;
    }
    frame = bufs->bufs[0];

    /* find channel */
    for (i = 0; i < IRIS_CHANNEL_TYPE_MAX; i++) {
        if (pme->channels[i].ch_id == bufs->ch_id) {
            channel = &pme->channels[i];
            break;
        }
    }

    if (NULL == channel) {
        IRIS_CAMERA_LOG("%s: Channel object is NULL ", __func__);
        return;
    }

    /* find meta stream */
    for (i = 0; i < channel->num_streams; i++) {
        if (channel->streams[i].s_config.stream_info->stream_type == CAM_STREAM_TYPE_METADATA) {
            p_stream = &channel->streams[i];
            break;
        }
    }

    if (NULL == p_stream) {
        IRIS_CAMERA_LOG("%s: cannot find metadata stream", __func__);
        return;
    }

    /* find meta frame */
    for (i = 0; i < bufs->num_bufs; i++) {
        if (bufs->bufs[i]->stream_id == p_stream->s_id) {
            frame = bufs->bufs[i];
            break;
        }
    }

    IRIS_CAMERA_LOG("Received a meta frame");
    if (pme->metadata == NULL) {
        /* The app will free the meta data, we don't need to bother here */
        pme->metadata = malloc(sizeof(metadata_buffer_t));
        if (NULL == pme->metadata) {
            IRIS_CAMERA_LOG("%s: Canot allocate metadata memory\n", __func__);
            return;
        }
    }
    memcpy(pme->metadata, frame->buffer, sizeof(metadata_buffer_t));

    pMetadata = (metadata_buffer_t *)frame->buffer;
    if (pme->user_metadata_cb) {
        IRIS_CAMERA_LOG("[DBG] %s, user defined own metadata cb. calling it...", __func__);
        cb_frame.user_data = user_data;
        cb_frame.camera_handle = bufs->camera_handle;
        cb_frame.ch_id = bufs->ch_id;
        cb_frame.frame = frame;
        pme->user_metadata_cb(&cb_frame);
    }

    if (IRIS_CAMERA_OK != pme->cam->ops->qbuf(bufs->camera_handle,
                                          bufs->ch_id,
                                          frame)) {
        IRIS_CAMERA_LOG("%s: Failed in Preview Qbuf\n", __func__);
    }
    iris_camera_cache_ops((iris_camera_meminfo_t *)frame->mem_info,
                   ION_IOC_INV_CACHES);
}

static iris_camera_stream_t * iris_camera_add_metadata_stream(iris_camera_ctx_t *ctx,
                                               iris_camera_channel_t *channel,
                                               mm_camera_buf_notify_t stream_cb,
                                               void *userdata,
                                               uint8_t num_bufs)
{
    int rc = IRIS_CAMERA_OK;
    iris_camera_stream_t *stream = NULL;
    cam_capability_t *cam_cap = (cam_capability_t *)(ctx->cap_buf.buf.buffer);
    stream = iris_camera_add_stream(ctx, channel);
    if (NULL == stream) {
        IRIS_CAMERA_LOG("%s: add stream failed\n", __func__);
        return NULL;
    }

    stream->s_config.mem_vtbl.get_bufs = iris_camera_stream_init_meta_buf;
    stream->s_config.mem_vtbl.put_bufs = iris_camera_stream_deinit_buf;
    stream->s_config.mem_vtbl.clean_invalidate_buf =
      iris_camera_stream_clean_invalidate_buf;
    stream->s_config.mem_vtbl.invalidate_buf = iris_camera_stream_invalidate_buf;
    stream->s_config.mem_vtbl.user_data = (void *)stream;
    stream->s_config.stream_cb = stream_cb;
    stream->s_config.stream_cb_sync = NULL;
    stream->s_config.userdata = userdata;
    stream->num_of_bufs = num_bufs;

    stream->s_config.stream_info = (cam_stream_info_t *)stream->s_info_buf.buf.buffer;
    memset(stream->s_config.stream_info, 0, sizeof(cam_stream_info_t));
    stream->s_config.stream_info->stream_type = CAM_STREAM_TYPE_METADATA;
    stream->s_config.stream_info->streaming_mode = CAM_STREAMING_MODE_CONTINUOUS;
    stream->s_config.stream_info->fmt = DEFAULT_PREVIEW_FORMAT;
    stream->s_config.stream_info->dim.width = sizeof(metadata_buffer_t);
    stream->s_config.stream_info->dim.height = 1;
    stream->s_config.padding_info = cam_cap->padding_info;

    rc = iris_camera_config_stream(ctx, channel, stream, &stream->s_config);
    if (IRIS_CAMERA_OK != rc) {
        IRIS_CAMERA_LOG("%s:config preview stream err=%d\n", __func__, rc);
        return NULL;
    }

    return stream;
}

// Camera Preview Functions
static void iris_camera_preview_notify_cb(mm_camera_super_buf_t *bufs,
                                     void *user_data)
{
    uint32_t i = 0;
    iris_camera_channel_t *channel = NULL;
    iris_camera_stream_t *p_stream = NULL;
    mm_camera_buf_def_t *frame = NULL;
    iris_camera_ctx_t *pme = (iris_camera_ctx_t *)user_data;
    iris_camera_callback_buf_t cb_frame;

    if (NULL == bufs || NULL == user_data) {
        IRIS_CAMERA_LOG("%s: bufs or user_data are not valid ", __func__);
        return;
    }

    frame = bufs->bufs[0];

    /* find channel */
    for (i = 0; i < IRIS_CHANNEL_TYPE_MAX; i++) {
        if (pme->channels[i].ch_id == bufs->ch_id) {
            channel = &pme->channels[i];
            break;
        }
    }
    if (NULL == channel) {
        IRIS_CAMERA_LOG("%s: Channel object is NULL ", __func__);
        return;
    }
    /* find preview stream */
    for (i = 0; i < channel->num_streams; i++) {
        if (channel->streams[i].s_config.stream_info->stream_type == CAM_STREAM_TYPE_PREVIEW) {
            p_stream = &channel->streams[i];
            break;
        }
    }

    if (NULL == p_stream) {
        IRIS_CAMERA_LOG("%s: cannot find preview stream", __func__);
        return;
    }

    /* find preview frame */
    for (i = 0; i < bufs->num_bufs; i++) {
        if (bufs->bufs[i]->stream_id == p_stream->s_id) {
            frame = bufs->bufs[i];
            break;
        }
    }

    IRIS_CAMERA_LOG("[DBG] %s, Get one preview frame %d...", __func__, frame->frame_idx);

    if (pme->user_preview_cb) {
        cb_frame.user_data = user_data;
        cb_frame.camera_handle = bufs->camera_handle;
        cb_frame.ch_id = bufs->ch_id;
        cb_frame.buf_frame_num = p_stream->s_bufs[frame->buf_idx].buf_frame_num;
        cb_frame.exp_time_ns = p_stream->s_bufs[frame->buf_idx].exp_time_ns;;
        cb_frame.sensitivity = p_stream->s_bufs[frame->buf_idx].sensitivity;;
        cb_frame.frame = frame;
        IRIS_CAMERA_LOG("[DBG] %s, user defined own preview cb. calling it...", __func__);
        pme->user_preview_cb(&cb_frame);
    }

    if (!pme->need_release_preview_buf) {
        if (IRIS_CAMERA_OK != iris_camera_put_preview_buf(pme,
                    bufs->camera_handle, bufs->ch_id, frame)) {
            IRIS_CAMERA_LOG("%s: Failed in Preview Qbuf\n", __func__);
            return;
        }
        iris_camera_cache_ops((iris_camera_meminfo_t *)frame->mem_info,
                ION_IOC_INV_CACHES);
    }
    IRIS_CAMERA_LOG("%s: END\n", __func__);
}

static iris_camera_stream_t * iris_camera_add_preview_stream(iris_camera_ctx_t *ctx,
                                               iris_camera_channel_t *channel,
                                               mm_camera_buf_notify_t stream_cb,
                                               void *userdata,
                                               uint8_t num_bufs)
{
    int rc = IRIS_CAMERA_OK;
    iris_camera_stream_t *stream = NULL;
    cam_capability_t *cam_cap = (cam_capability_t *)(ctx->cap_buf.buf.buffer);
    cam_dimension_t preview_dim = {0, 0};

    preview_dim.width = ctx->preview_width;
    preview_dim.height = ctx->preview_height;
    ALOGI("%s, preview dimesion: %d x %d\n", __func__, preview_dim.width, preview_dim.height);

    cam_stream_size_info_t stream_config_info ;
    memset (&stream_config_info , 0, sizeof (cam_stream_size_info_t));

    stream_config_info.num_streams = 1;
    stream_config_info.postprocess_mask[0] = 2178;
    stream_config_info.stream_sizes[0].width = preview_dim.width;
    stream_config_info.stream_sizes[0].height = preview_dim.height;
    stream_config_info.type[0] = CAM_STREAM_TYPE_PREVIEW;

    stream_config_info.buffer_info.min_buffers = MIN_INFLIGHT_REQUESTS;
    stream_config_info.buffer_info.max_buffers = MAX_INFLIGHT_REQUESTS;
    stream_config_info.is_type = IS_TYPE_NONE;

    rc = iris_camera_set_metainfo(ctx, &stream_config_info);
    if (rc != IRIS_CAMERA_OK) {
       IRIS_CAMERA_LOG("%s: meta info command failed\n", __func__);
    }

    stream = iris_camera_add_stream(ctx, channel);
    if (NULL == stream) {
        IRIS_CAMERA_LOG("%s: add stream failed\n", __func__);
        return NULL;
    }
    stream->s_config.mem_vtbl.get_bufs = iris_camera_stream_init_preview_buf;
    stream->s_config.mem_vtbl.put_bufs = iris_camera_stream_deinit_buf;
    stream->s_config.mem_vtbl.clean_invalidate_buf =
      iris_camera_stream_clean_invalidate_buf;
    stream->s_config.mem_vtbl.invalidate_buf = iris_camera_stream_invalidate_buf;
    stream->s_config.mem_vtbl.user_data = (void *)stream;
    stream->s_config.stream_cb = stream_cb;
    stream->s_config.userdata = userdata;
    stream->num_of_bufs = num_bufs;
    stream->next_frame_num = 0;

    stream->s_config.stream_info = (cam_stream_info_t *)stream->s_info_buf.buf.buffer;
    memset(stream->s_config.stream_info, 0, sizeof(cam_stream_info_t));
    stream->s_config.stream_info->stream_type = CAM_STREAM_TYPE_PREVIEW;
    stream->s_config.stream_info->streaming_mode = CAM_STREAMING_MODE_CONTINUOUS;
    stream->s_config.stream_info->fmt = CAM_FORMAT_YUV_420_NV21;

    stream->s_config.stream_info->dim.width = preview_dim.width;
    stream->s_config.stream_info->dim.height = preview_dim.height;

    stream->s_config.padding_info = cam_cap->padding_info;

    rc = iris_camera_config_stream(ctx, channel, stream, &stream->s_config);
    if (IRIS_CAMERA_OK != rc) {
        IRIS_CAMERA_LOG("%s:config preview stream err=%d\n", __func__, rc);
        return NULL;
    }

    return stream;
}

static iris_camera_channel_t * iris_camera_add_preview_channel(iris_camera_ctx_t *ctx)
{
    iris_camera_channel_t *channel = NULL;
    iris_camera_stream_t *stream = NULL;

    channel = iris_camera_add_channel(ctx,
                                 IRIS_CHANNEL_TYPE_PREVIEW,
                                 NULL,
                                 NULL,
                                 NULL);
    if (NULL == channel) {
        IRIS_CAMERA_LOG("%s: add channel failed", __func__);
        return NULL;
    }

    stream = iris_camera_add_preview_stream(ctx,
                                       channel,
                                       iris_camera_preview_notify_cb,
                                       (void *)ctx,
                                       PREVIEW_BUF_NUM);
    if (NULL == stream) {
        IRIS_CAMERA_LOG("%s: add stream failed\n", __func__);
        iris_camera_del_channel(ctx, channel);
        return NULL;
    }

    return channel;
}

static int iris_camera_stop_and_del_channel(iris_camera_ctx_t *ctx,
                                iris_camera_channel_t *channel)
{
    int rc = IRIS_CAMERA_OK;
    iris_camera_stream_t *stream = NULL;
    cam_stream_size_info_t abc ;
    uint8_t i;

    memset (&abc , 0, sizeof (cam_stream_size_info_t));

    rc = iris_camera_stop_channel(ctx, channel);
    if (IRIS_CAMERA_OK != rc) {
        IRIS_CAMERA_LOG("%s:Stop Preview failed rc=%d\n", __func__, rc);
    }

    if (channel->num_streams <= MAX_STREAM_NUM_IN_BUNDLE) {
        for (i = 0; i < channel->num_streams; i++) {
            stream = &channel->streams[i];
            rc = iris_camera_del_stream(ctx, channel, stream);
            if (IRIS_CAMERA_OK != rc) {
                IRIS_CAMERA_LOG("%s:del stream(%d) failed rc=%d\n", __func__, i, rc);
            }
        }
    } else {
        IRIS_CAMERA_LOG("%s: num_streams = %d. Should not be more than %d\n",
            __func__, channel->num_streams, MAX_STREAM_NUM_IN_BUNDLE);
    }

    rc = iris_camera_set_metainfo(ctx, &abc);
    if (rc != IRIS_CAMERA_OK) {
       IRIS_CAMERA_LOG("%s: meta info command failed\n", __func__);
    }

    rc = iris_camera_del_channel(ctx, channel);
    if (IRIS_CAMERA_OK != rc) {
        IRIS_CAMERA_LOG("%s:delete channel failed rc=%d\n", __func__, rc);
    }

    return rc;
}

static int iris_camera_start_preview_internal(iris_camera_ctx_t *ctx)
{
    int rc = IRIS_CAMERA_OK;
    iris_camera_channel_t *channel = NULL;
    iris_camera_stream_t *stream = NULL;
    iris_camera_stream_t *metadata_stream = NULL;
    uint8_t i;

    channel =  iris_camera_add_preview_channel(ctx);
    if (NULL == channel) {
        IRIS_CAMERA_LOG("%s: add channel failed", __func__);
        return IRIS_CAMERA_E_GENERAL;
    }

    metadata_stream = iris_camera_add_metadata_stream(ctx,
                                            channel,
                                            iris_camera_metadata_notify_cb,
                                            (void *)ctx,
                                            PREVIEW_BUF_NUM);
    if (NULL == metadata_stream) {
        IRIS_CAMERA_LOG("%s: add metadata stream failed\n", __func__);
        iris_camera_del_channel(ctx, channel);
        return rc;
    }

    rc = iris_camera_start_channel(ctx, channel);
    if (IRIS_CAMERA_OK != rc) {
        IRIS_CAMERA_LOG("%s:start preview failed rc=%d\n", __func__, rc);
        if (channel->num_streams <= MAX_STREAM_NUM_IN_BUNDLE) {
            for (i = 0; i < channel->num_streams; i++) {
                stream = &channel->streams[i];
                iris_camera_del_stream(ctx, channel, stream);
            }
        }
        iris_camera_del_channel(ctx, channel);
        return rc;
    }

    return rc;
}

static int iris_camera_stop_preview_internal(iris_camera_ctx_t *ctx)
{
    int rc = IRIS_CAMERA_OK;
    iris_camera_channel_t *channel =
        iris_camera_get_channel_by_type(ctx, IRIS_CHANNEL_TYPE_PREVIEW);

    rc = iris_camera_stop_and_del_channel(ctx, channel);
    if (IRIS_CAMERA_OK != rc) {
        IRIS_CAMERA_LOG("%s:Stop Preview failed rc=%d\n", __func__, rc);
    }

    return rc;
}

// Exported Functions

int iris_camera_open(iris_camera_handle *handle, int cam_id)
{
    int rc = IRIS_CAMERA_OK;
    cam_capability_t camera_cap;

    if ( NULL == handle ) {
        IRIS_CAMERA_LOG(" %s : Invalid handle", __func__);
        return IRIS_CAMERA_E_INVALID_INPUT;
    }

    memset(handle, 0, sizeof(iris_camera_handle));
    rc = iris_camera_load_hal(&handle->hal_info);
    if( IRIS_CAMERA_OK != rc ) {
        IRIS_CAMERA_LOG("%s:iris_camera_init err\n", __func__);
        return rc;
    }

    rc = iris_camera_open_internal(&handle->hal_info, (uint8_t)cam_id, &handle->ctx);
    if (rc != IRIS_CAMERA_OK) {
        IRIS_CAMERA_LOG("%s:iris_camera_open() cam_idx=%d, err=%d\n",
                   __func__, cam_id, rc);
        return rc;
    }

    // Configure focus mode after stream starts
    rc = iris_camera_get_caps(handle, &camera_cap);
    if ( IRIS_CAMERA_OK != rc ) {
        IRIS_CAMERA_LOG("%s:mm_camera_lib_get_caps() err=%d\n", __func__, rc);
        iris_camera_close_internal(&handle->ctx);
        return rc;
    }

    if (camera_cap.supported_focus_modes_cnt == 1 &&
        camera_cap.supported_focus_modes[0] == CAM_FOCUS_MODE_FIXED) {
        IRIS_CAMERA_LOG("focus not supported");
        handle->ctx.focus_supported = false;
        handle->ctx.af_mode = CAM_FOCUS_MODE_FIXED;
    } else {
        handle->ctx.focus_supported = true;
    }

    return rc;
}

int iris_camera_start_preview(iris_camera_handle *handle)
{
    int rc = IRIS_CAMERA_OK;

    if ( NULL == handle ) {
        IRIS_CAMERA_LOG(" %s : Invalid handle", __func__);
        return IRIS_CAMERA_E_INVALID_INPUT;
    }

    rc = iris_camera_start_preview_internal(&handle->ctx);
    if (rc != IRIS_CAMERA_OK) {
        IRIS_CAMERA_LOG("%s: iris_camera_start_preview() err=%d\n",
                   __func__, rc);
        return rc;
    }

    rc = iris_camera_set_focus_mode(&handle->ctx, handle->ctx.af_mode);
    if (rc != IRIS_CAMERA_OK) {
      IRIS_CAMERA_LOG("%s:autofocus error\n", __func__);
    }
    handle->stream_running = 1;

#ifndef IRIS_CAMERA_HAL_V1
    iris_camera_streaming_thread_start(&handle->ctx);
#endif

    return rc;
}

int iris_camera_stop_preview(iris_camera_handle *handle)
{
    int rc = IRIS_CAMERA_OK;

    if ( NULL == handle ) {
        IRIS_CAMERA_LOG(" %s : Invalid handle", __func__);
        return IRIS_CAMERA_E_INVALID_INPUT;
    }

    iris_camera_streaming_thread_stop(&handle->ctx);

    rc = iris_camera_stop_preview_internal(&handle->ctx);
    if (rc != IRIS_CAMERA_OK) {
        IRIS_CAMERA_LOG("%s: iris_camera_stop_preview() err=%d\n",
                   __func__, rc);
    }

    handle->stream_running = 0;

    return rc;
}

int iris_camera_send_command(iris_camera_handle *handle,
                               iris_camera_commands cmd,
                               void *in_data, void *out_data)
{
    int rc = IRIS_CAMERA_OK;

    if ( NULL == handle ) {
        IRIS_CAMERA_LOG(" %s : Invalid handle", __func__);
        return IRIS_CAMERA_E_INVALID_INPUT;
    }

    switch(cmd) {
    case IRIS_CAMERA_CMD_FLASH:
        if ( NULL != in_data ) {
            cam_flash_mode_t flash = *(( int * )in_data);
            rc = iris_camera_set_flash_mode(&handle->ctx, flash);
            if (rc != IRIS_CAMERA_OK) {
                    IRIS_CAMERA_LOG("%s: setFlash() err=%d\n", __func__, rc);
            }
        }
        break;

    case IRIS_CAMERA_CMD_SET_FOCUS_MODE:
        if ( NULL != in_data) {
            cam_focus_mode_type mode = *((cam_focus_mode_type *)in_data);
            handle->ctx.af_mode = mode;
            rc = iris_camera_set_focus_mode(&handle->ctx, mode);
            if (rc != IRIS_CAMERA_OK) {
              IRIS_CAMERA_LOG("%s:autofocus error\n", __func__);
            }
        }
        break;

    case IRIS_CAMERA_CMD_SET_NR_MODE:
        if ( NULL != in_data) {
            handle->ctx.nr_mode = *((cam_noise_reduction_mode_t *)in_data);
            handle->ctx.frame_param_update = true;
            IRIS_CAMERA_LOG("%s: new sensor nr mode %d", __func__, handle->ctx.nr_mode);
        }
        break;

    case IRIS_CAMERA_CMD_AEC_FORCE_GAIN:
        if ( NULL != in_data) {
            handle->ctx.sensitivity = *((uint32_t *)in_data);
            handle->ctx.frame_param_update = true;
            IRIS_CAMERA_LOG("%s: new sensor gain %d", __func__, handle->ctx.sensitivity);
        }
        break;

    case IRIS_CAMERA_CMD_AEC_FORCE_EXP:
        if ( NULL != in_data) {
            handle->ctx.exp_time_ns = *((uint32_t *)in_data) * 1000000;
            handle->ctx.frame_param_update = true;
            IRIS_CAMERA_LOG("%s: new sensor exposure time %lld", __func__, handle->ctx.exp_time_ns);
        }
        break;

    case IRIS_CAMERA_CMD_NO_ACTION:
        default:
            break;
    };

    return rc;
}

int iris_camera_get_number_of_cameras(iris_camera_handle *handle)
{
    if ( NULL == handle ) {
        IRIS_CAMERA_LOG(" %s : Invalid handle", __func__);
        return 0;
    }

    return  handle->hal_info.num_cameras;
}

int iris_camera_close(iris_camera_handle *handle)
{
    int rc = IRIS_CAMERA_OK;

    if ( NULL == handle ) {
        IRIS_CAMERA_LOG(" %s : Invalid handle", __func__);
        return IRIS_CAMERA_E_INVALID_INPUT;
    }

    rc = iris_camera_close_internal(&handle->ctx);
    if (rc != IRIS_CAMERA_OK) {
        IRIS_CAMERA_LOG("%s:iris_camera_close() err=%d\n",
                   __func__, rc);
    }

    return rc;
}

int iris_camera_set_preview_cb(
        iris_camera_handle *handle, cam_stream_user_cb cb)
{
    if (handle->ctx.user_preview_cb != NULL) {
        IRIS_CAMERA_LOG("%s, already set preview callbacks\n", __func__);
        return IRIS_CAMERA_E_INVALID_OPERATION;
    }
    handle->ctx.user_preview_cb = *cb;

    return IRIS_CAMERA_OK;
}

int iris_camera_set_preview_resolution(iris_camera_handle *handle,
        uint16_t width, uint16_t height)
{
    handle->ctx.preview_width = width;
    handle->ctx.preview_height = height;

    return IRIS_CAMERA_OK;
}

int iris_camera_release_preview_buf(
        iris_camera_callback_buf_t *buf)
{
    int rc = IRIS_CAMERA_OK;
    iris_camera_ctx_t *ctx = (iris_camera_ctx_t*)buf->user_data;

     if (ctx->need_release_preview_buf)
        rc = iris_camera_put_preview_buf(ctx, buf->camera_handle, buf->ch_id, buf->frame);

    return rc;
}

