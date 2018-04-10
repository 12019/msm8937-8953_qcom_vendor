/*=============================================================================
Copyright (c) 2016 Qualcomm Technologies, Inc.
All Rights Reserved.
Confidential and Proprietary - Qualcomm Technologies, Inc.
=============================================================================*/

/**
 * @file spdaemon.c
 * @brief - Secure Processor Daemon (spdaemon)
 *
 * This driver is responsible for loading SP Applications that
 * doesn't have an owner HLOS Application.
 */

/*-------------------------------------------------------------------------
 * Include Files
 * ----------------------------------------------------------------------*/
#include <stdlib.h>    /* malloc() */
#include <stdio.h>     /* fopen() */
#include <fcntl.h>     /* O_RDWR */
#include <unistd.h>    /* sleep() / usleep() */
#include <errno.h>     /* ENODEV */
#include <memory.h>
#include <pthread.h>

#include <cutils/log.h>        /* SLOGE() */

#include <spcomlib.h>

#include "mdm_detect.h"
#include "pm-service.h"

extern void* spcom_ion_alloc(uint32_t size, int *fd_ptr);
extern void spcom_ion_free(void *vbuf, int size, int fd);

/*-------------------------------------------------------------------------
 * Preprocessor Definitions and Constants
 * ----------------------------------------------------------------------*/
#ifdef PRINT_LOG_TO_STDOUT
  #define LOGD(fmt, x...) printf("spdaemon: dbg: %s: " fmt, __func__, ##x)
  #define LOGE(fmt, x...) printf("spdaemon: err: %s: " fmt, __func__, ##x)
#else /* print to system log a.k.a logcat */
  #undef LOG_TAG
  #undef LOGD
  #undef LOGE
  #define LOG_TAG "spdaemon"
  #define LOGD SLOGD
  #define LOGE SLOGE
#endif

#define SZ_1K (1024)
#define SZ_8K (8 * 1024)
#define SZ_1M (1024 * 1024)

#define KEYMASTER_ION_BUF_SIZE (4 * SZ_1K)

#ifndef OFFSET_OF
    #define OFFSET_OF(field, base) ((int)((char*)(field) - (char*)(base)))
#endif

/** Command Opcode */
#define KEYMASTER_INIT_CMD_ID  0xAA

/*-------------------------------------------------------------------------
 * Structures and enums
 * ----------------------------------------------------------------------*/

struct sp_app_info {
        const char *ch_name;
        const char *file_path;
        int swap_size;
};

struct keymaster_init_request {
        uint32_t cmd_opcode;
        uint64_t ion_buf_virt_addr;
} __attribute__((packed));

struct keymaster_init_response {
        uint32_t cmd_opcode;
        uint32_t error_code;
}  __attribute__((packed));

/* Note: The request and response structure format should be packed */

/*-------------------------------------------------------------------------
 * Global Variables
 * ----------------------------------------------------------------------*/

static const struct sp_app_info keymaster_app_info = {
        .ch_name = "sp_keymaster",
        .file_path = "/firmware/image/keymaster.signed",
        .swap_size = 256 * SZ_1K,
};

static const struct sp_app_info cryptoapp_app_info = {
        .ch_name = "cryptoapp",
        .file_path = "/firmware/image/cryptoapp.signed",
        .swap_size = 256 * SZ_1K,
};

static int keymaster_ion_fd = -1;
static void *keymaster_ion_vbuf = NULL;
static struct spcom_client *keymaster_spcom_client = NULL;

static pthread_mutex_t pthread_mutex;
static pthread_cond_t  pthread_cond;
static bool sp_reset_detected = false;

static void *pm_spss_handle = NULL;

/*-------------------------------------------------------------------------
 * Function Implementations
 * ----------------------------------------------------------------------*/

static void suspend_me(void)
{
    pthread_mutex_lock(&pthread_mutex);
    pthread_cond_wait(&pthread_cond, &pthread_mutex);
}

static void resume_me(void)
{
    pthread_cond_signal(&pthread_cond);
    pthread_mutex_unlock(&pthread_mutex);
}

static int load_app(const struct sp_app_info *info)
{
       int ret;
       bool is_loaded = false;
       int load_timeout_sec = 60;
       const char *ch_name = info->ch_name;

       /* check if already loaded */
       is_loaded = spcom_is_app_loaded(ch_name);
       if (is_loaded) {
               LOGD("SP App [%s] already loaded.\n", ch_name);
               return 0;
       }

       /* Load the app */
       ret = spcom_load_app(info->ch_name,
                            info->file_path,
                            info->swap_size);
       if (ret < 0) {
               LOGE("Loading SP App [%s] failed. ret [%d].\n", ch_name, ret);
       }

       /* wait for chardev node created */
       while (!is_loaded) {
              is_loaded = spcom_is_app_loaded(ch_name);
              sleep(1);
              load_timeout_sec--;
              if (load_timeout_sec == 0) {
                      LOGE("Timeout wait for char dev creation.\n");
                      return -ETIMEDOUT;
              }
       }

       LOGD("SP App chardev [%s] created ok.\n", ch_name);

       return 0;
}

/**
 * spcom_notify_ssr_cb() - a callback to notify on SP SubSystem-Reset (SSR).
 *
 * The spcom shall notify upon SP reset.
 * This callback should wait until the SP is up again (LINK is UP),
 * and then re-load the SP Applications and do any init sequence required.
 */
static void spcom_notify_ssr_cb(void)
{
        LOGD("SP subsystem reset detected.\n");

        sp_reset_detected = true;

        resume_me();
}

/**
 * Init Keymaster App.
 *
 * The SP App needs a "work-buffer" carved from the DDR by ION.
 *
 */
static int sp_keymaster_init(void)
{
        int ret;
        int ion_fd = -1;
        void *ion_vbuf = NULL;
        struct keymaster_init_request req = {0};
        struct keymaster_init_response resp = {0};
        struct spcom_client *client = NULL;
        struct spcom_client_info reg_info = {0};
        bool is_connected = false;
        int connect_timeout_sec = 60;
        struct spcom_ion_info_table ion_info_table;
        struct spcom_ion_info *ion_info = ion_info_table.ion_info;
        uint32_t cmd_timeout_msec = 1000; // timeout for SP to handle the command
        const char *ch_name = keymaster_app_info.ch_name;

        ion_info_table.ion_info[0].fd = -1 ; // mark as invalid
        ion_info_table.ion_info[1].fd = -1 ; // mark as invalid
        ion_info_table.ion_info[2].fd = -1 ; // mark as invalid
        ion_info_table.ion_info[3].fd = -1 ; // mark as invalid

        reg_info.ch_name = ch_name;
        reg_info.notify_ssr_cb = spcom_notify_ssr_cb;

        /* register to spcom for sending request */
        client = spcom_register_client(&reg_info);
        if (client == NULL) {
                LOGE("spcom register failed.\n");
                goto exit_err;
        }

        /* wait for remote SP App to connect */
        while (!is_connected) {
              is_connected = spcom_client_is_server_connected(client);
              sleep(1);
              connect_timeout_sec--;
              if (connect_timeout_sec == 0) {
                      LOGE("Timeout wait for ch CONNECT.\n");
                      goto exit_err;
              }
        }

        ion_vbuf = spcom_ion_alloc(KEYMASTER_ION_BUF_SIZE, &ion_fd);
        if (ion_vbuf == NULL) {
                LOGE("Failed to allocate ION buffer.\n");
                goto exit_err;
        }

        ion_info->fd = ion_fd;
        ion_info->buf_offset = OFFSET_OF(&req.ion_buf_virt_addr, &req);

        req.cmd_opcode = KEYMASTER_INIT_CMD_ID;
        req.ion_buf_virt_addr = (uint64_t) ion_vbuf;

        ret = spcom_client_send_modified_command(client,
                                                 &req, sizeof(req),
                                                 &resp, sizeof(resp),
                                                 &ion_info_table,
                                                 cmd_timeout_msec);
        if (ret <= 0) {
                LOGE("send command failed.\n");
                goto exit_err;
        }

        if (resp.error_code != 0) {
                LOGE("response error_code [%d]\n", resp.error_code);
                goto exit_err;
        }

        ret = spcom_lock_ion_buffer(ch_name, ion_fd);
        if (ret < 0) {
                LOGE("lock ION buf failed.\n");
                goto exit_err;
        }

        /* Note1: don't unregister spcom for SSR awareness */

        /* Note2: don't free ION buf as long as SP App is using it. */

        LOGD("Keymaster init completed ok.\n");
        keymaster_ion_fd = ion_fd;
        keymaster_ion_vbuf = ion_vbuf;
        keymaster_spcom_client = client;

        return 0;
exit_err:
        if (client != NULL)
                spcom_unregister_client(client);
        if (ion_vbuf != NULL)
                spcom_ion_free(ion_vbuf, KEYMASTER_ION_BUF_SIZE, ion_fd);

        return -EFAULT;
}

/**
 * Keymaster App exit cleanup.
 *
 */
static int sp_keymaster_exit(void)
{
        spcom_unlock_ion_buffer(keymaster_app_info.ch_name, keymaster_ion_fd);
        spcom_ion_free(keymaster_ion_vbuf, KEYMASTER_ION_BUF_SIZE, keymaster_ion_fd);
        spcom_unregister_client(keymaster_spcom_client);

        return 0;
}


/**
 *  Wait until SP is up and running.
 */
static int wait_for_sp_link_up(int timeout_sec)
{
        bool sp_is_up = false;

        while (!sp_is_up && timeout_sec > 0) {
                sp_is_up = spcom_is_sp_subsystem_link_up();
                sleep(1);
                timeout_sec--;
                if (timeout_sec == 0) {
                       LOGE("Timeout wait for SP link UP.\n");
                       return -ETIMEDOUT;
                }
        }

        LOGD("SP LINK is UP.\n");

        return 0;
}

/**
 * Load SP App
 */
static int keymaster_load_and_init(void)
{
        int ret;

        ret = load_app(&keymaster_app_info);
        if (ret < 0) {
               LOGE("Loading SP keymaster App failed. ret [%d].\n", ret);
               return -EFAULT;
        }

        LOGD("SP keymaster App is loaded.\n");

        ret = sp_keymaster_init();
        if (ret < 0) {
              LOGE("Init SP keymaster App failed. ret [%d].\n", ret);
              return -EFAULT;
        }

        LOGD("SP keymaster App is initialized successfully.\n");

        return 0;
}

static void pm_spss_event_notifier(void *client_cookie, enum pm_event event)
{
        /* Don't really care much here about events.Just ack whatever comes in. */
        pm_client_event_acknowledge(pm_spss_handle, event);
}

/*
 * PIL load sp
 */
static int pil_load_sp(void)
{
        int i, ret = 0;
        struct dev_info devinfo = {0};
        enum pm_event event = 0;
        bool spss_found = false;

        LOGD("Starting to get system info");
        ret = get_system_info(&devinfo);
        if (ret != RET_SUCCESS) {
                LOGE("Failed to get_system_info");
                goto error;
        }

        LOGD("devinfo.num_additional_subsystems = %d",
            devinfo.num_additional_subsystems);
        for (i = 0; i < devinfo.num_additional_subsystems; i++) {
             LOGD("devinfo.subsystem_list[%d].type = %d",
                  i, devinfo.subsystem_list[i].type);
             if (devinfo.subsystem_list[i].type == SUBSYS_TYPE_SPSS) {
                  spss_found = true;
                  LOGD("Found spss subsystem.");
                  LOGD("devinfo.subsystem_list[%d].mdm_name = %s",
                        i , devinfo.subsystem_list[i].mdm_name);
                  ret = pm_client_register(pm_spss_event_notifier,
                                 pm_spss_handle,
                                 devinfo.subsystem_list[i].mdm_name,
                                 "spss",
                                 &event,
                                 &pm_spss_handle);
                  if (ret != PM_RET_SUCCESS) {
                       LOGE("pm_client_register failed. ret [%d].\n", ret);
                       goto error;
                  }
                  LOGD("pm-spss-thread Voting for spss subsystem");
                  ret = pm_client_connect(pm_spss_handle);
                  if (ret != PM_RET_SUCCESS) {
                       LOGE("pm_client_connect() for spss fail. ret [%d].\n", ret);
                       goto error;
                  }
             }
        }

        if (!spss_found) {
                LOGE("SUBSYS_TYPE_SPSS not found.\n");
                ret = -1;
        }
        LOGD("SPSS-PIL completed successfully");

        return 0;
error:
        LOGD("SPSS-PIL failed.");
        return ret;
}

/**
 *  re-load SP App after SP reset
 */
static int handle_sp_reset_event(void)
{
        int ret;
        int link_up_timeout_sec = 60;

        ret = wait_for_sp_link_up(link_up_timeout_sec);
        if (ret < 0) {
               LOGE("wait_for_sp_link_up failed. ret [%d].\n", ret);
               return ret;
        }

        ret = load_app(&cryptoapp_app_info);
        if (ret < 0) {
               LOGE("Loading SP cryptoapp App failed. ret [%d].\n", ret);
               return ret;
        }

        ret = keymaster_load_and_init();
        if (ret < 0) {
               LOGE("keymaster_load_and_init failed. ret [%d].\n", ret);
               return ret;
        }

        return 0;
}

int main(void)
{
        int ret;
        int link_up_timeout_sec = 60;
        pthread_t self_id = pthread_self();

        LOGD("Version 1.0 16-Aug-2016.\n");

        ret = pil_load_sp();
        if (ret != 0) {
            LOGE("pil_load_sp failed. ret [%d].\n", ret);
            return ret;
        }

        LOGD("Wait for sp link up.\n");
        ret = wait_for_sp_link_up(link_up_timeout_sec);
        if (ret < 0)
                return ret;

        LOGD("Check if cryptoapp app is already loaded.\n");
        if (!spcom_is_app_loaded(cryptoapp_app_info.ch_name)) {
            LOGD("Load SP cryptoapp App.\n");
            ret = load_app(&cryptoapp_app_info);
            if (ret < 0) {
                 LOGE("Loading SP cryptoapp App failed. ret [%d].\n", ret);
                 return -EFAULT;
            }
        }

        LOGD("Check if keymaster app is already loaded.\n");
        if (!spcom_is_app_loaded(keymaster_app_info.ch_name)) {
            LOGD("Load SP Keymaster App.\n");
            ret = keymaster_load_and_init();
            if (ret < 0) {
                 LOGE("Loading SP keymaster App failed. ret [%d].\n", ret);
                 return ret;
            }
        }

        LOGD("Go to sleep , nothing to do now , wake up upon SP reset event.\n");

        /* go to sleep , nothing to do now , wake up upon SP reset event */
        pthread_cond_init(&pthread_cond, NULL);
        pthread_mutex_init(&pthread_mutex, NULL);

        while(1) {
                sp_reset_detected = false;
                suspend_me();
                if (sp_reset_detected)
                        handle_sp_reset_event();
        }

        sp_keymaster_exit(); /* should probably never happen */

        return 0;
}
