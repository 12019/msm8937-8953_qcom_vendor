/*
 * Copyright (c) 2015 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */

#ifndef BPIRIS_DAEMON_H_
#define BPIRIS_DAEMON_H_

#include "IIrisDaemon.h"

namespace android {

class BpIrisDaemon : public BpInterface<IIrisDaemon> {
public:
	BpIrisDaemon(const sp<IBinder>& impl);

	// Binder interface methods
	virtual void init(const sp<IIrisDaemonCallback>& callback);
	virtual int32_t enroll(const uint8_t* token, ssize_t tokenLength, int32_t groupId,
	int32_t timeout, int32_t userId);
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

	virtual void binderDied(const wp<IBinder>& who);
};


} // namespace android

#endif // BPIRIS_DAEMON_H_


