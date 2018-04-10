/*
 * Copyright (c) 2015 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */

#define LOG_TAG "BnIrisDaemonCallback"
#include <stdint.h>
#include <sys/types.h>
#include <utils/Log.h>
#include <binder/Parcel.h>

#include "BnIrisDaemonCallback.h"

namespace android {

status_t BnIrisDaemonCallback::onTransact(uint32_t code, const Parcel& data, Parcel* reply,
               uint32_t flags)
{
	switch(code) {
		case ON_ENROLL_RESULT: {
			ALOGD("ON_ENROLL_RESULT");
			CHECK_INTERFACE(IIrisDaemonCallback, data, reply);
			const int64_t devId = data.readInt64();
			const int32_t irisId = data.readInt32();
			const int32_t groupId = data.readInt32();
			const int32_t progress = data.readInt32();
			const int32_t quality = data.readInt32();
			onEnrollResult(devId, irisId, groupId, progress, quality);
			return NO_ERROR;
		}

		case ON_AUTHENTICATED: {
			ALOGD("ON_AUTHENTICATED");
			CHECK_INTERFACE(IIrisDaemonCallback, data, reply);
			const int64_t devId = data.readInt64();
			const int32_t irisId = data.readInt32();
			const int32_t groupId = data.readInt32();
			onAuthenticated(devId, irisId, groupId);
			return NO_ERROR;
		}

		case ON_REMOVED: {
			ALOGD("ON_AUTHENTICATED");
			CHECK_INTERFACE(IIrisDaemonCallback, data, reply);
			const int64_t devId = data.readInt64();
			const int32_t irisId = data.readInt32();
			const int32_t groupId = data.readInt32();
			onRemoved(devId, irisId, groupId);
			return NO_ERROR;
		}

		case ON_ERROR: {
			CHECK_INTERFACE(IIrisDaemonCallback, data, reply);
			const int64_t devId = data.readInt64();
			const int32_t err = data.readInt32();
			onError(devId, err);
			return NO_ERROR;
		}

		case ON_AUTHENTICATING: {
			ALOGD("ON_AUTHENTICATING");
			CHECK_INTERFACE(IIrisDaemonCallback, data, reply);
			const int64_t devId = data.readInt64();
			const int32_t quality = data.readInt32();
			onAuthenticating(devId, quality);
			return NO_ERROR;
		}

		default:
			return -EINVAL;
	}
}

}; // namespace android

