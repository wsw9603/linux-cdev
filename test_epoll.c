#include <stdio.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/epoll.h>

#include "cdev_module.h"

#define READ_BUF_LENGTH 100
#define MAX_NAME_LENGTH 100
#define FILES_TO_MONITOR 3
#define FILE_BASENAME "/dev/globalfifo"

int main(int argc, const char *argv[])
{
	int fd, epfd;
	int fds[FILES_TO_MONITOR];
	char name_buf[MAX_NAME_LENGTH];
	struct epoll_event event, events[FILES_TO_MONITOR];

	epfd = epoll_create(FILES_TO_MONITOR);
	if (epfd < 0) {
		printf("epoll_create failed, %d\n");
		return -1;
	} else
		printf("create epoll successfully, fd: %d\n", epfd);
	event.events = EPOLLIN | EPOLLOUT;

	for (int i = 0; i < FILES_TO_MONITOR; i++) {
		sprintf(name_buf, "%s%d", FILE_BASENAME, i + 1);
		fd = open(name_buf, O_RDONLY | O_NONBLOCK);
		if (fd < 0) {
			printf("open file '%s' failed, %d\n", name_buf, fd);
			return fd;
		} else
			printf("open file '%s' successfully, fd: %d\n",
				name_buf, fd);

		if (ioctl(fd, MEM_CLEAR) < 0) {
			printf("ioctl failed\n");
			return -1;
		}

		event.data.fd = fd;
		if (epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &event) < 0) {
			printf("add fd %d to epfd %d failed\n", fd, epfd);
			return -1;
		}

		fds[i] = fd;
	}

	for (int i = 0; i < 50; i++) {
		int length;
		char read_buf[READ_BUF_LENGTH];
		int ret = epoll_wait(epfd, events, FILES_TO_MONITOR, -1);
		if (ret < 0) {
			printf("epoll_wait failed, %d\n", ret);
			return -1;
		}
		printf("epoll_wait result: %d\n", ret);

		for (int j = 0; j < ret; j++) {
			fd = events[j].data.fd;
			if (events[j].events & EPOLLIN) {
				printf("file %d ready to be read\n", fd);
				length = read(fd, read_buf, READ_BUF_LENGTH);
				if (length < 0)
					printf("read file failed, %d\n", length);
				else
					printf("read reault: %s, length: %d\n",
						read_buf, length);
			}

			if (events[j].events & EPOLLOUT)
				printf("file %d ready to be written\n", fd);
		}

		sleep(1);
	}

	close(epfd);

	return 0;
}
