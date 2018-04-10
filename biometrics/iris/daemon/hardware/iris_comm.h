/*
 * Copyright (c) 2015 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */

#ifndef __IRIS_COMM_H__
#define __IRIS_COMM_H__

#include <stdint.h>

#define IRIS_MAX_TZ_BUFFER_LEN 0x400

typedef struct {
	int32_t fd;
	uint32_t cmd_buf_offset;
} iris_ion_fd_info;

class iris_comm_interface {
public:
	virtual ~iris_comm_interface() {};
	virtual int open(const char *ta) = 0;
	virtual int close() = 0;
	virtual int send(void const *req_buf, size_t req_buf_len, void *rsp_buf, size_t rsp_buf_len) = 0;
	virtual int send_modified_cmd(void const *req_buf, size_t req_buf_len, void *rsp_buf, size_t rsp_buf_len, iris_ion_fd_info *fd_info) = 0;
};

#endif

