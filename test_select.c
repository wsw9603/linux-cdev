#include <stdio.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "cdev_module.h"

int main(int argc, const char *argv[])
{
	int fd;
	fd_set rfds, wfds;

	if (argc != 2) {
		printf("Usage: %s filename\n", argv[0]);
		return -1;
	}

	fd = open(fname, O_RDONLY | O_NONBLOCK);
	if (fd < 0) {
		printf("open file failed\n");
		return fd;
	}

	if (ioctl(fd, MEM_CLEAR) < 0) {
		printf("ioctl failed\n");
		return -1;
	}

	FD_ZERO(&rfds);
	FD_ZERO(&wfds);
	FD_SET(fd, &rfds);
	FD_SET(fd, &wfds);

	for (int i = 0; i < 50; i++) {
		int ret = select(fd + 1, &rfds, &wfds, NULL, NULL);
		printf("select result: %d\n", ret);
		if (FD_ISSET(fd, &rfds))
			printf("Poll monitor: ready to be read\n");
		if (FD_ISSET(fd, &wfds))
			printf("Poll monitor: ready to be written\n");
		sleep(1);
	}

	return 0;
}
