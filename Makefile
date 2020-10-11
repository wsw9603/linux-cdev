ifeq ($(KERNELRELEASE),)

CUR := $(CURDIR)
KERNEL_DIR := ~/qemu-lab/build-kernel
export ARCH=arm64
export CROSS_COMPILE=aarch64-linux-gnu-

default:
	make -C $(KERNEL_DIR) M=$(CUR) modules
clean:
	make -C $(KERNEL_DIR) M=$(CUR) clean
help:
	make -C $(KERNEL_DIR) M=$(CUR) help

install: cdev_module.ko
	cp $^ ~/qemu-lab/rootfs/modules/
	~/qemu-lab/scripts/mkrootfs.sh
test:
	~/qemu-lab/scripts/start_qemu.sh

else

obj-m += cdev_module.o

endif
