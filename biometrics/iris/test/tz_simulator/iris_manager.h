/*
 * Copyright (c) 2015 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */

#ifndef _IRIS_MANAGER_H_
#define _IRIS_MANAGER_H_

#include "iris_common.h"

#define IRIS_MAX_ENROLLEE_ID_LEN 40

struct iris_version {
	uint32_t version;
};

struct iris_enroll_status {
	int32_t status;
	uint32_t progress;
};

struct iris_enroll_result {
	uint32_t irisId;
};

struct iris_enrollee_info {
	uint32_t userId;
	uint64_t enrollment_date;
};

struct hw_auth_token {
	uint8_t version;  // Current version is 0
	uint64_t challenge;
	uint64_t user_id;			  // secure user ID, not Android user ID
	uint64_t authenticator_id;	  // secure authenticator ID
	uint32_t authenticator_type;  // hw_authenticator_type_t, in network order
	uint64_t timestamp; 		  // in network order
	uint8_t hmac[32];
};

struct iris_verify_result {
	int32_t status;
	uint32_t irisId;
    int32_t matched;
};

struct iris_identify_result {
	uint32_t userId;
};

struct iris_identifier {
	uint32_t irisId;
	uint32_t userId;
};

IRIS_API iris_status iris_manager_init(void);
IRIS_API iris_status iris_manager_deinit(void);
IRIS_API iris_status iris_manager_pre_enroll(uint64_t *challenge);
IRIS_API iris_status iris_manager_post_enroll(void);
IRIS_API iris_status iris_manager_get_version(struct iris_version *version);
IRIS_API iris_status iris_manager_enroll_begin(uint32_t user_id);
IRIS_API iris_status iris_manager_enroll_capture(struct iris_enroll_status *enroll_status);
IRIS_API iris_status iris_manager_enroll_commit(struct iris_enroll_result *enroll_result);
IRIS_API iris_status iris_manager_enroll_cancel(void);
IRIS_API iris_status iris_manager_identify_begin(void);
IRIS_API iris_status iris_manager_identify_capture(struct iris_identify_result *identify_result);
IRIS_API iris_status iris_manager_identify_end(void);
IRIS_API iris_status iris_manager_verify_begin(uint64_t operationId, uint32_t user_id);
IRIS_API iris_status iris_manager_verify_capture(struct iris_verify_result *verify_result);
IRIS_API iris_status iris_manager_verify_end(void);

IRIS_API iris_status iris_manager_retrieve_enrollee(uint32_t user_id, struct iris_enrollee_info *enrollee_info);
IRIS_API iris_status iris_manager_delete_enrollee(uint32_t irisId, uint32_t user_id);
IRIS_API iris_status iris_manager_delete_all_enrollee(void);

IRIS_API iris_status iris_manager_get_authenticator_id(uint64_t *challenge);

IRIS_API iris_status iris_manager_verify_token(struct hw_auth_token *token);
IRIS_API iris_status iris_manager_get_auth_token(struct hw_auth_token *token);

IRIS_API iris_status iris_manager_set_meta_data(struct iris_meta_data *meta);

IRIS_API iris_status iris_manager_set_token_key(uint8_t *token_key, uint32_t key_len);

IRIS_API iris_status iris_manager_enumerate_enrollment(int32_t *count, struct iris_identifier *ids);

#endif

