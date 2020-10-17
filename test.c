#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

int main(int argc, const char *argv[])
{
	if (argc != 2) {
		printf("Usage: %s /dev/devfile\n", argv[0]);
		return -1;
	}

	int fd = open(argv[1], O_RDWR);
	if (fd < 0) {
		printf("open file \'%s\' failed\n", argv[1]);
		return -1;
	}

	getchar();
	int ret = read(fd, 0x321, 0);
	printf("read result is %d\n", ret);

	getchar();
	ret = write(fd, 0x321, 0);
	printf("write result is %d\n", ret);

	getchar();

	close(fd);

	return 0;
}
