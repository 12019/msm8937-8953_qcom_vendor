/*
 * Copyright (c) 2015 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */

#ifndef __IRIS_PARSER_H__
#define __IRIS_PARSER_H__

#include <stdint.h>

/* forward declaration*/
struct iris_frame_info;
struct iris_frame_buffer;
struct hw_auth_token;
struct iris_meta_data;

struct iris_request {
	void *buf;
	uint32_t len;
	int index;
};

struct iris_response {
	void *buf;
	uint32_t len;
	int index;
};

int iris_req_init(struct iris_request *req, void *buf, uint32_t len);
void iris_req_deinit(struct iris_request *req);
int iris_req_get_uint8(struct iris_request *req, uint8_t *val);
int iris_req_get_uint32(struct iris_request *req, uint32_t *val);
int iris_req_get_uint64(struct iris_request *req, uint64_t *val);
int iris_req_get_string(struct iris_request *req, unsigned char **val);
int iris_req_get_data(struct iris_request *req, void *data, uint32_t len);
int iris_req_get_frame_info(struct iris_request *req, struct iris_frame_info *info);
int iris_req_get_frame(struct iris_request *req, struct iris_frame_buffer *frame);
int iris_req_get_token(struct iris_request *req, struct hw_auth_token *token);
int iris_req_get_meta_data(struct iris_request *req, struct iris_meta_data *token);

int iris_response_init(struct iris_response *response, void *buf, uint32_t len);
void iris_response_deinit(struct iris_response *response);
int iris_response_put_uint8(struct iris_response *response, uint8_t val);
int iris_response_put_int32(struct iris_response *response, int32_t val);
int iris_response_put_uint32(struct iris_response *response, uint32_t val);
int iris_response_put_string(struct iris_response *response, unsigned char *str);
int iris_response_put_uint64(struct iris_response *response, uint64_t val);
int iris_response_put_data(struct iris_response *response, void *data, uint32_t len);
int iris_response_put_token(struct iris_response *response, struct hw_auth_token *token);

#endif

