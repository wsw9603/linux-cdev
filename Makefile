CURRENT_DIR := $(CURDIR)
KERNEL_DIR := ~/qemu-lab/build-kernel
export ARCH=arm64
export CROSS_COMPILE=aarch64-linux-gnu-

# 调用内核的Makefile来编译ko，不需要自己处理依赖关系，但是需要确保每次都能执行
cdev_module.ko: FORCE
	make -C $(KERNEL_DIR) M=$(CURRENT_DIR) modules
# 普通二进制文件是否也可以借用内核的Makefile来编译？
test_file: test.c
	$(CROSS_COMPILE)gcc -static -o $@ $^

.PHONY: install test clean help FORCE
install: cdev_module.ko test_file
	cp $^ ~/qemu-lab/rootfs/modules/
	~/qemu-lab/scripts/mkrootfs.sh
test:
	~/qemu-lab/scripts/start_qemu.sh
clean:
	make -C $(KERNEL_DIR) M=$(CURRENT_DIR) clean
	rm -f test_file
help:
	make -C $(KERNEL_DIR) M=$(CURRENT_DIR) help
FORCE: ;
