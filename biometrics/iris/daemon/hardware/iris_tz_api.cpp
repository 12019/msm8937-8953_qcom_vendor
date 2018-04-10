/*
 * Copyright (c) 2015-2016 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */

#define LOG_TAG "IrisTzApi"
#include <utils/Log.h>

#include <utils/Log.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>

#include "iris_tz_api.h"
#include "iris_tz_comm.h"
#include "iris_socket_comm.h"
#include "iris_fd_socket_mapper.h"

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

	IRIS_CMD_PRE_ENROLL				= 0x10C,
	IRIS_CMD_POST_ENROLL			= 0x10D,

	IRIS_CMD_GET_AUTHENTICATOR_ID	= 0x10E,

	IRIS_CMD_VERIFY_TOKEN			= 0x10F,
	IRIS_CMD_GET_AUTH_TOKEN			= 0x110,

	IRIS_CMD_IDENTIFY_END			= 0x111,
	IRIS_CMD_VERIFY_END 			= 0x112,

	IRIS_CMD_SET_META_DATA			= 0x113,
	IRIS_CMD_SET_TOKEN_KEY			= 0x114,

	IRIS_CMD_ENUMERATE_ENROLLMENT	= 0x115,

	IRIS_CMD_TEST					= 0x8000
};

#define KM_CMD_ID_GET_AUTH_TOKEN_KEY	0x205UL


class iris_msg_serializer {
public:
	iris_msg_serializer();
	virtual ~iris_msg_serializer();

	void init(void);
	void *buffer(int &buf_len);

	int write_uint8(uint8_t num);
	int write_uint32(uint32_t num);
	int write_uint64(uint64_t num);
	int write_string(const unsigned char *str);
	int write_data(const void *data, uint32_t len);
	int write_frame(struct iris_frame *frame);
	int write_frame_info(struct iris_frame_info *info);

private:
	unsigned char mBuf[IRIS_MAX_TZ_BUFFER_LEN];
	int mLen;
	int mIndex;
};

class iris_msg_deserializer {
public:
	iris_msg_deserializer();
	virtual ~iris_msg_deserializer();

	int init(void);
	int set_buffer(unsigned char *data, int data_len);

	int read_uint8(uint8_t &num);
	int read_int32(int32_t &num);
	int read_uint32(uint32_t &num);
	int read_uint64(uint64_t &num);
	int read_string(unsigned char *str, uint32_t max_len);
	int read_data(void *data, uint32_t len);

private:
	unsigned char *mBuf;
	unsigned int mLen;
	int mIndex;
};

class iris_tzee: public iris_interface {
public:
	iris_tzee();
	~iris_tzee();

	int init(bool tz_comm);
	void deinit();

	virtual int get_version(struct iris_version &version);

	virtual int pre_enroll(uint64_t &challenge);
	virtual int post_enroll(void);
	virtual int enroll_begin(uint32_t user_id);
	virtual int enroll_capture(struct iris_frame *frame, struct iris_enroll_status &enroll_status);
	virtual int enroll_commit(struct iris_enroll_result &enroll_result);
	virtual int enroll_cancel();

	virtual int identify_begin();
	virtual int identify_capture(struct iris_frame *frame, struct iris_identify_result &identify_result);
	virtual int identify_end();

	virtual int verify_begin(uint64_t operationId, uint32_t user_id);
	virtual int verify_capture(struct iris_frame *frame, struct iris_verify_result &verify_result);
	virtual int verify_end();

	virtual int retrieve_enrollee(uint32_t user_id, struct iris_enroll_record &enroll_record);
	virtual int delete_enrollee(uint32_t irisId, uint32_t user_id);
	virtual int delete_all_enrollee(void);
	virtual int enumerate_enrollment(struct iris_enrollment_list &enrollment_list);

	virtual int get_authenticator_id(uint64_t &id);

	virtual int verify_token(const hw_auth_token_t &token);

	virtual int get_auth_token(hw_auth_token_t &token);

	virtual int set_meta_data(struct iris_meta_data &meta);

	virtual int test(uint32_t num_input, char **buf);

private:
	int command_prepare(void);
	int command_send(int response_len, iris_ion_fd_info *fd_info = NULL);
	int send_simple_command(uint32_t cmd);
	int send_key(uint8_t *key, uint32_t key_len);
	int get_key(uint8_t **key, uint32_t *len);

private:
	iris_comm_interface		*mComm;
	iris_fd_mapper			*mFdMapper;
	iris_msg_serializer		*mSeralizer;
	iris_msg_deserializer	*mDeseralizer;
	unsigned char			mBuf[IRIS_MAX_TZ_BUFFER_LEN];
	bool					mTzComm;
};


/* iris_msg_serializer */
iris_msg_serializer::iris_msg_serializer()
{
	init();
}

iris_msg_serializer::~iris_msg_serializer()
{
}

void iris_msg_serializer::init()
{
	memset(mBuf, 0, IRIS_MAX_TZ_BUFFER_LEN);
	mLen = 0;
	mIndex = 0;
}

void *iris_msg_serializer::buffer(int &buf_len)
{
	if (!mLen)
		return NULL;

	buf_len = mLen;
	return mBuf;
}

int iris_msg_serializer::write_uint8(uint8_t num)
{
	memcpy(mBuf + mIndex, &num, sizeof(num));
	mIndex += sizeof(num);
	mLen += sizeof(num);
	return 0;
}

int iris_msg_serializer::write_uint32(uint32_t num)
{
	memcpy(mBuf + mIndex, &num, sizeof(num));
	mIndex += sizeof(num);
	mLen += sizeof(num);
	return 0;
}

int iris_msg_serializer::write_string(const unsigned char *str)
{
	//copy the null at the end as well
	uint32_t str_len = strlen((const char *)str) + 1;

	write_uint32(str_len);

	memcpy(mBuf + mIndex, str, str_len);
	mIndex += str_len;
	mLen += str_len;
	return 0;
}

int iris_msg_serializer::write_data(const void *data, uint32_t len)
{
	write_uint32(len);
	
	memcpy(mBuf + mIndex, data, len);
	mIndex += len;
	mLen += len;
	return 0;
}

int iris_msg_serializer::write_uint64(uint64_t num)
{
	memcpy(mBuf + mIndex, &num, sizeof(num));
	mIndex += sizeof(num);
	mLen += sizeof(num);
	return 0;
}

int iris_msg_serializer::write_frame_info(struct iris_frame_info *info)
{
	int ret = 0;

	ret |= write_uint32(info->width);
	ret |= write_uint32(info->height);
	ret |= write_uint32(info->stride);
	ret |= write_uint32(info->format);
	ret |= write_uint32(info->sensitivity);
	ret |= write_uint32(info->exp_time_ms);

	return ret;
}

int iris_msg_serializer::write_frame(struct iris_frame *frame)
{
	int ret = 0;

	if (frame) {
		ret |= write_frame_info(&frame->info);
		ret |= write_uint32(frame->frame_handle);
		ALOGD("write frame_handle %x", frame->frame_handle);
		ret |= write_uint32(0);
		ret |= write_uint32(frame->frame_len);
		ALOGD("write frame_len %x", frame->frame_len);
	} else {
		struct iris_frame_info info = {0, 0, 0, 0, 0, 0};
		ret |= write_frame_info(&info);
		ret |= write_uint32(0);
		ret |= write_uint32(0);
		ret |= write_uint32(0);
		ALOGE("write empty frame");
	}
	return ret;
}


/* iris_msg_deserializer */
iris_msg_deserializer::iris_msg_deserializer()
{
	init();
}

iris_msg_deserializer::~iris_msg_deserializer()
{
}

int iris_msg_deserializer::init(void)
{
	mBuf = NULL;
	mLen = 0;
	mIndex = 0;
	return 0;
}

int iris_msg_deserializer::set_buffer(unsigned char *data, int data_len)
{
	mBuf = data;
	mLen = data_len;
	mIndex = 0;
	return 0;
}

int iris_msg_deserializer::read_uint8(uint8_t &num)
{
	if (mIndex + sizeof(uint8_t) > mLen)
		return -EINVAL;

	memcpy(&num, mBuf + mIndex, sizeof(uint8_t));
	mIndex += sizeof(uint8_t);
	return 0;
}

int iris_msg_deserializer::read_int32(int32_t &num)
{
	if (mIndex + sizeof(int32_t) > mLen)
		return -EINVAL;

	memcpy(&num, mBuf + mIndex, sizeof(int32_t));
	mIndex += sizeof(int32_t);
	return 0;
}

int iris_msg_deserializer::read_uint32(uint32_t &num)
{
	if (mIndex + sizeof(uint32_t) > mLen)
		return -EINVAL;

	memcpy(&num, mBuf + mIndex, sizeof(uint32_t));
	mIndex += sizeof(uint32_t);
	return 0;
}

int iris_msg_deserializer::read_uint64(uint64_t &num)
{
	if (mIndex + sizeof(uint64_t) > mLen)
		return -EINVAL;

	memcpy(&num, mBuf + mIndex, sizeof(uint64_t));
	mIndex += sizeof(uint64_t);
	return 0;
}

int iris_msg_deserializer::read_string(unsigned char *str, uint32_t max_len)
{
	int ret;
	uint32_t str_len;

	ret = read_uint32(str_len);
	if (ret)
		return ret;

	if (str_len == 0 || mIndex + str_len > IRIS_MAX_TZ_BUFFER_LEN || str_len > max_len)
		return -EINVAL;

	if (*(mBuf + str_len - 1) != 0)
		return -EINVAL;

	memcpy(str, mBuf + mIndex, str_len);
	mIndex += str_len;

	return 0;
}

int iris_msg_deserializer::read_data(void *data, uint32_t len)
{
	int ret;
	uint32_t data_len;

	ret = read_uint32(data_len);
	if (ret)
		return ret;

	if (data_len != len)
		return -EINVAL;

	if (mIndex + data_len > IRIS_MAX_TZ_BUFFER_LEN)
		return -EINVAL;

	memcpy(data, mBuf + mIndex, data_len);
	mIndex += data_len;
	
	return 0;
}

/* iris_tzee */
iris_tzee::iris_tzee()
	:mComm(NULL), mFdMapper(NULL), mSeralizer(NULL), mDeseralizer(NULL), mTzComm(false)
{
}

iris_tzee::~iris_tzee()
{
	deinit();
}

int iris_tzee::command_prepare(void)
{
	int ret = -EINVAL;

	if (mComm) {
		mSeralizer->init();
		mDeseralizer->init();
		ret = 0;
	}
	return ret;
}

int iris_tzee::command_send(int response_len, iris_ion_fd_info *fd_info)
{
	void *request_buf;
	int request_buf_len;
	int ret;

	request_buf = mSeralizer->buffer(request_buf_len);
	if (!request_buf || request_buf_len == 0)
		return -EINVAL;

	if (response_len <= 0 || response_len > IRIS_MAX_TZ_BUFFER_LEN)
		return -EINVAL;

	if (fd_info)
		ret = mComm->send_modified_cmd(request_buf, request_buf_len, mBuf, response_len, fd_info);
	else
		ret = mComm->send(request_buf, request_buf_len, mBuf, response_len);

	if (ret) {
		ALOGE("fail to send command, deinit communication channel");
		deinit();
		return ret;
	}

	mDeseralizer->set_buffer(mBuf, response_len);

	return ret;
}

int iris_tzee::send_simple_command(uint32_t cmd)
{
	int ret, status;

	ret = command_prepare();
	if (ret) {
		ALOGE("fail to send command");
		return ret;
	}
	mSeralizer->write_uint32(cmd);
	ret = command_send(sizeof(int32_t));
	if (ret)
		return ret;

	ret = mDeseralizer->read_int32(status);

	return ret ? ret : status;
}

int iris_tzee::get_version(struct iris_version &version)
{
	int ret, status;
	int response_len = sizeof(int32_t) + sizeof(iris_version);

	ret = command_prepare();
	if (ret)
		return ret;

	mSeralizer->write_uint32(IRIS_CMD_GET_VERSION);
	ret = command_send(response_len);
	if (ret)
		return ret;

	ret = mDeseralizer->read_int32(status);
	if (ret)
		return ret;

	ret = mDeseralizer->read_uint32(version.version);

	return ret ? ret : status;
}

int iris_tzee::pre_enroll(uint64_t &challenge)
{
	int ret, status;
	int response_len = sizeof(int32_t) + sizeof(uint64_t);

	ret = command_prepare();
	if (ret)
		return ret;

	mSeralizer->write_uint32(IRIS_CMD_PRE_ENROLL);
	ret = command_send(response_len);
	if (ret)
		return ret;

	ret = mDeseralizer->read_int32(status);
	if (ret)
		return ret;

	ret = mDeseralizer->read_uint64(challenge);

	return ret ? ret : status;
}

int iris_tzee::post_enroll(void)
{
	return send_simple_command(IRIS_CMD_POST_ENROLL);
}

int iris_tzee::enroll_begin(uint32_t user_id)
{
	int ret, status;
	int response_len = sizeof(int32_t);

	ret = command_prepare();
	if (ret)
		return ret;

	mSeralizer->write_uint32(IRIS_CMD_ENROLL_BEGIN);
	mSeralizer->write_uint32(user_id);
	ret = command_send(response_len);
	if (ret)
		return ret;

	ret = mDeseralizer->read_int32(status);
	return ret ? ret : status;
}

int iris_tzee::enroll_capture(struct iris_frame *frame, struct iris_enroll_status &enroll_status)
{
	int ret, status;
	struct iris_frame mappedFrame;
	iris_ion_fd_info fd_info;
	int response_len = sizeof(int32_t) + sizeof(iris_enroll_status);

	ret = command_prepare();
	if (ret)
		return ret;

	if (frame) {
		mappedFrame = *frame;

		if (mFdMapper) {
			int mappedFd;
			mappedFd = mFdMapper->map((int)frame->frame_handle);
			if (mappedFd < 0)
				return mappedFd;
			mappedFrame.frame_handle = mappedFd;
		}
	}

	mSeralizer->write_uint32(IRIS_CMD_ENROLL_CAPTURE);

	if (frame) {
		mSeralizer->write_frame(&mappedFrame);
		/* pass original fd for tz map */
		fd_info.fd = frame->frame_handle;
		fd_info.cmd_buf_offset = ((char*)&(frame->frame_data) - (char*)frame) + sizeof(uint32_t);
		ret = command_send(response_len, &fd_info);
	} else {
		mSeralizer->write_frame(NULL);
		ret = command_send(response_len);
	}

	if (ret)
		return ret;

	ret = mDeseralizer->read_int32(status);
	if (ret)
		return ret;

	ret = mDeseralizer->read_int32(enroll_status.status);
	if (ret)
		return ret;

	ret = mDeseralizer->read_uint32(enroll_status.progress);

	return ret ? ret : status;
}

int iris_tzee::enroll_commit(struct iris_enroll_result &enroll_result)
{
	int ret, status;
	int response_len = sizeof(int32_t) + sizeof(iris_enroll_result);

	ret = command_prepare();
	if (ret)
		return ret;

	mSeralizer->write_uint32(IRIS_CMD_ENROLL_COMMIT);
	ret = command_send(response_len);
	if (ret)
		return ret;
	
	ret = mDeseralizer->read_int32(status);
	if (ret)
		return ret;
	
	ret = mDeseralizer->read_uint32(enroll_result.iris_id);
	
	return ret ? ret : status;
}

int iris_tzee::enroll_cancel(void)
{
	return send_simple_command(IRIS_CMD_ENROLL_CANCEL);
}

int iris_tzee::identify_begin()
{
	return send_simple_command(IRIS_CMD_IDENTIFY_BEGIN);
}

int iris_tzee::identify_capture(struct iris_frame *frame, struct iris_identify_result &identify_result)
{
	int ret, status;
	struct iris_frame mappedFrame;
	iris_ion_fd_info fd_info;
	int response_len = sizeof(int32_t) + sizeof(identify_result);

	ret = command_prepare();
	if (ret)
		return ret;

	if (frame) {
		int mappedFd;
		mappedFrame = *frame;
		if (mFdMapper) {
			mappedFd = mFdMapper->map((int)frame->frame_handle);
			if (mappedFd < 0)
				return mappedFd;
			mappedFrame.frame_handle = mappedFd;
		}
	}

	mSeralizer->write_uint32(IRIS_CMD_IDENTIFY_CAPTURE);

	if (frame) {
		mSeralizer->write_frame(&mappedFrame);
		/* pass original fd for tz map */
		fd_info.fd = frame->frame_handle;
		fd_info.cmd_buf_offset = ((char*)&(frame->frame_data) - (char*)frame) + sizeof(uint32_t);
		ret = command_send(response_len, &fd_info);
	} else {
		mSeralizer->write_frame(NULL);
		ret = command_send(response_len);
	}

	if (ret)
		return ret;

	ret = mDeseralizer->read_int32(status);
	if (ret)
		return ret;

	ret = mDeseralizer->read_uint32(identify_result.user_id);

	return ret ? ret : status;
}

int iris_tzee::identify_end()
{
	return send_simple_command(IRIS_CMD_IDENTIFY_END);
}

int iris_tzee::verify_begin(uint64_t operationId, uint32_t user_id)
{
	int ret, status;
	int response_len = sizeof(int32_t);

	ret = command_prepare();
	if (ret)
		return ret;

	mSeralizer->write_uint32(IRIS_CMD_VERIFY_BEGIN);
	mSeralizer->write_uint64(operationId);
	mSeralizer->write_uint32(user_id);
	ret = command_send(response_len);
	if (ret)
		return ret;

	ret = mDeseralizer->read_int32(status);
	return ret ? ret : status;
}

int iris_tzee::verify_capture(struct iris_frame *frame, struct iris_verify_result &verify_result)
{
	int ret, status;
	uint32_t matched;
	struct iris_frame mappedFrame;
	iris_ion_fd_info fd_info;
	int response_len = sizeof(int32_t) + sizeof(verify_result);

	ret = command_prepare();
	if (ret)
		return ret;

	if (frame) {
		int mappedFd;
		mappedFrame = *frame;
		if (mFdMapper) {
			mappedFd = mFdMapper->map((int)frame->frame_handle);
			if (mappedFd < 0)
				return mappedFd;
			mappedFrame.frame_handle = mappedFd;
		}
	}

	mSeralizer->write_uint32(IRIS_CMD_VERIFY_CAPTURE);
	if (frame) {
		mSeralizer->write_frame(&mappedFrame);
		/* pass original fd for tz map */
		fd_info.fd = frame->frame_handle;
		fd_info.cmd_buf_offset = ((char*)&(frame->frame_data) - (char*)frame) + sizeof(uint32_t);
		ret = command_send(response_len, &fd_info);
	} else {
		mSeralizer->write_frame(NULL);
		ret = command_send(response_len);
	}

	if (ret)
		return ret;

	ret = mDeseralizer->read_int32(status);
	if (ret)
		return ret;

	ret = mDeseralizer->read_int32(verify_result.status);
	if (ret)
		return ret;

	ret = mDeseralizer->read_uint32(verify_result.iris_id);
	if (ret)
		return ret;

	ret = mDeseralizer->read_uint32(matched);
	if (!ret)
		verify_result.matched = (matched != 0);

	return ret ? ret : status;
}

int iris_tzee::verify_end()
{
	return send_simple_command(IRIS_CMD_VERIFY_END);
}

int iris_tzee::retrieve_enrollee(uint32_t user_id, struct iris_enroll_record &enroll_record)
{
	int ret, status;
	int response_len = sizeof(int32_t) + sizeof(iris_enroll_record);

	ret = command_prepare();
	if (ret)
		return ret;

	mSeralizer->write_uint32(IRIS_CMD_RETRIEVE_ENROLLMENT);
	mSeralizer->write_uint32(user_id);
	ret = command_send(response_len);
	if (ret)
		return ret;

	ret = mDeseralizer->read_int32(status);
	if (ret)
		return ret;

	ret = mDeseralizer->read_uint32(enroll_record.user_id);
	if (ret)
		return ret;

	ret = mDeseralizer->read_uint64(enroll_record.enrollment_date);

	return ret ? ret : status;
}

int iris_tzee::delete_enrollee(uint32_t irisId, uint32_t user_id)
{
	int ret, status;
	int response_len = sizeof(int32_t);

	ret = command_prepare();
	if (ret)
		return ret;

	mSeralizer->write_uint32(IRIS_CMD_DELETE_ENROLLMENT);
	mSeralizer->write_uint32(irisId);
	mSeralizer->write_uint32(user_id);
	ret = command_send(response_len);
	if (ret)
		return ret;

	ret = mDeseralizer->read_int32(status);
	return ret ? ret : status;
}

int iris_tzee::delete_all_enrollee(void)
{
	int ret, status;
	int response_len = sizeof(int32_t);

	ret = command_prepare();
	if (ret)
		return ret;

	mSeralizer->write_uint32(IRIS_CMD_DELETE_ALL_ENROLLMENTS);
	ret = command_send(response_len);
	if (ret)
		return ret;

	ret = mDeseralizer->read_int32(status);
	return ret ? ret : status;
}

int iris_tzee::enumerate_enrollment(struct iris_enrollment_list &enrollment_list)
{
	int ret, status;
	uint32_t i;
	int response_len = sizeof(int32_t) + sizeof(iris_enrollment_list);
	
	ret = command_prepare();
	if (ret)
		return ret;
	
	mSeralizer->write_uint32(IRIS_CMD_ENUMERATE_ENROLLMENT);
	ret = command_send(response_len);
	if (ret)
		return ret;
	
	ret = mDeseralizer->read_int32(status);
	if (ret)
		return ret;

	ret = mDeseralizer->read_uint32(enrollment_list.count);
	if (ret)
		return ret;

	ALOGD("enrollment count=%u", enrollment_list.count);
	for (i = 0; i < enrollment_list.count; i++) {
		ret = mDeseralizer->read_uint32(enrollment_list.data[i].user_id);
		ret |= mDeseralizer->read_uint32(enrollment_list.data[i].iris_id);
		ALOGD("enrollment irisid=%u, userid=%u", 
			enrollment_list.data[i].iris_id,
			enrollment_list.data[i].user_id);
		if (ret)
			break;
	}

	return ret ? ret : status;
}

int iris_tzee::get_authenticator_id(uint64_t &id)
{
	int ret, status;
	int response_len = sizeof(int32_t) + sizeof(uint64_t);

	ret = command_prepare();
	if (ret)
		return ret;

	mSeralizer->write_uint32(IRIS_CMD_GET_AUTHENTICATOR_ID);
	ret = command_send(response_len);
	if (ret)
		return ret;

	ret = mDeseralizer->read_int32(status);
	if (ret)
		return ret;

	ret = mDeseralizer->read_uint64(id);

	return ret ? ret : status;
}

int iris_tzee::verify_token(const hw_auth_token_t &token)
{
	int ret, status;
	int response_len = sizeof(int32_t);

	ret = command_prepare();
	if (ret)
		return ret;

	mSeralizer->write_uint32(IRIS_CMD_VERIFY_TOKEN);
	mSeralizer->write_uint8(token.version);
	mSeralizer->write_uint64(token.challenge);
	mSeralizer->write_uint64(token.user_id);
	mSeralizer->write_uint64(token.authenticator_id);
	mSeralizer->write_uint32(token.authenticator_type);
	mSeralizer->write_uint64(token.timestamp);
	mSeralizer->write_data(token.hmac, sizeof(token.hmac));
	ret = command_send(response_len);
	if (ret)
		return ret;

	ret = mDeseralizer->read_int32(status);
	return ret ? ret : status;

}

int iris_tzee::get_auth_token(hw_auth_token_t &token)
{
	int ret, status;
	uint64_t temp64;
	uint32_t temp32;
	int response_len = sizeof(int32_t) + sizeof(hw_auth_token_t);

	ret = command_prepare();
	if (ret)
		return ret;

	mSeralizer->write_uint32(IRIS_CMD_GET_AUTH_TOKEN);
	ret = command_send(response_len);
	if (ret)
		return ret;

	ret = mDeseralizer->read_int32(status);
	if (ret)
		return ret;

	ret |= mDeseralizer->read_uint8(token.version);
	ret |= mDeseralizer->read_uint64(temp64);
	token.challenge = temp64;
	ret |= mDeseralizer->read_uint64(temp64);
	token.user_id = temp64;
	ret |= mDeseralizer->read_uint64(temp64);
	token.authenticator_id = temp64;
	ret |= mDeseralizer->read_uint32(temp32);
	token.authenticator_type = temp32;
	ret |= mDeseralizer->read_uint64(temp64);
	token.timestamp = temp64;
	ret |= mDeseralizer->read_data(token.hmac, sizeof(token.hmac));

	return ret ? ret : status;
}

int iris_tzee::set_meta_data(struct iris_meta_data &meta)
{
	int ret, status;
	int response_len = sizeof(int32_t);

	ret = command_prepare();
	if (ret)
		return ret;

	mSeralizer->write_uint32(IRIS_CMD_SET_META_DATA);
	mSeralizer->write_frame_info(&meta.frame_info);
	mSeralizer->write_uint32(meta.auto_exposure);
	mSeralizer->write_uint32(meta.orientation);
	ret = command_send(response_len);
	if (ret)
		return ret;

	ret = mDeseralizer->read_int32(status);
	return ret ? ret : status;
}

int iris_tzee::send_key(uint8_t *key, uint32_t key_len)
{
	int ret, status;
	int response_len = sizeof(int32_t);

	ret = command_prepare();
	if (ret)
		return ret;

	mSeralizer->write_uint32(IRIS_CMD_SET_TOKEN_KEY);
	mSeralizer->write_data(key, key_len);
	ret = command_send(response_len);
	if (ret)
		return ret;

	ret = mDeseralizer->read_int32(status);
	return ret ? ret : status;
}

int iris_tzee::get_key(uint8_t **key, uint32_t *len)
{
	int ret, status;
	uint32_t key_offset;
	uint32_t key_len = 0;
	iris_tz_communicator *km = NULL;
	void *request_buf;
	int request_buf_len;

	km = new iris_tz_communicator();
	if (!km) {
		ALOGE("fail to create km tz communicator\n");
		return -ENOMEM;
	}

	ret = km->open("keymaster");
	if (ret) {
		ALOGE("fail to connect to keymaster");
		goto iris_get_key_error;
	}

	command_prepare();
	mSeralizer->write_uint32(KM_CMD_ID_GET_AUTH_TOKEN_KEY);
	mSeralizer->write_uint32(4); //todo

	request_buf = mSeralizer->buffer(request_buf_len);
	ret = km->send(request_buf, request_buf_len, mBuf, IRIS_MAX_TZ_BUFFER_LEN);
	if (ret) {
		ALOGE("fail to get token key");
		goto iris_get_key_error;
	}
	mDeseralizer->set_buffer(mBuf, IRIS_MAX_TZ_BUFFER_LEN);
	ret = mDeseralizer->read_int32(status);
	ret |= mDeseralizer->read_uint32(key_offset);
	ret |=  mDeseralizer->read_uint32(key_len);
	if (ret || status || key_len <= 0) {
		ALOGE("fail to get key from km\n");
		goto iris_get_key_error;
	}

	*key = new uint8_t[key_len];
	*len = key_len;
	memcpy(*key, mBuf + key_offset, key_len);
	ALOGD("key len=%d", key_len);

iris_get_key_error:
	delete km;
	return ret ? ret : status;
}

int iris_tzee::test(uint32_t num_input, char **buf)
{
	int ret, status;
	int response_len = sizeof(int32_t);

	ret = command_prepare();
	if (ret)
		return ret;

	mSeralizer->write_uint32(IRIS_CMD_TEST);
	mSeralizer->write_uint32(num_input);
	for (uint32_t i = 0; i < num_input; i++) {
		printf("Ken %d %s", i, buf[i]);
		mSeralizer->write_string((const unsigned char *)buf[i]);
	}
	ret = command_send(response_len);
	if (ret)
		return ret;

	ret = mDeseralizer->read_int32(status);
	if (ret)
		return ret;

	return ret ? ret : status;
}

int iris_tzee::init(bool tz_comm)
{
	int ret;
	uint8_t *key = NULL;
	uint32_t key_len;

	mTzComm = tz_comm;

	if (tz_comm) {
		ALOGD("Create TZ communication");
		mComm = new iris_tz_communicator();
		mFdMapper = NULL;
	} else {
		ALOGD("Create Socket communication");
		mComm = new iris_socket_communicator();
		mFdMapper = new iris_fd_socket_mapper();
	}

	if (!mComm || (!tz_comm && !mFdMapper)) {
		ALOGE("Create communication failed");
		return -ENOMEM;
	}

	ALOGD("Communication created");

	mSeralizer = new iris_msg_serializer();
	if (!mSeralizer) {
		ALOGE("Create msg serializer failed");
		return -ENOMEM;
	}

	ALOGD("Serializer created");

	mDeseralizer = new iris_msg_deserializer();
	if (!mDeseralizer) {
		ALOGE("Create msg deserializer failed");
		return -ENOMEM;
	}

	ALOGD("Deserializer created");

	ret = mComm->open("iris");
	if (ret) {
		ALOGE("Communication open failed");
		return ret;
	}

	ALOGD("Communication opened");

	if (mFdMapper)
		ret = mFdMapper->open();
	if (ret) {
		ALOGE("fail to open fd mapper");
		return ret;
	}

	if (mTzComm) {
		ret = get_key(&key, &key_len);
		if (ret) {
			ALOGE("fail to initialize TZ communication %x", ret);
			return ret;
		}

		ret = send_key(key, key_len);
		delete []key;
	}

	return ret;
}

void iris_tzee::deinit()
{
	if (mComm) {
		delete mComm;
		mComm = NULL;
	}

	if (mFdMapper) {
		delete mFdMapper;
		mFdMapper = NULL;
	}

	if (mSeralizer) {
		delete mSeralizer;
		mSeralizer = NULL;
	}

	if (mDeseralizer) {
		delete mDeseralizer;
		mDeseralizer = NULL;
	}
}

iris_interface *create_iris_tzee_obj(bool tz_comm)
{
	int ret;
	iris_tzee *tzee;

	tzee= new iris_tzee();
	if (!tzee)
		return NULL;

	ret = tzee->init(tz_comm);
	if (ret) {
		ALOGE("Iris_tzee init failed");
		delete tzee;
		return NULL;
	}

	return tzee;
}

