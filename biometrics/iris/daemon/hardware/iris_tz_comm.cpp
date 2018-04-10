/*
 * Copyright (c) 2015 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */

#define LOG_TAG "IrisTzComm"
#include <utils/Log.h>

#include <errno.h>
#include <string.h>

#include "iris_tz_comm.h"
#include "QSEEComAPI.h"

static char const* const IRIS_TZ_DIR   = "/firmware/image";

/* iris_tz_communicator */
iris_tz_communicator::iris_tz_communicator()
	:qseecom_handle(NULL)
{
}

iris_tz_communicator::~iris_tz_communicator()
{
	close();
}

int iris_tz_communicator::open(const char *ta)
{
	int ret = 0;

	if (qseecom_handle)
		return ret;

	ret = QSEECom_start_app(&qseecom_handle, IRIS_TZ_DIR, ta, IRIS_MAX_TZ_BUFFER_LEN);
	if (ret)
		return ret;

	ret = QSEECom_set_bandwidth(qseecom_handle, true);

	return ret;
}

int iris_tz_communicator::close()
{
	int ret = 0;

	if (!qseecom_handle)
		return 0;

	QSEECom_set_bandwidth(qseecom_handle, false);
	ret = QSEECom_shutdown_app(&qseecom_handle);
	qseecom_handle = NULL;
	ALOGD("Shut down TZ app %d", ret);
	return ret;
}

int iris_tz_communicator::send(void const *req_buf, size_t req_buf_len, void *rsp_buf, size_t rsp_buf_len)
{
	int ret;
	uint8_t* shared_buf_req, *shared_buf_rsp;;

	if (!qseecom_handle)
		return -EINVAL;


	shared_buf_req = qseecom_handle->ion_sbuffer;
	shared_buf_rsp = qseecom_handle->ion_sbuffer;

	// align buffers
	if (req_buf_len & QSEECOM_ALIGN_MASK) {
	  req_buf_len = QSEECOM_ALIGN(req_buf_len);
	}
	if (rsp_buf_len & QSEECOM_ALIGN_MASK) {
	  rsp_buf_len = QSEECOM_ALIGN(rsp_buf_len);
	}

	memcpy(shared_buf_req, req_buf, req_buf_len);

	ret = QSEECom_send_cmd(qseecom_handle, shared_buf_req, req_buf_len, shared_buf_rsp, rsp_buf_len);
	if (!ret)
		memcpy(rsp_buf, shared_buf_rsp, rsp_buf_len);

	return ret;
}

int iris_tz_communicator::send_modified_cmd(void const *req_buf, size_t req_buf_len, void *rsp_buf, size_t rsp_buf_len, iris_ion_fd_info *fd_info)
{
	int ret;
	uint8_t* shared_buf_req, *shared_buf_rsp;;
	struct QSEECom_ion_fd_info ion_fd_info;

	if (!qseecom_handle)
		return -EINVAL;


	shared_buf_req = qseecom_handle->ion_sbuffer;
	shared_buf_rsp = qseecom_handle->ion_sbuffer;

	// align buffers
	if (req_buf_len & QSEECOM_ALIGN_MASK) {
	  req_buf_len = QSEECOM_ALIGN(req_buf_len);
	}
	if (rsp_buf_len & QSEECOM_ALIGN_MASK) {
	  rsp_buf_len = QSEECOM_ALIGN(rsp_buf_len);
	}

	memcpy(shared_buf_req, req_buf, req_buf_len);

	memset(&ion_fd_info, 0, sizeof(ion_fd_info));
	ion_fd_info.data[0].fd = fd_info->fd;
	ion_fd_info.data[0].cmd_buf_offset = fd_info->cmd_buf_offset;
	ALOGD("Send modified command: fd %d offset 0x%x, req_buf_len %d rsp_buf_len %d", fd_info->fd, fd_info->cmd_buf_offset, req_buf_len, rsp_buf_len);

	ret = QSEECom_send_modified_cmd(qseecom_handle, shared_buf_req, req_buf_len, shared_buf_rsp, rsp_buf_len, &ion_fd_info);
	if (!ret)
		memcpy(rsp_buf, shared_buf_rsp, rsp_buf_len);
	else
		ALOGD("Send modified command failed");

	return ret;
}

