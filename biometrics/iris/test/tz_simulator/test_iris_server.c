/*
 * Copyright (c) 2015 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */

#include <stdio.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <cutils/sockets.h>

#define IRIS_MAX_BUFFER_LEN 0x400

#define iris_socket_path "iris_tz_sim"
#define iris_mapper_path "iris_fd_mapper"

static char buf[IRIS_MAX_BUFFER_LEN];
static char resp_buf[IRIS_MAX_BUFFER_LEN];

void tz_app_init(void);
void tz_app_cmd_handler(void* request, uint32_t req_length, void* response, uint32_t rsp_length);
void tz_app_shutdown(void);

#if 0
int create_listen_socket(const char *address)
{
	struct sockaddr_un addr;
	int fd = -1;
	size_t len;
	int ret;

	fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (fd < 0) {
		printf("socket error");
		return fd;
	}

	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, address, strlen(address));

	unlink(address);

	ret = bind(fd, (struct sockaddr*)&addr, sizeof(addr));
	if (ret < 0) {
		perror("bind error");
		close(fd);
		return ret;
	}

	return fd;
}
#else
int create_listen_socket(const char *name)
{
	int fd = android_get_control_socket(name);
	return fd;
}
#endif

int do_map(int socket)
{
	struct msghdr message;
	struct iovec iov[1];
	struct cmsghdr *control_message = NULL;
	char ctrl_buf[CMSG_SPACE(sizeof(int))];
	int data;
	int ret, mappedFd = -1;

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

	ret = recvmsg(socket, &message, 0);
	if (ret <= 0)
		return ret;

	control_message = CMSG_FIRSTHDR(&message);
	if( control_message &&
		(control_message->cmsg_level == SOL_SOCKET) &&
	     (control_message->cmsg_type == SCM_RIGHTS)) {
	     mappedFd = *((int *) CMSG_DATA(control_message));
		 printf("mappedFd = %d\n", mappedFd);
	} else {
		return -EINVAL;
	}

	message.msg_control = NULL;
	message.msg_controllen = 0;
	data = mappedFd;
	iov[0].iov_base = &data;
	iov[0].iov_len = sizeof(data);

	ret = sendmsg(socket, &message, 0);
	if (ret <= 0) {
		printf("fail to send mapper message\n");
		return ret;
	}

	return ret;
}

void *iris_mapper_main(void *arg)
{
	int fd, conn_fd;
	int *stop = (int *)arg;
	int ret;

	fd = create_listen_socket(iris_mapper_path);
	if (fd < 0) {
		perror("fail to create mapper socket\n");
		return NULL;
	}

	while (*stop != 1) {
		printf("listening for new mapper connection\n");
		ret = listen(fd, 1);
		if (ret < 0) {
			printf("mapper listen error");
			goto map_listen_error;
		}

		conn_fd = accept(fd, NULL, NULL);
		if (conn_fd < 0) {
			printf("mapper accept error");
			goto map_listen_error;
		}
	
		printf("got a mapper connection\n");
		while (*stop != 1) {
			ret = do_map(conn_fd);
			if (ret <= 0) {
				printf("client exited!\n");
				break;
			}
		}

		close(conn_fd);
	}
	
map_listen_error:
	close(fd);
	return NULL;
}

int create_mapper_thread(pthread_t *thread, int *stop)
{
	int ret;

	ret = pthread_create(thread, NULL, iris_mapper_main, stop);
	return ret;
}

void stop_mapper_thread(pthread_t thread, int *stop)
{
	void *ret;
	*stop = 1;
	pthread_join(thread, &ret);
}

int main(int argc, char *argv[])
{
	int fd, conn_fd;
	pthread_t mapper_thread;
	int ret, len, stop = 0;

	ret = create_mapper_thread(&mapper_thread, &stop);
	if (ret) {
		perror("fail to create mapper thread\n");
		return ret;
	}

	fd = create_listen_socket(iris_socket_path);
	if (fd < 0) {
		perror("fail to create iris socket\n");
		ret = fd;
		goto bind_error;
	}

	while (1) {
		printf("listening for new connection\n");
		ret = listen(fd, 1);
		if (ret < 0) {
			printf("listen error");
			goto listen_error;
		}


		conn_fd = accept(fd, NULL, NULL);
		if (conn_fd < 0) {
			printf("accept error");
			goto listen_error;
		}

		printf("got a connection\n");
	 	tz_app_init();
		while (1) {
			len = read(conn_fd, buf, IRIS_MAX_BUFFER_LEN);
			if (len == 0) {
				printf("client exited!\n");
				break;
			}

			tz_app_cmd_handler(buf, len, resp_buf, IRIS_MAX_BUFFER_LEN);
			len = write(conn_fd, resp_buf, IRIS_MAX_BUFFER_LEN);
			if (len == 0) {
				printf("write failed!\n");
				ret = -EIO;
				break;
			}
		}

		tz_app_shutdown();

		close(conn_fd);
	}
listen_error:
	close(fd);
bind_error:
	stop_mapper_thread(mapper_thread, &stop);
	return ret;
}

