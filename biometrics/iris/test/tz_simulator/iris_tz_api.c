/*
 * Copyright (c) 2015 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */

#include "iris_tz_api.h"
#include "iris_manager.h"
#include "iris_parser.h"
#include "iris_common.h"

void exec_get_version(struct iris_request *req, struct iris_response *response)
{
	int ret;
	struct iris_version version;
	ret = iris_manager_get_version(&version);
	iris_response_put_int32(response, ret);
	iris_response_put_uint32(response, version.version);
}

void exec_pre_enroll(struct iris_request *req, struct iris_response *response)
{
	int ret;
	uint64_t challenge;

	ret = iris_manager_pre_enroll(&challenge);
	iris_response_put_int32(response, ret);
	iris_response_put_uint64(response, challenge);
}

void exec_post_enroll(struct iris_request *req, struct iris_response *response)
{
	int ret;

	ret = iris_manager_post_enroll();
	iris_response_put_int32(response, ret);
}


void exec_enroll_begin(struct iris_request *req, struct iris_response *response)
{
	int ret;
	uint32_t user_id;

	ret = iris_req_get_uint32(req, &user_id);
	if (ret) {
		iris_response_put_int32(response, ret);
		return;
	}

	ret = iris_manager_enroll_begin(user_id);
	iris_response_put_int32(response, ret);
}

void exec_enroll_capture(struct iris_request *req, struct iris_response *response)
{
	struct iris_enroll_status enroll_status;
	struct iris_frame_buffer buffer;
	int ret;

	ret = iris_req_get_frame(req, &buffer);
	if (ret) {
		iris_response_put_int32(response, ret);
		return;
	}
	/*todo parse the frame buffer to iris manager */
	ret = iris_manager_enroll_capture(&enroll_status);
	iris_response_put_int32(response, ret);
	iris_response_put_int32(response, enroll_status.status);
	iris_response_put_uint32(response, enroll_status.progress);
}

void exec_enroll_commit(struct iris_request *req, struct iris_response *response)
{
	int ret;
	struct iris_enroll_result enroll_result;

	ret = iris_manager_enroll_commit(&enroll_result);
	iris_response_put_int32(response, ret);
	iris_response_put_uint32(response, enroll_result.irisId);
}

void exec_enroll_cancel(struct iris_request *req, struct iris_response *response)
{
	int ret;

	ret = iris_manager_enroll_cancel();
	iris_response_put_int32(response, ret);
}

void exec_retrieve_enrollee(struct iris_request *req, struct iris_response *response)
{
	int ret;
	uint32_t user_id;
	struct iris_enrollee_info info;

	ret = iris_req_get_uint32(req, &user_id);
	if (ret) {
		iris_response_put_int32(response, ret);
		return;
	}

	ret = iris_manager_retrieve_enrollee(user_id, &info);

	iris_response_put_int32(response, ret);	
	iris_response_put_uint32(response, info.userId);
	iris_response_put_uint64(response, info.enrollment_date);
}

void exec_delete_enrollee(struct iris_request *req, struct iris_response *response)
{
	int ret;
	uint32_t user_id;
	uint32_t irisId = 0;

	ret = iris_req_get_uint32(req, &irisId);
	if (ret) {
		iris_response_put_int32(response, ret);
		return;
	}

	ret = iris_req_get_uint32(req, &user_id);
	if (ret) {
		iris_response_put_int32(response, ret);
		return;
	}

	ret = iris_manager_delete_enrollee(irisId, user_id);
	iris_response_put_int32(response, ret);
}

void exec_delete_all_enrollees(struct iris_request *req, struct iris_response *response)
{
	int ret;

	ret = iris_manager_delete_all_enrollee();
	iris_response_put_int32(response, ret);
}

void exec_identify_begin(struct iris_request *req, struct iris_response *response)
{
	int ret;

	ret = iris_manager_identify_begin();
	iris_response_put_int32(response, ret);

}

void exec_identify_capture(struct iris_request *req, struct iris_response *response)
{
	int ret;
	struct iris_identify_result identify_result;

	ret = iris_manager_identify_capture(&identify_result);

	iris_response_put_int32(response, ret);
	iris_response_put_uint32(response, identify_result.userId);
}

void exec_identify_end(struct iris_request *req, struct iris_response *response)
{
	int ret;

	ret = iris_manager_identify_end();
	iris_response_put_int32(response, ret);
}

void exec_verify_begin(struct iris_request *req, struct iris_response *response)
{
	int ret;
	uint32_t user_id;
	uint64_t operationId; 

	ret = iris_req_get_uint64(req, &operationId);
	if (ret) {
		iris_response_put_int32(response, ret);
		return;
	}

	ret = iris_req_get_uint32(req, &user_id);
	if (ret) {
		iris_response_put_int32(response, ret);
		return;
	}

	ret = iris_manager_verify_begin(operationId, user_id);
	iris_response_put_int32(response, ret);
}

void exec_verify_capture(struct iris_request *req, struct iris_response *response)
{
	int ret;
	struct iris_verify_result verify_result;

	ret = iris_manager_verify_capture(&verify_result);
	
	iris_response_put_int32(response, ret);
	iris_response_put_uint32(response, verify_result.status);
	iris_response_put_int32(response, verify_result.irisId);
	iris_response_put_int32(response, verify_result.matched);
}

void exec_verify_end(struct iris_request *req, struct iris_response *response)
{
	int ret;

	ret = iris_manager_verify_end();
	iris_response_put_int32(response, ret);
}

void exec_get_authenticator_id(struct iris_request *req, struct iris_response *response)
{
	int ret;
	uint64_t id;

	ret = iris_manager_get_authenticator_id(&id);
	iris_response_put_int32(response, ret);
	iris_response_put_uint64(response, id);
}

void exec_verify_token(struct iris_request *req, struct iris_response *response)
{
	int ret;
	struct hw_auth_token token;

	ret = iris_req_get_token(req, &token);
	if (ret) {
		iris_response_put_int32(response, ret);
		return;
	}

	ret = iris_manager_verify_token(&token);
	iris_response_put_int32(response, ret);
}

void exec_get_auth_token(struct iris_request *req, struct iris_response *response)
{
	int ret;
	struct hw_auth_token token;

	(void)req;
	ret = iris_manager_get_auth_token(&token);
	iris_response_put_int32(response, ret);
	if (!ret)
		iris_response_put_token(response, &token);
}

void exec_set_meta_data(struct iris_request *req, struct iris_response *response)
{
	int ret;
	struct iris_meta_data meta;

	ret = iris_req_get_meta_data(req, &meta);
	if (ret) {
		iris_response_put_int32(response, ret);
		return;
	}

	ret = iris_manager_set_meta_data(&meta);
	iris_response_put_int32(response, ret);
}

void exec_set_token_key(struct iris_request *req, struct iris_response *response)
{
	int ret;
	uint8_t *key_data;
	uint32_t key_len;

	ret = iris_req_get_uint32(req, &key_len);
	if (ret) {
		iris_response_put_int32(response, ret);
		return;
	}

	ret = iris_req_get_data(req, &key_data, key_len);
	if (ret) {
		iris_response_put_int32(response, ret);
		return;
	}

	ret = iris_manager_set_token_key(key_data, key_len);
	iris_response_put_int32(response, ret);
}

void exec_enumerate_enrollment(struct iris_request *req, struct iris_response *response)
{
	int ret;
	int i, count;
	struct iris_identifier ids[5];
	
	ret = iris_manager_enumerate_enrollment(&count, ids);
	iris_response_put_int32(response, ret);
	if (!ret) {
		iris_response_put_uint32(response, count);
		for (i = 0; i < count; i++) {
			iris_response_put_uint32(response, ids[i].userId);
			iris_response_put_uint32(response, ids[i].irisId);
		}
	}
}

