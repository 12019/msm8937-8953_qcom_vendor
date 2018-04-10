/*
 * Copyright (c) 2015 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */

#ifndef __IRIS_TZ_COMM_H__
#define __IRIS_TZ_COMM_H__

#include "iris_comm.h"

struct QSEECom_handle;

class iris_tz_communicator : public iris_comm_interface {
public:
	iris_tz_communicator();
	virtual ~iris_tz_communicator();

	int open(const char *ta);
	int close();

	int send(void const *req_buf, size_t req_buf_len, void *rsp_buf, size_t rsp_buf_len);
	int send_modified_cmd(void const *req_buf, size_t req_buf_len, void *rsp_buf, size_t rsp_buf_len, iris_ion_fd_info *fd_info);
private:
	struct QSEECom_handle *qseecom_handle;
};


#endif

