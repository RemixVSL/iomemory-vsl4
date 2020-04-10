//-----------------------------------------------------------------------------
// Copyright (c) 2006-2014 Fusion-io, Inc. (acquired by SanDisk Corp. 2014)
// Copyright (c) 2014-2015 SanDisk Corp. and/or all its affiliates. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
// * Redistributions of source code must retain the above copyright notice,
//   this list of conditions and the following disclaimer.
// * Redistributions in binary form must reproduce the above copyright notice,
//   this list of conditions and the following disclaimer in the documentation
//   and/or other materials provided with the distribution.
// * Neither the name of the SanDisk Corp. nor the names of its contributors
//   may be used to endorse or promote products derived from this software
//   without specific prior written permission.
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED.
// IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
// OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
// OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
// WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//-----------------------------------------------------------------------------

#include "port-internal.h"
#include <fio/port/dbgset.h>
#include <fio/port/porting_cdev.h>
#include <fio/port/cdev.h>
#include <fio/port/message_ids.h>

#include <linux/miscdevice.h>
#include <linux/poll.h>

#include <linux/namei.h>
#include <linux/delay.h>
#include <linux/sched.h>

/**
 * @ingroup PORT_COMMON_LINUX
 * @{
 */

/**
 * Return events that are set.  If the cdev is not alerted,
 * this waits using the cdev's poll_queue, so the user-land application
 * that has called poll(), epoll(), or select() will get notified when something
 * interesting happens.  After return an ioctl needs to be called to get the
 * actual event data.
 */
static unsigned int coms_control_ioctl_poll(
                    struct file* filep,
                    struct poll_table_struct* polltable)
{
    struct coms_cdev *cdev = (struct coms_cdev *)(filep->private_data);
    coms_poll_struct *ps;

    ps = coms_cdev_get_poll_struct(cdev);
    poll_wait(filep, (wait_queue_head_t *)(ps), polltable);

    if (coms_cdev_is_alerted(cdev) )
    {
        return (POLLIN | POLLPRI);
    }

    return (0);
}

static int coms_port_cdev_has_minor(void *port_cdev, void *arg)
{
    struct miscdevice *md = (struct miscdevice *)port_cdev;
    int *minor = arg;

    return (md != NULL && minor != NULL && md->minor == *minor) ? 1 : 0;
}

static int coms_control_open(struct inode *inode, struct file *file)
{
    struct coms_cdev *cdev;
    int minor = iminor(inode);

#if !defined(__VMKLNX__)
    if (imajor(inode) != MISC_MAJOR)
    {
        errprint_all(ERRID_CMN_LINUX_CDEV_INVALID_INODE, "inode has major %d and is not a MISC_MAJOR device\n",
                imajor(inode));
        return -ENODEV;
    }
#endif

    cdev = coms_port_cdev_enumerate(coms_port_cdev_has_minor, 0, &minor);
    if (cdev == NULL)
    {
        return -ENODEV;
    }

    file->private_data = cdev;
    return 0;
}

static int coms_control_release(struct inode *inode, struct file *filep)
{
    return 0;
}

#if KFIOC_FOPS_USE_LOCKED_IOCTL
static int coms_control_ioctl_internal(struct inode *inode, struct file *file, unsigned int cmd, fio_uintptr_t arg)
#else
static long coms_control_ioctl_internal(struct file *file, unsigned int cmd, fio_uintptr_t arg)
#endif
{
    struct coms_cdev *cdev = (struct coms_cdev *)(file->private_data);

    return coms_cdev_ioctl( cdev, cmd, arg );
}

/*
 *  Needed for 32 bit apps to be able to ioctl a 64 bit driver
 */
static long coms_control_compat_ioctl_internal(struct file *file, unsigned int cmd, fio_uintptr_t arg)
{
    struct coms_cdev *cdev = (struct coms_cdev *)(file->private_data);

    return coms_cdev_ioctl( cdev, cmd, arg );
}

static struct file_operations coms_control_ops =
{
    open:    coms_control_open,
    release: coms_control_release,
#if KFIOC_FOPS_USE_LOCKED_IOCTL
    ioctl:   coms_control_ioctl_internal,
#else
    unlocked_ioctl: coms_control_ioctl_internal,
#endif
    compat_ioctl: coms_control_compat_ioctl_internal,
    poll:    coms_control_ioctl_poll,
};

/**
 * Called when events of interest have occurred
 */
void coms_port_cdev_wake(void *port_cdev, coms_poll_struct *ps)
{
    wake_up_all((wait_queue_head_t *) ps);
}

/******************************************************************************
* OS-specific interfaces for char device functions that call into the
* core fucntions.
*/

/**
* @brief OS-specific char device initialization.
*/
static void misc_dev_init(struct miscdevice *md, const char *dev_name)
{
    coms_control_ops.owner = THIS_MODULE;
    kfio_memset(md, 0, sizeof(*md));
    md->minor = MISC_DYNAMIC_MINOR;
    md->name  = dev_name;
    md->fops  = &coms_control_ops;
}

/// @brief OS-specific init for char device.
int coms_port_cdev_create(struct coms_cdev *cdev, void *port_param, void **handlep)
{
    struct miscdevice *md;
    int result;
#if defined(__VMKLNX__)
    int minor = 254;
#endif

    init_waitqueue_head((wait_queue_head_t *) coms_cdev_get_poll_struct(cdev));

    md = kfio_malloc(sizeof(*md));

    misc_dev_init(md, coms_cdev_get_name(cdev));

#if defined(__VMKLNX__)
    do
    {
        md->minor = minor--;
#endif
        result = misc_register(md);
#if defined(__VMKLNX__)
        /*
         * Yet another ugly hack for ESX: misc_register does not return 0 on success.
         * From the comments in misc_register() in the ESX 4.0 DDK:
         *  ESX Deviation Notes:
         *  On ESX, misc_register returns the assigned character-device major
         *  associated with the device (which is always MISC_MAJOR) upon a
         *  successful registration. For failure, a negative error code is
         *  returned.
         *  ....
         *  This is a deviation from Linux - should return 0 for success
         */
        if (result > 0)
        {
            result = 0;
        }
    }
    while (result < 0 && minor > 0);
#endif
    if (result < 0)
    {
        errprint_lbl(coms_cdev_get_bus_name(cdev), ERRID_CMN_LINUX_CDEV_INIT_FAIL,
                     "Unable to initialize misc device '%s'\n", coms_cdev_get_name(cdev));
        kfio_free(md, sizeof(*md));
    }
    else
    {
        *handlep = md;
#if !defined(__VMKLNX__) && !defined(__TENCENT_KERNEL__)
        coms_wait_for_dev(coms_cdev_get_name(cdev));
#endif
    }

    return result;
}

void coms_port_cdev_destroy(void *port_cdev)
{
    struct miscdevice *md = (struct miscdevice *)port_cdev;

    misc_deregister(md);
    kfio_free(md, sizeof(*md));
}

#if !defined(__VMKLNX__)

static int coms_path_lookup(const char *filename)
{
#if KFIOC_HAS_PATH_LOOKUP
    struct nameidata nd;

    return path_lookup(filename, 0, &nd);
#else
    struct path path;

    return kern_path(filename, LOOKUP_PARENT, &path);
#endif /* KFIOC_HAS_PATH_LOOKUP */
}

/**
 * @brief OS-specific init for char device.
 *
 * Wait for the device to be created by udev before we
 * continue on and allow modprobe to exit.  This is because on
 * some older Linux distros, there is a problem where the devices
 * aren't yet created by the time they are needed by init scripts
 * and bad stuff happens.
 *
 */
int coms_wait_for_dev(const char *devname)
{
    char abs_filename[UFIO_DEVICE_FILE_MAX_LEN];
    int i = 0;

    snprintf(abs_filename, sizeof(abs_filename) - 1,
            "%s%s", UFIO_CONTROL_DEVICE_PATH, devname);

    while ((coms_path_lookup(abs_filename) != 0) &&
           (i < fio_dev_wait_timeout_secs * 10))
    {
        if (i == 0)
        {
            infprint("Waiting for %s to be created\n", abs_filename);
        }
        msleep(100);  // Sleep 100msecs
        i++;
    }

    if (i >= fio_dev_wait_timeout_secs * 10)
    {
        infprint("warning: timeout on device %s creation\n", devname);
    }

    return 0;
}

#else /* !defined(__VMKLNX__) */

int coms_wait_for_dev(const char *devname)
{
    // Unimplemented on ESX
    return 0;
}

#endif /* !defined(__VMKLNX__) */

/**
 * @}
 */
