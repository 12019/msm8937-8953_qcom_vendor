/*
 * Copyright (c) 2015 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */

#ifndef __IRIS_FD_SOCKET_MAPPER_H__
#define __IRIS_FD_SOCKET_MAPPER_H__

#include "iris_fd_mapper.h"

class iris_fd_socket_mapper : public iris_fd_mapper {
public:
	iris_fd_socket_mapper();
	virtual ~iris_fd_socket_mapper();

	virtual int open();
	virtual void close();
	virtual int map(int fd);

private:
	int mFd;
};

#endif

