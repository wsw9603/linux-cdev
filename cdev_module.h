#ifndef __CDEV_MODULE_H__
#define __CDEV_MODULE_H__

#define GLOBALMEM_IOCTL_MAGIC 'g'

/* 清空缓冲区 */
#define MEM_CLEAR _IO(GLOBALMEM_IOCTL_MAGIC, 0)

#endif
