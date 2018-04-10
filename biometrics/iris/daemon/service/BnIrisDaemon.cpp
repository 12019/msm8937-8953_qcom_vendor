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


#define LOG_TAG "IrisDaemon"
#include <inttypes.h>

#include "BnIrisDaemon.h"
#include "IIrisDaemonCallback.h"

namespace android {

status_t BnIrisDaemon::onTransact(uint32_t code, const Parcel& data, Parcel* reply,
		uint32_t flags) {
	switch(code) {
		case AUTHENTICATE: {
			CHECK_INTERFACE(IIrisDaemon, data, reply);
			const uint64_t sessionId = data.readInt64();
			const int32_t groupId = data.readInt32();
			const int32_t userId = data.readInt32();
			const int32_t ret = authenticate(sessionId, groupId, userId);
			reply->writeNoException();
			reply->writeInt32(ret);
			return NO_ERROR;
		};
		case CANCEL_AUTHENTICATION: {
			CHECK_INTERFACE(IIrisDaemon, data, reply);
			const int32_t ret = stopAuthentication();
			reply->writeNoException();
			reply->writeInt32(ret);
			return NO_ERROR;
		}
		case ENROLL: {
			CHECK_INTERFACE(IIrisDaemon, data, reply);
			const ssize_t tokenSize = data.readInt32();
			const uint8_t* token = static_cast<const uint8_t *>(data.readInplace(tokenSize));
			const int32_t groupId = data.readInt32();
			const int32_t timeout = data.readInt32();
			const int32_t userId = data.readInt32();
			const int32_t ret = enroll(token, tokenSize, groupId, timeout, userId);
			reply->writeNoException();
			reply->writeInt32(ret);
			return NO_ERROR;
		}
		case CANCEL_ENROLLMENT: {
			CHECK_INTERFACE(IIrisDaemon, data, reply);
			const int32_t ret = stopEnrollment();
			reply->writeNoException();
			reply->writeInt32(ret);
			return NO_ERROR;
		}
		case PRE_ENROLL: {
			CHECK_INTERFACE(IIrisDaemon, data, reply);
			const uint64_t ret = preEnroll();
			reply->writeNoException();
			reply->writeInt64(ret);
			return NO_ERROR;
		}
		case POST_ENROLL: {
			CHECK_INTERFACE(IIrisDaemon, data, reply);
			const int32_t ret = postEnroll();
			reply->writeNoException();
			reply->writeInt32(ret);
			return NO_ERROR;
		}
		case REMOVE: {
			CHECK_INTERFACE(IIrisDaemon, data, reply);
			const int32_t irisId = data.readInt32();
			const int32_t groupId = data.readInt32();
			const int32_t ret = remove(irisId, groupId);
			reply->writeNoException();
			reply->writeInt32(ret);
			return NO_ERROR;
		}
		case GET_AUTHENTICATOR_ID: {
			CHECK_INTERFACE(IIrisDaemon, data, reply);
			const uint64_t ret = getAuthenticatorId();
			reply->writeNoException();
			reply->writeInt64(ret);
			return NO_ERROR;
		}
		case SET_ACTIVE_GROUP: {
			CHECK_INTERFACE(IIrisDaemon, data, reply);
			const int32_t group = data.readInt32();
			const ssize_t pathSize = data.readInt32();
			const uint8_t* path = static_cast<const uint8_t *>(data.readInplace(pathSize));
			const int32_t ret = setActiveGroup(group, path, pathSize);
			reply->writeNoException();
			reply->writeInt32(ret);
			return NO_ERROR;
		}
		case OPEN_HAL: {
			CHECK_INTERFACE(IIrisDaemon, data, reply);
			const int64_t ret = openHal();
			reply->writeNoException();
			reply->writeInt64(ret);
			return NO_ERROR;
		}
		case CLOSE_HAL: {
			CHECK_INTERFACE(IIrisDaemon, data, reply);
			const int32_t ret = closeHal();
			reply->writeNoException();
			reply->writeInt32(ret);
			return NO_ERROR;
		}
		case INIT: {
			CHECK_INTERFACE(IIrisDaemon, data, reply);
			sp<IIrisDaemonCallback> callback =
					interface_cast<IIrisDaemonCallback>(data.readStrongBinder());
			init(callback);
			reply->writeNoException();
			return NO_ERROR;
		}
		case ENUMERATE_ENROLLMENT: {
			CHECK_INTERFACE(IIrisDaemon, data, reply);
			struct iris_id ids[5];
			uint32_t i, max_size = 5;
			const int32_t ret = enumerateEnrollment(&max_size, ids);
			reply->writeNoException();
			if (ret < 0) {
				reply->writeInt32(0);
				return NO_ERROR;
			}
			reply->writeInt32(max_size);
			for (i = 0; i < max_size; i++) {
				reply->writeInt32(1);
				reply->writeInt32(ids[i].gid);
				reply->writeInt32(ids[i].irisId);
			}

			return NO_ERROR;
		}
		default:
			return BBinder::onTransact(code, data, reply, flags);
	}
};

}; // namespace android

