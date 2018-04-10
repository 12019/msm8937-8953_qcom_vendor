/*
 * Copyright (c) 2015 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */

#define LOG_TAG "Iris_parser"
#include <utils/Log.h>

#include "iris_parser.h"
#include "iris_common.h"
#include "iris_manager.h"
#include "qcosal.h"
#include <unistd.h>

int iris_req_init(struct iris_request *req, void *buf, uint32_t len)
{
	if (!buf || len < sizeof(uint32_t))
		return -EINVAL;

	req->buf = buf;
	req->len = len;
	req->index = 0;
	return 0;
}

void iris_req_deinit(struct iris_request *req)
{
	(void)req;
}

int iris_req_get_uint8(struct iris_request *req, uint8_t *val)
{
	if (req->index + sizeof(uint8_t) > req->len)
		return -EINVAL;

	os_memscpy(val, sizeof(uint8_t), (char *)req->buf + req->index, sizeof(uint8_t));
	req->index += sizeof(uint8_t);
	return 0;
}

int iris_req_get_uint32(struct iris_request *req, uint32_t *val)
{
	if (req->index + sizeof(uint32_t) > req->len)
		return -EINVAL;

	os_memscpy(val, sizeof(uint32_t), (char *)req->buf + req->index, sizeof(uint32_t));
	req->index += sizeof(uint32_t);
	return 0;
}

int iris_req_get_uint64(struct iris_request *req, uint64_t *val)
{
	if (req->index + sizeof(uint64_t) > req->len)
		return -EINVAL;
	
	os_memscpy(val, sizeof(uint64_t), (char *)req->buf + req->index, sizeof(uint64_t));
	req->index += sizeof(uint64_t);
	return 0;
}

int iris_req_get_string(struct iris_request *req, unsigned char **val)
{
	int ret;
	uint32_t str_len;

	*val = NULL;

	ret = iris_req_get_uint32(req, &str_len);
	if (ret)
		return ret;

	if (!str_len || req->index + str_len > req->len)
		return -EINVAL;

	*val = (unsigned char *)req->buf + req->index;
	req->index += str_len;
	return 0;
}

int iris_req_get_data(struct iris_request *req, void *data, uint32_t len)
{
	int ret;
	uint32_t data_len;

	ret = iris_req_get_uint32(req, &data_len);
	if (ret)
		return ret;

	if (!data_len || req->index + data_len > req->len || data_len != len)
		return -EINVAL;

	os_memscpy(data, len, (char *)req->buf + req->index, len);

	req->index += len;
	return 0;
}

int iris_req_get_frame_info(struct iris_request *req, struct iris_frame_info *info)
{
	int ret = 0;

	ret |= iris_req_get_uint32(req, &info->width);
	ret |= iris_req_get_uint32(req, &info->height);
	ret |= iris_req_get_uint32(req, &info->stride);
	ret |= iris_req_get_uint32(req, &info->color_format);
	ret |= iris_req_get_uint32(req, &info->sensitivity);
	ret |= iris_req_get_uint32(req, &info->exp_time_ms);
	return ret;
}

int iris_req_get_frame(struct iris_request *req, struct iris_frame_buffer *frame)
{
	int ret = 0;
	uint32_t frame_handle;

	ret |= iris_req_get_frame_info(req, &frame->meta);
	ret |= iris_req_get_uint32(req, &frame_handle);
	ret |= iris_req_get_uint32(req, &frame->buffer_length);

	if (!ret) {
		ALOGD("closing frame handle=%d\n", frame_handle);
		if (frame_handle > 0)
			close(frame_handle);
		/*todo translate the handle into buffer pointer*/
		//frame->buffer = (uint8 *)frame_handle;
	}
	return ret;
}

int iris_req_get_token(struct iris_request *req, struct hw_auth_token *token)
{
	int ret = 0;
	ret |= iris_req_get_uint8(req, &token->version);
	ret |= iris_req_get_uint64(req, &token->challenge);
	ret |= iris_req_get_uint64(req, &token->user_id);
	ret |= iris_req_get_uint64(req, &token->authenticator_id);
	ret |= iris_req_get_uint32(req, &token->authenticator_type);
	ret |= iris_req_get_uint64(req, &token->timestamp);
	ret |= iris_req_get_data(req, &token->hmac, sizeof(token->hmac));
	return ret;
}

int iris_req_get_meta_data(struct iris_request *req, struct iris_meta_data *meta)
{
	int ret = 0;
	uint32_t frame_handle;
	
	ret |= iris_req_get_frame_info(req, &meta->frame_info);
	ret |= iris_req_get_uint32(req, &meta->auto_exposure);
	ret |= iris_req_get_uint32(req, &meta->orientation);
	return ret;
}

int iris_response_init(struct iris_response *response, void *buf, uint32_t len)
{
	if (!buf || len < sizeof(uint32_t))
		return -EINVAL;

	response->buf = buf;
	response->len = len;
	response->index = 0;
	return 0;
}

void iris_response_deinit(struct iris_response *response)
{
	(void)response;
}

int iris_response_put_int32(struct iris_response *response, int32_t val)
{
	if (response->index + sizeof(int32_t) > response->len)
		return -EINVAL;

	os_memscpy((char *)response->buf + response->index, sizeof(int32_t), &val, sizeof(int32_t));
	response->index += sizeof(int32_t);
	return 0;
}

int iris_response_put_uint32(struct iris_response *response, uint32_t val)
{
	if (response->index + sizeof(uint32_t) > response->len)
		return -EINVAL;
	
	os_memscpy((char *)response->buf + response->index, sizeof(uint32_t), &val, sizeof(uint32_t));
	response->index += sizeof(uint32_t);
	return 0;
}

int iris_response_put_string(struct iris_response *response, unsigned char *str)
{
	int ret;
	uint32_t str_len = os_strlen((char *)str) + 1;

	ret = iris_response_put_uint32(response, str_len);
	if (ret)
		return ret;

	if (response->index + str_len > response->len)
		return -EINVAL;

	os_memscpy((char *)response->buf + response->index, str_len, str, str_len);
	response->index += str_len;
	return 0;

}

int iris_response_put_uint8(struct iris_response *response, uint8_t val)
{
	if (response->index + sizeof(uint8_t) > response->len)
		return -EINVAL;

	os_memscpy((char *)response->buf + response->index, sizeof(uint8_t), &val, sizeof(uint8_t));
	response->index += sizeof(uint8_t);
	return 0;
}

int iris_response_put_uint64(struct iris_response *response, uint64_t val)
{
	if (response->index + sizeof(uint64_t) > response->len)
		return -EINVAL;

	os_memscpy((char *)response->buf + response->index, sizeof(uint64_t), &val, sizeof(uint64_t));
	response->index += sizeof(uint64_t);
	return 0;
}

int iris_response_put_data(struct iris_response *response, void *data, uint32_t len)
{
	int ret;

	ret = iris_response_put_uint32(response, len);
	if (ret)
		return ret;

	if (response->index + len > response->len)
		return -EINVAL;

	os_memscpy((char *)response->buf + response->index, len, data, len);
	response->index += len;
	return 0;
}

int iris_response_put_token(struct iris_response *response, struct hw_auth_token *token)
{
	int ret;

	ret = iris_response_put_uint8(response, token->version);
	if (ret)
		return ret;

	ret = iris_response_put_uint64(response, token->challenge);
	if (ret)
		return ret;

	ret = iris_response_put_uint64(response, token->user_id);
	if (ret)
		return ret;

	ret = iris_response_put_uint64(response, token->authenticator_id);
	if (ret)
		return ret;

	ret = iris_response_put_uint32(response, token->authenticator_type);
	if (ret)
		return ret;

	ret = iris_response_put_uint64(response, token->timestamp);
	if (ret)
		return ret;

	ret = iris_response_put_data(response, token->hmac, sizeof(token->hmac));

	return ret;
}


