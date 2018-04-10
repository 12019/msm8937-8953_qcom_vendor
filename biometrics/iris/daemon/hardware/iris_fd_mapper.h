/*
 * Copyright (c) 2015 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */

#ifndef __IRIS_FD_MAPPER_H__
#define __IRIS_FD_MAPPER_H__

#include <stdint.h>

class iris_fd_mapper {
public:
	virtual ~iris_fd_mapper() {};
	virtual int open() = 0;
	virtual void close() = 0;
	virtual int map(int fd) = 0;
};

#endif

