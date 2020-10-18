#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/slab.h>

#define DEVNAME "globalmem"
#define GLOBALMEM_SIZE 0x1000

#define wws_pr_info(fmt, ...)	\
	printk(KERN_INFO "[func: %s, line: %d] " fmt "\n",\
			__func__, __LINE__, ##__VA_ARGS__)

static int major = 0;
module_param(major, int, S_IRUGO);
static int minor = 1;
static const int count = 3;

struct globalmem_dev {
	struct class *pclass;
	struct cdev cdev;
	char buf[GLOBALMEM_SIZE];
};

static struct globalmem_dev *pglobalmem_dev;

static int globalmem_open(struct inode *inode, struct file *filp)
{
	wws_pr_info("file opened by, (%s: pid = %d)",
		    current->comm, current->pid);
	wws_pr_info("major: %d, minor: %d", imajor(inode), iminor(inode));

	filp->private_data = pglobalmem_dev;

	return 0;
}

static int globalmem_release(struct inode *inode, struct file *filp)
{
	wws_pr_info("file release by, (%s: pid = %d)",
		    current->comm, current->pid);
	wws_pr_info("major: %d, minor: %d", imajor(inode), iminor(inode));

	filp->private_data = NULL;

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

	if (copy_to_user(buf, pdev->buf + *offset, size))
		return -EFAULT;
	else
		*offset += size;

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

	if (copy_from_user(pdev->buf + *offset, buf, size))
		return -EFAULT;
	else
		*offset += size;

	wws_pr_info("write to file successfully, size %zu", size);

	return size;
}

static struct file_operations fops = {
	.owner		= THIS_MODULE,
	.open		= globalmem_open,
	.release	= globalmem_release,
	.read		= globalmem_read,
	.write		= globalmem_write,
};

static int __init globalmem_init(void)
{
	dev_t devnum = MKDEV(major, minor);
	int ret, i;
	struct device *pdev;

	wws_pr_info("init demo: (%s: pid=%d)", current->comm, current->pid);

	pglobalmem_dev = kzalloc(sizeof(*pglobalmem_dev), GFP_KERNEL);
	if (!pglobalmem_dev)
		return -ENOMEM;

	cdev_init(&pglobalmem_dev->cdev, &fops);

	if (major)
		ret = register_chrdev_region(devnum, count, DEVNAME);
	else
		ret = alloc_chrdev_region(&devnum, minor, count, DEVNAME);
	if (ret) {
		wws_pr_info("register/alloc cdev number failed");
		goto error_alloc;
	}
	major = MAJOR(devnum);

	ret = cdev_add(&pglobalmem_dev->cdev, devnum, count);
	if (ret) {
		wws_pr_info("register cdev failed");
		goto error_reg;
	}

	pglobalmem_dev->pclass = class_create(THIS_MODULE, DEVNAME);
	if (IS_ERR(pglobalmem_dev->pclass)) {
		wws_pr_info("create device class failed");
		ret = PTR_ERR(pglobalmem_dev->pclass);
		goto error_reg;
	}

	for (i = minor; i < minor + count; i++) {
		pdev = device_create(pglobalmem_dev->pclass, NULL,
				     MKDEV(major, i), NULL,
				     "%s%d", DEVNAME, i);
		if (IS_ERR(pdev)) {
			wws_pr_info("create device %d failed", i);
			ret = PTR_ERR(pdev);
			goto error_create_device;
		}
	}

	wws_pr_info("register cdev successfully, major: %d", major);
	return 0;

error_create_device:
	for (i--; i >= minor; i--)
		device_destroy(pglobalmem_dev->pclass, MKDEV(major, i));
	class_destroy(pglobalmem_dev->pclass);
error_reg:
	unregister_chrdev_region(devnum, count);
error_alloc:
	cdev_del(&pglobalmem_dev->cdev);

	return ret;
}

static void __exit globalmem_exit(void)
{
	int i;

	wws_pr_info("exit demo: (%s: pid=%d)", current->comm, current->pid);

	for (i = minor; i < minor + count; i++)
		device_destroy(pglobalmem_dev->pclass, MKDEV(major, i));
	class_destroy(pglobalmem_dev->pclass);

	unregister_chrdev_region(MKDEV(major, minor), count);
	cdev_del(&pglobalmem_dev->cdev);
}

module_init(globalmem_init);
module_exit(globalmem_exit);

MODULE_LICENSE("GPL v2");
