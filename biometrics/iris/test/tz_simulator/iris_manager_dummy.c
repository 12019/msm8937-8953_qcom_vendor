/*
 * Copyright (c) 2015 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */

#include <stdio.h>
#define LOG_TAG "Iris_Manager_Dummpy"
#include <utils/Log.h>

#include "iris_manager.h"

static int progress;
static uint64_t challenge_magic_id = 0x10102020;
static uint64_t verify_operationId = 0;
static uint32_t irisId = 0x1020;

IRIS_API iris_status iris_manager_init(void)
{
	progress = 0;
	return IRIS_STATUS_SUCCESS;
}

IRIS_API iris_status iris_manager_deinit(void)
{
	return IRIS_STATUS_SUCCESS;
}

IRIS_API iris_status iris_manager_pre_enroll(uint64_t *challenge)
{
	*challenge = challenge_magic_id;
	progress = 0;
	return IRIS_STATUS_SUCCESS;
}

IRIS_API iris_status iris_manager_post_enroll(void)
{
	progress = 0;
	return IRIS_STATUS_SUCCESS;
}


IRIS_API iris_status iris_manager_get_version(struct iris_version *version)
{
	printf("get version\n");
	version->version = 281;
	return IRIS_STATUS_SUCCESS;
}

IRIS_API iris_status iris_manager_enroll_begin(uint32_t user_id)
{
	printf("enroll begin\n");
	progress = 0;
	return IRIS_STATUS_SUCCESS;
}

IRIS_API iris_status iris_manager_enroll_capture(struct iris_enroll_status *enroll_status)
{
	progress += 1;
	usleep(100000);
	enroll_status->progress = progress;
	if (progress == 30)
		enroll_status->status = IRIS_STATUS_BAD_EYE_DISTANCE;
	else
		enroll_status->status = IRIS_STATUS_BAD_EYE_OPENNESS;
	ALOGD("progres=%d\n", progress);
	return (progress == 100) ? IRIS_STATUS_SUCCESS : IRIS_STATUS_NO_FRAME;
}

IRIS_API iris_status iris_manager_enroll_commit(struct iris_enroll_result *enroll_result)
{
	enroll_result->irisId = irisId++;
	return IRIS_STATUS_SUCCESS;
}

IRIS_API iris_status iris_manager_enroll_cancel(void)
{
	progress = 0;
	return IRIS_STATUS_SUCCESS;
}

IRIS_API iris_status iris_manager_identify_begin(void)
{
	progress = 0;

	return IRIS_STATUS_SUCCESS;
}

IRIS_API iris_status iris_manager_identify_capture(struct iris_identify_result *identify_result)
{
	return IRIS_STATUS_SUCCESS;
}

IRIS_API iris_status iris_manager_identify_end()
{
	return IRIS_STATUS_SUCCESS;
}

IRIS_API iris_status iris_manager_verify_begin(uint64_t operationId, uint32_t user_id)
{
	progress = 0;
	verify_operationId = operationId;
	return IRIS_STATUS_SUCCESS;
}

IRIS_API iris_status iris_manager_verify_capture(struct iris_verify_result *verify_result)
{
	progress += 10;
	usleep(300000);
	if (progress == 20)
		verify_result->status = 0;
	if (progress == 100)
		verify_result->irisId = irisId - 1;
	else
		verify_result->irisId = 0;
	ALOGD("verify progres=%d\n", progress);
	return (progress == 100) ? IRIS_STATUS_SUCCESS : IRIS_STATUS_NO_FRAME;

}

IRIS_API iris_status iris_manager_verify_end()
{
	return IRIS_STATUS_SUCCESS;
}

IRIS_API iris_status iris_manager_retrieve_enrollee(uint32_t user_id, struct iris_enrollee_info *enrollee_info)
{
	return IRIS_STATUS_SUCCESS;
}

IRIS_API iris_status iris_manager_delete_enrollee(uint32_t irisId, uint32_t user_id)
{
	ALOGD("enrollee irisId = %u, id %u\n", (unsigned int)irisId, user_id);
	return IRIS_STATUS_SUCCESS;
}

IRIS_API iris_status iris_manager_delete_all_enrollee(void)
{
	return IRIS_STATUS_SUCCESS;
}

IRIS_API iris_status iris_manager_get_authenticator_id(uint64_t *id)
{
	*id = 0x101010;
	return IRIS_STATUS_SUCCESS;
}

IRIS_API iris_status iris_manager_verify_token(struct hw_auth_token *token)
{
	ALOGD("token challenge =%llx\n", (long long unsigned int)token->challenge);

	if (token->challenge == challenge_magic_id) {
		ALOGD("token verified\n");
		return IRIS_STATUS_SUCCESS;
	} 

	return IRIS_STATUS_FAIL;
}

IRIS_API iris_status iris_manager_get_auth_token(struct hw_auth_token *token)
{
	token->version = 1;
	token->challenge == verify_operationId;
	printf("token challenge =%llx\n", (long long unsigned int)token->challenge);
	token->authenticator_type = 1 << 3;
	token->authenticator_id = 2020;
	return IRIS_STATUS_SUCCESS;
}

IRIS_API iris_status iris_manager_set_meta_data(struct iris_meta_data *meta)
{
	return IRIS_STATUS_SUCCESS;
}

IRIS_API iris_status iris_manager_set_token_key(uint8_t *token_key, uint32_t key_len)
{
	ALOGD("token key length=%d", (int)key_len);
	return IRIS_STATUS_SUCCESS;
}

IRIS_API iris_status iris_manager_enumerate_enrollment(int32_t *count, struct iris_identifier *ids)
{
	int i;
	*count = 3;

	ALOGD("enumeration enrollment");
	for (i = 0; i < *count; i++) {
		ids[i].irisId = i+1;
		ids[i].userId = 0;
	}
	return IRIS_STATUS_SUCCESS;

}


