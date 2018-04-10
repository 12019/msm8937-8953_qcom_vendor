/*
 * Copyright (c) 2015 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */

#include <stdio.h>
#include <errno.h>
#include <unistd.h>

#include <binder/ProcessState.h>
#include <binder/IPCThreadState.h>
#include <binder/IServiceManager.h>
#include <hardware/hw_auth_token.h>

#include "IIrisDaemon.h"
#include "BnIrisDaemonCallback.h"

#define ENROLL_TIMEOUT_SEC	5

using namespace android;

static int32_t enrollee_id = 121212;
static volatile uint32_t request_done = 0;
static volatile int ret_status = 0;
static const uint64_t sessionId = 0x222333;

class IrisDaemonTestCallback : public BnIrisDaemonCallback {
public:
	virtual status_t onEnrollResult(int64_t devId, int32_t irisId, int32_t groupId, uint32_t progress, uint32_t quality);
	virtual status_t onAuthenticated(int64_t devId, int32_t irisId, int32_t groupId);
	virtual status_t onError(int64_t devId, int32_t error);
	virtual status_t onRemoved(int64_t devId, int32_t irisId, int32_t groupId);
	virtual status_t onEnumerate(int64_t devId, const int32_t* irisIds, const int32_t* gpIds,
			int32_t sz) { return -EINVAL; };
	virtual status_t onAuthenticating(int64_t devId,  uint32_t quality);
};

status_t IrisDaemonTestCallback::onEnrollResult(int64_t devId, int32_t irisId, int32_t groupId, uint32_t progress, uint32_t quality)
{
	printf("onEnrollResult 0x%llx, irisId=%d groupId=%d, progress=%d\n", devId, irisId, groupId, progress);
	if (progress == 100)
		request_done = 1;
	return 0;
}

status_t IrisDaemonTestCallback::onError(int64_t devId, int32_t error)
{
	printf("onEror 0x%llx, %d\n", devId, error);
	ret_status = error;
	return 0;
}

status_t IrisDaemonTestCallback::onAuthenticating(int64_t devId,  uint32_t quality)
{
	return 0;
}

status_t IrisDaemonTestCallback::onAuthenticated(int64_t devId, int32_t irisId, int32_t groupId)
{
	printf("onAuthenticated 0x%llx, irisId=%d groupId=%d\n", devId, irisId, groupId);
	request_done = 1;
	return 0;
}

status_t IrisDaemonTestCallback::onRemoved(int64_t devId, int32_t irisId, int32_t groupId)
{
	printf("onRemoved 0x%llx, irisId=%d, groupId=%d\n", devId, irisId, groupId);
	request_done = 1;
	return 0;
}

int test_enroll(sp<IIrisDaemon> &irisDaemonClient)
{
	int ret, count = 0;
	uint64_t challenge;
	hw_auth_token_t token;

	request_done = 0;
	ret_status = 0;

	challenge = irisDaemonClient->preEnroll();
	
	token.challenge = challenge;
	
	ret = irisDaemonClient->enroll((uint8_t *)(&token), sizeof(hw_auth_token_t), enrollee_id, ENROLL_TIMEOUT_SEC, 0);
	if (ret) {
		printf("Client enroll failed, %d\n", ret);
		return ret;
	}
	
	printf("wait for done...\n");
	while (!request_done && !ret_status) {
		printf("ret_status=%d\n", ret_status);
		sleep(1);
		count++;
		if (count > 3) {
			printf("cancel the pending enrollment\n");
			irisDaemonClient->stopEnrollment();
			ret = -ETIME;
			break;
		}
	}

	if (request_done) {
		printf("post enroll\n");
		ret = irisDaemonClient->postEnroll();
	}
	return ret;
}

int test_authenticate(sp<IIrisDaemon> &irisDaemonClient)
{
	int ret, count = 0;

	request_done = 0;
	ret_status = 0;

	ret = irisDaemonClient->authenticate(sessionId, enrollee_id,  0);
	if (ret) {
		printf("Client authenticate failed, %d\n", ret);
		return ret;
	}
	
	printf("wait for authenticate done...\n");
	while (!request_done && !ret_status) {
		printf("ret_status=%d\n", ret_status);
		sleep(1);
		count++;
		if (count > 3) {
			printf("cancel the pending authentication\n");
			irisDaemonClient->stopAuthentication();
			ret = -ETIME;
			break;
		}
	}

	return ret;
}

int test_remove(sp<IIrisDaemon> &irisDaemonClient)
{
	int ret, count = 0;

	request_done = 0;
	ret_status = 0;

	ret = irisDaemonClient->remove(0, enrollee_id);
	if (ret) {
		printf("Client remove failed, %d\n", ret);
		return ret;
	}
	
	printf("wait for remove done...\n");
	while (!request_done && !ret_status) {
		printf("ret_status=%d\n", ret_status);
		sleep(1);
		count++;
		if (count > 3) {
			printf("cancel the pending authentication\n");
			ret = -ETIME;
			break;
		}
	}

	return ret;
}

int main(int argc, char** argv)
{
	int ret;
	int64_t devId;
	sp<IrisDaemonTestCallback> callback = new IrisDaemonTestCallback;

	sp<IServiceManager> sm = defaultServiceManager();
	if (sm == NULL) {
		printf("Get sm failed\n");
		return -1;
	}

	sp<IBinder> binder = sm->getService(IIrisDaemon::descriptor);
	if (binder == NULL) {
		printf("Get service failed\n");
		return -1;
	}

	sp<IIrisDaemon> irisDaemonClient = interface_cast<IIrisDaemon>(binder);
	if (irisDaemonClient == NULL) {
		printf("Create client failed\n");
		return -1;
	}

	android::ProcessState::self()->startThreadPool();

	irisDaemonClient->init(callback);

	devId = irisDaemonClient->openHal();
	if (devId == 0) {
		printf("Client openHal failed\n");
		return -1;
	}

	ret = test_enroll(irisDaemonClient);
	if (ret)
		printf("fail to enroll ret=%d\n", ret);

	ret = test_authenticate(irisDaemonClient);
	if (ret)
		printf("fail to authenticate\n");

	ret = test_remove(irisDaemonClient);
	if (ret)
		printf("fail to remove\n");

	irisDaemonClient->closeHal();

	return ret_status;
}


