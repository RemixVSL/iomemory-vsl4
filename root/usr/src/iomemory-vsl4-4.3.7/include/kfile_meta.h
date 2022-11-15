#ifndef __FIO_KFILE_META_H__
#define __FIO_KFILE_META_H__

#include <linux/version.h>

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5,17,0)
#define KFIO_PDE_DATA pde_data(ip)
#else
#define KFIO_PDE_DATA PDE_DATA(ip)
#endif

#endif /* __FIO_FILE_META_H__ */

