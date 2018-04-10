/*
 * Copyright (c) 2015-2016 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 *
 * Not a Contribution.
 * Apache license notifications and license are retained
 * for attribution purposes only.
 */

/**
 * Copyright (C) 2015 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *		http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */


#ifndef IRIS_DAEMON_PROXY_H_
#define IRIS_DAEMON_PROXY_H_

#include "BnIrisDaemon.h"
#include "IIrisDaemonCallback.h"
#include "iris_dev.h"

namespace android {

class IrisDaemonProxy : public BnIrisDaemon {
public:
	static IrisDaemonProxy* getInstance();

	virtual void init(const sp<IIrisDaemonCallback>& callback);
	virtual int32_t enroll(const uint8_t* token, ssize_t tokenLength, int32_t groupId, int32_t timeout, int32_t userId);
	virtual uint64_t preEnroll();
	virtual int32_t postEnroll();
	virtual int32_t stopEnrollment();
	virtual int32_t authenticate(uint64_t sessionId, int32_t groupId, int32_t userId);
	virtual int32_t stopAuthentication();
	virtual int32_t remove(int32_t irisId, int32_t groupId);
	virtual uint64_t getAuthenticatorId();
	virtual int32_t setActiveGroup(int32_t groupId, const uint8_t* path, ssize_t pathLen);
	virtual int64_t openHal();
	virtual int32_t closeHal();
	virtual int32_t enumerateEnrollment(uint32_t *maxSize, iris_id *ids);

private:
	IrisDaemonProxy();
	virtual ~IrisDaemonProxy();

	virtual void binderDied(const wp<IBinder>& who);

	void notifyKeystore(const uint8_t *auth_token, const size_t auth_token_length);
	String16 getPackageName(int32_t uid);
	static void hal_notify_callback(const iris_msg_t *msg, void *data);

private:
	static IrisDaemonProxy *sInstance;
	iris_device* mDevice;
	sp<IIrisDaemonCallback> mCallback;
};

} // namespace android

#endif // IRIS_DAEMON_PROXY_H_

