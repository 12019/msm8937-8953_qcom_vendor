/*
 * Copyright (c) 2016 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 *
 * Not a Contribution.
 * Apache license notifications and license are retained
 * for attribution purposes only.
 *
 * Copyright (C) 2008 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define LOG_TAG "wigig_jni"

#include "jni.h"
#include <ScopedUtfChars.h>
#include <android_runtime/AndroidRuntime.h>
#include <utils/Log.h>
#include <ctype.h>

#include "wigig.h"

#define REPLY_BUF_SIZE 4096 // wpa_supplicant's maximum size.
#define EVENT_BUF_SIZE 2048

namespace android
{

static jint DBG = true;

static bool doCommand(JNIEnv* env, jstring javaCommand,
                      char* reply, size_t reply_len)
{
    ScopedUtfChars command(env, javaCommand);
    if (command.c_str() == NULL)
        return false; // ScopedUtfChars already threw on error.

    if (DBG)
        ALOGD("doCommand: %s", command.c_str());

    --reply_len; // Ensure we have room to add NUL termination.
    if (::wigig_command(command.c_str(), reply, &reply_len) != 0) {
        ALOGE("failed to perform command");
        return false;
    }

    // Strip off trailing newline.
    if (reply_len > 0 && reply[reply_len-1] == '\n')
        reply[reply_len-1] = '\0';
    else
        reply[reply_len] = '\0';

    if (DBG)
        ALOGD("reply: %s", reply);

    return true;
}

static jint doIntCommand(JNIEnv* env, jstring javaCommand)
{
    char reply[REPLY_BUF_SIZE];

    if (!doCommand(env, javaCommand, reply, sizeof(reply)))
        return -1;

    return static_cast<jint>(atoi(reply));
}

static jboolean doBooleanCommand(JNIEnv* env, jstring javaCommand)
{
    char reply[REPLY_BUF_SIZE];

    if (!doCommand(env, javaCommand, reply, sizeof(reply)))
        return JNI_FALSE;

    return (strcmp(reply, "OK") == 0);
}

// Send a command to the supplicant, and return the reply as a String.
static jstring doStringCommand(JNIEnv* env, jstring javaCommand)
{
    char reply[REPLY_BUF_SIZE];

    if (!doCommand(env, javaCommand, reply, sizeof(reply)))
        return NULL;

    return env->NewStringUTF(reply);
}

static jboolean com_qualcomm_qti_server_wigig_isDriverLoaded(JNIEnv* env, jobject)
{
    return (::is_wigig_driver_loaded() == 1);
}

static jboolean com_qualcomm_qti_server_wigig_loadDriver(JNIEnv* env, jobject)
{
    return (::wigig_load_driver() == 0);
}

static jboolean com_qualcomm_qti_server_wigig_unloadDriver(JNIEnv* env, jobject)
{
    return (::wigig_unload_driver() == 0);
}

static jboolean com_qualcomm_qti_server_wigig_startSupplicant(JNIEnv* env, jobject)
{
    return (::wigig_start_supplicant() == 0);
}

static jboolean com_qualcomm_qti_server_wigig_killSupplicant(JNIEnv* env, jobject)
{
    return (::wigig_stop_supplicant() == 0);
}

static jboolean com_qualcomm_qti_server_wigig_connectToSupplicant(JNIEnv* env, jobject)
{
    return (::wigig_connect_to_supplicant() == 0);
}

static void com_qualcomm_qti_server_wigig_closeSupplicantConnection(JNIEnv* env, jobject)
{
    ::wigig_close_supplicant_connection();
}

static jstring com_qualcomm_qti_server_wigig_waitForEvent(JNIEnv* env, jobject)
{
    char buf[EVENT_BUF_SIZE];

    int nread = ::wigig_wait_for_event(buf, sizeof buf);
    if (nread > 0)
        return env->NewStringUTF(buf);
    else
        return NULL;
}

static jboolean com_qualcomm_qti_server_wigig_doBooleanCommand(JNIEnv* env, jobject, jstring javaCommand)
{
    return doBooleanCommand(env, javaCommand);
}

static jint com_qualcomm_qti_server_wigig_doIntCommand(JNIEnv* env, jobject, jstring javaCommand)
{
    return doIntCommand(env, javaCommand);
}

static jstring com_qualcomm_qti_server_wigig_doStringCommand(JNIEnv* env, jobject, jstring javaCommand)
{
    return doStringCommand(env,javaCommand);
}

/**
 * Soft AP
 */

static jboolean com_qualcomm_qti_server_wigig_setSoftAp(
        JNIEnv* env, jobject, jstring jstr_ifname, jstring jstr_ssid, jboolean hidden, jint channel,
        jboolean secured, jstring jstr_key)
{
    const char *ifname;
    const char *ssid;
    const char *key;
    int res;

    if (jstr_ifname == NULL) {
        ALOGE("invalid ifname");
        return false;
    }
    if (jstr_ssid == NULL) {
        ALOGE("invalid ssid");
        return false;
    }
    if (secured && (jstr_key == NULL)) {
        ALOGE("invalid key");
        return false;
    }

    ScopedUtfChars ifnameChars(env, jstr_ifname);
    ifname = ifnameChars.c_str();
    ScopedUtfChars ssidChars(env, jstr_ssid);
    ssid = ssidChars.c_str();
    if (secured) {
        ScopedUtfChars keyChars(env, jstr_key);
        key = keyChars.c_str();
        res = ::wigig_set_softap(ifname, ssid, hidden, channel, secured, key);
    } else {
        res = ::wigig_set_softap(ifname, ssid, hidden, channel, secured, NULL);
    }

    return (res == 0);
}

// ----------------------------------------------------------------------------

/*
 * JNI registration.
 */
static JNINativeMethod gWigigMethods[] = {
    /* name, signature, funcPtr */
    { "loadDriver", "()Z",  (void *)com_qualcomm_qti_server_wigig_loadDriver },
    { "isDriverLoaded", "()Z",  (void *)com_qualcomm_qti_server_wigig_isDriverLoaded },
    { "unloadDriver", "()Z",  (void *)com_qualcomm_qti_server_wigig_unloadDriver },
    { "startSupplicant", "()Z",  (void *)com_qualcomm_qti_server_wigig_startSupplicant },
    { "killSupplicant", "()Z",  (void *)com_qualcomm_qti_server_wigig_killSupplicant },
    { "connectToSupplicantNative", "()Z", (void *)com_qualcomm_qti_server_wigig_connectToSupplicant },
    { "closeSupplicantConnectionNative", "()V",
        (void *)com_qualcomm_qti_server_wigig_closeSupplicantConnection },
    { "waitForEventNative", "()Ljava/lang/String;", (void*)com_qualcomm_qti_server_wigig_waitForEvent },
    { "doBooleanCommandNative", "(Ljava/lang/String;)Z", (void*)com_qualcomm_qti_server_wigig_doBooleanCommand },
    { "doIntCommandNative", "(Ljava/lang/String;)I", (void*)com_qualcomm_qti_server_wigig_doIntCommand },
    { "doStringCommandNative", "(Ljava/lang/String;)Ljava/lang/String;",
        (void*) com_qualcomm_qti_server_wigig_doStringCommand },
    { "setSoftApNative", "(Ljava/lang/String;Ljava/lang/String;ZIZLjava/lang/String;)Z",
        (void *)com_qualcomm_qti_server_wigig_setSoftAp},
};

/* User to register native functions */
extern "C"
jint Java_com_qualcomm_qti_server_wigig_WigigNative_registerNatives(JNIEnv* env, jclass clazz)
{
    return AndroidRuntime::registerNativeMethods(env,
            "com/qualcomm/qti/server/wigig/WigigNative", gWigigMethods, NELEM(gWigigMethods));
}

}; // namespace android
