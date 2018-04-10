/*
 * Copyright (c) 2015 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */

#ifndef BPIRIS_DAEMON_CALLBACK_H_
#define BPIRIS_DAEMON_CALLBACK_H_

#include "IIrisDaemonCallback.h"

namespace android {

class BpIrisDaemonCallback : public BpInterface<IIrisDaemonCallback>
{
public:
	BpIrisDaemonCallback(const sp<IBinder>& impl);
	virtual status_t onEnrollResult(int64_t devId,  int32_t irisId, int32_t groupId, uint32_t progress, uint32_t quality);
	virtual status_t onAuthenticated(int64_t devId, int32_t irisId, int32_t gpId);
	virtual status_t onError(int64_t devId, int32_t error);
	virtual status_t onRemoved(int64_t devId, int32_t irisId, int32_t gpId);
	virtual status_t onEnumerate(int64_t devId, const int32_t* irisIds, const int32_t* gpIds, int32_t sz);
	virtual status_t onAuthenticating(int64_t devId,  uint32_t quality);
};

}; // namespace android

#endif // BPIRIS_DAEMON_CALLBACK_H_

