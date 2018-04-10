/*!
 * @file test_vpp.c
 *
 * @cr
 * Copyright (c) 2015-2016 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.

 * @services    <Describe what this is supposed to test>
 */

#include <sys/types.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>

#include "dvpTest.h"
#include "dvpTest_tb.h"

#define VPP_LOG_TAG     VPP_LOG_UT_VPP_CORE_TAG
#define VPP_LOG_MODULE  VPP_LOG_UT_VPP_CORE
#include "vpp_dbg.h"
#include "vpp_def.h"
#include "vpp_ctx.h"
#include "vpp_core.h"
#include "vpp_utils.h"
#include "vpp.h"
#include "vpp_uc.h"

#include "test_mock_registry.h"
#include "test_utils.h"


/************************************************************************
 * Local definitions
 ***********************************************************************/
static pthread_cond_t sCond = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t sMutex = PTHREAD_MUTEX_INITIALIZER;

static uint32_t u32InputFlushRx;
static uint32_t u32OutputFlushRx;
static uint32_t u32ReconfigDoneRx;
/************************************************************************
 * Local static variables
 ***********************************************************************/
static void vpp_ibd_cb(void *pv, struct vpp_buffer *buf);
static void vpp_obd_cb(void *pv, struct vpp_buffer *buf);
static void vpp_evt_cb(void *pv, struct vpp_event e);

static struct vpp_callbacks sCb = {
    .input_buffer_done = vpp_ibd_cb,
    .output_buffer_done = vpp_obd_cb,
    .vpp_event = vpp_evt_cb,
};

/************************************************************************
 * Forward Declarations
 ************************************************************************/

/************************************************************************
 * Local Functions
 ***********************************************************************/
static void vpp_ibd_cb(void *pv, struct vpp_buffer *buf)
{
    (void)(buf);
    (void)(pv);
}

static void vpp_obd_cb(void *pv, struct vpp_buffer *buf)
{
    (void)(buf);
    (void)(pv);
}

static void vpp_evt_cb(void *pv, struct vpp_event e)
{
    (void)(pv);
    LOGI("%s(), event_type=%u", __func__, e.type);
    if (e.type == VPP_EVENT_FLUSH_DONE)
    {
        pthread_mutex_lock(&sMutex);
        if (e.flush_done.port == VPP_PORT_INPUT)
        {
            u32InputFlushRx = VPP_TRUE;
        }
        else if (e.flush_done.port == VPP_PORT_OUTPUT)
        {
            u32OutputFlushRx = VPP_TRUE;
        }
        pthread_mutex_unlock(&sMutex);
        pthread_cond_signal(&sCond);
    }
    else if (e.type == VPP_EVENT_RECONFIG_DONE)
    {
        pthread_mutex_lock(&sMutex);

        DVP_ASSERT_EQUAL(VPP_OK, e.port_reconfig_done.reconfig_status);

        u32ReconfigDoneRx = VPP_TRUE;
        pthread_mutex_unlock(&sMutex);
        pthread_cond_signal(&sCond);
    }
}

/************************************************************************
 * Test Functions
 ***********************************************************************/
TEST_SUITE_INIT(VppSuiteInit)
{
    init_test_registry(TEST_SUITE_VPP_CLIENT);
}

TEST_SUITE_TERM(VppSuiteTerm)
{
    term_test_registry();
}

TEST_SETUP(VppTestInit)
{
}

TEST_CLEANUP(VppTestTerm)
{
}

TEST(Vpp_InitTermSingle)
{
    void *p;

    p = vpp_init(0, sCb);
    DVP_ASSERT_PTR_NNULL(p);
    vpp_term(p);
}


#define MAX_CONCURRENT_SESSIONS 2
TEST(Vpp_InitTermMultiple)
{
    uint32_t i;
    void *apvCtx[MAX_CONCURRENT_SESSIONS];
    memset(apvCtx, 0, sizeof(void *));


    for (i = 0; i < MAX_CONCURRENT_SESSIONS; i++)
    {
        apvCtx[i] = vpp_init(0, sCb);
        DVP_ASSERT_PTR_NNULL(apvCtx[i]);
    }

    for (i = 0; i < MAX_CONCURRENT_SESSIONS; i++)
    {
        if (i == 0)
            continue;
        DVP_ASSERT_PTR_NEQUAL(apvCtx[i], apvCtx[i - 1]);
    }

    for (i = 0; i < MAX_CONCURRENT_SESSIONS; i++)
    {
        vpp_term(apvCtx[i]);
    }

    // Make sure we don't crash
    vpp_term(NULL);

    for (i = 0; i < 10; i++)
    {
        void *p = vpp_init(0, sCb);
        DVP_ASSERT_PTR_NNULL(p);
        vpp_term(p);
        p = NULL;
    }
}

TEST(__unused Vpp_InitCpNotSupported)
{
    term_test_registry();
    init_test_registry(TEST_SUITE_VPP_CLIENT_NO_CP);
    void *p;

    p = vpp_init(VPP_SESSION_SECURE, sCb);
    DVP_ASSERT_PTR_NULL(p);

    p = vpp_init(0, sCb);
    DVP_ASSERT_PTR_NNULL(p);

    vpp_term(p);
    term_test_registry();
    init_test_registry(TEST_SUITE_VPP_CLIENT);
}

TEST(Vpp_SetCtrl)
{
    uint32_t u32;
    struct hqv_control ctrl;
    struct vpp_requirements req;

    void *p;

    memset(&ctrl, 0, sizeof(struct hqv_control));

    u32 = vpp_set_ctrl(NULL, ctrl, &req);
    DVP_ASSERT_NEQUAL(u32, VPP_OK);

    p = vpp_init(0, sCb);
    DVP_ASSERT_PTR_NNULL(p);

    // Set some invalid controls
    ctrl.mode = 902389;
    u32 = vpp_set_ctrl(p, ctrl, &req);
    DVP_ASSERT_NEQUAL(u32, VPP_OK);

    // Set to auto
    ctrl.mode = HQV_MODE_AUTO;
    u32 = vpp_set_ctrl(p, ctrl, &req);
    DVP_ASSERT_EQUAL(u32, VPP_OK);

    // Set to off
    ctrl.mode = HQV_MODE_OFF;
    u32 = vpp_set_ctrl(p, ctrl, &req);
    DVP_ASSERT_EQUAL(u32, VPP_OK);

    // Set to manual
    ctrl.mode = HQV_MODE_MANUAL;
    ctrl.ctrl_type = HQV_CONTROL_CADE;
    ctrl.cade.mode = HQV_MODE_AUTO;
    u32 = vpp_set_ctrl(p, ctrl, &req);
    DVP_ASSERT_TRUE(u32 == VPP_OK || u32 == VPP_ERR_INVALID_CFG);

    vpp_term(p);
}

TEST(Vpp_SetParameter)
{
    uint32_t u32;
    struct vpp_port_param param;
    void *p;

    memset(&param, 0, sizeof(struct vpp_port_param));

    p = vpp_init(0, sCb);
    DVP_ASSERT_PTR_NNULL(p);

    // Garbage input
    u32 = vpp_set_parameter(NULL, VPP_PORT_INPUT, param);
    DVP_ASSERT_NEQUAL(u32, VPP_OK);
    u32 = vpp_set_parameter(NULL, VPP_PORT_OUTPUT, param);
    DVP_ASSERT_NEQUAL(u32, VPP_OK);
    u32 = vpp_set_parameter(NULL, 123198, param);
    DVP_ASSERT_NEQUAL(u32, VPP_OK);
    u32 = vpp_set_parameter(p, 123198, param);
    DVP_ASSERT_NEQUAL(u32, VPP_OK);

    // Garbage parameters
    param.width = 0;
    param.height = 0;
    param.fmt = VPP_COLOR_FORMAT_NV12_VENUS;

    // Setting input and output port will cause state transition
    u32 = vpp_set_parameter(p, VPP_PORT_INPUT, param);
    DVP_ASSERT_EQUAL(u32, VPP_OK);
    u32 = vpp_set_parameter(p, VPP_PORT_OUTPUT, param);
    DVP_ASSERT_NEQUAL(u32, VPP_OK);

    // Valid input
    param.width = 1920;
    param.height = 1080;
    vpp_term(p);

    // Reset and try again. Should error because we didn't set hqv controls
    // However, still will be put into active state
    p = vpp_init(0, sCb);
    DVP_ASSERT_PTR_NNULL(p);
    u32 = vpp_set_parameter(p, VPP_PORT_INPUT, param);
    DVP_ASSERT_EQUAL(u32, VPP_OK);
    u32 = vpp_set_parameter(p, VPP_PORT_OUTPUT, param);
    DVP_ASSERT_NEQUAL(u32, VPP_OK);

    // Now in active state. Should reject any attempts to set params.
    param.width = 1280;
    param.height = 720;
    u32 = vpp_set_parameter(p, VPP_PORT_INPUT, param);
    DVP_ASSERT_EQUAL(u32, VPP_ERR_STATE);
    u32 = vpp_set_parameter(p, VPP_PORT_OUTPUT, param);
    DVP_ASSERT_EQUAL(u32, VPP_ERR_STATE);

    vpp_term(p);
}

TEST(Vpp_QueueBuf)
{
    uint32_t u32;
    void *p;
    struct vpp_buffer buf;
    struct vpp_buffer *pBuf = &buf;

    p = vpp_init(0, sCb);
    DVP_ASSERT_PTR_NNULL(p);

    // Garbage
    u32 = vpp_queue_buf(NULL, (enum vpp_port)9102831092, NULL);
    DVP_ASSERT_NEQUAL(VPP_OK, u32);
    u32 = vpp_queue_buf(NULL, VPP_PORT_INPUT, NULL);
    DVP_ASSERT_NEQUAL(VPP_OK, u32);
    u32 = vpp_queue_buf(NULL, VPP_PORT_OUTPUT, NULL);
    DVP_ASSERT_NEQUAL(VPP_OK, u32);
    u32 = vpp_queue_buf(NULL, VPP_PORT_INPUT, pBuf);
    DVP_ASSERT_NEQUAL(VPP_OK, u32);
    u32 = vpp_queue_buf(NULL, VPP_PORT_OUTPUT, pBuf);
    DVP_ASSERT_NEQUAL(VPP_OK, u32);
    u32 = vpp_queue_buf(p, (enum vpp_port)9102831092, NULL);
    DVP_ASSERT_NEQUAL(VPP_OK, u32);
    u32 = vpp_queue_buf(p, VPP_PORT_INPUT, NULL);
    DVP_ASSERT_NEQUAL(VPP_OK, u32);
    u32 = vpp_queue_buf(p, VPP_PORT_OUTPUT, NULL);
    DVP_ASSERT_NEQUAL(VPP_OK, u32);

    // Valid input, invalid state
    u32 = vpp_queue_buf(p, VPP_PORT_INPUT, pBuf);
    DVP_ASSERT_NEQUAL(VPP_OK, u32);
    u32 = vpp_queue_buf(p, VPP_PORT_OUTPUT, pBuf);
    DVP_ASSERT_NEQUAL(VPP_OK, u32);

    vpp_term(p);
}

TEST(__unused Vpp_Timestamp)
{
    uint32_t u32;
    void *p;
    struct hqv_control ctrl;
    struct vpp_requirements req;
    struct vpp_port_param param;
    struct vpp_buffer buf;
    struct vpp_buffer *pBuf = &buf;
    t_StVppCb *pstCb;
    uint32_t u32FrameRate;
    p = vpp_init(0, sCb);
    DVP_ASSERT_PTR_NNULL(p);

    pstCb = ((t_StVppCtx *)p)->pstVppCb;

    // Set to manual
    //ctrl.mode = HQV_MODE_AUTO;
    ctrl.mode = HQV_MODE_MANUAL;
    ctrl.ctrl_type = HQV_CONTROL_AIE;
    ctrl.cade.mode = HQV_MODE_AUTO;

    u32 = vpp_set_ctrl(p, ctrl, &req);
    DVP_ASSERT_TRUE(u32 == VPP_OK || u32 == VPP_ERR_INVALID_CFG);

    param.width = 1920;
    param.height = 1080;
    param.fmt = VPP_COLOR_FORMAT_NV12_VENUS;

    // Setting input and output port will cause state transition
    u32 = vpp_set_parameter(p, VPP_PORT_INPUT, param);
    DVP_ASSERT_EQUAL(u32, VPP_OK);
    u32 = vpp_set_parameter(p, VPP_PORT_OUTPUT, param);
    DVP_ASSERT_EQUAL(u32, VPP_OK);

    // Valid input
    // 60Hz
    pBuf->timestamp = 33333;
    u32 = vpp_queue_buf(p, VPP_PORT_INPUT, pBuf);
    DVP_ASSERT_EQUAL(VPP_OK, u32);

    pBuf->timestamp = 50000;
    u32 = vpp_queue_buf(p, VPP_PORT_INPUT, pBuf);
    DVP_ASSERT_EQUAL(VPP_OK, u32);
    u32FrameRate = pstCb->stFrameRate.u32AvgFrameRate;
    DVP_ASSERT_TRUE((u32FrameRate >= 59) && (u32FrameRate <= 60));

    pBuf->timestamp = 66666;
    u32 = vpp_queue_buf(p, VPP_PORT_INPUT, pBuf);
    DVP_ASSERT_EQUAL(VPP_OK, u32);
    u32FrameRate = pstCb->stFrameRate.u32AvgFrameRate;
    DVP_ASSERT_TRUE((u32FrameRate >= 59) && (u32FrameRate <= 60));

    pBuf->timestamp = 83333;
    u32 = vpp_queue_buf(p, VPP_PORT_INPUT, pBuf);
    DVP_ASSERT_EQUAL(VPP_OK, u32);
    u32FrameRate = pstCb->stFrameRate.u32AvgFrameRate;
    DVP_ASSERT_TRUE((u32FrameRate >= 59) && (u32FrameRate <= 60));

    pBuf->timestamp = 100000;
    u32 = vpp_queue_buf(p, VPP_PORT_INPUT, pBuf);
    DVP_ASSERT_EQUAL(VPP_OK, u32);
    u32FrameRate = pstCb->stFrameRate.u32AvgFrameRate;
    DVP_ASSERT_TRUE((u32FrameRate >= 59) && (u32FrameRate <= 60));

    pBuf->timestamp = 116666;
    u32 = vpp_queue_buf(p, VPP_PORT_INPUT, pBuf);
    DVP_ASSERT_EQUAL(VPP_OK, u32);
    u32FrameRate = pstCb->stFrameRate.u32AvgFrameRate;
    DVP_ASSERT_TRUE((u32FrameRate >= 59) && (u32FrameRate <= 60));

    pBuf->timestamp = 133333;
    u32 = vpp_queue_buf(p, VPP_PORT_INPUT, pBuf);
    DVP_ASSERT_EQUAL(VPP_OK, u32);
    u32FrameRate = pstCb->stFrameRate.u32AvgFrameRate;
    DVP_ASSERT_TRUE((u32FrameRate >= 59) && (u32FrameRate <= 60));


    pBuf->timestamp = 150000;
    u32 = vpp_queue_buf(p, VPP_PORT_INPUT, pBuf);
    DVP_ASSERT_EQUAL(VPP_OK, u32);
    u32FrameRate = pstCb->stFrameRate.u32AvgFrameRate;
    DVP_ASSERT_TRUE((u32FrameRate >= 59) && (u32FrameRate <= 60));

    // Insert glitch (1000000Hz)
    pBuf->timestamp = 150001;
    u32 = vpp_queue_buf(p, VPP_PORT_INPUT, pBuf);
    DVP_ASSERT_EQUAL(VPP_OK, u32);
    u32FrameRate = pstCb->stFrameRate.u32AvgFrameRate;
    DVP_ASSERT_TRUE((u32FrameRate >= 60) && (u32FrameRate <= 70));

    // 30Hz
    pBuf->timestamp = 183333;
    u32 = vpp_queue_buf(p, VPP_PORT_INPUT, pBuf);
    DVP_ASSERT_EQUAL(VPP_OK, u32);

    pBuf->timestamp = 216666;
    u32 = vpp_queue_buf(p, VPP_PORT_INPUT, pBuf);
    DVP_ASSERT_EQUAL(VPP_OK, u32);

    pBuf->timestamp = 250000;
    u32 = vpp_queue_buf(p, VPP_PORT_INPUT, pBuf);
    DVP_ASSERT_EQUAL(VPP_OK, u32);

    pBuf->timestamp = 283333;
    u32 = vpp_queue_buf(p, VPP_PORT_INPUT, pBuf);
    DVP_ASSERT_EQUAL(VPP_OK, u32);

    pBuf->timestamp = 316666;
    u32 = vpp_queue_buf(p, VPP_PORT_INPUT, pBuf);
    DVP_ASSERT_EQUAL(VPP_OK, u32);

    pBuf->timestamp = 350000;
    u32 = vpp_queue_buf(p, VPP_PORT_INPUT, pBuf);
    DVP_ASSERT_EQUAL(VPP_OK, u32);

    pBuf->timestamp = 383333;
    u32 = vpp_queue_buf(p, VPP_PORT_INPUT, pBuf);
    DVP_ASSERT_EQUAL(VPP_OK, u32);
    u32FrameRate = pstCb->stFrameRate.u32AvgFrameRate;
    DVP_ASSERT_TRUE((u32FrameRate >= 29) && (u32FrameRate <= 30));

    // Send EOS to reset timestampe
    pBuf->flags |= VPP_BUFFER_FLAG_EOS;
    pBuf->timestamp = 10000;
    u32 = vpp_queue_buf(p, VPP_PORT_INPUT, pBuf);
    DVP_ASSERT_EQUAL(VPP_OK, u32);

    pBuf->flags = 0;
    pBuf->timestamp = 20000;
    u32 = vpp_queue_buf(p, VPP_PORT_INPUT, pBuf);
    DVP_ASSERT_EQUAL(VPP_OK, u32);
    u32FrameRate = pstCb->stFrameRate.u32AvgFrameRate;
    DVP_ASSERT_TRUE((u32FrameRate >= 99) && (u32FrameRate <= 100));
    DVP_ASSERT_TRUE(pstCb->stFrameRate.u32Bypass);

    // 100Hz Should be in bypass
    pBuf->timestamp = 30000;
    u32 = vpp_queue_buf(p, VPP_PORT_INPUT, pBuf);
    DVP_ASSERT_EQUAL(VPP_OK, u32);
    u32FrameRate = pstCb->stFrameRate.u32AvgFrameRate;
    DVP_ASSERT_TRUE((u32FrameRate >= 99) && (u32FrameRate <= 100));
    DVP_ASSERT_TRUE(pstCb->stFrameRate.u32Bypass);

    // Go to 30 Hz
    pBuf->timestamp = 50000;
    u32 = vpp_queue_buf(p, VPP_PORT_INPUT, pBuf);
    DVP_ASSERT_EQUAL(VPP_OK, u32);

    pBuf->timestamp = 83333;
    u32 = vpp_queue_buf(p, VPP_PORT_INPUT, pBuf);
    DVP_ASSERT_EQUAL(VPP_OK, u32);

    pBuf->timestamp = 116666;
    u32 = vpp_queue_buf(p, VPP_PORT_INPUT, pBuf);
    DVP_ASSERT_EQUAL(VPP_OK, u32);

    pBuf->timestamp = 150000;
    u32 = vpp_queue_buf(p, VPP_PORT_INPUT, pBuf);
    DVP_ASSERT_EQUAL(VPP_OK, u32);

    pBuf->timestamp = 183333;
    u32 = vpp_queue_buf(p, VPP_PORT_INPUT, pBuf);
    DVP_ASSERT_EQUAL(VPP_OK, u32);

    pBuf->timestamp = 216666;
    u32 = vpp_queue_buf(p, VPP_PORT_INPUT, pBuf);
    DVP_ASSERT_EQUAL(VPP_OK, u32);

    pBuf->timestamp = 250000;
    u32 = vpp_queue_buf(p, VPP_PORT_INPUT, pBuf);
    DVP_ASSERT_EQUAL(VPP_OK, u32);

    pBuf->timestamp = 283333;
    u32 = vpp_queue_buf(p, VPP_PORT_INPUT, pBuf);
    DVP_ASSERT_EQUAL(VPP_OK, u32);

    // Should exit bypass
    pBuf->timestamp = 316666;
    u32 = vpp_queue_buf(p, VPP_PORT_INPUT, pBuf);
    DVP_ASSERT_EQUAL(VPP_OK, u32);
    u32FrameRate = pstCb->stFrameRate.u32AvgFrameRate;
    DVP_ASSERT_TRUE((u32FrameRate >= 29) && (u32FrameRate <= 30));
    DVP_ASSERT_FALSE(pstCb->stFrameRate.u32Bypass);

    vpp_term(p);
}

TEST(Vpp_Flush)
{
    uint32_t u32;
    struct hqv_control ctrl;
    struct vpp_requirements req;
    struct vpp_port_param param;
    void *p;

    t_StVppUsecase *pstUcAuto = getUsecaseAuto(TEST_SUITE_REAL_REGISTRY);
    if (!pstUcAuto)
    {
        DVP_ASSERT_FAIL();
        return;
    }

    u32 = u32VppUsecase_GetMaxResForUc(pstUcAuto);

    ctrl.mode = HQV_MODE_AUTO;
    param.width = u32GetMaxWidthForRes(u32);
    param.height = u32GetMaxHeightForRes(u32);;
    param.fmt = VPP_COLOR_FORMAT_NV12_VENUS;

    LOGI("%s: w/h = %u/%u", __func__, param.width, param.height);

    p = vpp_init(0, sCb);
    DVP_ASSERT_PTR_NNULL(p);

    // Garbage inputs
    u32 = vpp_flush(NULL, (enum vpp_port)9210834);
    DVP_ASSERT_NEQUAL(VPP_OK, u32);
    u32 = vpp_flush(NULL, VPP_PORT_INPUT);
    DVP_ASSERT_NEQUAL(VPP_OK, u32);
    u32 = vpp_flush(NULL, VPP_PORT_OUTPUT);
    DVP_ASSERT_NEQUAL(VPP_OK, u32);
    u32 = vpp_flush(p, (enum vpp_port)10989123);
    DVP_ASSERT_NEQUAL(VPP_OK, u32);

    // Invalid state
    u32 = vpp_flush(p, VPP_PORT_INPUT);
    DVP_ASSERT_NEQUAL(VPP_OK, u32);
    u32 = vpp_flush(p, VPP_PORT_OUTPUT);
    DVP_ASSERT_NEQUAL(VPP_OK, u32);

    // Transition to active valid state
    u32 = vpp_set_ctrl(p, ctrl, &req);
    DVP_ASSERT_EQUAL(VPP_OK, u32);
    u32 = vpp_set_parameter(p, VPP_PORT_INPUT, param);
    DVP_ASSERT_EQUAL(VPP_OK, u32);
    u32 = vpp_set_parameter(p, VPP_PORT_OUTPUT, param);
    DVP_ASSERT_EQUAL(VPP_OK, u32);

    // Valid state. No buffers. Should expect flush done events
    // Input
    u32 = vpp_flush(p, VPP_PORT_INPUT);
    DVP_ASSERT_EQUAL(VPP_OK, u32);
    pthread_mutex_lock(&sMutex);
    while (u32InputFlushRx != VPP_TRUE)
        pthread_cond_wait(&sCond, &sMutex);
    pthread_mutex_unlock(&sMutex);

    // Output
    u32 = vpp_flush(p, VPP_PORT_OUTPUT);
    DVP_ASSERT_EQUAL(VPP_OK, u32);
    pthread_mutex_lock(&sMutex);
    while (u32OutputFlushRx != VPP_TRUE)
        pthread_cond_wait(&sCond, &sMutex);
    pthread_mutex_unlock(&sMutex);

    vpp_term(p);
}

TEST(Vpp_ReconfigBasic)
{
    uint32_t u32;
    void *p = NULL;
    struct vpp_requirements req;

    struct hqv_control ctrl = {
        .mode = HQV_MODE_AUTO,
    };

    t_StVppUsecase *pstUcAuto = getUsecaseAuto(TEST_SUITE_REAL_REGISTRY);
    if (!pstUcAuto)
    {
        DVP_ASSERT_FAIL();
        return;
    }

    u32 = u32VppUsecase_GetMaxResForUc(pstUcAuto);

    struct vpp_port_param param = {
        .width = u32GetMaxWidthForRes(u32),
        .height = u32GetMaxHeightForRes(u32),
        .fmt = VPP_COLOR_FORMAT_NV12_VENUS,
    };

    param.stride = u32VppUtils_CalculateStride(param.width, param.fmt);
    param.scanlines = u32VppUtils_CalculateScanlines(param.height, param.fmt);

    // Garbage params
    u32 = vpp_reconfigure(NULL, param, param);
    DVP_ASSERT_EQUAL(u32, VPP_ERR_PARAM);

    // Transition to inited state
    p = vpp_init(0, sCb);
    DVP_ASSERT_PTR_NNULL(p);

    // Should not be able to reconfigure in INITED state
    u32 = vpp_reconfigure(p, param, param);
    DVP_ASSERT_EQUAL(u32, VPP_ERR_STATE);

    // Transition to active state
    u32 = vpp_set_ctrl(p, ctrl, &req);
    DVP_ASSERT_EQUAL(u32, VPP_OK);
    u32 = vpp_set_parameter(p, VPP_PORT_INPUT, param);
    DVP_ASSERT_EQUAL(u32, VPP_OK);
    u32 = vpp_set_parameter(p, VPP_PORT_OUTPUT, param);
    DVP_ASSERT_EQUAL(u32, VPP_OK);

    // Request reconfigure with no buffers having been queued
    param.width = u32GetMaxWidthForRes(u32 ? u32 - 1 : u32);
    param.height = u32GetMaxHeightForRes (u32 ? u32 - 1 : u32);
    param.stride = u32VppUtils_CalculateStride(param.width, param.fmt);
    param.scanlines = u32VppUtils_CalculateScanlines(param.height, param.fmt);
    u32 = vpp_reconfigure(p, param, param);

    pthread_mutex_lock(&sMutex);
    while (u32ReconfigDoneRx != VPP_TRUE)
        pthread_cond_wait(&sCond, &sMutex);
    pthread_mutex_unlock(&sMutex);

    vpp_term(p);
}

TEST(Vpp_SetVidProp)
{
    uint32_t u32;
    void *p = NULL;
    struct video_property ctrl_set, ctrl_get;

    p = vpp_init(0, sCb);
    DVP_ASSERT_PTR_NNULL(p);

    // Valid set
    ctrl_set.property_type = VID_PROP_CODEC;
    ctrl_set.codec.eCodec = VPP_CODEC_TYPE_H263;
    u32 = vpp_set_vid_prop(p, ctrl_set);
    DVP_ASSERT_EQUAL(u32, VPP_OK);

    // Null context set
    u32 = vpp_set_vid_prop(NULL, ctrl_set);
    DVP_ASSERT_NEQUAL(u32, VPP_OK);

    // Invalid property type set
    ctrl_set.property_type = VID_PROP_MAX;
    u32 = vpp_set_vid_prop(p, ctrl_set);
    DVP_ASSERT_NEQUAL(u32, VPP_OK);

    // Invalid property type get
    ctrl_get.property_type = VID_PROP_MAX;
    u32 = vpp_set_vid_prop(p, ctrl_set);
    DVP_ASSERT_NEQUAL(u32, VPP_OK);

    // Valid get
    ctrl_get.property_type = VID_PROP_CODEC;
    u32 = u32VppUtils_GetVidProp(p, &ctrl_get);
    DVP_ASSERT_EQUAL(u32, VPP_OK);
    DVP_ASSERT_EQUAL(ctrl_set.codec.eCodec, ctrl_get.codec.eCodec);

    // Valid set codec out of range
    ctrl_set.property_type = VID_PROP_CODEC;
    ctrl_set.codec.eCodec = 0xFFFFFFFF;
    u32 = vpp_set_vid_prop(p, ctrl_set);
    DVP_ASSERT_EQUAL(u32, VPP_OK);

    // Valid get (should be unknown)
    ctrl_get.property_type = VID_PROP_CODEC;
    u32 = u32VppUtils_GetVidProp(p, &ctrl_get);
    DVP_ASSERT_EQUAL(u32, VPP_OK);
    DVP_ASSERT_EQUAL(VPP_CODEC_TYPE_UNKNOWN, ctrl_get.codec.eCodec);

    // Null context get
    u32 = u32VppUtils_GetVidProp(NULL, &ctrl_get);
    DVP_ASSERT_NEQUAL(u32, VPP_OK);

    vpp_term(p);
}
/************************************************************************
 * Global Functions
 ***********************************************************************/
TEST_CASES VppTests[] = {
    TEST_CASE(Vpp_InitTermSingle),
    TEST_CASE(Vpp_InitTermMultiple),
    // TEST_CASE(Vpp_InitCpNotSupported),
    TEST_CASE(Vpp_SetCtrl),
    TEST_CASE(Vpp_SetParameter),
    TEST_CASE(Vpp_QueueBuf),
    // TEST_CASE(Vpp_Timestamp),
    TEST_CASE(Vpp_Flush),
    TEST_CASE(Vpp_ReconfigBasic),
    TEST_CASE(Vpp_SetVidProp),
    TEST_CASE_NULL(),
};

TEST_SUITE(VppSuite,
           "Vpp Tests",
           VppSuiteInit,
           VppSuiteTerm,
           VppTestInit,
           VppTestTerm,
           VppTests);
