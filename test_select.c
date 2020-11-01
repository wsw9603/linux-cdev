#include <stdio.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "cdev_module.h"

#define READ_BUF_LENGTH 100
#define MAX_NAME_LENGTH 100
#define FILES_TO_MONITOR 3
#define FILE_BASENAME "/dev/globalfifo"

int main(int argc, const char *argv[])
{
	int max_fd = 0, fd;
	int fds[FILES_TO_MONITOR];
	fd_set rfds, wfds;
	char name_buf[MAX_NAME_LENGTH];

	FD_ZERO(&rfds);
	FD_ZERO(&wfds);
	for (int i = 0; i < FILES_TO_MONITOR; i++) {
		sprintf(name_buf, "%s%d", FILE_BASENAME, i + 1);
		fd = open(name_buf, O_RDONLY | O_NONBLOCK);
		if (fd < 0) {
			printf("open file '%s' failed, %d\n", name_buf, fd);
			return fd;
		}

		if (ioctl(fd, MEM_CLEAR) < 0) {
			printf("ioctl failed\n");
			return -1;
		}

		max_fd = max_fd > fd ? max_fd : fd;
		FD_SET(fd, &rfds);
		FD_SET(fd, &wfds);
		fds[i] = fd;
	}

	for (int i = 0; i < 50; i++) {
		int length;
		char read_buf[READ_BUF_LENGTH];
		int ret = select(max_fd + 1, &rfds, &wfds, NULL, NULL);
		printf("select result: %d\n", ret);

		for (int j = 0; j < FILES_TO_MONITOR; j++) {
			fd = fds[j];

			if (FD_ISSET(fd, &rfds)) {
				printf("file%d ready to be read\n", j + 1);
				length = read(fd, read_buf, READ_BUF_LENGTH);
				if (length < 0)
					printf("read file failed, %d\n", length);
				else
					printf("read reault: %s, length: %d\n",
							read_buf, length);
			} else
				FD_SET(fd, &rfds); /* 需要重新设置fd */

			if (FD_ISSET(fd, &wfds))
				printf("file%d ready to be written\n", j + 1);
			else
				FD_SET(fd, &wfds);
		}

		sleep(1);
	}

	return 0;
}
