/*
 * Copyright (c) 2016 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 *
 * Not a Contribution.
 * Apache license notifications and license are retained
 * for attribution purposes only.
 *
 * Copyright 2008, The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define LOG_TAG "WigigHW"

#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <dirent.h>
#include <sys/socket.h>
#include <unistd.h>
#include <poll.h>
#include <sys/syscall.h>
#include <sys/stat.h>

#include "wigig.h"
#include "libwpa_client/wpa_ctrl.h"

#include "cutils/log.h"
#include "cutils/memory.h"
#include "cutils/misc.h"
#include "cutils/properties.h"
#include "private/android_filesystem_config.h"
#define _REALLY_INCLUDE_SYS__SYSTEM_PROPERTIES_H_
#include <sys/_system_properties.h>

#include <openssl/evp.h>
#include <openssl/sha.h>

extern int delete_module(const char *, unsigned int);

static struct wpa_ctrl *ctrl_conn;
static struct wpa_ctrl *monitor_conn;

/* socket pair used to exit from a blocking read */
static int exit_sockets[2];

static char primary_iface[PROPERTY_VALUE_MAX];

static const char IFACE_DIR[]           = "/data/system/wigig_supplicant";
static const char SUPPLICANT_NAME[]     = "wigig_supplicant";
static const char SUPP_PROP_NAME[]      = "init.svc.wigig_supplicant";
static const char SUPP_CONFIG_TEMPLATE[]= "/system/etc/wifi/wigig_supplicant.conf";
static const char SUPP_CONFIG_FILE[]    = "/data/misc/wifi/wigig_supplicant.conf";
static const char SUPP_CLI_SOCK_PATH[]  = "/data/misc/wifi/wigig_sockets";
static const char SUPP_P2P_CONFIG_TEMPLATE[]= "/system/etc/wifi/wigig_p2p_supplicant.conf";
static const char SUPP_P2P_CONFIG_FILE[]    = "/data/misc/wifi/wigig_p2p_supplicant.conf";
static const char MODULE_FILE[]         = "/proc/modules";

static const char HOSTAPD_CONF_FILE[]    = "/data/misc/wifi/wigig_hostapd.conf";

static const char IFNAME[]              = "IFNAME=";
#define IFNAMELEN                       (sizeof(IFNAME) - 1)
static const char WPA_EVENT_IGNORE[]    = "CTRL-EVENT-IGNORE ";
#define WIGIG_DRIVER_MODULE_NAME         "wil6210"
static const char DRIVER_MODULE_NAME[]  = WIGIG_DRIVER_MODULE_NAME;
static const char DRIVER_MODULE_TAG[]   = WIGIG_DRIVER_MODULE_NAME " ";
#define WIGIG_DRIVER_MODULE_PATH         "/system/lib/modules/wil6210.ko"
static const char DRIVER_MODULE_PATH[]  = WIGIG_DRIVER_MODULE_PATH;
#define WIGIG_DRIVER_MODULE_ARG         "alt_ifname=1"
static const char DRIVER_MODULE_ARG[]   = WIGIG_DRIVER_MODULE_ARG;
static const char SUPP_ENTROPY_FILE[]   = "/data/misc/wifi/wigig_entropy.bin";
static unsigned char dummy_key[21]      = { 0x02, 0x11, 0xbe, 0x33, 0x43, 0x35,
                                            0x68, 0x47, 0x84, 0x99, 0xa9, 0x2b,
                                            0x1c, 0xd3, 0xee, 0xff, 0xf1, 0xe2,
                                            0xf3, 0xf4, 0xf5 };

#define WIGIG_DEFAULT_INTERFACE         "wigig0"
#define SYSFS_PATH_MAX                  256
#define WIGIG_AP_CHANNEL_DEFAULT        2
#define PSK_STR_LEN                     (2*SHA256_DIGEST_LENGTH+1)

static int insmod(const char *filename, const char *args)
{
    int rc;
    int fd;

    fd = open(filename, O_RDONLY | O_CLOEXEC);
    if (fd < 0) {
        ALOGE("insmod: open(\"%s\") failed: %s", filename, strerror(errno));
        return -1;
    }

    ALOGD("open file '%s' succeeded\n", filename);

    rc = syscall(__NR_finit_module, fd, args, 0);
    if (rc < 0) {
        ALOGE("finit_module for \"%s\" failed: %s", filename, strerror(errno));
    }
    close(fd);
    return rc;
}

static int rmmod(const char *modname)
{
    int ret = -1;
    int maxtry = 10;

    while (maxtry-- > 0) {
        ret = delete_module(modname, O_NONBLOCK | O_EXCL);
        if (ret < 0 && errno == EAGAIN)
            usleep(500000);
        else
            break;
    }

    if (ret != 0)
        ALOGD("Unable to unload driver module \"%s\": %s\n",
              modname, strerror(errno));
    return ret;
}

int is_wigig_driver_loaded()
{
    char line[sizeof(DRIVER_MODULE_TAG)+10];
    FILE *proc;

    if ((proc = fopen(MODULE_FILE, "r")) == NULL) {
        ALOGW("Could not open %s: %s", MODULE_FILE, strerror(errno));
        return 0;
    }

    while ((fgets(line, sizeof(line), proc)) != NULL) {
        if (strncmp(line, DRIVER_MODULE_TAG, strlen(DRIVER_MODULE_TAG)) == 0) {
            fclose(proc);
            return 1;
        }
    }
    fclose(proc);

    return 0;
}

int wigig_load_driver()
{
    int rc;

    ALOGD("%s", __func__);

    if (is_wigig_driver_loaded())
        return 0;

    rc = insmod(DRIVER_MODULE_PATH, DRIVER_MODULE_ARG);
    if (rc < 0) {
        ALOGE("insmod '%s' failed %d", DRIVER_MODULE_PATH, errno);
        return -1;
    }

    return 0;
}

int wigig_unload_driver()
{
    ALOGD("%s", __func__);

    usleep(200000); /* allow to finish interface down */

    if (!rmmod(DRIVER_MODULE_NAME)) {
        int count = 20; /* wait at most 10 seconds for completion */
        while (count-- > 0) {
            if (!is_wigig_driver_loaded()) {
                usleep(500000); /* allow card removal */
                return 0;
            }
            usleep(500000);
        }
    }
    return -1;
}

int wigig_ensure_config_file_exists(const char *config_file, const char *template_file)
{
    char buf[2048];
    int srcfd, destfd;
    struct stat sb;
    int nread;
    int ret;

    ret = access(config_file, R_OK|W_OK);
    if ((ret == 0) || (errno == EACCES)) {
        if ((ret != 0) &&
            (chmod(config_file, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP) != 0)) {
            ALOGE("Cannot set RW to \"%s\": %s", config_file, strerror(errno));
            return -1;
        }
        return 0;
    }
    if (errno != ENOENT) {
        ALOGE("Cannot access \"%s\": %s", config_file, strerror(errno));
        return -1;
    }

    srcfd = TEMP_FAILURE_RETRY(open(template_file, O_RDONLY));
    if (srcfd < 0) {
        ALOGE("Cannot open \"%s\": %s", template_file, strerror(errno));
        return -1;
    }

    destfd = TEMP_FAILURE_RETRY(open(config_file, O_CREAT|O_RDWR, 0660));
    if (destfd < 0) {
        close(srcfd);
        ALOGE("Cannot create \"%s\": %s", config_file, strerror(errno));
        return -1;
    }

    while ((nread = TEMP_FAILURE_RETRY(read(srcfd, buf, sizeof(buf)))) != 0) {
        if (nread < 0) {
            ALOGE("Error reading \"%s\": %s", template_file, strerror(errno));
            close(srcfd);
            close(destfd);
            unlink(config_file);
            return -1;
        }
        TEMP_FAILURE_RETRY(write(destfd, buf, nread));
    }

    close(destfd);
    close(srcfd);

    /* chmod is needed because open() didn't set permisions properly */
    if (chmod(config_file, 0660) < 0) {
        ALOGE("Error changing permissions of %s to 0660: %s",
              config_file, strerror(errno));
        unlink(config_file);
        return -1;
    }

    if (chown(config_file, AID_SYSTEM, AID_WIFI) < 0) {
        ALOGE("Error changing group ownership of %s to %d: %s",
              config_file, AID_WIFI, strerror(errno));
        unlink(config_file);
        return -1;
    }
    return 0;
}

int wigig_ensure_entropy_file_exists()
{
    int ret;
    int destfd;

    ret = access(SUPP_ENTROPY_FILE, R_OK|W_OK);
    if ((ret == 0) || (errno == EACCES)) {
        if ((ret != 0) &&
            (chmod(SUPP_ENTROPY_FILE, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP) != 0)) {
            ALOGE("Cannot set RW to \"%s\": %s", SUPP_ENTROPY_FILE, strerror(errno));
            return -1;
        }
        return 0;
    }
    destfd = TEMP_FAILURE_RETRY(open(SUPP_ENTROPY_FILE, O_CREAT|O_RDWR, 0660));
    if (destfd < 0) {
        ALOGE("Cannot create \"%s\": %s", SUPP_ENTROPY_FILE, strerror(errno));
        return -1;
    }

    if (TEMP_FAILURE_RETRY(write(destfd, dummy_key, sizeof(dummy_key))) != sizeof(dummy_key)) {
        ALOGE("Error writing \"%s\": %s", SUPP_ENTROPY_FILE, strerror(errno));
        close(destfd);
        return -1;
    }
    close(destfd);

    /* chmod is needed because open() didn't set permisions properly */
    if (chmod(SUPP_ENTROPY_FILE, 0660) < 0) {
        ALOGE("Error changing permissions of %s to 0660: %s",
              SUPP_ENTROPY_FILE, strerror(errno));
        unlink(SUPP_ENTROPY_FILE);
        return -1;
    }

    if (chown(SUPP_ENTROPY_FILE, AID_SYSTEM, AID_WIFI) < 0) {
        ALOGE("Error changing group ownership of %s to %d: %s",
              SUPP_ENTROPY_FILE, AID_WIFI, strerror(errno));
        unlink(SUPP_ENTROPY_FILE);
        return -1;
    }
    return 0;
}

void wigig_wpa_cleanup(const char *cli_path)
{
    DIR *dir;
    struct dirent entry;
    struct dirent *result;
    size_t dirnamelen;
    size_t maxcopy;
    char pathname[PATH_MAX];
    char *namep;

    if ((dir = opendir(cli_path)) == NULL)
        return;

    dirnamelen = (size_t) snprintf(pathname, sizeof(pathname), "%s/",
                                   cli_path);
    if (dirnamelen >= sizeof(pathname)) {
        closedir(dir);
        return;
    }
    namep = pathname + dirnamelen;
    maxcopy = PATH_MAX - dirnamelen;
    while (readdir_r(dir, &entry, &result) == 0 && result != NULL) {
        if (strlcpy(namep, entry.d_name, maxcopy) < maxcopy)
            unlink(pathname);
    }
    closedir(dir);
}

int wigig_start_supplicant()
{
    char supp_status[PROPERTY_VALUE_MAX] = {'\0'};
    int count = 200; /* wait at most 20 seconds for completion */
    const prop_info *pi;
    unsigned serial = 0, i;

    /* Check whether already running */
    if (property_get(SUPP_PROP_NAME, supp_status, NULL)
            && strcmp(supp_status, "running") == 0)
        return 0;

    /* Before starting the daemon, make sure its config file exists */
    if ((wigig_ensure_config_file_exists(SUPP_CONFIG_FILE, SUPP_CONFIG_TEMPLATE) < 0) ||
        (wigig_ensure_config_file_exists(SUPP_P2P_CONFIG_FILE, SUPP_P2P_CONFIG_TEMPLATE) < 0)) {
        ALOGE("Wigig will not be enabled");
        return -1;
    }

    if (wigig_ensure_entropy_file_exists() < 0)
        ALOGE("Wigig entropy file was not created");

    /* Clear out any stale socket files that might be left over. */
    wigig_wpa_cleanup(SUPP_CLI_SOCK_PATH);

    /* Reset sockets used for exiting from hung state */
    exit_sockets[0] = exit_sockets[1] = -1;

    /*
     * Get a reference to the status property, so we can distinguish
     * the case where it goes stopped => running => stopped (i.e.,
     * it start up, but fails right away) from the case in which
     * it starts in the stopped state and never manages to start
     * running at all.
     */
    pi = __system_property_find(SUPP_PROP_NAME);
    if (pi != NULL)
        serial = __system_property_serial(pi);

    property_get("wigig.interface", primary_iface, WIGIG_DEFAULT_INTERFACE);

    property_set("ctl.start", SUPPLICANT_NAME);
    sched_yield();

    while (count-- > 0) {
        if (pi == NULL)
            pi = __system_property_find(SUPP_PROP_NAME);
        if (pi != NULL) {
            /*
             * property serial updated means that init process is scheduled
             * after we sched_yield, further property status checking is based on this */
            if (__system_property_serial(pi) != serial) {
                __system_property_read(pi, NULL, supp_status);
                if (strcmp(supp_status, "running") == 0)
                    return 0;
                else if (strcmp(supp_status, "stopped") == 0)
                    return -1;
            }
        }
        usleep(100000);
    }
    return -1;
}

int wigig_stop_supplicant()
{
    char supp_status[PROPERTY_VALUE_MAX] = {'\0'};
    int count = 50; /* wait at most 5 seconds for completion */

    /* Check whether supplicant already stopped */
    if (property_get(SUPP_PROP_NAME, supp_status, NULL)
            && strcmp(supp_status, "stopped") == 0)
        return 0;

    property_set("ctl.stop", SUPPLICANT_NAME);
    sched_yield();

    while (count-- > 0) {
        if (property_get(SUPP_PROP_NAME, supp_status, NULL)) {
            if (strcmp(supp_status, "stopped") == 0)
                return 0;
        }
        usleep(100000);
    }
    ALOGE("Failed to stop supplicant");
    return -1;
}

int wigig_connect_on_socket_path(const char *path)
{
    char supp_status[PROPERTY_VALUE_MAX] = {'\0'};

    /* Make sure supplicant is running */
    if (!property_get(SUPP_PROP_NAME, supp_status, NULL)
            || strcmp(supp_status, "running")) {
        ALOGE("Supplicant not running, cannot connect");
        return -1;
    }

    ctrl_conn = wpa_ctrl_open2(path, SUPP_CLI_SOCK_PATH);
    ALOGI("Trying to connect to path: %s\n", path);
    if (ctrl_conn == NULL) {
        ALOGE("Unable to open connection to supplicant on \"%s\": %s",
              path, strerror(errno));
        return -1;
    }

    ALOGI("Opened connection to supplicant on %s\n", path);

    monitor_conn = wpa_ctrl_open2(path, SUPP_CLI_SOCK_PATH);
    if (monitor_conn == NULL) {
        wpa_ctrl_close(ctrl_conn);
        ctrl_conn = NULL;
        return -1;
    }
    if (wpa_ctrl_attach(monitor_conn) != 0) {
        wpa_ctrl_close(monitor_conn);
        wpa_ctrl_close(ctrl_conn);
        ctrl_conn = monitor_conn = NULL;
        return -1;
    }

    if (socketpair(AF_UNIX, SOCK_STREAM, 0, exit_sockets) == -1) {
        wpa_ctrl_close(monitor_conn);
        wpa_ctrl_close(ctrl_conn);
        ctrl_conn = monitor_conn = NULL;
        return -1;
    }

    return 0;
}

/* Establishes the control and monitor socket connections on the interface */
int wigig_connect_to_supplicant()
{
    static char path[PATH_MAX];

    if (access(IFACE_DIR, F_OK) == 0)
        snprintf(path, sizeof(path), "%s/%s", IFACE_DIR, primary_iface);
    else
        snprintf(path, sizeof(path), "@android:wpa_%s", primary_iface);

    return wigig_connect_on_socket_path(path);
}

int wigig_send_command(const char *cmd, char *reply, size_t *reply_len)
{
    int ret;

    if (ctrl_conn == NULL) {
        ALOGV("Not connected to wpa_supplicant - \"%s\" command dropped.\n", cmd);
        return -1;
    }
    ret = wpa_ctrl_request(ctrl_conn, cmd, strlen(cmd), reply, reply_len, NULL);
    if (ret == -2) {
        ALOGD("'%s' command timed out.\n", cmd);
        /* unblocks the monitor receive socket for termination */
        TEMP_FAILURE_RETRY(write(exit_sockets[0], "T", 1));
        return -2;
    }
    else if (ret < 0 || strncmp(reply, "FAIL", 4) == 0)
        return -1;

    if (strncmp(cmd, "PING", 4) == 0)
        reply[*reply_len] = '\0';

    return 0;
}

int wigig_supplicant_connection_active()
{
    char supp_status[PROPERTY_VALUE_MAX] = {'\0'};

    if (property_get(SUPP_PROP_NAME, supp_status, NULL)) {
        if (strcmp(supp_status, "stopped") == 0)
            return -1;
    }

    return 0;
}

int wigig_ctrl_recv(char *reply, size_t *reply_len)
{
    int res;
    int ctrlfd = wpa_ctrl_get_fd(monitor_conn);
    struct pollfd rfds[2];

    memset(rfds, 0, 2 * sizeof(struct pollfd));
    rfds[0].fd = ctrlfd;
    rfds[0].events |= POLLIN;
    rfds[1].fd = exit_sockets[1];
    rfds[1].events |= POLLIN;

    do {
        res = TEMP_FAILURE_RETRY(poll(rfds, 2, 30000));
        if (res < 0) {
            ALOGE("Error poll = %d", res);
            return res;
        }
        else if (res == 0) {
            /* timed out, check if supplicant is active
             * or not ..
             */
            res = wigig_supplicant_connection_active();
            if (res < 0)
                return -2;
        }
    } while (res == 0);

    if (rfds[0].revents & POLLIN)
        return wpa_ctrl_recv(monitor_conn, reply, reply_len);

    /* it is not rfds[0], then it must be rfts[1] (i.e. the exit socket)
     * or we timed out. In either case, this call has failed ..
     */
    return -2;
}

int wigig_wait_on_socket(char *buf, size_t buflen)
{
    size_t nread = buflen - 1;
    int result;
    char *match, *match2;

    if (monitor_conn == NULL)
        return snprintf(buf, buflen, "IFNAME=%s %s - connection closed",
                        primary_iface, WPA_EVENT_TERMINATING);

    result = wigig_ctrl_recv(buf, &nread);

    /* Terminate reception on exit socket */
    if (result == -2)
        return snprintf(buf, buflen, "IFNAME=%s %s - connection closed",
                        primary_iface, WPA_EVENT_TERMINATING);

    if (result < 0) {
        ALOGD("wigig_ctrl_recv failed: %s\n", strerror(errno));
        return snprintf(buf, buflen, "IFNAME=%s %s - recv error",
                        primary_iface, WPA_EVENT_TERMINATING);
    }
    buf[nread] = '\0';
    /* Check for EOF on the socket */
    if (result == 0 && nread == 0) {
        /* Fabricate an event to pass up */
        ALOGD("Received EOF on supplicant socket\n");
        return snprintf(buf, buflen, "IFNAME=%s %s - signal 0 received",
                        primary_iface, WPA_EVENT_TERMINATING);
    }
    /*
     * Events strings are in the format
     *
     *     IFNAME=iface <N>CTRL-EVENT-XXX
     *        or
     *     <N>CTRL-EVENT-XXX
     *
     * where N is the message level in numerical form (0=VERBOSE, 1=DEBUG,
     * etc.) and XXX is the event name. The level information is not useful
     * to us, so strip it off.
     */

    if (strncmp(buf, IFNAME, IFNAMELEN) == 0) {
        match = strchr(buf, ' ');
        if (match != NULL) {
            if (match[1] == '<') {
                match2 = strchr(match + 2, '>');
                if (match2 != NULL) {
                    nread -= (match2 - match);
                    memmove(match + 1, match2 + 1, nread - (match - buf) + 1);
                }
            }
        }
        else
            return snprintf(buf, buflen, "%s", WPA_EVENT_IGNORE);
    }
    else if (buf[0] == '<') {
        match = strchr(buf, '>');
        if (match != NULL) {
            nread -= (match + 1 - buf);
            memmove(buf, match + 1, nread + 1);
            ALOGV("supplicant generated event without interface - %s\n", buf);
        }
    }
    else
        /* let the event go as is! */
        ALOGW("supplicant generated event without interface and without message level - %s\n", buf);

    return nread;
}

int wigig_wait_for_event(char *buf, size_t buflen)
{
    return wigig_wait_on_socket(buf, buflen);
}

void wigig_close_sockets()
{
    if (ctrl_conn != NULL) {
        wpa_ctrl_close(ctrl_conn);
        ctrl_conn = NULL;
    }

    if (monitor_conn != NULL) {
        wpa_ctrl_close(monitor_conn);
        monitor_conn = NULL;
    }

    if (exit_sockets[0] >= 0) {
        close(exit_sockets[0]);
        exit_sockets[0] = -1;
    }

    if (exit_sockets[1] >= 0) {
        close(exit_sockets[1]);
        exit_sockets[1] = -1;
    }
}

void wigig_close_supplicant_connection()
{
    char supp_status[PROPERTY_VALUE_MAX] = {'\0'};
    int count = 50; /* wait at most 5 seconds to ensure init has stopped stupplicant */

    wigig_close_sockets();

    while (count-- > 0) {
        if (property_get(SUPP_PROP_NAME, supp_status, NULL)) {
            if (strcmp(supp_status, "stopped") == 0)
                return;
        }
        usleep(100000);
    }
}

int wigig_command(const char *command, char *reply, size_t *reply_len)
{
    return wigig_send_command(command, reply, reply_len);
}

/**
 * Soft AP
 */

static int wigig_generate_psk(const char *ssid, const char *passphrase, char *psk_str,
                              size_t psk_str_len)
{
    unsigned char psk[SHA256_DIGEST_LENGTH];
    int j;

    if (psk_str_len < PSK_STR_LEN) {
        ALOGE("psk_str_len is too short (%zu)", psk_str_len);
        return -1;
    }

    // Use the PKCS#5 PBKDF2 with 4096 iterations
    if (PKCS5_PBKDF2_HMAC_SHA1(passphrase, strlen(passphrase),
                               (const uint8_t *)ssid, strlen(ssid), 4096, SHA256_DIGEST_LENGTH,
                               psk) != 1) {
        ALOGE("Cannot generate PSK using PKCS#5 PBKDF2");
        return -1;
    }

    for (j=0; j < SHA256_DIGEST_LENGTH; j++) {
        snprintf(&psk_str[j*2], psk_str_len - j*2, "%02x", psk[j]);
    }

    return 0;
}

int wigig_set_softap(const char* ifname, const char* ssid, int hidden, int channel,
                     int secured, const char* key)
{
    char buf[512];
    int res = 0;

    FILE *f;
    f = fopen(HOSTAPD_CONF_FILE, "w");
    if (f == NULL) {
        ALOGE("%s : unable to open the file\n", __func__);
        return -1;
    }

    if (channel <= 0) {
        channel = WIGIG_AP_CHANNEL_DEFAULT;
    }

    res = snprintf(buf, sizeof(buf),
                   "interface=%s\n"
                   "ctrl_interface=/data/misc/wifi/hostapd\n"
                   "ssid=%s\n"
                   "channel=%d\n"
                   "hw_mode=ad\n"
                   "ignore_broadcast_ssid=%d\n", ifname, ssid, channel, hidden);
    if (res < 0) {
        ALOGE("fail to create hostapd conf file");
        goto out;
    }

    res = fprintf(f, "%s", buf);
    if (res < 0) {
        ALOGE("fail to write to hostapd conf file");
        goto out;
    }

    if (secured) {
        char psk_str[PSK_STR_LEN];

        if (wigig_generate_psk(ssid, key, psk_str, sizeof(psk_str))) {
            res = -1;
            goto out;
        }

        res = snprintf(buf, sizeof(buf),
                       "wpa=2\n"
                       "rsn_pairwise=GCMP\n"
                       "wpa_psk=%s\n", psk_str);
        if (res < 0) {
            ALOGE("fail to create secured config");
            goto out;
        }

        res = fprintf(f, "%s", buf);
        if (res < 0) {
            ALOGE("fail to write secured config to hostapd conf file");
            goto out;
        }
    }

    /* chmod is needed because open() didn't set permissions properly */
    if (chmod(HOSTAPD_CONF_FILE, 0660) < 0) {
        ALOGE("Error changing permissions of %s to 0660: %s",
              HOSTAPD_CONF_FILE, strerror(errno));
        unlink(HOSTAPD_CONF_FILE);
        res = -1;
        goto out;
    }

    if (chown(HOSTAPD_CONF_FILE, AID_SYSTEM, AID_WIFI) < 0) {
        ALOGE("Error changing group ownership of %s to %d: %s",
              HOSTAPD_CONF_FILE, AID_WIFI, strerror(errno));
        unlink(HOSTAPD_CONF_FILE);
        res = -1;
        goto out;
    }

out:
    fclose(f);

    return ((res < 0) ? res : 0);
}
