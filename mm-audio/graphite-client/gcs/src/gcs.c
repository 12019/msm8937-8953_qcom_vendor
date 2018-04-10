/*
 * gcs.c
 *
 * This library contains the API to load setup, start, stop and tear-down
 * a use case for Codec DSP using Graphite (Graph-Lite) architecture.
 *
 * Copyright (c) 2016 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */

#include <unistd.h>
#include <stdlib.h>
#include <osal_types.h>
#include <osal_errorcodes.h>
#include <osal_log.h>
#include <osal_list.h>
#include <osal_mem.h>
#include <graphite.h>
#include <packetizer.h>
#include <cal_wrapper.h>
#include <gcs.h>

#define gcs_for_each_module_in_subgraph(sg, mod_cnt, mod_ptr, n)             \
    for(n = 0,                                                               \
        mod_ptr = (struct graphite_module *)(((int8_t *)sg->mod_list)        \
            + sizeof(struct graphite_cal_header));                           \
        n < mod_cnt;                                                         \
        n++, mod_ptr = (struct graphite_module *)(((int8_t *)mod_ptr) +      \
                 sizeof(struct graphite_module)))

static uint32_t gcs_init_ref_cnt;
struct gcs *gcs;

/* Structure for Indirect calibration payload */
#include <osal_pack_start.h>
struct gcs_indirect_data_pld {
    struct graphite_cal_header cal_hdr;
    struct graphite_indirect_cal_data cal_info;
}
#include <osal_pack_end.h>
;

/* Structure for Event registration/deregistration payload */
#include <osal_pack_start.h>
struct gcs_event_pld {
    struct graphite_cal_header cal_hdr;
    struct graphite_event_header ev_hdr;
}
#include <osal_pack_end.h>
;

/* Callback handlers to be registered with IPC layer */
int32_t gcs_event_cb_handler(void *payload, uint32_t payload_size);
int32_t gcs_data_cmd_cb_handler(void *payload, uint32_t payload_size,
                                void *priv_data, bool *is_basic_rsp);

/*
 * This function is called by caller in _unlocked_ context GCS lock.
 * The function will take GCS lock.
 */
static struct gcs_graph_pdata *gcs_get_pdata_from_graph_handle(uint32_t handle)
{
    struct gcs_graph_info *g_pos;
    struct gcs_graph_pdata *pdata = NULL;

    osal_lock_acquire(gcs->lock);

    /* Search through the list of graphs and find right graph_handle */
    osal_list_for_each_entry(struct gcs_graph_info, g_pos,
                             &gcs->graph_list, list) {
        if (g_pos->graph_handle == handle) {
            pdata = g_pos->priv_data;
            break;
        }
    }

    osal_lock_release(gcs->lock);

    return pdata;
}

static int32_t gcs_check_graphite_response(uint32_t status)
{
    int32_t ret = OSAL_SUCCESS;

    switch(status) {
    case GRAPHITE_EOK:
        ret = OSAL_SUCCESS;
        break;
    case GRAPHITE_EFAILED:
        ret = OSAL_ENOTRECOVERABLE;
        break;
    case GRAPHITE_EBADPARAM:
        ret = OSAL_EBADPARAM;
        break;
    case GRAPHITE_EUNSUPPORTED:
        ret = OSAL_EUNSUPPORTED;
        break;
    case GRAPHITE_EUNEXPECTED:
        ret = OSAL_ENOTRECOVERABLE;
        break;
    case GRAPHITE_EALREADY:
        ret = OSAL_EALREADY;
        break;
    case GRAPHITE_ENOTREADY:
        ret = OSAL_EPERM;
        break;
    case GRAPHITE_ENOTIMPL:
        ret = OSAL_ENOSYS;
        break;
    default:
        OSAL_LOGE("%s: Unknown response %d\n", __func__, status);
        ret = OSAL_ENOTRECOVERABLE;
    }

    return ret;
}

static int32_t gcs_allocate_cmd_rsp(struct gcs_graph_pdata *pdata,
                                  struct pktzr_cmd_rsp **cmd_rsp)
{
    int32_t ret = OSAL_SUCCESS;
    struct pktzr_cmd_rsp *tmp;

    /* Allocate memory for command response struct to pass to IPC */
    tmp = osal_mem_zalloc(sizeof(struct pktzr_cmd_rsp));
    if (!tmp) {
        OSAL_LOGE("%s: Failed to allocate memory for pktzr_cmd_rsp\n", __func__);
        ret = OSAL_ENOMEMORY;
        goto exit;
    }

    /* Pass pre-allocated response buffer for response */
    tmp->buf = pdata->rsp;
    tmp->buf_size = pdata->rsp_buff_size;

    *cmd_rsp = tmp;
    return 0;

exit:
    return ret;
}

static int32_t __graph_open(struct gcs_graph_pdata *pdata)
{
    int32_t ret = OSAL_SUCCESS;
    struct pktzr_cmd_rsp *cmd_rsp;
    struct graphite_basic_rsp_result *rsp;

    ret = gcs_allocate_cmd_rsp(pdata, &cmd_rsp);
    if (ret)
        goto alloc_cmd_rsp_failed;

    /* Call IPC layer to send the command to Graphite server */
    ret = pktzr_graph_open(pdata->payload, pdata->payload_data_size, cmd_rsp);
    if (ret) {
        OSAL_LOGE("%s: Graph Open command failed. Exiting\n", __func__);
        goto graph_open_cmd_failed;
    }

    if (!cmd_rsp->rsp_size) {
        OSAL_LOGE("%s: 0 size rsp received. Exiting\n", __func__);
        ret = OSAL_ENOTRECOVERABLE;
        goto zero_size_cmd_rsp_rcvd;
    }

    /*
     * Check Graphite response for the command and return a
     * corresponding OS-specific error to caller.
     */
    rsp = (struct graphite_basic_rsp_result *) cmd_rsp->buf;
    ret = gcs_check_graphite_response(rsp->status);

zero_size_cmd_rsp_rcvd:
graph_open_cmd_failed:
    osal_mem_free(cmd_rsp);

alloc_cmd_rsp_failed:
    pdata->payload_data_size = 0;
    return ret;
}

static int32_t __graph_close(struct gcs_graph_pdata *pdata)
{
    int32_t ret = OSAL_SUCCESS;
    struct pktzr_cmd_rsp *cmd_rsp;
    struct graphite_basic_rsp_result *rsp;

    ret = gcs_allocate_cmd_rsp(pdata, &cmd_rsp);
    if (ret)
        goto alloc_cmd_rsp_failed;

    /* Call IPC layer to send the command to Graphite server */
    ret = pktzr_graph_close(pdata->payload, pdata->payload_data_size, cmd_rsp);
    if (ret) {
        OSAL_LOGE("%s: Graph Close command failed. Exiting\n", __func__);
        goto graph_close_cmd_failed;
    }

    if (!cmd_rsp->rsp_size) {
        OSAL_LOGE("%s: 0 size rsp received. Exiting\n", __func__);
        ret = OSAL_ENOTRECOVERABLE;
        goto zero_size_cmd_rsp_rcvd;
    }

    /*
     * Check Graphite response for the command and return a
     * corresponding OS-specific error to caller.
     */
    rsp = (struct graphite_basic_rsp_result *) cmd_rsp->buf;
    ret = gcs_check_graphite_response(rsp->status);

zero_size_cmd_rsp_rcvd:
graph_close_cmd_failed:
    osal_mem_free(cmd_rsp);

alloc_cmd_rsp_failed:
    pdata->payload_data_size = 0;
    return ret;
}

static uint32_t __load_data(struct gcs_graph_pdata *pdata, void *data,
                            uint32_t data_size, bool use_island,
                            uint32_t *load_addr)
{
    int32_t ret = OSAL_SUCCESS;
    struct pktzr_cmd_rsp *cmd_rsp;
    struct graphite_basic_rsp_result *rsp_basic;
    struct graphite_load_data_rsp *rsp;

    /* Allocate memory for command response struct to pass to IPC */
    ret = gcs_allocate_cmd_rsp(pdata, &cmd_rsp);
    if (ret)
        goto alloc_cmd_rsp_failed;

    /* Call IPC layer to send the command to Graphite server */
    ret = pktzr_load_data(data, data_size, cmd_rsp, use_island);
    if (ret) {
        OSAL_LOGE("%s: Load Data command failed. Exiting\n", __func__);
        goto load_data_cmd_failed;
    }

    if (cmd_rsp->is_basic_rsp == true) {
        /*
         * For some reason, the command failed.
         * Check Graphite error response for the command and return a
         * corresponding OS-specific error to caller
         */
        rsp_basic = (struct graphite_basic_rsp_result *) cmd_rsp->buf;
        ret = gcs_check_graphite_response(rsp_basic->status);
    } else {
        if (!cmd_rsp->rsp_size) {
            OSAL_LOGE("%s: 0 size rsp received. Exiting\n", __func__);
            ret = OSAL_ENOTRECOVERABLE;
            goto zero_size_cmd_rsp_rcvd;
        }

        rsp = (struct graphite_load_data_rsp *) cmd_rsp->buf;
        if (!rsp->addr) {
            OSAL_LOGE("%s: Failed to load Data in memory. Exiting\n", __func__);
            ret = OSAL_ENOTRECOVERABLE;
        } else {
            *load_addr = rsp->addr;
            ret = 0;
       }
    }

zero_size_cmd_rsp_rcvd:
load_data_cmd_failed:
    osal_mem_free(cmd_rsp);

alloc_cmd_rsp_failed:
    pdata->payload_data_size = 0;
    return ret;
}

static int32_t __unload_data(struct gcs_graph_pdata *pdata,
                             uint32_t persist_cal_addr)
{
    int32_t ret = OSAL_SUCCESS;
    struct graphite_unload_data *cmd;
    struct pktzr_cmd_rsp *cmd_rsp;
    struct graphite_basic_rsp_result *rsp;

    /* Allocate memory for command response struct to pass to IPC */
    ret = gcs_allocate_cmd_rsp(pdata, &cmd_rsp);
    if (ret)
        goto alloc_cmd_rsp_failed;

    /* Fill payload */
    cmd = (struct graphite_unload_data *) pdata->payload;
    cmd->addr = persist_cal_addr;

    /* Call IPC layer to send the command to Graphite server */
    ret = pktzr_unload_data(cmd, sizeof(struct graphite_unload_data), cmd_rsp);
    if (ret) {
        OSAL_LOGE("%s: Unload Data command failed. Exiting\n", __func__);
        goto unload_data_cmd_failed;
    }

    if (!cmd_rsp->rsp_size) {
        OSAL_LOGE("%s: 0 size rsp received. Exiting\n", __func__);
        ret = OSAL_ENOTRECOVERABLE;
        goto zero_size_cmd_rsp_rcvd;
    }

    /*
     * Check Graphite response for the command and return a
     * corresponding OS-specific error to caller.
     */
    rsp = (struct graphite_basic_rsp_result *) cmd_rsp->buf;
    ret = gcs_check_graphite_response(rsp->status);

zero_size_cmd_rsp_rcvd:
    osal_mem_free(cmd_rsp);

unload_data_cmd_failed:
alloc_cmd_rsp_failed:
    pdata->payload_data_size = 0;
    return ret;
}

static int32_t gcs_copy_to_payload(struct gcs_graph_pdata *pdata,
                                   uint8_t *src, uint32_t src_size)
{
    int8_t *ptr;

    /*
     * Check if the payload buffer size is large enough to hold new data.
     * Else re-alloc the buffer with the new size.
     */
    if ((pdata->payload_data_size + src_size) > pdata->payload_buff_size) {
        OSAL_LOGI("%s: graph payload memory realloc, new size = %d\n",
                  __func__, (pdata->payload_data_size + src_size));
        ptr = (int8_t *) osal_mem_realloc(pdata->payload,
                                          (pdata->payload_data_size +
                                          src_size));
        if (!ptr) {
            OSAL_LOGE("%s: Mem realloc failed for graph payload\n", __func__);
            return OSAL_ENOMEMORY;
        }
        /* Update payload pointer and size */
        pdata->payload = ptr;
        pdata->payload_buff_size += GCS_GRAPH_PAYLOAD_SIZE;
    }
    /* Copy the new content to payload */
    osal_mem_cpy((pdata->payload + pdata->payload_data_size),
                 src_size, src, src_size);
    pdata->payload_data_size += src_size;

    return 0;
}

static int32_t gcs_get_sg_module_list(struct gcs_graph_pdata *pdata,
                                      struct gcs_sub_graph *sg)
{
    int32_t ret = OSAL_SUCCESS;
    uint32_t mod_list_size = 0;
    uint8_t *mod_list_buff;

    ret = gcs_cal_get_subgraph_mod_info_list_size(pdata->u_id, pdata->d_id,
                                                  sg->sg_id,
                                                  &mod_list_size);
    if (ret) {
        OSAL_LOGE("%s: Failed to get module list cal size. ret = %d\n",
                __func__, ret);
        goto get_mod_list_size_failed;
    }
    if (mod_list_size == 0) {
        OSAL_LOGE("%s: Module list cal size 0. Exiting\n", __func__);
        goto get_mod_list_size_failed;
    }

    mod_list_buff = osal_mem_zalloc(mod_list_size);
    if (!mod_list_buff) {
        OSAL_LOGE("%s: Memory alloc failed for sg_list cal buffer, size = %d\n",
                  __func__, mod_list_size);
        ret = OSAL_ENOMEMORY;
        goto get_mod_list_buff_alloc_failed;
    }
    ret = gcs_cal_get_subgraph_mod_info_list(pdata->u_id, pdata->d_id,
                                             sg->sg_id,
                                             mod_list_buff,
                                             mod_list_size);
    if (ret) {
        OSAL_LOGE("%s: Failed to get module list for %s, ret = %d\n",
                __func__, gcs_sgt_name[sg->sg_id - 1], ret);
        goto get_mod_list_failed;
    }

    sg->mod_list = (uint32_t *)mod_list_buff;
    sg->mod_list_size = mod_list_size;

    return 0;

get_mod_list_failed:
    osal_mem_free(mod_list_buff);

get_mod_list_buff_alloc_failed:
get_mod_list_size_failed:
    return ret;
}

static int32_t gcs_get_sg_cal_and_update_pld(struct gcs_graph_pdata *pdata,
                                           struct gcs_sub_graph *sg_pos)
{
    int32_t ret = OSAL_SUCCESS;
    uint32_t sg_cal_size = 0;
    uint8_t *sg_cal_buff;

    ret = gcs_cal_get_subgraph_mod_cal_size(pdata->u_id,
                                            pdata->d_id,
                                            sg_pos->sg_id,
                                            &sg_cal_size);
    if (ret) {
        OSAL_LOGE("%s: Failed to get size of subgraph cal, ret = %d\n",
                __func__, ret);
        goto get_sg_cal_size_failed;
    }
    if (sg_cal_size == 0) {
        OSAL_LOGE("%s: subgraph cal size 0. Exiting\n", __func__);
        ret = OSAL_ENOTRECOVERABLE;
        goto get_sg_cal_size_failed;
    }

    sg_cal_buff = osal_mem_zalloc(sg_cal_size);
    if (!sg_cal_buff) {
        OSAL_LOGE("%s: Memory alloc failed for sg cal buffer, size = %d\n",
                  __func__, sg_cal_size);
        ret = OSAL_ENOMEMORY;
        goto get_sg_cal_buff_alloc_failed;
    }

    ret = gcs_cal_get_subgraph_mod_cal(pdata->u_id, pdata->d_id,
                                       sg_pos->sg_id,
                                       sg_cal_buff, sg_cal_size);
    if (ret) {
        OSAL_LOGE("%s: Failed to get subgraph cal, ret = %d\n",
                __func__, ret);
        goto get_sg_cal_failed;
    }
    ret = gcs_copy_to_payload(pdata, sg_cal_buff, sg_cal_size);
    if (ret )
        OSAL_LOGE("%s: Failed to copy sg%d cal to payload, ret = %d\n",
                __func__, sg_pos->sg_id, ret);

get_sg_cal_failed:
    osal_mem_free(sg_cal_buff);

get_sg_cal_buff_alloc_failed:
get_sg_cal_size_failed:
    return ret;
}

static int32_t gcs_get_sg_mod_conn_and_update_pld(struct gcs_graph_pdata *pdata,
                                                struct gcs_sub_graph *sg_pos)
{
    int32_t ret = OSAL_SUCCESS;
    uint32_t sg_mod_conn_size = 0;
    uint8_t *sg_mod_conn_buff;

    ret = gcs_cal_get_subgraph_mod_conn_size(pdata->u_id,
                                             pdata->d_id,
                                             sg_pos->sg_id,
                                             &sg_mod_conn_size);
    if (ret) {
        OSAL_LOGE("%s: Failed to get size of sg mod conn, ret = %d\n",
                __func__, ret);
        goto sg_mod_conn_size_failed;
    }
    if (sg_mod_conn_size == 0) {
        OSAL_LOGE("%s: subgraph mod conn size 0. Exiting\n",
                __func__);
        ret = 0;
        goto sg_mod_conn_returned_size_zero;
    }

    sg_mod_conn_buff = osal_mem_zalloc(sg_mod_conn_size);
    if (!sg_mod_conn_buff) {
        OSAL_LOGE("%s: Memory alloc failed for sg mod conn buffer, size = %d\n",
                __func__, sg_mod_conn_size);
        ret = OSAL_ENOMEMORY;
        goto sg_mod_conn_buff_alloc_failed;
    }

    ret = gcs_cal_get_subgraph_mod_conn(pdata->u_id, pdata->d_id,
                                        sg_pos->sg_id,
                                        sg_mod_conn_buff,
                                        sg_mod_conn_size);
    if (ret) {
        OSAL_LOGE("%s: Failed to get subgraph mod conn, ret = %d\n",
                __func__, ret);
        goto sg_mod_conn_failed;
    }
    ret = gcs_copy_to_payload(pdata, sg_mod_conn_buff, sg_mod_conn_size);
    if (ret )
        OSAL_LOGE("%s: Failed to copy sg%d mod conn to payload, ret = %d\n",
                __func__, sg_pos->sg_id, ret);

sg_mod_conn_failed:
    osal_mem_free(sg_mod_conn_buff);

sg_mod_conn_buff_alloc_failed:
sg_mod_conn_returned_size_zero:
sg_mod_conn_size_failed:
    return ret;
}

static int32_t __gcs_get_sg_conn_and_update_pld(struct gcs_graph_pdata *pdata,
                                            struct gcs_sub_graph *src,
                                            struct gcs_sub_graph *dst)
{
    int32_t ret = OSAL_SUCCESS;
    uint32_t sg_conn_size = 0;
    uint8_t *sg_conn_buff;

    ret = gcs_cal_get_subgraph_conn_size(pdata->u_id, pdata->d_id,
                                         src->sg_id, dst->sg_id,
                                         &sg_conn_size);
    if (ret) {
        OSAL_LOGE("%s: Failed to get size of sg-%d and sg-%d conn, ret = %d\n",
                __func__, src->sg_id, dst->sg_id, ret);
        goto get_sg_conn_size_failed;
    }
    if (sg_conn_size == 0) {
        OSAL_LOGE("%s: subgraph conn size 0. Exiting\n", __func__);
        ret = 0;
        goto get_sg_conn_returned_size_zero;
    }

    sg_conn_buff = osal_mem_zalloc(sg_conn_size);
    if (!sg_conn_buff) {
        OSAL_LOGE("%s: Memory alloc failed for sg conn buffer, size = %d\n",
                  __func__, sg_conn_size);
        ret = OSAL_ENOMEMORY;
        goto get_sg_conn_buff_alloc_failed;
    }
    ret = gcs_cal_get_subgraph_conn(pdata->u_id, pdata->d_id,
                                    src->sg_id, dst->sg_id,
                                    sg_conn_buff, sg_conn_size);
    if (ret) {
        OSAL_LOGE("%s: Failed to get subgraph conn, ret = %d\n", __func__, ret);
        goto get_sg_conn_failed;
    }
    ret = gcs_copy_to_payload(pdata, sg_conn_buff, sg_conn_size);
    if (ret )
        OSAL_LOGE("%s: Failed to copy sg%d to sg-%d conn to payload, ret = %d\n",
                __func__, src->sg_id, dst->sg_id, ret);

get_sg_conn_failed:
    osal_mem_free(sg_conn_buff);

get_sg_conn_buff_alloc_failed:
get_sg_conn_returned_size_zero:
get_sg_conn_size_failed:
    return ret;
}

static int32_t gcs_get_sg_conn_and_update_pld(struct gcs_graph_pdata *pdata)
{
    int32_t ret = OSAL_SUCCESS;
    struct gcs_sub_graph *sg_pos;
    struct gcs_sub_graph *nxt;

    osal_list_for_each_entry(struct gcs_sub_graph, sg_pos, &pdata->sg_list, list) {
        if (sg_pos->sg_id == GCS_SGT_SINK)
            continue;

        nxt = osal_get_list_item(sg_pos->list.next, struct gcs_sub_graph, list);

        osal_list_for_each_entry_from(nxt, &pdata->sg_list, list) {
            if (nxt->sg_id == GCS_SGT_SINK)
                continue;

            ret = __gcs_get_sg_conn_and_update_pld(pdata, sg_pos, nxt);
            if (ret) {
                OSAL_LOGE("%s: Failed to get conn from sg-%d to sg-%d\n",
                        __func__, sg_pos->sg_id, nxt->sg_id);
                goto exit;
            }
            ret = __gcs_get_sg_conn_and_update_pld(pdata, nxt, sg_pos);
            if (ret) {
                OSAL_LOGE("%s: Failed to get conn from sg-%d to sg-%d\n",
                        __func__, nxt->sg_id, sg_pos->sg_id);
                goto exit;
            }
        }
    }

exit:
    return ret;
}

static int32_t gcs_get_sg_cal_and_update_pl(struct gcs_graph_pdata *pdata, uint32_t cmd)
{
    int32_t ret = OSAL_SUCCESS;
    struct gcs_sub_graph *sg_pos;

    osal_list_for_each_entry(struct gcs_sub_graph, sg_pos, &pdata->sg_list, list) {
        if (sg_pos->sg_id == GCS_SGT_SINK)
            continue;

        switch(cmd) {
        case GCS_CMD_COPY_MOD_LIST_TO_PL:
            ret = gcs_copy_to_payload(pdata, (uint8_t *)sg_pos->mod_list,
                                      sg_pos->mod_list_size);
            if (ret)
                goto exit;
            break;
        case GCS_CMD_GET_SG_CAL_AND_UPDATE_PL:
            ret = gcs_get_sg_cal_and_update_pld(pdata, sg_pos);
            if (ret)
                goto exit;
            break;
        case GCS_CMD_GET_SG_MOD_CONN_AND_UPDATE_PL:
            ret = gcs_get_sg_mod_conn_and_update_pld(pdata, sg_pos);
            if (ret)
                goto exit;
            break;
        }
    }

exit:
    return ret;
}

static int32_t gcs_create_indirect_data_pld(struct gcs_graph_pdata *pdata,
                                            struct gcs_persist_data_info *prst_data)
{
    int32_t ret = OSAL_SUCCESS;
    struct gcs_indirect_data_pld *ind_cal;

    ind_cal = (struct gcs_indirect_data_pld *)osal_mem_zalloc(
                  sizeof(struct gcs_indirect_data_pld));
    if (!ind_cal) {
        OSAL_LOGE("%s: Memory alloc failed for indirect cal payload\n",
                __func__);
        ret = OSAL_ENOMEMORY;
        goto failed_to_alloc_ind_cal;
    }

    /* Fill Calibration header */
    ind_cal->cal_hdr.module_id = GRAPHITE_MODULE_FWK;
    ind_cal->cal_hdr.instance_id = 0;
    ind_cal->cal_hdr.param_id = GRAPHITE_PARAM_ID_INDIRECT_CALIB_DATA;
    ind_cal->cal_hdr.size = sizeof(struct graphite_indirect_cal_data);

    /* Fill Calibration Information */
    ind_cal->cal_info.addr = prst_data->cal_load_addr;
    ind_cal->cal_info.size = prst_data->data_size;

    ret = gcs_copy_to_payload(pdata, (uint8_t *)ind_cal,
                              sizeof(struct gcs_indirect_data_pld));
    if (ret ) {
        OSAL_LOGE("%s: Failed to copy ind cal for data_handle = %d, ret = %d\n",
                __func__, prst_data->data_handle, ret);
        goto copy_ind_cal_to_payload_failed;
    }

copy_ind_cal_to_payload_failed:
failed_to_alloc_ind_cal:
    return ret;
}

static int32_t gcs_create_event_payload(struct gcs_graph_pdata *pdata,
                                        struct gcs_event *ev_info)
{
    int32_t ret = OSAL_SUCCESS;
    struct gcs_event_pld *ev_pld;

    ev_pld = (struct gcs_event_pld *)osal_mem_zalloc(
                 sizeof(struct gcs_event_pld));
    if (!ev_pld) {
        OSAL_LOGE("%s: Memory alloc failed for event payload\n", __func__);
        ret = OSAL_ENOMEMORY;
        goto failed_to_alloc_ev_pld;
    }

    /* Fill Calibration header */
    ev_pld->cal_hdr.module_id = GRAPHITE_MODULE_FWK;
    ev_pld->cal_hdr.instance_id = 0;
    ev_pld->cal_hdr.param_id = GRAPHITE_PARAM_ID_EVENT_REGISTER;
    ev_pld->cal_hdr.size = sizeof(struct graphite_event_header);

    /* Fill Event header */
    ev_pld->ev_hdr.module_id = ev_info->module_param_info.module_info.MID;
    ev_pld->ev_hdr.instance_id = ev_info->module_param_info.module_info.IID;
    ev_pld->ev_hdr.param_id = ev_info->module_param_info.PID;

    ret = gcs_copy_to_payload(pdata, (uint8_t *)ev_pld,
                              sizeof(struct gcs_event_pld));
    if (ret ) {
        OSAL_LOGE("%s: Failed to copy event info for event PID = %d, ret = %d\n",
                __func__, ev_info->module_param_info.PID, ret);
        goto copy_event_info_to_payload_failed;
    }

copy_event_info_to_payload_failed:
failed_to_alloc_ev_pld:
    return ret;
}

static inline bool match_event(struct gcs_module_param_info *info,
                               struct graphite_cal_header *ev_hdr)
{
    return ((info->module_info.MID == ev_hdr->module_id) &&
            (info->module_info.IID == ev_hdr->instance_id) &&
            (info->PID == ev_hdr->param_id));
}

static int32_t gcs_dispatch_event(struct gcs_graph_info *graph,
                                  struct graphite_cal_header *ev_hdr,
                                  struct gcs_event_rsp *ev_rsp)
{
    struct gcs_graph_pdata *pdata = NULL;
    uint32_t graph_handle;
    struct gcs_event *ev_pos;
    int32_t found = 0;

    graph_handle = graph->graph_handle;
    pdata = graph->priv_data;

    osal_lock_acquire(pdata->lock);

    osal_list_for_each_entry(struct gcs_event, ev_pos ,&pdata->event_list, list) {
        if (match_event(&ev_pos->module_param_info, ev_hdr)) {
            OSAL_LOGI("%s: Event found in graph_handle = %d\n",
                     __func__, graph_handle);
            osal_mem_set(ev_rsp, 0, sizeof(struct gcs_event_rsp));
            osal_mem_cpy(&ev_rsp->module_param_info,
                         sizeof(struct gcs_module_param_info),
                         &ev_pos->module_param_info,
                         sizeof(struct gcs_module_param_info));
            ev_rsp->payload_size = ev_hdr->size;
            ev_rsp->payload = ((int8_t *)ev_hdr) +
                               sizeof(struct graphite_cal_header);
            if (ev_pos->cb_handler)
                ev_pos->cb_handler(graph_handle, ev_rsp, ev_pos->cookie);
            found = 1;
            goto event_dispatch_done;
        }
    }

event_dispatch_done:
    osal_lock_release(pdata->lock);

    return found;
}

int32_t gcs_event_cb_handler(void *payload, uint32_t payload_size)
{
    int32_t ret = OSAL_SUCCESS;
    struct gcs_event_rsp *ev_rsp;
    struct gcs_graph_info *g_pos;
    struct graphite_cal_header *ev_hdr;
    int32_t match_found = 0;
    size_t ev_size;

    if (gcs == NULL) {
        OSAL_LOGE("%s: GCS not initialized. Exiting\n", __func__);
        return OSAL_EBADPARAM;
    }

    if (!payload) {
        OSAL_LOGE("%s: Event payload is NULL. Exiting\n", __func__);
        ret = OSAL_EBADPARAM;
        goto exit;
    }

    /* The payload should at a minimum contain one event (with header) */
    if (payload_size < sizeof(struct graphite_cal_header)) {
        OSAL_LOGE("%s: Event payload corrupted. Exiting\n", __func__);
        ret = OSAL_EBADPARAM;
        goto exit;
    }

    osal_lock_acquire(gcs->lock);

    ev_rsp = osal_mem_zalloc(sizeof(struct gcs_event_rsp));
    if (!ev_rsp) {
        OSAL_LOGE("%s: Memory alloc failed for event rsp struct\n", __func__);
        ret = OSAL_ENOMEMORY;
        goto failed_to_alloc_event_rsp;
    }

    /*
     * Multiple events can be concatenated in the same payload.
     * Event header is same as calibration header.
     * Each event has to be delivered to all the clients registered
     * for the event.
     */
    do {
        ev_hdr = (struct graphite_cal_header *) payload;
        OSAL_LOGI("%s: Event received, MID = %d, IID = %d, PID = %d\n",
                 __func__, ev_hdr->module_id, ev_hdr->instance_id,
                 ev_hdr->param_id);
        OSAL_LOGI("%s: Event payload = %p, payload_size = %d\n",
                 __func__, ev_hdr, ev_hdr->size);

        /*
         * Search through the list of graphs and match event in
         * each graph. Once found dispatch the event to the user
         * which registered to receive the event.
         */
        osal_list_for_each_entry(struct gcs_graph_info, g_pos,
                                 &gcs->graph_list, list)
            match_found += gcs_dispatch_event(g_pos, ev_hdr, ev_rsp);

        OSAL_LOGI("%s: Found %d match for event = %d\n",
                 __func__, match_found, ev_hdr->param_id);

        /* Go on to the next event in the payload */
        ev_size = sizeof(struct graphite_cal_header) + ev_hdr->size;
        payload = ((int8_t *)payload) + ev_size;
        payload_size -= ev_size;
    } while (payload_size >= sizeof(struct graphite_cal_header));

    osal_mem_free(ev_rsp);

failed_to_alloc_event_rsp:
    osal_lock_release(gcs->lock);

exit:
    return ret;
}

int32_t gcs_data_cmd_cb_handler(void *payload, uint32_t payload_size,
                                void *priv_data, bool *is_basic_rsp)
{
    /* TBD */
    OSAL_LOGI("%s: Called\n", __func__);
    return 0;
}

/*
 * gcs_register_for_event - Register a callback with GCS for an event
 *                          from a module
 *
 * @graph_handle:         handle to the graph in GCS
 * @module_param_info:    pointer to structure of type gcs_module_param_info that
 *                        contains Module and Event identifiers (MID, IID, PID).
 * @cb_handler:           pointer to the event callback handler
 * @cookie:               private data pointer that caller wants to get back with
 *                        Callback handler.
 *
 * Return 0 on success or error code otherwise
 *
 * Prerequisites
 *    Graph must be setup with GCS.
 */
int32_t gcs_register_for_event(uint32_t graph_handle,
                               struct gcs_module_param_info *module_param_info,
                               event_cb_ptr cb_handler, void *cookie)
{
    int32_t ret = OSAL_SUCCESS;
    struct gcs_graph_pdata *pdata = NULL;
    struct gcs_event *event;

    if (gcs == NULL) {
        OSAL_LOGE("%s: GCS not initialized. Exiting\n", __func__);
        return OSAL_EBADPARAM;
    }

    /* Identify private data from graph handle */
    pdata = gcs_get_pdata_from_graph_handle(graph_handle);
    if (pdata == NULL) {
        OSAL_LOGE("%s: No graph present for graph_handle = %d\n",
                __func__, graph_handle);
        ret = OSAL_EBADPARAM;
        goto exit;
    }

    osal_lock_acquire(pdata->lock);

    event = (struct gcs_event *) osal_mem_zalloc(sizeof (struct gcs_event));
    if (!event) {
        OSAL_LOGE("%s: Memory alloc failed for event struct\n",
                __func__);
        ret = OSAL_ENOMEMORY;
        goto failed_to_alloc_mem_for_event;
    }
    event->module_param_info.module_info.MID = module_param_info->module_info.MID;
    event->module_param_info.module_info.IID = module_param_info->module_info.IID;
    event->module_param_info.PID = module_param_info->PID;
    event->cb_handler = cb_handler;
    event->cookie = cookie;

    /* Add this event to the private data event list */
    osal_list_add_tail(&event->list, &pdata->event_list);

failed_to_alloc_mem_for_event:
    osal_lock_release(pdata->lock);

exit:
    return ret;
}

/*
 * gcs_load_data - Load user data in persistent memory for a graph
 *
 * @graph_handle:    handle to the graph in GCS
 * @data:            data to be loaded into persistent memory
 * @data_size:       size of the data to be loaded into persistent memory
 * @data_handle:     pointer to a handle to be returned by GCS for the
 *                   data once loaded into memory
 *
 * Return 0 on success or error code otherwise
 *
 * Prerequisites:
 *    Graph must be setup with GCS, but must NOT be active in Graphite server.
 *    The 'data' blob sent to this API must be packaged in Graphite format.
 *
 * Note:
 *    Information for all Persist data blob loaded for a graph will be sent to
 *    Graphite server only when enabling a graph (gcs_enable). Hence, the loaded
 *    data will be available to graphite server only at use case boundary.
 */
int32_t gcs_load_data(uint32_t graph_handle, void *data, uint32_t data_size,
                      uint32_t *data_handle)
{
    int32_t ret = OSAL_SUCCESS;
    struct gcs_graph_pdata *pdata;
    struct gcs_sub_graph *sg;
    struct graphite_cal_header *cal_hdr;
    uint32_t MID;
    uint16_t IID;
    uint32_t mod_cnt;
    struct graphite_module *mod_ptr;
    uint32_t i;
    bool mod_found = false;
    bool use_island = false;
    uint32_t load_addr = 0;
    struct gcs_persist_data_info *persist_data_info;

    if (gcs == NULL) {
        OSAL_LOGE("%s: GCS not initialized. Exiting\n", __func__);
        return OSAL_EBADPARAM;
    }

    /* Identify private data from graph handle */
    pdata = gcs_get_pdata_from_graph_handle(graph_handle);
    if (pdata == NULL) {
        OSAL_LOGE("%s: No graph present for graph_handle = %d\n",
                __func__, graph_handle);
        ret = OSAL_EBADPARAM;
        goto exit;
    }

    osal_lock_acquire(pdata->lock);

    /* If the Graph is not in setup state, do not load any persist data */
    if (pdata->state != GCS_GRAPH_SETUP) {
        OSAL_LOGE("%s: Cannot load data for the graph. Current state is: %s\n",
                __func__, gcs_state_name[pdata->state]);
        ret = OSAL_EBADPARAM;
        goto wrong_state;
    }

    /* Find out MID/IID of the intended module from data to be loaded */
    cal_hdr = (struct graphite_cal_header *)data;
    MID = cal_hdr->module_id;
    IID = cal_hdr->instance_id;

    /*
     * Compare MID/IID and find out whether the intended module will be
     * instantiated in island or non-island memory. The requested data
     * must be loaded in the same type of memory as used for module
     * instantiation.
     */
    osal_list_for_each_entry(struct gcs_sub_graph, sg, &pdata->sg_list, list) {
        cal_hdr = (struct graphite_cal_header *)sg->mod_list;
        mod_cnt = cal_hdr->size / (sizeof(struct graphite_module));
        gcs_for_each_module_in_subgraph(sg, mod_cnt, mod_ptr, i) {
            if (mod_ptr->module_id == MID &&
                mod_ptr->instance_id == IID) {
                mod_found = true;
                use_island = (mod_ptr->metadata &
                              (GRAPHITE_METADATA_ISLAND_FLAG_MASK <<
                               GRAPHITE_METADATA_ISLAND_FLAG_SHIFT) ?
                             true : false);
                break;
            }
        }
    }

    if (mod_found == false) {
        OSAL_LOGE("%s: No module found for loading data. Exiting\n", __func__);
        goto mod_not_found;
    }

    /* Allocate a local struct to store info about the loaded data */
    persist_data_info = osal_mem_zalloc(sizeof(struct gcs_persist_data_info));
    if (!persist_data_info) {
        OSAL_LOGE("%s: Memory alloc failed for persist data info struct\n",
                __func__);
        ret = OSAL_ENOMEMORY;
        goto failed_to_alloc_mem_for_persist_data_info;
    }

    /* Send command to load data into memory */
    ret = __load_data(pdata, data, data_size, use_island, &load_addr);
    if (ret) {
        OSAL_LOGE("%s: Data load failed, ret = %d\n", __func__, ret);
        goto data_load_failed;
    }

    /*
     * Store the persist data info in the private data persist
     * data list
     */
    persist_data_info->cal_load_addr = load_addr;
    persist_data_info->data_size = data_size;
    persist_data_info->data_handle = gcs_generate_data_handle(pdata);
    osal_list_add_tail(&persist_data_info->list, &pdata->persist_data_list);

    /* Return a handle to the loaded data to the caller */
    *data_handle = persist_data_info->data_handle;

    ret = OSAL_SUCCESS;
    goto done;

data_load_failed:
    osal_mem_free(persist_data_info);

done:
failed_to_alloc_mem_for_persist_data_info:
mod_not_found:
wrong_state:
    osal_lock_release(pdata->lock);

exit:
    return ret;
}

/*
 * gcs_unload_data - Unload/Free previously loaded data for a graph from
 *                   persistent memory.
 *
 * @graph_handle:    handle to the graph in GCS
 * @data_handle:     handle to the data loaded
 *
 * Return: 0 on success and error code otherwise
 *
 * Prerequisites:
 *    Graph must be setup with GCS, but must NOT be active in Graphite server.
 */
int32_t gcs_unload_data(uint32_t graph_handle, uint32_t data_handle)
{
    int32_t ret = OSAL_SUCCESS;
    struct gcs_graph_pdata *pdata;
    struct gcs_persist_data_info *pd_pos;
    bool found = false;

    if (gcs == NULL) {
        OSAL_LOGE("%s: GCS not initialized. Exiting\n", __func__);
        return OSAL_EBADPARAM;
    }

    /* Identify private data from graph handle */
    pdata = gcs_get_pdata_from_graph_handle(graph_handle);
    if (pdata == NULL) {
        OSAL_LOGE("%s: No graph present for graph_handle = %d\n",
                __func__, graph_handle);
        ret = OSAL_EBADPARAM;
        goto exit;
    }

    osal_lock_acquire(pdata->lock);

    /*
     * If the Graph is not in setup state, do not unload any
     * persist data.
     */
    if (pdata->state != GCS_GRAPH_SETUP) {
        OSAL_LOGE("%s: Cannot unload data for the graph. Current state is: %s\n",
                __func__, gcs_state_name[pdata->state] );
        ret = OSAL_EBADPARAM;
        goto wrong_state;
    }


    osal_list_for_each_entry(struct gcs_persist_data_info, pd_pos,
                             &pdata->persist_data_list, list) {
        if (pd_pos->data_handle == data_handle) {
            found = true;
            ret = __unload_data(pdata, pd_pos->cal_load_addr);
            if (ret)
                OSAL_LOGE("%s: Unload data failed. ret = %d\n", __func__, ret);
            osal_list_del(&pd_pos->list);
            osal_mem_free(pd_pos);
            break;
        }
    }

    if (found == false) {
        OSAL_LOGE("%s: No entry found for data_handle = %d\n",
                __func__, data_handle);
        ret = OSAL_ENOTRECOVERABLE;
        goto data_not_found;
    }

    ret = OSAL_SUCCESS;

data_not_found:
wrong_state:
    osal_lock_release(pdata->lock);

exit:
    return ret;
}

/*
 * gcs_enable - Enable a graph
 *
 * @graph_handle:        handle to the graph in GCS
 * @non_persist_ucal:    pointer to user calibration data that are not expected
 *                       to persist across multiple enable/disable of
 *                       the graph.
 * @size_ucal:           size of the non-persistent user calibration data blob.
 *
 * Return 0 on success or error code otherwise
 *
 * Prerequisites
 *    Graph must be setup with GCS.
 */
int32_t gcs_enable(uint32_t graph_handle, void *non_persist_ucal,
                   uint32_t size_ucal)
{
    int32_t ret = OSAL_SUCCESS;
    struct gcs_graph_pdata *pdata = NULL;
    struct gcs_persist_data_info *p_d_pos;
    struct gcs_event *event_pos;

    if (gcs == NULL) {
        OSAL_LOGE("%s: GCS not initialized. Exiting\n", __func__);
        return OSAL_EBADPARAM;
    }

    /* Identify private data from graph handle */
    pdata = gcs_get_pdata_from_graph_handle(graph_handle);
    if (pdata == NULL) {
        OSAL_LOGE("%s: No graph present for graph_handle = %d\n",
                __func__, graph_handle);
        ret = OSAL_EBADPARAM;
        goto exit;
    }

    osal_lock_acquire(pdata->lock);

    /* If the Graph is not in setup state, cannot enable the graph */
    if (pdata->state != GCS_GRAPH_SETUP) {
        OSAL_LOGE("%s: Cannot enable the graph. Current state is: %s\n",
                __func__, gcs_state_name[pdata->state] );
        ret = OSAL_EBADPARAM;
        goto wrong_state;
    }

    /* Copy module list for each subgraph to payload */
    ret = gcs_get_sg_cal_and_update_pl(pdata, GCS_CMD_COPY_MOD_LIST_TO_PL);
    if (ret ) {
        OSAL_LOGE("%s: Module list copy to payload failed, ret = %d\n",
                  __func__, ret);
        goto mod_list_copy_to_payload_failed;
    }

    /* Get Calibration data for each subgraph and copy to payload */
    ret = gcs_get_sg_cal_and_update_pl(pdata,
                                       GCS_CMD_GET_SG_CAL_AND_UPDATE_PL);
    if (ret) {
        OSAL_LOGE("%s: Get SG cal and update payload failed, ret = %d\n",
                  __func__, ret);
        goto sg_cal_get_failed;
    }

    /*
     * Get links between different modules inside each subgraph and
     * copy to payload.
     */
    ret = gcs_get_sg_cal_and_update_pl(pdata,
                                       GCS_CMD_GET_SG_MOD_CONN_AND_UPDATE_PL);
    if (ret) {
        OSAL_LOGE("%s: Get SG module conn and update payload failed, ret = %d\n",
                  __func__, ret);
       goto sg_mod_conn_get_failed;
    }

    /*
     * Get link info between different subgraphs in the graph and
     * copy to payload.
     */
    if (pdata->sg_count > 1) {
        ret = gcs_get_sg_conn_and_update_pld(pdata);
        if (ret) {
            OSAL_LOGE("%s: Subgraph conn get failed, ret = %d\n", __func__, ret);
            goto sg_conn_get_failed;
        }
    }

    if ((non_persist_ucal == NULL) ||
        (size_ucal > GCS_MAX_NON_PERSIST_CAL_SIZE)) {
            OSAL_LOGE("%s: Invalid non-persist cal. ptr = %p, size = %d\n",
                      __func__, non_persist_ucal, size_ucal);
            ret = OSAL_EBADPARAM;
            goto non_persist_ucal_invalid;
    } else {
        /*
         * Copy User space non-persist calibration blobs to
         * payload
         */
        ret = gcs_copy_to_payload(pdata, (uint8_t *)non_persist_ucal,
                                  size_ucal);
        if (ret ) {
            OSAL_LOGE("%s: Failed to copy non-persist ucal to payload, ret = %d\n",
                    __func__, ret);
            goto copy_non_persist_data_to_payload_failed;
        }
    }

    /*
     * Create payload for each of the Persist data blobs loaded by
     * user for this graph. All these blobs were loaded before calling
     * into this function.
     */
    osal_list_for_each_entry(struct gcs_persist_data_info, p_d_pos,
                             &pdata->persist_data_list, list) {
        ret = gcs_create_indirect_data_pld(pdata, p_d_pos);
        if (ret)
            goto ind_data_pld_create_failed;
    }

    /*
     * Create payload for each of the event which user wants to register
     * for. The events will be registered for modules in this graph.
     */
    osal_list_for_each_entry(struct gcs_event, event_pos,
                             &pdata->event_list, list) {
        ret = gcs_create_event_payload(pdata, event_pos);
        if (ret)
            goto event_pld_create_failed;
    }

    /* Send Graph Open command for this Graph */
    ret = __graph_open(pdata);
    if (ret) {
        OSAL_LOGE("%s: graph_open failed, ret = %d\n", __func__, ret);
        goto graph_open_failed;
    }

    /* Update Graph state to Running */
    pdata->state = GCS_GRAPH_RUNNING;

graph_open_failed:
ind_data_pld_create_failed:
event_pld_create_failed:
copy_non_persist_data_to_payload_failed:
non_persist_ucal_invalid:
sg_conn_get_failed:
sg_mod_conn_get_failed:
sg_cal_get_failed:
mod_list_copy_to_payload_failed:
wrong_state:
    osal_lock_release(pdata->lock);

exit:
    return ret;
}

/*
 * gcs_disable - Disable an active graph
 *
 * @graph_handle:    handle to the graph in GCS
 *
 * Return 0 on success or error code otherwise
 *
 * Prerequisites
 *    Graph must be active.
 */
int32_t gcs_disable(uint32_t graph_handle)
{
    int32_t ret = OSAL_SUCCESS;
    struct gcs_graph_pdata *pdata;
    struct gcs_sub_graph *sg;

    if (gcs == NULL) {
        OSAL_LOGE("%s: GCS not initialized. Exiting\n", __func__);
        return OSAL_EBADPARAM;
    }

    /* Identify private data from graph handle */
    pdata = gcs_get_pdata_from_graph_handle(graph_handle);
    if (pdata == NULL) {
        OSAL_LOGE("%s: No graph present for graph_handle = %d\n",
                __func__, graph_handle);
        ret = OSAL_EBADPARAM;
        goto exit;
    }

    osal_lock_acquire(pdata->lock);

    /* If the Graph is not in running state, cannot disable the Graph */
    if (pdata->state != GCS_GRAPH_RUNNING) {
        OSAL_LOGE("%s: Graph not running, cannot disable. Current state is %s\n",
                __func__, gcs_state_name[pdata->state] );
        ret = OSAL_EBADPARAM;
        goto wrong_state;
    }

    osal_list_for_each_entry(struct gcs_sub_graph, sg, &pdata->sg_list, list) {
        if (sg->sg_id == GCS_SGT_SINK)
            continue;

        ret = gcs_copy_to_payload(pdata, (uint8_t *)sg->mod_list,
                                  sg->mod_list_size);
        if (ret ) {
            OSAL_LOGE("%s: Module list copy to payload failed, ret = %d\n",
                    __func__, ret);
            goto mod_list_copy_to_payload_failed;
        }
    }

    /* Send Graph Close command for this Graph */
    ret = __graph_close(pdata);
    if (ret) {
        OSAL_LOGE("%s: graph_close failed, ret = %d\n", __func__, ret);
        goto graph_close_failed;
    }

    /* Update Graph state to Running */
    pdata->state = GCS_GRAPH_SETUP;

graph_close_failed:
mod_list_copy_to_payload_failed:
wrong_state:
    osal_lock_release(pdata->lock);

exit:
    return ret;
}

/*
 * gcs_open - Setup a new graph with GCS
 *
 * @u_id:    unique ID to identify the graph to be setup
 * @d_id:    device ID of the device associated with the graph
 *
 * Return 0 on success or error code otherwise
 *
 * Prerequisites
 *    GCS must be initialized.
 */
int32_t gcs_open(uint32_t u_id, uint32_t d_id, uint32_t *graph_handle)
{
    int32_t ret = OSAL_SUCCESS;
    uint32_t i;
    struct gcs_graph_pdata *pdata;
    uint32_t sg_list_cal_size = 0;
    uint32_t *sg_list_buff;
    uint32_t total_sg;
    uint32_t sg_id;
    struct gcs_sub_graph *sg;
    struct gcs_sub_graph *pos, *n;
    struct gcs_graph_info *graph_info;

    if (gcs == NULL) {
        OSAL_LOGE("%s: GCS not initialized. Exiting\n", __func__);
        return OSAL_EBADPARAM;
    }

    osal_lock_acquire(gcs->lock);

    /* Allocate and initialize private data for the graph */
    pdata = osal_mem_zalloc(sizeof(struct gcs_graph_pdata));
    if (!pdata) {
        OSAL_LOGE("%s: Memory alloc failed for graph pdata\n", __func__);
        return OSAL_ENOMEMORY;
    }

    osal_lock_create(&pdata->lock);
    osal_init_list(&pdata->sg_list);
    osal_init_list(&pdata->persist_data_list);
    osal_init_list(&pdata->event_list);

    osal_lock_acquire(pdata->lock);

    /* Allocate memory for payload buff for this graph */
    pdata->payload_buff_size = GCS_GRAPH_PAYLOAD_SIZE;
    pdata->payload = osal_mem_zalloc(pdata->payload_buff_size);
    if (!pdata->payload) {
        OSAL_LOGE("%s: Memory alloc failed for graph payload\n", __func__);
        ret = OSAL_ENOMEMORY;
        goto graph_payload_alloc_failed;
    }

    /* Allocate memory for CMD response buff for this graph */
    pdata->rsp_buff_size = GCS_GRAPH_CMD_RSP_SIZE;
    pdata->rsp = osal_mem_zalloc(pdata->rsp_buff_size);
    if (!pdata->rsp) {
        OSAL_LOGE("%s: Memory alloc failed for graph CMD rsp\n", __func__);
        ret = OSAL_ENOMEMORY;
        goto graph_rsp_buff_alloc_failed;
    }

    pdata->u_id = u_id;
    pdata->d_id = d_id;
    pdata->state = GCS_GRAPH_IDLE;

    /*
     * Call Calibration wrapper APIs to get list of sub-graphs for
     * the Unique ID (u_id) and Device ID (d_id) combination
     */
    ret = gcs_cal_get_subgraph_list_size(pdata->u_id, pdata->d_id,
                                         &sg_list_cal_size);
    if (ret) {
        OSAL_LOGE("%s: Failed to get size of subgraph list cal\n", __func__);
        goto get_sg_list_size_failed;
    }
    if (sg_list_cal_size == 0) {
        OSAL_LOGE("%s: subgraph list cal size 0. Exiting\n", __func__);
        goto get_sg_list_size_failed;
    }

    sg_list_buff = osal_mem_zalloc(sg_list_cal_size);
    if (!sg_list_buff) {
        OSAL_LOGE("%s: Memory alloc failed for sg_list cal buffer, size = %d\n",
                  __func__, sg_list_cal_size);
        ret = OSAL_ENOMEMORY;
        goto get_sg_cal_buff_alloc_failed;
    }

    ret = gcs_cal_get_subgraph_list(pdata->u_id, pdata->d_id,
                                    (uint8_t *)sg_list_buff, sg_list_cal_size);
    if (ret) {
        OSAL_LOGE("%s: Failed to get subgraph list, ret = %d\n", __func__, ret);
        goto get_sg_list_failed;
    }

    total_sg = sg_list_buff[0];
    if (total_sg == 0) {
        OSAL_LOGE("%s: subgraph list empty. Exiting\n", __func__);
        ret = -EINVAL;
        goto sg_list_empty;
    }
    /* Store number of subgraph in this graph in private data */
    pdata->sg_count = total_sg;

    /* Get module list for each subgraph and store in private data */
    for (i = 0; i < total_sg; i++) {
        sg = osal_mem_zalloc(sizeof(struct gcs_sub_graph));
        if (!sg) {
            OSAL_LOGE("%s: Memory alloc failed for subgraph\n", __func__);
            ret = OSAL_ENOMEMORY;
            goto sg_struct_alloc_failed;
        }

        sg->sg_id = sg_list_buff[GCS_SG_MOD_LIST_OFFSET + i];
        if (sg->sg_id >= GCS_SGT_MAX) {
            OSAL_LOGE("%s: Invalid subgraph ID %d\n", __func__, sg->sg_id);
            ret = -EINVAL;
            goto invalid_sg_id;
        }

        ret = gcs_get_sg_module_list(pdata, sg);
        if (ret) {
            OSAL_LOGE("%s: Failed to get module list for Sub-Graph = %s\n",
                    __func__, gcs_sgt_name[sg->sg_id - 1]);
            goto get_mod_list_failed;
        }

        /* Add this sub-graph to the private data subgraph list */
        osal_list_add_tail(&sg->list, &pdata->sg_list);
    }

    graph_info = osal_mem_zalloc(sizeof(struct gcs_graph_info));
    if (!graph_info) {
        OSAL_LOGE("%s: Memory alloc failed for graph_info\n", __func__);
        ret = OSAL_ENOMEMORY;
        goto graph_info_alloc_failed;

    }
    graph_info->priv_data = pdata;
    graph_info->graph_handle = gcs_generate_graph_handle(gcs);

    osal_list_add_tail(&graph_info->list, &gcs->graph_list);

    /* Assign the graph handle to the callers variable to return */
    *graph_handle = graph_info->graph_handle;

    /* Change state to SETUP */
    pdata->state = GCS_GRAPH_SETUP;

    osal_lock_release(pdata->lock);

    ret = 0;
    goto done;

graph_info_alloc_failed:
get_mod_list_failed:
sg_struct_alloc_failed:
invalid_sg_id:
    /* Release all SG struct and empty SG list from pdata */
    if (!osal_list_is_empty(&pdata->sg_list)) {
        osal_list_for_each_entry_safe(struct gcs_sub_graph, pos, n,
                                      &pdata->sg_list, list) {
            osal_mem_free(pos->mod_list);
            osal_list_del(&pos->list);
            osal_mem_free(pos);
        }
    }

sg_list_empty:
get_sg_list_failed:
    osal_mem_free(sg_list_buff);

get_sg_cal_buff_alloc_failed:
get_sg_list_size_failed:
    /* Deallocate response memory in private data */
    osal_mem_free(pdata->rsp);

graph_rsp_buff_alloc_failed:
    /* Deallocate payload memory in private data */
    osal_mem_free(pdata->payload);

graph_payload_alloc_failed:
    osal_lock_release(pdata->lock);
    osal_list_del(&pdata->event_list);
    osal_list_del(&pdata->persist_data_list);
    osal_list_del(&pdata->sg_list);
    osal_lock_destroy(pdata->lock);

create_lock_failed:
    /* Deallocate private data struct */
    osal_mem_free(pdata);

done:
    osal_lock_release(gcs->lock);

exit:
    return ret;
}

/*
 * gcs_close - Tear down an existing graph in GCS
 *
 * @graph_handle:    handle to the graph in GCS
 *
 * Return 0 on success or error code otherwise
 *
 * Prerequisites
 *    Graph must be setup with GCS.
 */
int32_t gcs_close(uint32_t graph_handle)
{
    int32_t ret = OSAL_SUCCESS;
    struct gcs_graph_pdata *pdata;
    struct gcs_graph_info *g_pos;
    struct gcs_sub_graph *sg_pos, *sg_n;
    struct gcs_persist_data_info *p_data_pos, *pd_n;
    struct gcs_event *event_pos, *ev_n;

    if (gcs == NULL) {
        OSAL_LOGE("%s: GCS not initialized. Exiting\n", __func__);
        return OSAL_EBADPARAM;
    }

    /* Identify private data from graph handle */
    pdata = gcs_get_pdata_from_graph_handle(graph_handle);
    if (pdata == NULL) {
        OSAL_LOGE("%s: No graph present for graph_handle = %d\n",
                __func__, graph_handle);
        ret = OSAL_EBADPARAM;
        goto exit;
    }

    osal_lock_acquire(gcs->lock);

    osal_lock_acquire(pdata->lock);

    /* If the Graph is not in setup state, no need to close the Graph */
    if (pdata->state != GCS_GRAPH_SETUP) {
        OSAL_LOGE("%s: Cannot close graph. Current state is %s\n",
                __func__, gcs_state_name[pdata->state] );
        ret = OSAL_EBADPARAM;
        goto wrong_state;
    }

    /* Remove pdata from GCS global list of graphs */
    osal_list_for_each_entry(struct gcs_graph_info, g_pos,
                             &gcs->graph_list, list) {
        if (g_pos->priv_data == pdata) {
            g_pos->graph_handle = 0;
            g_pos->priv_data = NULL;
            osal_list_del(&g_pos->list);
            osal_mem_free(g_pos);
            break;
        }
    }

    osal_lock_release(gcs->lock);

    /* Unload all persist data blobs for the graph */
    if (!osal_list_is_empty(&pdata->persist_data_list)) {
        osal_list_for_each_entry_safe(struct gcs_persist_data_info,
                                      p_data_pos, pd_n,
                                      &pdata->persist_data_list, list) {
            ret = __unload_data(pdata, p_data_pos->cal_load_addr);
            if (ret)
                OSAL_LOGE("%s: Unload data failed. ret = %d\n", __func__, ret);

            osal_list_del(&p_data_pos->list);
            osal_mem_free(p_data_pos);
        }
    }

    /* Release all SG struct and empty SG list from pdata */
    if (!osal_list_is_empty(&pdata->sg_list)) {
        osal_list_for_each_entry_safe(struct gcs_sub_graph, sg_pos, sg_n,
                                      &pdata->sg_list, list) {
            osal_mem_free(sg_pos->mod_list);
            osal_list_del(&sg_pos->list);
            osal_mem_free(sg_pos);
        }
    }

    /* Release all event struct and empty event list from pdata */
    if (!osal_list_is_empty(&pdata->event_list)) {
        osal_list_for_each_entry_safe(struct gcs_event, event_pos, ev_n,
                                      &pdata->event_list, list) {
            osal_list_del(&event_pos->list);
            osal_mem_free(event_pos);
        }
    }

    /* Deallocate response memory in private data */
    osal_mem_free(pdata->rsp);
    pdata->rsp_buff_size = 0;
    /* Deallocate payload memory in private data */
    osal_mem_free(pdata->payload);
    pdata->payload = NULL;
    pdata->payload_buff_size = 0;
    pdata->payload_data_size = 0;
    pdata->sg_count = 0;
    pdata->u_id = 0;
    pdata->d_id = 0;
    pdata->state = GCS_GRAPH_IDLE;
    osal_list_del(&pdata->event_list);
    osal_list_del(&pdata->persist_data_list);
    osal_list_del(&pdata->sg_list);

    osal_lock_release(pdata->lock);
    osal_lock_destroy(pdata->lock);

    osal_mem_free(pdata);

    ret = 0;
    goto exit;

wrong_state:
    osal_lock_release(pdata->lock);
    osal_lock_release(gcs->lock);

pdata_lock_acquire_failed:
exit:
    return ret;
}

/*
 * gcs_init - Initialize GCS
 *
 * Return 0 on success or error code otherwise
 *
 * Prerequisites
 *    None.
 */
int32_t gcs_init(void)
{
    int32_t ret = OSAL_SUCCESS;

    if (++gcs_init_ref_cnt > 1) {
        OSAL_LOGD("%s: GCS already initialized. ref_cnt = %d\n",
                  __func__, gcs_init_ref_cnt);
        goto exit;
    }

    /* Allocate global struct for use in GCS */
    gcs = osal_mem_zalloc(sizeof(struct gcs));
    if (!gcs) {
        OSAL_LOGE("%s: Memory alloc failed for GCS struct\n", __func__);
        ret = OSAL_ENOMEMORY;
	goto gcs_alloc_failed;
    }

    osal_init_list(&gcs->graph_list);
    osal_lock_create(&gcs->lock);

    /* Initialize GCS Calibration wrapper */
    ret = gcs_cal_wrapper_init();
    if (ret) {
        OSAL_LOGE("%s: Failed to initialize calibration wrapper\n", __func__);
        goto fail_init_cal;
    }

    /* Initialize IPC packetizer */
    ret = pktzr_init(gcs_event_cb_handler,
                     gcs_data_cmd_cb_handler);
    if (ret) {
        OSAL_LOGE("%s: Failed to initialize IPC packetizer\n", __func__);
        goto fail_init_ipc;
    }

    OSAL_LOGD("%s: GCS init done\n", __func__);
    goto exit;

fail_init_ipc:
    /* De-initialize GCS Calibration wrapper */
    gcs_cal_wrapper_deinit();

fail_init_cal:
    osal_lock_destroy(gcs->lock);
    osal_list_del(&gcs->graph_list);

    osal_mem_free(gcs);
    gcs = NULL;

gcs_alloc_failed:
    gcs_init_ref_cnt--;

exit:
    return ret;
}

/*
 * gcs_deinit - De-initialize GCS
 *
 * Return 0 on success or error code otherwise
 *
 * Prerequisites
 *    GCS must be initialized.
 */
int32_t gcs_deinit(void)
{
    if (gcs_init_ref_cnt == 0) {
        OSAL_LOGD("%s: GCS already de-initialized. Exit\n", __func__);
        goto exit;
    } else if (--gcs_init_ref_cnt != 0) {
        OSAL_LOGD("%s: Skipping GCS de-initialize. ref_cnt = %d\n",
                  __func__, gcs_init_ref_cnt);
        goto exit;
    }

    /* De-initialize IPC packetizer */
    pktzr_deinit();

    /* De-initialize GCS Calibration wrapper */
    gcs_cal_wrapper_deinit();

    osal_lock_destroy(gcs->lock);
    osal_list_del(&gcs->graph_list);

    /* Deallocate global GCS struct */
    osal_mem_free(gcs);
    gcs = NULL;

    OSAL_LOGD("%s: GCS deinit done\n", __func__);

exit:
    return 0;
}
