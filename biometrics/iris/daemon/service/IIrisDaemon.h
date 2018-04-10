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


#ifndef IIRIS_DAEMON_H_
#define IIRIS_DAEMON_H_

#include <binder/IInterface.h>
#include <binder/Parcel.h>


namespace android {

class IIrisDaemonCallback;

struct iris_id {
	uint32_t irisId;
	uint32_t gid;
};

/*
* Abstract base class for native implementation of IrisService.
*
* Note: This must be kept manually in sync with IIrisDaemon.aidl
*/
class IIrisDaemon : public IInterface, public IBinder::DeathRecipient {
public:
	DECLARE_META_INTERFACE(IrisDaemon);
	enum {
		AUTHENTICATE = IBinder::FIRST_CALL_TRANSACTION + 0,
		CANCEL_AUTHENTICATION = IBinder::FIRST_CALL_TRANSACTION + 1,
		ENROLL = IBinder::FIRST_CALL_TRANSACTION + 2,
		CANCEL_ENROLLMENT = IBinder::FIRST_CALL_TRANSACTION + 3,
		PRE_ENROLL = IBinder::FIRST_CALL_TRANSACTION + 4,
		REMOVE = IBinder::FIRST_CALL_TRANSACTION + 5,
		GET_AUTHENTICATOR_ID = IBinder::FIRST_CALL_TRANSACTION + 6,
		SET_ACTIVE_GROUP = IBinder::FIRST_CALL_TRANSACTION + 7,
		OPEN_HAL = IBinder::FIRST_CALL_TRANSACTION + 8,
		CLOSE_HAL = IBinder::FIRST_CALL_TRANSACTION + 9,
		INIT = IBinder::FIRST_CALL_TRANSACTION + 10,
		POST_ENROLL = IBinder::FIRST_CALL_TRANSACTION + 11,
		ENUMERATE_ENROLLMENT = IBinder::FIRST_CALL_TRANSACTION + 12,
	};

	// Binder interface methods
	virtual void init(const sp<IIrisDaemonCallback>& callback) = 0;
	virtual int32_t enroll(const uint8_t* token, ssize_t tokenLength, int32_t groupId,
	int32_t timeout, int32_t uid) = 0;
	virtual uint64_t preEnroll() = 0;
	virtual int32_t postEnroll() = 0;
	virtual int32_t stopEnrollment() = 0;
	virtual int32_t authenticate(uint64_t sessionId, int32_t groupId, int32_t uid) = 0;
	virtual int32_t stopAuthentication() = 0;
	virtual int32_t remove(int32_t irisId, int32_t groupId) = 0;
	virtual uint64_t getAuthenticatorId() = 0;
	virtual int32_t setActiveGroup(int32_t groupId, const uint8_t* path, ssize_t pathLen) = 0;
	virtual int64_t openHal() = 0;
	virtual int32_t closeHal() = 0;
	virtual int32_t enumerateEnrollment(uint32_t *maxSize, iris_id *ids) = 0;
};

} // namespace android

#endif // IRIS_DAEMON_H_


