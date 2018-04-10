/*
 * Copyright (c) 2015 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */

#define LOG_TAG "irisDaemon"

#include <cutils/log.h>
#include <utils/Log.h>

#include <binder/IPCThreadState.h>
#include <binder/IServiceManager.h>
#include <binder/PermissionCache.h>
#include <utils/String16.h>

#include "IrisDaemonProxy.h"

int main() {
	ALOGI("Starting " LOG_TAG);
	android::sp<android::IServiceManager> serviceManager = android::defaultServiceManager();
	android::sp<android::IrisDaemonProxy> proxy =
		android::IrisDaemonProxy::getInstance();
	android::status_t ret = serviceManager->addService(
		android::IrisDaemonProxy::descriptor, proxy);
	if (ret != android::OK) {
		ALOGE("Couldn't register " LOG_TAG " binder service!");
		return -1;
	}

	ALOGI("iris daemon is now ready");
	android::IPCThreadState::self()->joinThreadPool();
	ALOGI("iris daemon Done");
	return 0;
}

