/*
 * gcs.h
 *
 * This library contains the API to load setup, start, stop and tear-down
 * a use case for Codec DSP using Graphite (Graph-Lite) architecture.
 *
 * Copyright (c) 2016 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */

#ifndef __GCS_H__
#define __GCS_H__

#include <gcs_api.h>
#include <osal_lock.h>
#include <osal_list.h>

#define GCS_GRAPH_PAYLOAD_SIZE         (4 * 1024)
#define GCS_GRAPH_CMD_RSP_SIZE         (4 * 1024)
#define GCS_SG_MOD_LIST_OFFSET         1
#define GCS_MAX_NON_PERSIST_CAL_SIZE   (512 *1024)


/* Graph states */
enum gcs_state {
    GCS_GRAPH_IDLE = 0,
    GCS_GRAPH_LOADED,
    GCS_GRAPH_SETUP,
    GCS_GRAPH_RUNNING,
    GCS_GRAPH_BUFFERING,
    GCS_GRAPH_UNLOADED,
    /*
     * When adding more entry here,
     * please update the SGT name array below.
     */
    GCS_EVENT_MAX
};

/* Graph state names (to help print graph state) */
const char * const gcs_state_name[GCS_EVENT_MAX] = {
    "GCS_GRAPH_IDLE",
    "GCS_GRAPH_LOADED",
    "GCS_GRAPH_SETUP",
    "GCS_GRAPH_RUNNING",
    "GCS_GRAPH_BUFFERING",
    "GCS_GRAPH_UNLOADED"
};

/* Sub-graph IDs */
typedef enum {
    GCS_SGT_DEVICE = 1,
    GCS_SGT_STREAM,
    GCS_SGT_SINK,
    /*
     * When adding more entry here,
     * please update the SGT name array below.
     */
    GCS_SGT_MAX
} gcs_subgraph_id_t;

const char * const gcs_sgt_name[GCS_SGT_MAX] = {
    "GCS_SGT_DEVICE",
    "GCS_SGT_STREAM",
    "GCS_SGT_SINK"
};

/* GCS internal operations */
typedef enum {
    GCS_CMD_COPY_MOD_LIST_TO_PL = 1,
    GCS_CMD_GET_SG_CAL_AND_UPDATE_PL,
    GCS_CMD_GET_SG_MOD_CONN_AND_UPDATE_PL,
    GCS_CMD_MAX
} gcs_cmd_t;

/*
 * Structure for list of sub-graphs in a graph.
 * Used to store information related to each sub-graph in a graph
 */
struct gcs_sub_graph {
    /* List struct */
    osal_list_t list;

    /* Sub graph ID */
    uint32_t sg_id;

    /* List of modules for this sub-graph */
    uint32_t *mod_list;

    /* Size of module list for this sub-graph */
    uint32_t mod_list_size;
};

/*
 * Structure for persistent user data.
 * Used to store information related to User data loaded into
 * persistent memory.
 */
struct gcs_persist_data_info {
    /* List struct */
    osal_list_t list;

    /* Calibration handle for the data loaded. Returned to caller */
    uint32_t data_handle;

    /* Memory address for the data loaded. Not returned to caller */
    uint32_t cal_load_addr;

    /* Size of the data loaded. Not returned to caller */
    uint32_t data_size;
};

/*
 * Event structure. Used to store event related information
 * for any event registered with GCS.
 */
struct gcs_event {
    /* List struct */
    osal_list_t list;

    /* Type of event */
    struct gcs_module_param_info module_param_info;

    /* Private data from the caller. Will be returned with callback */
    void *cookie;

    /*
    * Callback handler for the event.
    * If NULL, event is dropped in GCS.
    * Callback handler signature is-
    *    int32_t cb_handler(uint32_t graph_handle,
    *                       struct gcs_event_rsp *event_rsp,
    *                       void *cookie)
    */
    event_cb_ptr cb_handler;
};

/* Graph Private data */
struct gcs_graph_pdata {
    /* Use case ID for this use case */
    uint32_t u_id;

    /* Device ID of devices used for this graph */
    uint32_t d_id;

    /* Lock for sequencing commands for this graph */
    osal_lock_t lock;

    /* Graphs state */
    enum gcs_state state;

    /* Number of sub-graphs for this graph */
    uint32_t sg_count;

    /* List of sub-graphs for this graph */
    osal_list_t sg_list;

    /* List of persistent user data blobs for this graph */
    osal_list_t persist_data_list;

    /* List of events registered for this graph */
    osal_list_t event_list;

    /* Pre-allocated memory for creating payload */
    int8_t *payload;

    /* Size of the payload buffer */
    size_t payload_buff_size;

    /* Size in bytes of the content in payload pointer above */
    uint32_t payload_data_size;

    /* Pre-allocated memory for response */
    int8_t *rsp;

    /* Size of the response buffer */
    size_t rsp_buff_size;

    /*
     * Persistent data handle counter variable for the graph.
     * Possible values = anything >= 1
     * Invalid value = 0
     */
    uint32_t data_handle;
};

/*
 * Structure to contain Graph information. An instance of this is added
 * to the list of graphs in global GCS struct, for each graph opened with GCS.
 */
struct gcs_graph_info {
    osal_list_t list;
    uint32_t graph_handle;
    struct gcs_graph_pdata *priv_data;
};

/* Global GCS structure */
struct gcs {
    /* Global lock for all list operations */
    osal_lock_t lock;

    /* List of graphs registered with GCS at any point of time */
    osal_list_t graph_list;

    /*
     * Graph handle counter variable.
     * Possible values = anything > 1
     * Invalid value = 0
     */
    uint32_t graph_handle;
};

/*
 * gcs_generate_graph_handle - Generate a graph handle to be returned to caller
 *
 * @gcs:    Pointer to the global struct gcs type.
 *
 * Prerequisites
 *    GCS must be initialized
 */
static inline uint32_t gcs_generate_graph_handle(struct gcs *gcs) {
    if (++gcs->graph_handle == 0)
        gcs->graph_handle = 1;

    return gcs->graph_handle;
}

/*
 * gcs_generate_data_handle - Generate a data handle for persistent data, to be
 *                            returned to caller
 *
 * @pdata:    Pointer to the graph private data.
 *
 * Prerequisites
 *    Graph must be initialized
 */
static inline uint32_t gcs_generate_data_handle(struct gcs_graph_pdata *pdata) {
    if (++pdata->data_handle == 0)
        pdata->data_handle = 1;

    return pdata->data_handle;
}

#endif /* __GCS_H__ */
