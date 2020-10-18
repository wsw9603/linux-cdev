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
	int ret = write(fd, "hello world", 12);
	printf("write result is %d\n", ret);

	getchar();
	printf("seek to the file start\n");
	lseek(fd, 0, SEEK_SET);
	char read_buf[100];
	ret = read(fd, read_buf, 12);
	printf("read result is \'%s\', len %d\n", read_buf, ret);

	getchar();
	printf("seek to the 6 bytes before\n");
	lseek(fd, -6, SEEK_CUR);
	ret = read(fd, read_buf, 12);
	printf("read result is \'%s\', len %d\n", read_buf, ret);

	getchar();
	close(fd);

	return 0;
}
