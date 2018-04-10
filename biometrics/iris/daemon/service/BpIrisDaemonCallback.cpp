/*
 * Copyright (c) 2015 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */

#define LOG_TAG "BpIrisDaemonCallback"
#include <stdint.h>
#include <sys/types.h>
#include <utils/Log.h>
#include <binder/Parcel.h>

#include "BpIrisDaemonCallback.h"

namespace android {

BpIrisDaemonCallback::BpIrisDaemonCallback(const sp<IBinder>& impl) :
		BpInterface<IIrisDaemonCallback>(impl) {
}

status_t BpIrisDaemonCallback::onEnrollResult(int64_t devId, int32_t irisId, int32_t groupId, uint32_t progress, uint32_t quality) {
	Parcel data, reply;
	data.writeInterfaceToken(IIrisDaemonCallback::getInterfaceDescriptor());
	data.writeInt64(devId);
	data.writeInt32(irisId);
	data.writeInt32(groupId);
	data.writeUint32(progress);
	data.writeUint32(quality);
	return remote()->transact(ON_ENROLL_RESULT, data, &reply, IBinder::FLAG_ONEWAY);

}

status_t BpIrisDaemonCallback::onAuthenticated(int64_t devId, int32_t irisId, int32_t gpId) {
	Parcel data, reply;
	data.writeInterfaceToken(IIrisDaemonCallback::getInterfaceDescriptor());
	data.writeInt64(devId);
	data.writeInt32(irisId);
	data.writeInt32(gpId);
	return remote()->transact(ON_AUTHENTICATED, data, &reply, IBinder::FLAG_ONEWAY);
}

status_t BpIrisDaemonCallback::onAuthenticating(int64_t devId,  uint32_t quality) {
	Parcel data, reply;
	data.writeInterfaceToken(IIrisDaemonCallback::getInterfaceDescriptor());
	data.writeInt64(devId);
	data.writeUint32(quality);
	return remote()->transact(ON_AUTHENTICATING, data, &reply, IBinder::FLAG_ONEWAY);
}

status_t BpIrisDaemonCallback::onError(int64_t devId, int32_t error) {
	Parcel data, reply;
	data.writeInterfaceToken(IIrisDaemonCallback::getInterfaceDescriptor());
	data.writeInt64(devId);
	data.writeInt32(error);
	return remote()->transact(ON_ERROR, data, &reply, IBinder::FLAG_ONEWAY);
}

status_t BpIrisDaemonCallback::onRemoved(int64_t devId, int32_t irisId, int32_t gpId) {
	Parcel data, reply;
	data.writeInterfaceToken(IIrisDaemonCallback::getInterfaceDescriptor());
	data.writeInt64(devId);
	data.writeInt32(irisId);
	data.writeInt32(gpId);
	return remote()->transact(ON_REMOVED, data, &reply, IBinder::FLAG_ONEWAY);
}

status_t BpIrisDaemonCallback::onEnumerate(int64_t devId, const int32_t* irisIds, const int32_t* gpIds, int32_t sz) {
	Parcel data, reply;
	data.writeInterfaceToken(IIrisDaemonCallback::getInterfaceDescriptor());
	data.writeInt64(devId);
	data.writeInt32Array(sz, irisIds);
	data.writeInt32Array(sz, gpIds);
	return remote()->transact(ON_ENUMERATE, data, &reply, IBinder::FLAG_ONEWAY);
};

IMPLEMENT_META_INTERFACE(IrisDaemonCallback,
		"android.hardware.iris.IIrisDaemonCallback");

}; // namespace android

