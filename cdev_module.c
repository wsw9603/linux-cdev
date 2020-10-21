#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/slab.h>

#include "cdev_module.h"

#define DEVNAME "globalmem"
#define GLOBALMEM_SIZE 0x1000

#define wws_pr_info(fmt, ...)	\
	printk(KERN_INFO "[func: %s, line: %d] " fmt "\n",\
			__func__, __LINE__, ##__VA_ARGS__)

static int major = 0;
module_param(major, int, S_IRUGO);
static int minor = 1;
static int count = 3; /* 驱动所创建的设备数目 */
module_param(count, int, S_IRUGO);

struct globalmem_dev {
	struct cdev cdev;
	char buf[GLOBALMEM_SIZE];
	struct mutex mutex;
};

static struct class *pclass;
static struct globalmem_dev *pglobalmem_dev;

static int globalmem_open(struct inode *inode, struct file *filp)
{
	wws_pr_info("file opened by, (%s: pid = %d)",
		    current->comm, current->pid);
	wws_pr_info("major: %d, minor: %d", imajor(inode), iminor(inode));

	filp->private_data = container_of(inode->i_cdev,
					  struct globalmem_dev, cdev);

	return 0;
}

static int globalmem_release(struct inode *inode, struct file *filp)
{
	wws_pr_info("file release by, (%s: pid = %d)",
		    current->comm, current->pid);
	wws_pr_info("major: %d, minor: %d", imajor(inode), iminor(inode));

	return 0;
}

static ssize_t globalmem_read(struct file *filp, char __user *buf,
			      size_t size, loff_t *offset)
{
	struct globalmem_dev *pdev = filp->private_data;
	struct inode *inode = filp->f_path.dentry->d_inode;

	wws_pr_info("file read by, (%s: pid = %d)",
		    current->comm, current->pid);
	wws_pr_info("major: %d, minor: %d", imajor(inode), iminor(inode));

	if (*offset >= GLOBALMEM_SIZE)
		return 0; /* 返回0表示读到文件结尾，不要返回错误 */

	if (size > GLOBALMEM_SIZE - *offset)
		size = GLOBALMEM_SIZE - *offset;

	mutex_lock(&pdev->mutex);
	if (copy_to_user(buf, pdev->buf + *offset, size))
		size = -EFAULT;
	else
		*offset += size;
	mutex_unlock(&pdev->mutex);

	wws_pr_info("read file successfully, size %zu", size);

	return size;
}

static ssize_t globalmem_write(struct file *filp, const char __user *buf,
			  size_t size, loff_t *offset)
{
	struct globalmem_dev *pdev = filp->private_data;
	struct inode *inode = filp->f_path.dentry->d_inode;

	wws_pr_info("file write by, (%s: pid = %d)",
		    current->comm, current->pid);
	wws_pr_info("major: %d, minor: %d", imajor(inode), iminor(inode));

	if (*offset >= GLOBALMEM_SIZE)
		return 0;
	if (size > GLOBALMEM_SIZE - *offset)
		size = GLOBALMEM_SIZE - *offset;

	mutex_lock(&pdev->mutex);
	if (copy_from_user(pdev->buf + *offset, buf, size))
		size = -EFAULT;
	else
		*offset += size;
	mutex_unlock(&pdev->mutex);

	wws_pr_info("write to file successfully, size %zu", size);

	return size;
}

static loff_t globalmem_llseek(struct file *filp, loff_t offset, int orig)
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

static long globalmem_ioctl(struct file *filp, unsigned int cmd,
			   unsigned long arg)
{
	struct globalmem_dev *pdev = filp->private_data;

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
	.open		= globalmem_open,
	.release	= globalmem_release,
	.read		= globalmem_read,
	.write		= globalmem_write,
	.llseek		= globalmem_llseek,
	.unlocked_ioctl	= globalmem_ioctl,
};

static int __init globalmem_setup(int idx)
{
	int ret;
	struct device *pdevice;
	dev_t devnum = MKDEV(major, minor + idx);
	struct globalmem_dev *pdev = pglobalmem_dev + idx;

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

	return 0;

error_out:
	cdev_del(&pdev->cdev);
	return ret;
}

static void globalmem_reset(int idx)
{
	struct globalmem_dev *pdev = pglobalmem_dev + idx;

	device_destroy(pclass, MKDEV(major, minor + idx));
	cdev_del(&pdev->cdev);
	mutex_destroy(&pdev->mutex);
}

static int __init globalmem_init(void)
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

	pglobalmem_dev = kzalloc(sizeof(*pglobalmem_dev) * count, GFP_KERNEL);
	if (!pglobalmem_dev) {
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
		ret = globalmem_setup(i);
		if (ret < 0)
			goto error_setup;
	}
	wws_pr_info("register cdev successfully, major: %d", major);
	return 0;

error_setup:
	for (i--; i >= 0; i--)
		globalmem_reset(i);
	class_destroy(pclass);
error_alloc:
	cdev_del(&pglobalmem_dev->cdev);
error_reg:
	unregister_chrdev_region(devnum, count);

	return ret;
}

static void __exit globalmem_exit(void)
{
	int i;

	wws_pr_info("exit demo: (%s: pid=%d)", current->comm, current->pid);

	for (i = 0; i < count; i++)
		globalmem_reset(i);
	class_destroy(pclass);

	unregister_chrdev_region(MKDEV(major, minor), count);
	kfree(pglobalmem_dev);
}

module_init(globalmem_init);
module_exit(globalmem_exit);

MODULE_LICENSE("GPL v2");
