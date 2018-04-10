/*
 * Copyright (c) 2015 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */

#ifndef __IRIS_TZ_API_H__
#define __IRIS_TZ_API_H__

struct iris_request;
struct iris_response;

enum {
	IRIS_CMD_GET_VERSION			= 0x100,

	//enroll
	IRIS_CMD_ENROLL_BEGIN			= 0x101,
	IRIS_CMD_ENROLL_CAPTURE 		= 0x102,
	IRIS_CMD_ENROLL_COMMIT			= 0x103,
	IRIS_CMD_ENROLL_CANCEL			= 0x104,

	//identify
	IRIS_CMD_IDENTIFY_BEGIN 		= 0x105,
	IRIS_CMD_IDENTIFY_CAPTURE		= 0x106,

	//verify
	IRIS_CMD_VERIFY_BEGIN			= 0x107,
	IRIS_CMD_VERIFY_CAPTURE 		= 0x108,


	//manage (get and delete)
	IRIS_CMD_RETRIEVE_ENROLLMENT	= 0x109,
	IRIS_CMD_DELETE_ENROLLMENT		= 0x10A,
	IRIS_CMD_DELETE_ALL_ENROLLMENTS = 0x10B,

	IRIS_CMD_PRE_ENROLL 			= 0x10C,
	IRIS_CMD_POST_ENROLL			= 0x10D,

	IRIS_CMD_GET_AUTHENTICATOR_ID	= 0x10E,

	IRIS_CMD_VERIFY_TOKEN			= 0x10F,
	IRIS_CMD_GET_AUTH_TOKEN 		= 0x110,

	IRIS_CMD_IDENTIFY_END			= 0x111,
	IRIS_CMD_VERIFY_END 			= 0x112,

	IRIS_CMD_SET_META_DATA			= 0x113,
	IRIS_CMD_SET_TOKEN_KEY			= 0x114,

	IRIS_CMD_ENUMERATE_ENROLLMENT	= 0x115,

	IRIS_CMD_INVALID				= 0x7FFFFFFF,
};

void exec_get_version(struct iris_request *req, struct iris_response *response);
void exec_pre_enroll(struct iris_request *req, struct iris_response *response);
void exec_post_enroll(struct iris_request *req, struct iris_response *response);
void exec_enroll_begin(struct iris_request *req, struct iris_response *response);
void exec_enroll_capture(struct iris_request *req, struct iris_response *response);
void exec_enroll_commit(struct iris_request *req, struct iris_response *response);
void exec_enroll_cancel(struct iris_request *req, struct iris_response *response);
void exec_retrieve_enrollee(struct iris_request *req, struct iris_response *response);
void exec_delete_enrollee(struct iris_request *req, struct iris_response *response);
void exec_delete_all_enrollees(struct iris_request *req, struct iris_response *response);
void exec_identify_begin(struct iris_request *req, struct iris_response *response);
void exec_identify_capture(struct iris_request *req, struct iris_response *response);
void exec_identify_end(struct iris_request *req, struct iris_response *response);
void exec_verify_begin(struct iris_request *req, struct iris_response *response);
void exec_verify_capture(struct iris_request *req, struct iris_response *response);
void exec_verify_end(struct iris_request *req, struct iris_response *response);
void exec_get_authenticator_id(struct iris_request *req, struct iris_response *response);
void exec_verify_token(struct iris_request *req, struct iris_response *response);
void exec_get_auth_token(struct iris_request *req, struct iris_response *response);
void exec_set_meta_data(struct iris_request *req, struct iris_response *response);
void exec_set_token_key(struct iris_request *req, struct iris_response *response);
void exec_enumerate_enrollment(struct iris_request *req, struct iris_response *response);

#endif

