/*
 * Copyright (c) 2015 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */

#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <utils/String16.h>
#include <hardware/hw_auth_token.h>

#include "iris_dev.h"

#define ENROLL_TIMEOUT_SEC	5

static uint32_t enrollee_id = 121212;
static uint32_t enroll_done = 0;
static int ret_status = 0;

void msg_handler(const iris_msg_t *msg, void *data)
{
	printf("message type %d\n", msg->type);
	switch (msg->type) {
		case IRIS_ERROR:
			printf("Error %d\n", msg->data.error);
			ret_status = msg->data.error;
			break;
		case IRIS_ENROLLING:
			printf("progress %d\n", msg->data.enroll.progress);
			if (msg->data.enroll.progress == 100)
				enroll_done = 1;
			break;
		default:
			break;
	}
}

int main(int argc, char** argv)
{
	int ret;
	iris_device *device = new iris_device;
	hw_auth_token_t hat;
	uint64_t challenge;
	String16 package("test");

	ret = device->open(false);
	if (ret < 0) {
		printf("open device failed, %d\n", ret);
		return ret;
	}

	device->set_notify(msg_handler, NULL);

	challenge = device->pre_enroll();
	hat.challenge = challenge;

	ret = device->enroll(&hat, enrollee_id, ENROLL_TIMEOUT_SEC, 0, package);
	if (ret) {
		printf("enroll failed, %d\n", ret);
		return ret;
	}

	while (!enroll_done && !ret_status)
		sleep(1);

	device->close();
	delete device;

	return ret_status;
}

