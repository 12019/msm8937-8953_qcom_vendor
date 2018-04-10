/*
 * Copyright (c) 2015 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */

#ifndef __IRIS_SOCKET_COMM_H__
#define __IRIS_SOCKET_COMM_H__

#include "iris_comm.h"

class iris_socket_communicator : public iris_comm_interface {
public:
	iris_socket_communicator();
	virtual ~iris_socket_communicator();

	virtual int open(const char *ta);
	virtual int close();
	virtual int send(void const *req_buf, size_t req_buf_len, void *rsp_buf, size_t rsp_buf_len);
	virtual int send_modified_cmd(void const *req_buf, size_t req_buf_len, void *rsp_buf, size_t rsp_buf_len, iris_ion_fd_info *fd_info);
private:
	int mFd;
};


#endif

