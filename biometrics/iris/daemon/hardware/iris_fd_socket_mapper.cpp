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

#include "iris_fd_socket_mapper.h"

const char *iris_mapper_path = "/dev/socket/iris_fd_mapper";

iris_fd_socket_mapper::iris_fd_socket_mapper()
	:mFd(-1)
{
}

iris_fd_socket_mapper::~iris_fd_socket_mapper()
{
	close();
}

int iris_fd_socket_mapper::open()
{
	struct sockaddr_un addr;
	int ret;
	
	mFd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (mFd < 0)
		return -EIO;
	
	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, iris_mapper_path, sizeof(addr.sun_path)-1);
	
	ret = connect(mFd, (struct sockaddr*)&addr, sizeof(addr));
	if (ret < 0)
		return -EIO;
	
	return ret;
}

void iris_fd_socket_mapper::close()
{
	if (mFd >= 0)
		::close(mFd);

	mFd = -1;
}

int iris_fd_socket_mapper::map(int fd)
{
	struct msghdr message;
	struct iovec iov[1];
	struct cmsghdr *control_message = NULL;
	char ctrl_buf[CMSG_SPACE(sizeof(int))];
	int data;
	int ret;

	memset(&message, 0, sizeof(struct msghdr));
	memset(ctrl_buf, 0, CMSG_SPACE(sizeof(int)));

	iov[0].iov_base = &data;
	iov[0].iov_len = sizeof(data);

	message.msg_name = NULL;
	message.msg_namelen = 0;
	message.msg_control = ctrl_buf;
	message.msg_controllen = CMSG_SPACE(sizeof(int));
	message.msg_iov = iov;
	message.msg_iovlen = 1;

	control_message = CMSG_FIRSTHDR(&message);
	control_message->cmsg_level = SOL_SOCKET;
	control_message->cmsg_type = SCM_RIGHTS;
	control_message->cmsg_len = CMSG_LEN(sizeof(int));

	*((int *) CMSG_DATA(control_message)) = fd;

	ret = sendmsg(mFd, &message, 0);
	if (ret < 0) {
		printf("fail to send message\n");
		return ret;
	}

	message.msg_control = NULL;
	message.msg_controllen = 0;
	ret = recvmsg(mFd, &message, 0);
	if (ret <= 0) {
		printf("fail to receive message\n");
		return ret;
	}

	return data;
}

