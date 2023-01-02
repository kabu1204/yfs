#ifndef YSSD_IOCTL_H
#define YSSD_IOCTL_H

#define IOCTL_MAGIC 0xcafe
#define IOCTL_GET   IOCTL_MAGIC + 0
#define IOCTL_SET   IOCTL_MAGIC + 1
#define IOCTL_DEL   IOCTL_MAGIC + 2
#define IOCTL_ITER  IOCTL_MAGIC + 3
#define IOCTL_NEXT  IOCTL_MAGIC + 4

#endif