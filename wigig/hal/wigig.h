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

#ifndef _WIGIG_H
#define _WIGIG_H

#if __cplusplus
extern "C" {
#endif

/**
 * Load the WiGiG driver.
 *
 * @return 0 on success, < 0 on failure.
 */
int wigig_load_driver();

/**
 * Unload the WiGiG driver.
 *
 * @return 0 on success, < 0 on failure.
 */
int wigig_unload_driver();

/**
 * Check if the WiGiG driver is loaded.
 *
 * @return 0 on success, < 0 on failure.
 */
int is_wigig_driver_loaded();


/**
 * Start supplicant.
 *
 * @return 0 on success, < 0 on failure.
 */
int wigig_start_supplicant();

/**
 * Stop supplicant.
 *
 * @return 0 on success, < 0 on failure.
 */
int wigig_stop_supplicant();

/**
 * Open a connection to supplicant
 *
 * @return 0 on success, < 0 on failure.
 */
int wigig_connect_to_supplicant();

/**
 * Close connection to supplicant
 *
 * @return 0 on success, < 0 on failure.
 */
void wigig_close_supplicant_connection();

/**
 * wigig_wait_for_event() performs a blocking call to
 * get a WiGiG event and returns a string representing
 * a WiGiG event when it occurs.
 *
 * @param buf is the buffer that receives the event
 * @param len is the maximum length of the buffer
 *
 * @returns number of bytes in buffer, 0 if no
 * event (for instance, no connection), and less than 0
 * if there is an error.
 */
int wigig_wait_for_event(char *buf, size_t len);

/**
 * wigig_command() issues a command to the WiGiG driver.
 *
 * Android extends the standard commands listed at
 * /link http://hostap.epitest.fi/wpa_supplicant/devel/ctrl_iface_page.html
 * to include support for sending commands to the driver:
 *
 * See wifi/java/android/net/wifi/WifiNative.java for the details of
 * driver commands that are supported
 *
 * @param command is the string command (preallocated with 32 bytes)
 * @param commandlen is command buffer length
 * @param reply is a buffer to receive a reply string
 * @param reply_len on entry, this is the maximum length of
 *        the reply buffer. On exit, the number of
 *        bytes in the reply buffer.
 *
 * @return 0 if successful, < 0 if an error.
 */
int wigig_command(const char *command, char *reply, size_t *reply_len);

/**
 * Set parameters for Soft AP
 *
 * This function creates and populates Wigig hostapd conf file with the requested configuration
 *
 * @return 0 on success, < 0 on failure.
 */
int wigig_set_softap(const char* ifname, const char* ssid, int hidden, int channel,
                     int secured, const char *key);

#if __cplusplus
};  // extern "C"
#endif

#endif  // _WIGIG_H
