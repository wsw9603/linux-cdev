#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/cdev.h>
#include <linux/fs.h>

#define DEVNAME "cdev_demo"

#define wws_pr_info(fmt, ...)	\
	printk(KERN_INFO "[func: %s, line: %d] " fmt "\n",\
			__func__, __LINE__, ##__VA_ARGS__)

static int major = 0;
static int minor = 1;
const int count = 3;

static struct cdev *demo_cdev = NULL;

static int demo_open(struct inode *inode, struct file *filp)
{
	wws_pr_info("file opened by, %s: pid = %d",
		    current->comm, current->pid);
	wws_pr_info("major: %d, minor: %d", imajor(inode), iminor(inode));
	return 0;
}

static int demo_release(struct inode *inode, struct file *filp)
{
	wws_pr_info("file release by, %s: pid = %d",
		    current->comm, current->pid);
	wws_pr_info("major: %d, minor: %d", imajor(inode), iminor(inode));
	return 0;
}

static ssize_t demo_read(struct file *filp, char __user *buf,
			 size_t size, loff_t *offset)
{
	struct inode *inode = filp->f_path.dentry->d_inode;
	wws_pr_info("file read by, %s: pid = %d",
		    current->comm, current->pid);
	wws_pr_info("major: %d, minor: %d", imajor(inode), iminor(inode));
	return 0;
}

static ssize_t demo_write(struct file *filp, const char __user *buf,
			  size_t size, loff_t *offset)
{
	struct inode *inode = filp->f_path.dentry->d_inode;
	wws_pr_info("file write by, %s: pid = %d",
		    current->comm, current->pid);
	wws_pr_info("major: %d, minor: %d", imajor(inode), iminor(inode));
	return 2;
}

static struct file_operations fops = {
	.owner		= THIS_MODULE,
	.open		= demo_open,
	.release	= demo_release,
	.read		= demo_read,
	.write		= demo_write,
};

static int __init demo_init(void)
{
	dev_t devnum;
	int ret;

	wws_pr_info("init demo: (%s: pid=%d)", current->comm, current->pid);

	demo_cdev = cdev_alloc();
	if (!demo_cdev) {
		wws_pr_info("alloc struct cdev failed");
		return -ENOMEM;
	}

	cdev_init(demo_cdev, &fops);
	ret = alloc_chrdev_region(&devnum, minor, count, DEVNAME);
	if (ret) {
		wws_pr_info("alloc cdev number failed");
		goto error_alloc;
	}
	major = MAJOR(devnum);

	ret = cdev_add(demo_cdev, devnum, count);
	if (ret) {
		wws_pr_info("register cdev failed");
		goto error_reg;
	}

	wws_pr_info("register cdev successfully, major: %d", major);
	return 0;

error_reg:
	unregister_chrdev_region(devnum, count);
error_alloc:
	cdev_del(demo_cdev);

	return ret;
}

static void __exit demo_exit(void)
{
	wws_pr_info("exit demo: (%s: pid=%d)", current->comm, current->pid);
	unregister_chrdev_region(MKDEV(major, minor), count);
	cdev_del(demo_cdev);
}

module_init(demo_init);
module_exit(demo_exit);

MODULE_LICENSE("GPL v2");
