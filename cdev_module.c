#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/poll.h>

#include "cdev_module.h"

#define DEVNAME "globalfifo"
#define GLOBALMEM_SIZE 0x1000

#define wws_pr_info(fmt, ...)	\
	printk(KERN_INFO "[func: %s, line: %d] " fmt "\n",\
			__func__, __LINE__, ##__VA_ARGS__)

static int major = 0;
module_param(major, int, S_IRUGO);
static int minor = 1;
static int count = 3; /* 驱动所创建的设备数目 */
module_param(count, int, S_IRUGO);

struct globalfifo_dev {
	/* 表示fifo中写入数据的大小，数据一定是从0位置开始写入到buf中的 */
	size_t length;
	struct cdev cdev;
	char buf[GLOBALMEM_SIZE];
	struct mutex mutex;

	/* 读写操作的等待队列 */
	wait_queue_head_t r_queue;
	wait_queue_head_t w_queue;
};

static struct class *pclass;
static struct globalfifo_dev *pglobalfifo_dev;

static int globalfifo_open(struct inode *inode, struct file *filp)
{
	wws_pr_info("file opened by, (%s: pid = %d)",
		    current->comm, current->pid);
	wws_pr_info("major: %d, minor: %d", imajor(inode), iminor(inode));

	filp->private_data = container_of(inode->i_cdev,
					  struct globalfifo_dev, cdev);

	return 0;
}

static int globalfifo_release(struct inode *inode, struct file *filp)
{
	wws_pr_info("file release by, (%s: pid = %d)",
		    current->comm, current->pid);
	wws_pr_info("major: %d, minor: %d", imajor(inode), iminor(inode));

	return 0;
}

static ssize_t globalfifo_read(struct file *filp, char __user *buf,
			      size_t size, loff_t *offset)
{
	struct globalfifo_dev *pdev = filp->private_data;
	struct inode *inode = filp->f_path.dentry->d_inode;

	/* QUES: 上来就将当前进程添加到等待队列，这样好吗? */
	DECLARE_WAITQUEUE(r_entry, current);
	/* QUES: add 和remove操作都没有加锁保护，会不会有竞争情况? */
	add_wait_queue(&pdev->r_queue, &r_entry);

	wws_pr_info("file read by, (%s: pid = %d)",
		    current->comm, current->pid);
	wws_pr_info("major: %d, minor: %d", imajor(inode), iminor(inode));

	mutex_lock(&pdev->mutex);

	/* NOTE: 此处应该用循环检查，而不是用if只判断一次 */
	while (!pdev->length) {
		if (filp->f_flags & O_NONBLOCK) {
			/* 非阻塞方式打开文件，立即返回 */
			size =  -EAGAIN;
			goto out_locked;
		}

		__set_current_state(TASK_INTERRUPTIBLE);
		/* QUES: unlock操作是否可以提前? */
		mutex_unlock(&pdev->mutex);
		schedule();
		if (signal_pending(current)) {
			size = -ERESTARTSYS;
			goto out_unlocked;
		}

		mutex_lock(&pdev->mutex);
	}

	size = size > pdev->length ? pdev->length : size;

	if (copy_to_user(buf, pdev->buf, size))
		size = -EFAULT;
	else {
		pdev->length -= size;
		memcpy(pdev->buf, pdev->buf + size, pdev->length);
		wake_up_interruptible(&pdev->w_queue);

		wws_pr_info("read file successfully, size %zu", size);
	}

out_locked:
	mutex_unlock(&pdev->mutex);
out_unlocked:
	remove_wait_queue(&pdev->r_queue, &r_entry);

	return size;
}

static ssize_t globalfifo_write(struct file *filp, const char __user *buf,
			  size_t size, loff_t *offset)
{
	struct globalfifo_dev *pdev = filp->private_data;
	struct inode *inode = filp->f_path.dentry->d_inode;

	DECLARE_WAITQUEUE(w_entry, current);
	add_wait_queue(&pdev->w_queue, &w_entry);

	wws_pr_info("file write by, (%s: pid = %d)",
		    current->comm, current->pid);
	wws_pr_info("major: %d, minor: %d", imajor(inode), iminor(inode));

	mutex_lock(&pdev->mutex);

	while (pdev->length == GLOBALMEM_SIZE) {
		if (filp->f_flags & O_NONBLOCK) {
			size = -EAGAIN;
			goto out_locked;
		}

		__set_current_state(TASK_INTERRUPTIBLE);
		mutex_unlock(&pdev->mutex);
		schedule();

		if (signal_pending(current)) {
			size = -ERESTARTSYS;
			goto out_unlocked;
		}

		mutex_lock(&pdev->mutex);
	}

	if (size > GLOBALMEM_SIZE - pdev->length)
		size = GLOBALMEM_SIZE - pdev->length;

	if (copy_from_user(pdev->buf + pdev->length, buf, size)) {
		size = -EFAULT;
		goto out_locked;
	}

	pdev->length += size;
	wake_up_interruptible(&pdev->r_queue);

	wws_pr_info("write to file successfully, size %zu", size);

out_locked:
	mutex_unlock(&pdev->mutex);
out_unlocked:
	remove_wait_queue(&pdev->w_queue, &w_entry);

	return size;
}

static __poll_t globalfifo_poll(struct file *filp,
			struct poll_table_struct *wait)
{
	__poll_t mask = 0;
	struct globalfifo_dev *pdev = filp->private_data;

	mutex_lock(&pdev->mutex);

	poll_wait(filp, &pdev->r_queue, wait);
	poll_wait(filp, &pdev->w_queue, wait);

	/*
	 * NOTE:此处的IN/OUT是对于用户态程序来说的，可读即IN，可写即OUT
	 * 对于设备来说的话这一点正好相反，用户的读是设备的out，容易混淆
	 */
	if (pdev->length != 0)
		mask |= POLLIN | POLLRDNORM; /* 两个标志位是等价的 */

	if (pdev->length != GLOBALMEM_SIZE)
		mask |= POLLOUT | POLLWRNORM; /* 两个标志位是等价的 */

	mutex_unlock(&pdev->mutex);

	return mask;
}

static loff_t globalfifo_llseek(struct file *filp, loff_t offset, int orig)
{
	switch (orig) {
	case SEEK_SET:
		if (offset < 0 || offset >= GLOBALMEM_SIZE)
			return -EINVAL;
		filp->f_pos = offset;
		return offset;
	case SEEK_CUR:
		if (filp->f_pos + offset < 0 ||
		    filp->f_pos + offset >= GLOBALMEM_SIZE)
			return -EINVAL;
		filp->f_pos += offset;
		return filp->f_pos;
	default:
		return -EINVAL;
	}
}

static long globalfifo_ioctl(struct file *filp, unsigned int cmd,
			   unsigned long arg)
{
	struct globalfifo_dev *pdev = filp->private_data;

	switch (cmd) {
	case MEM_CLEAR:
		mutex_lock(&pdev->mutex);
		memset(pdev->buf, 0, GLOBALMEM_SIZE);
		mutex_unlock(&pdev->mutex);

		wws_pr_info("clear buffer to zero");
		return 0;
	default:
		return -EINVAL;
	}
}

static struct file_operations fops = {
	.owner		= THIS_MODULE,
	.open		= globalfifo_open,
	.release	= globalfifo_release,
	.read		= globalfifo_read,
	.write		= globalfifo_write,
	.llseek		= globalfifo_llseek,
	.unlocked_ioctl	= globalfifo_ioctl,
	.poll		= globalfifo_poll,
};

static int __init globalfifo_setup(int idx)
{
	int ret;
	struct device *pdevice;
	dev_t devnum = MKDEV(major, minor + idx);
	struct globalfifo_dev *pdev = pglobalfifo_dev + idx;

	cdev_init(&pdev->cdev, &fops);

	ret = cdev_add(&pdev->cdev, devnum, 1);
	if (ret) {
		wws_pr_info("register cdev failed");
		return ret;
	}

	pdevice = device_create(pclass, NULL, devnum, NULL,
				"%s%d", DEVNAME, idx + minor);
	if (IS_ERR(pdevice)) {
		wws_pr_info("create device %d failed", idx);
		ret = PTR_ERR(pdevice);
		goto error_out;
	}

	mutex_init(&pdev->mutex);
	init_waitqueue_head(&pdev->r_queue);
	init_waitqueue_head(&pdev->w_queue);

	return 0;

error_out:
	cdev_del(&pdev->cdev);
	return ret;
}

static void globalfifo_reset(int idx)
{
	struct globalfifo_dev *pdev = pglobalfifo_dev + idx;

	device_destroy(pclass, MKDEV(major, minor + idx));
	cdev_del(&pdev->cdev);
	mutex_destroy(&pdev->mutex);
}

static int __init globalfifo_init(void)
{
	dev_t devnum = MKDEV(major, minor);
	int ret, i;

	wws_pr_info("init demo: (%s: pid=%d)", current->comm, current->pid);

	/*
	 * 注册设备号，创建class，创建设备文件和注册设备之间的时序关系是怎样
	 * 的？代码中的这种流程是否有问题？
	 */
	if (major)
		ret = register_chrdev_region(devnum, count, DEVNAME);
	else
		ret = alloc_chrdev_region(&devnum, minor, count, DEVNAME);
	if (ret) {
		wws_pr_info("register/alloc cdev number failed");
		return -EFAULT;
	}
	major = MAJOR(devnum);

	pglobalfifo_dev = kzalloc(sizeof(*pglobalfifo_dev) * count, GFP_KERNEL);
	if (!pglobalfifo_dev) {
		ret = -ENOMEM;
		goto error_reg;
	}

	pclass = class_create(THIS_MODULE, DEVNAME);
	if (IS_ERR(pclass)) {
		wws_pr_info("create device class failed");
		ret = PTR_ERR(pclass);
		goto error_alloc;
	}

	for (i = 0; i < count; i++) {
		ret = globalfifo_setup(i);
		if (ret < 0)
			goto error_setup;
	}
	wws_pr_info("register cdev successfully, major: %d", major);
	return 0;

error_setup:
	for (i--; i >= 0; i--)
		globalfifo_reset(i);
	class_destroy(pclass);
error_alloc:
	cdev_del(&pglobalfifo_dev->cdev);
error_reg:
	unregister_chrdev_region(devnum, count);

	return ret;
}

static void __exit globalfifo_exit(void)
{
	int i;

	wws_pr_info("exit demo: (%s: pid=%d)", current->comm, current->pid);

	for (i = 0; i < count; i++)
		globalfifo_reset(i);
	class_destroy(pclass);

	unregister_chrdev_region(MKDEV(major, minor), count);
	kfree(pglobalfifo_dev);
}

module_init(globalfifo_init);
module_exit(globalfifo_exit);

MODULE_LICENSE("GPL v2");
