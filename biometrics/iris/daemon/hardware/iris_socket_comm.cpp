/*
 * Copyright (c) 2015 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */

#include <errno.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "iris_socket_comm.h"

const char *iris_socket_path = "/dev/socket/iris_tz_sim";

/* iris_socket_communicator */
iris_socket_communicator::iris_socket_communicator()
	:mFd(-1)
{
}

iris_socket_communicator::~iris_socket_communicator()
{
	close();
}

int iris_socket_communicator::open(const char *ta)
{
	struct sockaddr_un addr;
	int ret;

	mFd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (mFd < 0)
		return -EIO;

	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, iris_socket_path, sizeof(addr.sun_path)-1);

	ret = connect(mFd, (struct sockaddr*)&addr, sizeof(addr));
	if (ret < 0)
		return -EIO;

	return ret;
}

int iris_socket_communicator::close()
{
	if (mFd >= 0)
		::close(mFd);

	mFd = -1;
	return 0;
}

int iris_socket_communicator::send(void const *req_buf, size_t req_buf_len, void *rsp_buf, size_t rsp_buf_len)
{
	size_t len;

	len = write(mFd, req_buf, IRIS_MAX_TZ_BUFFER_LEN);

	if (len < req_buf_len)
		return -EIO;


	len = read(mFd, rsp_buf, IRIS_MAX_TZ_BUFFER_LEN);

	if (len > 0)
		return 0;

	return -EIO;
}

int iris_socket_communicator::send_modified_cmd(void const *req_buf, size_t req_buf_len, void *rsp_buf, size_t rsp_buf_len, iris_ion_fd_info *fd_info)
{
	return send(req_buf, req_buf_len, rsp_buf, rsp_buf_len);
}
