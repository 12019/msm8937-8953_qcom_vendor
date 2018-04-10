/*
 * Copyright (c) 2015 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */

#define LOG_TAG "BpIrisDaemon"
#include <inttypes.h>

#include "BpIrisDaemon.h"
#include "IIrisDaemonCallback.h"

namespace android {

BpIrisDaemon::BpIrisDaemon(const sp<IBinder>& impl) : BpInterface<IIrisDaemon>(impl) {
}

void BpIrisDaemon::init(const sp<IIrisDaemonCallback>& callback) {
	Parcel data, reply;
	data.writeInterfaceToken(IIrisDaemon::getInterfaceDescriptor());
	data.writeStrongBinder(callback->asBinder(callback));
	remote()->transact(INIT, data, &reply);
	reply.readExceptionCode();
}

int64_t BpIrisDaemon::openHal() {
	Parcel data, reply;
	data.writeInterfaceToken(IIrisDaemon::getInterfaceDescriptor());
	remote()->transact(OPEN_HAL, data, &reply);
	reply.readExceptionCode();
	return reply.readInt64();
}

int32_t BpIrisDaemon::closeHal() {
	Parcel data, reply;
	data.writeInterfaceToken(IIrisDaemon::getInterfaceDescriptor());
	remote()->transact(CLOSE_HAL, data, &reply);
	reply.readExceptionCode();
	return reply.readInt32();
}

int32_t BpIrisDaemon::enroll(const uint8_t* token, ssize_t tokenLength, int32_t groupId,
	  int32_t timeout, int32_t userId) {
	Parcel data, reply;
	data.writeInterfaceToken(IIrisDaemon::getInterfaceDescriptor());
	data.writeInt32(tokenLength);
	data.write(token, tokenLength);
	data.writeInt32(groupId);
	data.writeInt32(timeout);
	data.writeInt32(userId);
	remote()->transact(ENROLL, data, &reply);
	reply.readExceptionCode();
	return reply.readInt32();
}

uint64_t BpIrisDaemon::preEnroll()
{
	Parcel data, reply;
	data.writeInterfaceToken(IIrisDaemon::getInterfaceDescriptor());
	remote()->transact(PRE_ENROLL, data, &reply);
	reply.readExceptionCode();
	return reply.readUint64();
}

int32_t BpIrisDaemon::postEnroll()
{
	Parcel data, reply;
	data.writeInterfaceToken(IIrisDaemon::getInterfaceDescriptor());
	remote()->transact(POST_ENROLL, data, &reply);
	reply.readExceptionCode();
	return reply.readInt32();
}

int32_t BpIrisDaemon::stopEnrollment()
{
	Parcel data, reply;
	data.writeInterfaceToken(IIrisDaemon::getInterfaceDescriptor());
	remote()->transact(CANCEL_ENROLLMENT, data, &reply);
	reply.readExceptionCode();
	return reply.readInt32();
}

int32_t BpIrisDaemon::authenticate(uint64_t sessionId, int32_t groupId, int32_t userId)
{
	Parcel data, reply;
	data.writeInterfaceToken(IIrisDaemon::getInterfaceDescriptor());
	data.writeUint64(sessionId);
	data.writeUint32(groupId);
	data.writeInt32(userId);
	remote()->transact(AUTHENTICATE, data, &reply);
	reply.readExceptionCode();
	return reply.readInt32();
}

int32_t BpIrisDaemon::stopAuthentication()
{
	Parcel data, reply;
	data.writeInterfaceToken(IIrisDaemon::getInterfaceDescriptor());
	remote()->transact(CANCEL_AUTHENTICATION, data, &reply);
	reply.readExceptionCode();
	return reply.readInt32();
}

int32_t BpIrisDaemon::remove(int32_t irisId, int32_t groupId)
{
	Parcel data, reply;
	data.writeInterfaceToken(IIrisDaemon::getInterfaceDescriptor());
	data.writeInt32(irisId);
	data.writeInt32(groupId);
	remote()->transact(REMOVE, data, &reply);
	reply.readExceptionCode();
	return reply.readInt32();
}

uint64_t BpIrisDaemon::getAuthenticatorId()
{
	Parcel data, reply;
	data.writeInterfaceToken(IIrisDaemon::getInterfaceDescriptor());
	remote()->transact(GET_AUTHENTICATOR_ID, data, &reply);
	reply.readExceptionCode();
	return reply.readUint64();
}

int32_t BpIrisDaemon::enumerateEnrollment(uint32_t *maxSize, iris_id *ids)
{
	Parcel data, reply;
	data.writeInterfaceToken(IIrisDaemon::getInterfaceDescriptor());
	remote()->transact(ENUMERATE_ENROLLMENT, data, &reply);
	reply.readExceptionCode();
	return reply.readInt32();
}

int32_t BpIrisDaemon::setActiveGroup(int32_t groupId, const uint8_t* path, ssize_t pathLen)
{
	return -EINVAL;
}

void  BpIrisDaemon::binderDied(const wp<IBinder>& who)
{
}

IMPLEMENT_META_INTERFACE(IrisDaemon, "android.hardware.iris.IIrisDaemon");

}; // namespace android

