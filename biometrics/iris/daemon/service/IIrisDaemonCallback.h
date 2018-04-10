/*
 * Copyright (c) 2015 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */

#ifndef IIRIS_DAEMON_CALLBACK_H_
#define IIRIS_DAEMON_CALLBACK_H_

#include <inttypes.h>
#include <utils/Errors.h>
#include <binder/IInterface.h>
#include <binder/Parcel.h>

namespace android {

/*
* Communication channel back to IrisService.java
*/
class IIrisDaemonCallback : public IInterface {
public:
	// must be kept in sync with IIrisService.aidl
	enum {
		ON_ENROLL_RESULT = IBinder::FIRST_CALL_TRANSACTION + 0,
		ON_AUTHENTICATED = IBinder::FIRST_CALL_TRANSACTION + 1,
		ON_ERROR = IBinder::FIRST_CALL_TRANSACTION + 2,
		ON_REMOVED = IBinder::FIRST_CALL_TRANSACTION + 3,
		ON_ENUMERATE = IBinder::FIRST_CALL_TRANSACTION + 4,
		ON_AUTHENTICATING = IBinder::FIRST_CALL_TRANSACTION + 5,
	};

	virtual status_t onEnrollResult(int64_t devId, int32_t irisId, int32_t groupId, uint32_t progress, uint32_t quality) = 0;
	virtual status_t onAuthenticated(int64_t devId, int32_t irisId, int32_t groupId) = 0;
	virtual status_t onError(int64_t devId, int32_t error) = 0;
	virtual status_t onRemoved(int64_t devId, int32_t irisId, int32_t groupId) = 0;
	virtual status_t onEnumerate(int64_t devId, const int32_t* irisIds, const int32_t* gpIds, int32_t sz) = 0;
	virtual status_t onAuthenticating(int64_t devId,  uint32_t quality) = 0;

	DECLARE_META_INTERFACE(IrisDaemonCallback);
};

}; // namespace android

#endif // IIRIS_DAEMON_CALLBACK_H_

