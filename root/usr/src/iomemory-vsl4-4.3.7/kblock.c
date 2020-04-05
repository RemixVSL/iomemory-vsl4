//-----------------------------------------------------------------------------
// Copyright (c) 2006-2014, Fusion-io, Inc.(acquired by SanDisk Corp. 2014)
// Copyright (c) 2014-2018 SanDisk Corp. and/or all its affiliates. (acquired by Western Digital Corp. 2016)
// Copyright (c) 2016-2019 Western Digital Technologies, Inc. All rights reserved.
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
#if !defined (__linux__)
#error "This file supports Linux only"
#endif

// Provides linux/types.h
#include <fio/port/port_config.h>
#include <fio/port/message_ids.h>

// Ignore this whole file, if the block device is not being included in the build.
#if KFIO_BLOCK_DEVICE

#if defined(__VMKLNX__)
// ESX block layer code is buggy.
// Map all block layer calls to our own block layer module (modified from the DDK version).
#define bdget fio_esx_bdget
#define bdput fio_esx_bdput
#define add_disk fio_esx_add_disk
#define vmklnx_block_register_sglimit fio_esx_vmklnx_block_register_sglimit
#define vmklnx_register_blkdev fio_esx_vmklnx_register_blkdev
#define unregister_blkdev fio_esx_unregister_blkdev
#define put_disk fio_esx_put_disk
#define vmklnx_block_init_done fio_esx_vmklnx_block_init_done
#define blk_complete_request fio_esx_blk_complete_request
#define end_that_request_last fio_esx_end_that_request_last
#define blk_queue_max_sectors fio_esx_blk_queue_max_sectors
#define blk_cleanup_queue fio_esx_blk_cleanup_queue
#define blk_queue_max_hw_segments fio_esx_blk_queue_max_hw_segments
#define blk_queue_softirq_done fio_esx_blk_queue_softirq_done
#define blk_queue_hardsect_size fio_esx_blk_queue_hardsect_size
#define blk_queue_max_phys_segments fio_esx_blk_queue_max_phys_segments
#define alloc_disk fio_esx_alloc_disk
#define blk_init_queue fio_esx_blk_init_queue
#define elv_next_request fio_esx_elv_next_request
#define blk_stop_queue fio_esx_blk_stop_queue
#define bio_endio fio_esx_bio_endio
#endif  /* __VMKLNX__ */

#include "port-internal.h"
#include <fio/port/dbgset.h>
#include <fio/port/kfio.h>
#include <fio/port/ktime.h>
#include <fio/port/kblock.h>
#include <fio/port/kscsi.h>
#include <fio/port/sched.h>
#include <fio/port/bitops.h>
#include <fio/port/atomic_list.h>
#include <fio/port/common-linux/kblock.h>

// This is a duplicate definition, from include/fio/common/units.h, because apparently we can't include that here.
#define FIO_NSEC_PER_USEC   1000
#define FIO_USEC_PER_SEC    1000000

#include <linux/version.h>
#include <linux/fs.h>
#if !defined(__VMKLNX__)
#include <fio/port/cdev.h>
#include <linux/buffer_head.h>
#endif

#if KFIOC_HAS_BLK_MQ
#include <linux/blk-mq.h>
#endif
extern int use_workqueue;
#if !defined(__VMKLNX__)
static int fio_major;
#endif

#if !KFIOC_HAS_BLK_QUEUE_MAX_SEGMENTS
// About the same time blk_queue_max_segments() came to be, the inline to extract it appeared as well.
#define queue_max_segments(q)   (q->limits.max_segments)
#endif

static void linux_bdev_name_disk(struct fio_bdev *bdev);
static int  linux_bdev_create_disk(struct fio_bdev *bdev);
static int  linux_bdev_expose_disk(struct fio_bdev *bdev);
static int  linux_bdev_hide_disk(struct fio_bdev *bdev, uint32_t opflags);
static void linux_bdev_destroy_disk(struct fio_bdev *bdev);
static void linux_bdev_backpressure(struct fio_bdev *bdev, int on);
static void linux_bdev_lock_pending(struct fio_bdev *bdev, int pending);
static void linux_bdev_update_stats(struct fio_bdev *bdev, int dir, uint64_t totalsize, uint64_t duration);
static void linux_bdev_update_inflight(struct fio_bdev *bdev, int rw, int in_flight);
#if !defined(__VMKLNX__) && !KFIOC_PARTITION_STATS
static int kfio_get_gd_in_flight(kfio_disk_t *disk, int rw);
#endif

#if KFIOC_HAS_BIOVEC_ITERATORS
#define BI_SIZE(bio) (bio->bi_iter.bi_size)
#define BI_SECTOR(bio) (bio->bi_iter.bi_sector)
#define BI_IDX(bio) (bio->bi_iter.bi_idx)
#else
#define BI_SIZE(bio) (bio->bi_size)
#define BI_SECTOR(bio) (bio->bi_sector)
#define BI_IDX(bio) (bio->bi_idx)
#endif

/******************************************************************************
 *   Block request and bio processing methods.                                *
 ******************************************************************************/

struct kfio_disk
{
    struct fio_bdev      *bdev;
    struct gendisk       *gd;
    struct request_queue *rq;
    fusion_spinlock_t     queue_lock;
    struct bio           *bio_head;
    struct bio           *bio_tail;
    fusion_bits_t         disk_state;
    fusion_condvar_t      state_cv;
    fusion_cv_lock_t      state_lk;
    struct fio_atomic_list  comp_list;
    unsigned int          pending;
    int                   last_dispatch_rw;
    sector_t              last_dispatch;
    atomic_t              lock_pending;
    uint32_t              sector_mask;
    int                   in_do_request;
    struct list_head      retry_list;
    unsigned int          retry_cnt;
    fusion_atomic32_t     ref_count;
    make_request_fn      *make_request_fn;
    int                   use_workqueue;

#if KFIOC_HAS_BLK_MQ
    struct blk_mq_tag_set tag_set;
#endif
#if defined(__VMKLNX__)
    volatile enum
    {
        threadState_none, Running, Stopping, Stopped
    } submit_thread_state;           ///< used if queuing requests via single thread: see use_workqueue

    fusion_condvar_t      submit_thread_cv;
    fusion_cv_lock_t      submit_thread_lk;
    int                   major;
#endif
};

enum {
    KFIO_DISK_HOLDOFF_BIT   = 0,
    KFIO_DISK_COMPLETION    = 1,
};

/*
 * RHEL6.1 will trigger both old and new scheme due to their backport,
 * whereas new kernels will trigger only the new scheme. So for the RHEL6.1
 * case, just disable the old scheme to avoid to much ifdef hackery.
 */
#if KFIOC_NEW_BARRIER_SCHEME == 1
#undef KFIOC_BARRIER
#define KFIOC_BARRIER   0
#endif

/*
 * Enable tag flush barriers by default, and default to safer mode of
 * operation on cards that don't have powercut support. Barrier mode can
 * also be QUEUE_ORDERED_TAG, or QUEUE_ORDERED_NONE for no barrier support.
 */
#if KFIOC_BARRIER == 1
static int iodrive_barrier_type = QUEUE_ORDERED_TAG_FLUSH;
int iodrive_barrier_sync = 1;
#else
int iodrive_barrier_sync = 0;
#endif

#if KFIOC_DISCARD == 1
extern int enable_discard;
#endif

#if KFIOC_HAS_SEPARATE_OP_FLAGS && !defined(bio_flags)
/* bi_opf defined in kernel 4.8, but bio_flags not defined until 4.9
   (and then disappeared in v4.10) */
#if defined(BIO_OP_SHIFT)
#define bio_flags(bio) ((bio)->bi_opf & ((1 << BIO_OP_SHIFT) - 1))
#else
#define bio_flags(bio) ((bio)->bi_opf & REQ_OP_MASK)
#endif
#endif

#if defined(__VMKLNX__) || KFIOC_HAS_RQ_POS_BYTES == 0
#define blk_rq_pos(rq)    ((rq)->sector)
#define blk_rq_bytes(rq)  ((rq)->nr_sectors << 9)
#endif

extern int kfio_sgl_map_bio(kfio_sg_list_t *sgl, struct bio *bio);

#if KFIOC_USE_IO_SCHED
static void kfio_blk_complete_request(struct request *req, int error);
static kfio_bio_t *kfio_request_to_bio(kfio_disk_t *disk, struct request *req,
                                       bool can_block);
#endif /* KFIOC_USE_IO_SCHED */

static int kfio_bio_cnt(const struct bio * const bio)
{
    return
#if KFIOC_BIO_HAS_USCORE_BI_CNT
    atomic_read(&bio->__bi_cnt);
#else
    atomic_read(&bio->bi_cnt);
#endif
}

/******************************************************************************
 *   Functions required to register and unregister fio block device driver    *
 ******************************************************************************/

int kfio_platform_init_block_interface(void)
{
    struct fio_bdev_storage_interface bdev_sif;

    bdev_sif.interface_type = FIO_BDEV_STORAGE_INTERFACE_BLOCK;
    bdev_sif.bdev_name_disk = linux_bdev_name_disk;
    bdev_sif.bdev_create_disk = linux_bdev_create_disk;
    bdev_sif.bdev_expose_disk = linux_bdev_expose_disk;
    bdev_sif.bdev_hide_disk = linux_bdev_hide_disk;
    bdev_sif.bdev_destroy_disk = linux_bdev_destroy_disk;
    bdev_sif.bdev_backpressure = linux_bdev_backpressure;
    bdev_sif.bdev_lock_pending = linux_bdev_lock_pending;
    bdev_sif.bdev_update_inflight = linux_bdev_update_inflight;
    bdev_sif.bdev_update_stats = linux_bdev_update_stats;
    fio_bdev_storage_interface_register(&bdev_sif);

#if defined(__VMKLNX__)
    return 0;
#else
    fio_major = register_blkdev(0, "fio");
    return fio_major <= 0 ? -EBUSY : 0;
#endif
}


int kfio_platform_teardown_block_interface(void)
{
#if defined(__VMKLNX__)
    return 0;
#else
    int rc = 0;

#if KFIOC_UNREGISTER_BLKDEV_RETURNS_VOID
    unregister_blkdev(fio_major, "fio");
#else
    rc = unregister_blkdev(fio_major, "fio");
#endif

    return rc;
#endif
}

#if defined(__VMKLNX__)
// "Real" ESX register functions
static int kfio_register_esx_blkdev(const char *name)
{
    // vmklnx_register_blkdev() expects PCI slot information, which is
    // unavailable/irrelevant/inappropriate with VSUs or dual-pipe.
    // Our modified block layer module allows skipping this information
    // by passing -1 and NULL instead.
    // Side effect: device gets listed as "Unknown" instead of "Adapter for iomemory-vsl"
    return vmklnx_register_blkdev(0, name, -1, -1, NULL);
}

static int kfio_unregister_esx_blkdev(unsigned int major, const char *name)
{
    return unregister_blkdev(major, name);
}
#endif

/******************************************************************************
 *   Block device open, close and ioctl handlers                              *
 ******************************************************************************/
static int kfio_open_disk(struct fio_bdev *bdev)
{
    struct kfio_disk *disk = (struct kfio_disk *)bdev->bdev_gd;
    int retval = 0;

    if (test_bit(QUEUE_FLAG_DEAD, &disk->rq->queue_flags))
    {
        return -ENXIO;
    }

    /*
     * We are executing under bdev_mutex protection here, so atomics are
     * not strictly necessary here.
     */
    if (fusion_atomic32_incr(&disk->ref_count) == 1)
    {
        retval = fio_bdev_open(bdev);
        if (retval != 0)
        {
            fusion_atomic32_dec(&disk->ref_count);
        }
    }
    return retval;
}

static void kfio_close_disk(struct fio_bdev *bdev)
{
    struct kfio_disk *disk = (struct kfio_disk *)bdev->bdev_gd;

    if (fusion_atomic32_decr(&disk->ref_count) == 0)
    {
        fio_bdev_release(bdev);

        if (test_bit(QUEUE_FLAG_DEAD, &disk->rq->queue_flags))
        {
            fusion_cv_lock_irq(&disk->state_lk);
            fusion_condvar_broadcast(&disk->state_cv);
            fusion_cv_unlock_irq(&disk->state_lk);
        }
    }
}

#if KFIOC_HAS_NEW_BLOCK_METHODS

static int kfio_open(struct block_device *blk_dev, fmode_t mode)
{
    struct fio_bdev *bdev = blk_dev->bd_disk->private_data;

    return kfio_open_disk(bdev);
}

#if KFIOC_BLOCK_DEVICE_RELEASE_RETURNS_INT
static int kfio_release(struct gendisk *gd, fmode_t mode)
{
    struct fio_bdev *bdev = gd->private_data;

    kfio_close_disk(bdev);
    return 0;
}
#else
static void kfio_release(struct gendisk *gd, fmode_t mode)
{
    struct fio_bdev *bdev = gd->private_data;

    kfio_close_disk(bdev);
}
#endif

static int kfio_ioctl(struct block_device *blk_dev, fmode_t mode, unsigned int cmd, unsigned long arg)
{
    struct fio_bdev *bdev = blk_dev->bd_disk->private_data;

    return fio_bdev_ioctl(bdev, cmd, arg);
}

static int kfio_compat_ioctl(struct block_device *blk_dev, fmode_t mode, unsigned cmd, unsigned long arg)
{
    int rc;

    rc = kfio_ioctl(blk_dev, mode, cmd, arg);

    if (rc == -ENOTTY)
    {
        return -ENOIOCTLCMD;
    }

    return rc;
}

#else

static void *kfio_inode_bd_disk_private_data(struct inode *ip)
{
    return ip->i_bdev->bd_disk->private_data;
}

static int kfio_open(struct inode *inode, struct file *filp)
{
    struct fio_bdev *bdev = kfio_inode_bd_disk_private_data(inode);

    filp->private_data = bdev;

    return kfio_open_disk(bdev);
}

static int kfio_release(struct inode *inode, struct file *filp)
{
    struct fio_bdev *bdev = kfio_inode_bd_disk_private_data(inode);

    kfio_close_disk(bdev);

    return 0;
}

static int kfio_ioctl(struct inode *inode, struct file *filp, unsigned int cmd, unsigned long arg)
{
    struct fio_bdev *bdev = kfio_inode_bd_disk_private_data(inode);

    return fio_bdev_ioctl(bdev, cmd, arg);
}

#if KFIOC_HAS_COMPAT_IOCTL_METHOD
#if KFIOC_COMPAT_IOCTL_RETURNS_LONG
static long kfio_compat_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
#else
static int kfio_compat_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
#endif
{
    struct fio_bdev *bdev;
    int rc;

    if (NULL == filp)
    {
        return -EBADF;
    }
    bdev = filp->private_data;

    if (NULL == bdev)
    {
        return -EINVAL;
    }

    rc = fio_bdev_ioctl(bdev, cmd, arg);

    if (rc == -ENOTTY)
    {
        return -ENOIOCTLCMD;
    }

    return rc;
 }
#endif
#endif

static struct block_device_operations fio_bdev_ops =
{
    .owner =        THIS_MODULE,
    .open =         kfio_open,
    .release =      kfio_release,
    .ioctl =        kfio_ioctl,
#if KFIOC_HAS_COMPAT_IOCTL_METHOD
    .compat_ioctl = kfio_compat_ioctl,
#endif
};



#if !defined(__VMKLNX__)
static struct request_queue *kfio_alloc_queue(struct kfio_disk *dp, kfio_numa_node_t node);
#if KFIOC_MAKE_REQUEST_FN_VOID
static void kfio_make_request(struct request_queue *queue, struct bio *bio);
#elif KFIOC_MAKE_REQUEST_FN_UINT
static unsigned int kfio_make_request(struct request_queue *queue, struct bio *bio);
#else
static int kfio_make_request(struct request_queue *queue, struct bio *bio);
#endif
static void __kfio_bio_complete(struct bio *bio, uint32_t bytes_complete, int error);
#endif

#if KFIOC_USE_IO_SCHED
static struct request_queue *kfio_init_queue(struct kfio_disk *dp, kfio_numa_node_t node);
static void kfio_do_request(struct request_queue *queue);
static struct request *kfio_blk_fetch_request(struct request_queue *q);
static void kfio_restart_queue(struct request_queue *q);
static void kfio_end_request(struct request *req, int error);
#else
static void kfio_restart_queue(struct request_queue *q)
{
}
#endif

#if KFIOC_BARRIER == 1
static void kfio_prepare_flush(struct request_queue *q, struct request *rq);
#endif

static void kfio_invalidate_bdev(struct block_device *bdev);

#if defined(__VMKLNX__)

static kfio_bio_t *kfio_fetch_next_bio(struct kfio_disk *disk)
{
    struct request_queue *q;
    struct request *creq;

    q = disk->rq;

    spin_lock_irq(q->queue_lock);
    creq = kfio_blk_fetch_request(q);
    spin_unlock_irq(q->queue_lock);

    if (creq != NULL)
    {
        kfio_bio_t *fbio;

        fbio = kfio_request_to_bio(disk, creq, true);
        if (fbio == NULL)
        {
            kfio_blk_complete_request(creq, -EIO);
        }
        return fbio;
    }
    return NULL;
}

/// @brief single-threaded handler for kfio_bio requests.
/// @param arg a pointer to the fio_bdev.
static int bio_submit_thread(void *arg)
{
    struct kfio_disk *disk = arg;
    kfio_bio_t *bio;

    /*
     * Set out thread state as Running, but only if caller
     * did not tell us to stop already, before this function
     * has got a chance to run.
     */
    fusion_cv_lock_irq(&disk->submit_thread_lk);
    if (disk->submit_thread_state != Stopping)
        disk->submit_thread_state = Running;
    fusion_cv_unlock_irq(&disk->submit_thread_lk);

    while (disk->submit_thread_state != Stopping)
    {
        /* Try to grab next bio to submit. */
        bio = kfio_fetch_next_bio(disk);

        if (bio != NULL)
        {
            kfio_bio_submit(bio);
            fusion_cond_resched();

            /* There was work to do, ask for more immediately, unless asked to stop. */
            continue;
        }

        fusion_cv_lock_irq(&disk->submit_thread_lk);
        if (disk->submit_thread_state != Stopping)
        {
            (void)fusion_condvar_timedwait_noload(&disk->submit_thread_cv,
                                                  &disk->submit_thread_lk,
                                                  100000);
        }
        fusion_cv_unlock_irq(&disk->submit_thread_lk);
    }

    /* Do a final handshake with whomever is stopping us. */
    fusion_cv_lock_irq(&disk->submit_thread_lk);
    disk->submit_thread_state = Stopped;
    fusion_condvar_broadcast(&disk->submit_thread_cv);
    fusion_cv_unlock_irq(&disk->submit_thread_lk);

    return 0;
}

/// @brief start bio_submit_thread.
static void fio_start_submit_thread(struct kfio_disk *disk)
{
    kassert(disk->use_workqueue == USE_QUEUE_RQ);

    fusion_create_kthread(bio_submit_thread, disk, NULL, "submit", "");
}

/// @brief stop bio_submit_thread.
static void fio_stop_submit_thread(struct kfio_disk *disk)
{
    kassert(disk->use_workqueue == USE_QUEUE_RQ);

    fusion_cv_lock_irq(&disk->submit_thread_lk);
    if (disk->submit_thread_state != Stopped)
    {
        disk->submit_thread_state = Stopping;
    }
    fusion_condvar_broadcast(&disk->submit_thread_cv);

    /* Wait for worker to check in. */
    while (disk->submit_thread_state != Stopped)
    {
        fusion_condvar_wait(&disk->submit_thread_cv, &disk->submit_thread_lk);
    }
    fusion_cv_unlock_irq(&disk->submit_thread_lk);
}

/// @brief process request by queueing it to submit thread
static void kfio_do_request(struct request_queue *q)
{
    kfio_disk_t *disk = q->queuedata;

    /*
     * In this mode we do queueing, so all we need is to signal the queue
     * handler to come back to us and start fetching newly added requests.
     */
    fusion_cv_lock_irq(&disk->submit_thread_lk);
    fusion_condvar_broadcast(&disk->submit_thread_cv);
    fusion_cv_unlock_irq(&disk->submit_thread_lk);
}

#endif /* defined(__VMKLNX__) */

#if KFIOC_HAS_BLK_MQ
static kfio_bio_t *kfio_request_to_bio(kfio_disk_t *disk, struct request *req,
                                       bool can_block);
#if KFIOC_BIO_ERROR_CHANGED_TO_STATUS
static blk_status_t fio_queue_rq(struct blk_mq_hw_ctx *hctx, const struct blk_mq_queue_data *bd)
#else
static int fio_queue_rq(struct blk_mq_hw_ctx *hctx, const struct blk_mq_queue_data *bd)
#endif
{
    struct kfio_disk *disk = hctx->driver_data;
    struct request *req = bd->rq;
    kfio_bio_t *fbio;
    int rc;

#if KFIOC_X_REQUEST_QUEUE_HAS_SPECIAL
    fbio = req->special;
#else
    // fbio = kfio_request_to_bio(disk, req, false);
#endif
    if (!fbio)
    {
        goto busy;
    }

    fbio->fbio_flags |= KBIO_FLG_NONBLOCK;

    blk_mq_start_request(req);

    rc = kfio_bio_submit_handle_retryable(fbio);
    if (rc)
    {
        if (kfio_bio_failure_is_retryable(rc))
        {
            /*
             * "busy error" conditions. Store the prepped part
             * for faster retry, and exit.
             */
#if KFIOC_X_REQUEST_QUEUE_HAS_SPECIAL
            req->special = fbio;
#endif
            goto retry;
        }
        /*
         * Bio already completed, we'll just return QUEUE_OK as we should not
         * touch it anymore.
         */
    }

#if KFIOC_BIO_ERROR_CHANGED_TO_STATUS
    return BLK_STS_OK;
#else
    return BLK_MQ_RQ_QUEUE_OK;
#endif
busy:
#if KFIOC_X_HAS_BLK_MQ_DELAY_QUEUE
    blk_mq_delay_queue(hctx, 1);
else
    blk_mq_delay_run_hw_queue(hctx, 1);
#endif
#if KFIOC_BIO_ERROR_CHANGED_TO_STATUS
    return BLK_STS_RESOURCE;
#else
    return BLK_MQ_RQ_QUEUE_BUSY;
#endif
retry:
    blk_mq_run_hw_queues(hctx->queue, true);
#if KFIOC_BIO_ERROR_CHANGED_TO_STATUS
    return BLK_STS_RESOURCE;
#else
    return BLK_MQ_RQ_QUEUE_BUSY;
#endif
}

static int fio_init_hctx(struct blk_mq_hw_ctx *hctx, void *data, unsigned int i)
{
    struct kfio_disk *disk = data;

    hctx->driver_data = disk;
    return 0;
}

static struct blk_mq_ops fio_mq_ops = {
    .queue_rq   = fio_queue_rq,
#if KFIOC_BLK_MQ_OPS_HAS_MAP_QUEUES
    /* We want to use blk_mq_map_queues, but it's GPL: however, the block
       driver will call it for us if the function pointer is NULL */
    .map_queues = NULL,
#else
    .map_queue  = blk_mq_map_queue,
#endif
    .init_hctx  = fio_init_hctx,
};

#endif


#if !defined(__VMKLNX__)

/* @brief Parameter to the work queue call. */
struct kfio_blk_add_disk_param
{
    struct fusion_work_struct work;
    struct kfio_disk *disk;
    bool              done;
};

/*
 * @brief Run add-disk in a worker thread context.
 */
static void kfio_blk_add_disk(fusion_work_struct_t *work)
{
    struct kfio_blk_add_disk_param *param = container_of(work, struct kfio_blk_add_disk_param, work);
    struct kfio_disk *disk = param->disk;

    add_disk(disk->gd);

    /* Tell waiter we are done. */
    fusion_cv_lock_irq(&disk->state_lk);
    param->done = true;
    fusion_condvar_broadcast(&disk->state_cv);
    fusion_cv_unlock_irq(&disk->state_lk);
}
#endif /* defined(__VMKLNX__) */

static void linux_bdev_name_disk(struct fio_bdev *bdev)
{
    fio_bdev_name_disk(bdev, UFIO_BLOCK_DEVICE_PREFIX, 1);
}

static int linux_bdev_create_disk(struct fio_bdev *bdev)
{
    struct kfio_disk *disk;

    engprint("Creating bdev name %s index %d\n",
             bdev->bdev_name, bdev->bdev_index);
    /*
     * We are not prepared to expose device kernel block layer cannot handle.
     */
    if (bdev->bdev_block_size < KERNEL_SECTOR_SIZE ||
        (bdev->bdev_block_size & (bdev->bdev_block_size - 1)) != 0)
    {
        return -EINVAL;
    }

    disk = kfio_malloc_node(sizeof(*disk), bdev->bdev_numa_node);
    if (disk == NULL)
    {
        return -ENOMEM;
    }

    memset(disk, 0, sizeof(*disk));
    disk->bdev = bdev;
    disk->last_dispatch_rw = -1;
    disk->sector_mask = bdev->bdev_block_size - 1;
    disk->use_workqueue = use_workqueue;

#if defined(__VMKLNX__)
    // FIXME: Need PCI device info here to avoid showing up as
    // "Gammagraphx, Inc. (or missing ID) Unknown" in "esxcfg-scsidevs -a".
    disk->major = kfio_register_esx_blkdev(bdev->bdev_name);
    if (disk->major < 0)
    {
        kfio_free(disk, sizeof(*disk));
        return -ENODEV;
    }

    disk->submit_thread_state = Stopped;
    fusion_condvar_init(&disk->submit_thread_cv, "fio_submit_cv");
    fusion_cv_lock_init(&disk->submit_thread_lk, "fio_submit_lk");
#endif
    fusion_init_spin(&disk->queue_lock, "queue_lock");

    fusion_condvar_init(&disk->state_cv, "fio_disk_cv");
    fusion_cv_lock_init(&disk->state_lk, "fio_disk_lk");

    fusion_atomic_list_init(&disk->comp_list);

    atomic_set(&disk->lock_pending, 0);
    INIT_LIST_HEAD(&disk->retry_list);

    bdev->bdev_gd = disk;

    return 0;
}

static int linux_bdev_expose_disk(struct fio_bdev *bdev)
{
    struct kfio_disk     *disk;
    struct request_queue *rq;
    struct gendisk       *gd;
#if !defined(__VMKLNX__)
    struct kfio_blk_add_disk_param *param;
#endif

    disk = bdev->bdev_gd;
    if (disk == NULL)
    {
        return -ENODEV;
    }
#if KFIOC_HAS_BLK_MQ
    if (use_workqueue == USE_QUEUE_MQ)
    {
       int rv;

       disk->rq = NULL;
       disk->tag_set.ops = &fio_mq_ops;
       /* single hw queue path */
       disk->tag_set.nr_hw_queues = 1;
       /* Limit to 256 qd , need to run perf tests if this is OK */
       disk->tag_set.queue_depth = 256;
       disk->tag_set.numa_node = bdev->bdev_numa_node;
       disk->tag_set.cmd_size = 0;
       disk->tag_set.flags = BLK_MQ_F_SHOULD_MERGE;
       disk->tag_set.driver_data = disk;

       rv = blk_mq_alloc_tag_set(&disk->tag_set);
       if (!rv)
       {
          disk->rq = blk_mq_init_queue(&disk->tag_set);
          if (IS_ERR(disk->rq))
          {
              blk_mq_free_tag_set(&disk->tag_set);
              disk->rq = NULL;
          }
       }
    }
    else
#endif
    if (disk->use_workqueue != USE_QUEUE_RQ)
    {
#if !defined(__VMKLNX__)
        disk->rq = kfio_alloc_queue(disk, bdev->bdev_numa_node);
#endif
    }
# if KFIOC_USE_IO_SCHED
    else
    {
        disk->rq = kfio_init_queue(disk, bdev->bdev_numa_node);
    }
# endif /* KFIOC_USE_IO_SCHED */

    if (disk->rq == NULL)
    {
        /* Undo work done so far. */
        linux_bdev_hide_disk(bdev, KFIO_DISK_OP_SHUTDOWN | KFIO_DISK_OP_FORCE);
        return -ENOMEM;
    }

    rq = disk->rq;

#if KFIOC_HAS_BLK_LIMITS_IO_MIN
    blk_limits_io_min(&rq->limits, bdev->bdev_block_size);
#endif
#if KFIOC_HAS_BLK_LIMITS_IO_OPT
    blk_limits_io_opt(&rq->limits, fio_dev_optimal_blk_size);
#endif

#if KFIOC_HAS_BLK_QUEUE_MAX_SEGMENTS
    // As of kernel v4.14 it appears that the kernel does not honor these requested limits. Hopefully that will change.
    blk_queue_max_hw_sectors(rq, FUSION_MAX_SECTORS_PER_OS_RW_REQUEST);
    blk_queue_max_segments(rq, bdev->bdev_max_sg_entries);
#else
    blk_queue_max_sectors(rq, FUSION_MAX_SECTORS_PER_OS_RW_REQUEST);
    blk_queue_max_phys_segments(rq, bdev->bdev_max_sg_entries);
    blk_queue_max_hw_segments  (rq, bdev->bdev_max_sg_entries);
#endif

#if KFIOC_HAS_QUEUE_FLAG_CLUSTER
# if KFIOC_USE_BLK_QUEUE_FLAGS_FUNCTIONS
    blk_queue_flag_clear(QUEUE_FLAG_CLUSTER, rq);
# elif KFIOC_HAS_QUEUE_FLAG_CLEAR_UNLOCKED
    queue_flag_clear_unlocked(QUEUE_FLAG_CLUSTER, rq);
# else
    rq->queue_flags &= ~(1 << QUEUE_FLAG_CLUSTER);
# endif
#elif KFIOC_HAS_QUEUE_LIMITS_CLUSTER
    rq->limits.cluster = 0;
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(5, 0, 0)
// Linux from 5.0 > removed the limits.cluster: https://patchwork.kernel.org/patch/10716231/
#else
# ifndef __VMKLNX__
#  error "Do not know how to disable request queue clustering for this kernel."
# endif
#endif

    blk_queue_max_segment_size(rq, PAGE_SIZE);

#if KFIOC_HAS_BLK_QUEUE_HARDSECT_SIZE
    blk_queue_hardsect_size(rq, bdev->bdev_block_size);
#else
    blk_queue_logical_block_size(rq, bdev->bdev_block_size);
#endif
#if KFIOC_DISCARD == 1
    if (enable_discard)
    {

#if KFIOC_DISCARD_ZEROES_IN_LIMITS == 1
        if (fio_bdev_ptrim_available(bdev))
        {
            rq->limits.discard_zeroes_data = 1;
        }
#endif  /* KFIOC_DISCARD_ZEROES_IN_LIMITS */

#if KFIOC_USE_BLK_QUEUE_FLAGS_FUNCTIONS == 1
        blk_queue_flag_set(QUEUE_FLAG_DISCARD, rq);
#else
        queue_flag_set_unlocked(QUEUE_FLAG_DISCARD, rq);
#endif
        // XXXXXXX !!! WARNING - power of two sector sizes only !!! (always true in standard linux)
        blk_queue_max_discard_sectors(rq, (UINT_MAX & ~((unsigned int) bdev->bdev_block_size - 1)) >> 9);
#if KFIOC_DISCARD_GRANULARITY_IN_LIMITS
        rq->limits.discard_granularity = bdev->bdev_block_size;
#endif
    }
#else
    if (enable_discard)
    {
        infprint("enable_discard set but discard not supported on this linux version\n");
        enable_discard = 0;         // Seems like a good idea to also disable our discard code.
    }
#endif  /* KFIOC_DISCARD */

#if KFIOC_NEW_BARRIER_SCHEME == 1
    /*
     * Do this manually, as blk_queue_flush() is a GPL only export.
     *
     * We set REQ_FUA and REQ_FLUSH to ensure ordering (barriers) and to flush (on non-powercut cards).
     */
    rq->flush_flags = REQ_FUA | REQ_FLUSH;

#elif KFIOC_USE_BLK_QUEUE_FLAGS_FUNCTIONS
    blk_queue_flag_set(QUEUE_FLAG_WC, rq);
#elif KFIOC_BARRIER_USES_QUEUE_FLAGS
    queue_flag_set(QUEUE_FLAG_WC, rq);
#elif KFIOC_BARRIER == 1
    // Ignore if ordered mode is wrong - linux will complain
    blk_queue_ordered(rq, iodrive_barrier_type, kfio_prepare_flush);
#else
#error No barrier scheme supported
#endif

#if defined(REQ_ATOMIC) || KFIOC_HAS_BIO_RW_ATOMIC
    /* the kernel side of the atomics is only wired up for USE_QUEUE_NONE */
    if (disk->use_workqueue == USE_QUEUE_NONE)
    {
        if (kfio_bdev_atomic_writes_enabled(bdev))
        {
            blk_queue_set_atomic_write(rq, kfio_bdev_max_atomic_iovs(bdev));
        }
    }
#endif

#if KFIOC_QUEUE_HAS_NONROT_FLAG
    /* Tell the kernel we are a non-rotational storage device */
#if KFIOC_USE_BLK_QUEUE_FLAGS_FUNCTIONS
    blk_queue_flag_set(QUEUE_FLAG_NONROT, rq);
#else
    queue_flag_set_unlocked(QUEUE_FLAG_NONROT, rq);
#endif
#endif
#if KFIOC_QUEUE_HAS_RANDOM_FLAG
    /* Disable device global entropy contribution */
#if KFIOC_USE_BLK_QUEUE_FLAGS_FUNCTIONS
    blk_queue_flag_clear(QUEUE_FLAG_ADD_RANDOM, rq);
#else
    queue_flag_clear_unlocked(QUEUE_FLAG_ADD_RANDOM, rq);
#endif
#endif

    disk->gd = gd = alloc_disk(FIO_NUM_MINORS);
    if (disk->gd == NULL)
    {
        linux_bdev_hide_disk(bdev, KFIO_DISK_OP_SHUTDOWN | KFIO_DISK_OP_FORCE);
        return -ENOMEM;
    }

#if defined(__VMKLNX__)
    gd->major = disk->major;
#else
    gd->major = fio_major;
#endif

    gd->first_minor = FIO_NUM_MINORS * bdev->bdev_index;
    gd->minors = FIO_NUM_MINORS;
    gd->fops = &fio_bdev_ops;
    gd->queue = rq;
    gd->private_data = bdev;
#if defined GENHD_FL_EXT_DEVT
    gd->flags = GENHD_FL_EXT_DEVT;
#endif

#if defined(__VMKLNX__)
    gd->maxXfer = 1024 * 1024; // 1M - matches BLK_DEF_MAX_SECTORS
#endif

    fio_bdev_ops.owner = THIS_MODULE;

    strncpy(gd->disk_name, bdev->bdev_name, sizeof(gd->disk_name)-1);
    gd->disk_name[sizeof(gd->disk_name)-1] = 0;

    set_capacity(gd, bdev->bdev_num_blocks * bdev->bdev_block_size / KERNEL_SECTOR_SIZE);

    infprint("%s: Creating block device %s: major: %d minor: %d sector size: %d...\n",
             fio_bdev_get_bus_name(bdev), gd->disk_name, gd->major,
             gd->first_minor, bdev->bdev_block_size);

#if !defined(__VMKLNX__)
    /*
     * Offload device exposure to separate worker thread. On some kernels from
     * certain vendores add_disk is happy do do a lot of nested processing,
     * eating through limited kernel stack space.
     */
    param = kfio_malloc(sizeof(*param));
    if (param != NULL)
    {
        fusion_init_work(&param->work, kfio_blk_add_disk);

        param->disk = disk;
        param->done = false;
        fusion_schedule_work(&param->work);

        /* Wait for work thread to expose our disk. */
        fusion_cv_lock_irq(&disk->state_lk);
        while (!param->done)
        {
            fusion_condvar_wait(&disk->state_cv, &disk->state_lk);
        }
        fusion_cv_unlock_irq(&disk->state_lk);

        fusion_destroy_work(&param->work);

        kfio_free(param, sizeof(*param));
    }
    else
    {
        /* Release gendisk allocated above. */
        put_disk(disk->gd);

        /*
         * Break the link, so that linux_bdev_hid_disk does not attempt to
         * delete the disk we didn't add.
         */
        disk->gd = NULL;

        /* Cleanup. */
        linux_bdev_hide_disk(bdev, KFIO_DISK_OP_SHUTDOWN | KFIO_DISK_OP_FORCE);
        return -ENOMEM;
    }

    /*
     * The following is used because access to udev events are restricted
     * to GPL licensed symbols.  Since we have a propriatary licence, we can't
     * use the better interface.  Instead we must poll for the device creation.
     */
    coms_wait_for_dev(gd->disk_name);
#else // __VMKLNX__
    /*
     * Submit thread should be started before we call add_disk
     * in case add_disk would want to make some nested IO
     * for partition table probing.
     */
    fio_start_submit_thread(disk);

    add_disk(gd);

    vmklnx_block_register_sglimit(gd->major, bdev->bdev_max_sg_entries);
    vmklnx_block_init_done(gd->major);
#endif

    return 0;
}

static void kfio_kill_requests(struct request_queue *q)
{
#if KFIOC_USE_IO_SCHED
    struct kfio_disk *disk = q->queuedata;
    struct request *req;

    while ((req = kfio_blk_fetch_request(disk->rq)) != NULL)
    {
        kfio_end_request(req, -EIO);
    }

    while (!list_empty(&disk->retry_list))
    {
        req = list_entry(disk->retry_list.next, struct request, queuelist);
        list_del_init(&req->queuelist);

        // If special is Not NULL this request was retried and hence the bio associated
        // with the request stored in special might not be freed.
        if (req->special != NULL)
        {
            kfio_bio_free(req->special);
        }
        kfio_end_request(req, -EIO);
    }
#endif
}

static int linux_bdev_hide_disk(struct fio_bdev *bdev, uint32_t opflags)
{
    struct kfio_disk *disk = bdev->bdev_gd;
    struct block_device *linux_bdev;

    engprint("linux_bdev_hide_disk: %s disk: %p gd: %p rq: %p flags 0x%x\n",
             bdev->bdev_name, disk, disk->gd, disk->rq, opflags);

    if (disk->gd != NULL)
    {
        linux_bdev = bdget_disk(disk->gd, 0);

        if (linux_bdev != NULL)
        {
            kfio_invalidate_bdev(linux_bdev);
        }

        set_capacity(disk->gd, 0);

        fusion_spin_lock_irqsave(&disk->queue_lock);

        /* Stop delivery of new io from user. */
        set_bit(QUEUE_FLAG_DEAD, &disk->rq->queue_flags);

        /*
         * Prevent request_fn callback from interfering with
         * the queue shutdown.
         */
#if KFIOC_HAS_BLK_MQ
        if (disk->rq->mq_ops)
        {
            blk_mq_stop_hw_queues(disk->rq);
        }
#endif
#if KFIOC_X_HAS_BLK_STOP_QUEUE
        blk_stop_queue(disk->rq);
#endif
        /*
         * The queue is stopped and dead and no new user requests will be
         * coming to it anymore. Fetch remaining already queued requests
         * and fail them,
         */
        if (disk->use_workqueue == USE_QUEUE_RQ)
        {
            kfio_kill_requests(disk->rq);
        }
#if !defined(__VMKLNX__)
        else if (disk->use_workqueue != USE_QUEUE_MQ)
        {
            /* Fail all bio's already on internal bio queue. */
            struct bio *bio;

            while ((bio = disk->bio_head) != NULL)
            {
                disk->bio_head = bio->bi_next;
                __kfio_bio_complete(bio,  0, -EIO);
            }
            disk->bio_tail = NULL;
        }
#endif

        fusion_spin_unlock_irqrestore(&disk->queue_lock);

        /* Wait for all IO against bdev to finish. */
        fio_bdev_drain_wait(bdev);

        /* Tell Linux that disk is gone. */
        del_gendisk(disk->gd);

#if defined(__VMKLNX__)
        /* Stop submit thread here, we do not need it anymore. */
        fio_stop_submit_thread(disk);
#endif

        /*
         * If we have block device for the whole disk, wait for all
         * openers to release it. This protects against release calls
         * arriving after corresponding fio_bdev device is long gone.
         */
        if (linux_bdev != NULL)
        {
            /*
             * Wait for openere to go away before tearing the disk down. Skip waiting
             * if the system is being shut down - if we happen to hold the root system
             * or swap, last close will never happen.
             */
            if ((opflags & KFIO_DISK_OP_SHUTDOWN) == 0)
            {
                /*
                 * Tricky: lock and unlock sequence ensures that there is
                 * no parallel open or close operation happening. All opens
                 * that were running up to this point have either suceeded or
                 * failed, all future opens will be failed due to queue marked
                 * as dead above. The bd_openers count can now only go down from
                 * here and no concurrent open-close operation can race with the
                 * wait below.
                 */
#if KFIOC_HAS_BLKDEV_OLD_BD_SEM
                down(&linux_bdev->bd_sem);
                up(&linux_bdev->bd_sem);
#else
                mutex_lock(&linux_bdev->bd_mutex);
                mutex_unlock(&linux_bdev->bd_mutex);
#endif

                fusion_cv_lock_irq(&disk->state_lk);
                while (linux_bdev->bd_openers > 0 && linux_bdev->bd_disk == disk->gd)
                {
                    fusion_condvar_wait(&disk->state_cv, &disk->state_lk);
                }
                fusion_cv_unlock_irq(&disk->state_lk);
            }
            else
            {
                /*
                 * System is being torn down, we do not expect anyone to try and open
                 * or close this device now. Just release all of the outstanding references
                 * to the parent device, if any. This allows lower levels of the driver to
                 * finish tearing the underlying infrastructure down.
                 */
                if (fusion_atomic32_decr(&disk->ref_count) > 0)
                {
                    fio_bdev_release(bdev);
                }
            }

            bdput(linux_bdev);
        }

        put_disk(disk->gd);

        disk->gd = NULL;

    }

    if (disk->rq != NULL)
    {
        blk_cleanup_queue(disk->rq);
#if KFIOC_HAS_BLK_MQ
        if (use_workqueue == USE_QUEUE_MQ)
        {
            blk_mq_free_tag_set(&disk->tag_set);
        }
#endif
        disk->rq = NULL;
    }

    return 0;
}

static void linux_bdev_destroy_disk(struct fio_bdev *bdev)
{
    struct kfio_disk *disk = bdev->bdev_gd;

#if defined(__VMKLNX__)
    kfio_unregister_esx_blkdev(disk->major, bdev->bdev_name);

    fusion_condvar_destroy(&disk->submit_thread_cv);
    fusion_cv_lock_destroy(&disk->submit_thread_lk);
#endif
    fusion_destroy_spin(&disk->queue_lock);

    fusion_condvar_destroy(&disk->state_cv);
    fusion_cv_lock_destroy(&disk->state_lk);
    kfio_free(disk, sizeof(*disk));

    bdev->bdev_gd = NULL;
}

static void kfio_invalidate_bdev(struct block_device *bdev)
{
#if !defined(__VMKLNX__)
#if ! KFIOC_INVALIDATE_BDEV_REMOVED_DESTROY_DIRTY_BUFFERS
    invalidate_bdev(bdev, 0);
#else
    invalidate_bdev(bdev);
#endif /* KFIOC_INVALIDATE_BDEV_REMOVED_DESTROY_DIRTY_BUFFERS */
#else
    /* XXXesx missing API */
#endif
}

void linux_bdev_update_stats(struct fio_bdev *bdev, int dir, uint64_t totalsize, uint64_t duration)
{
#if !defined(__VMKLNX__)
    kfio_disk_t *disk = (kfio_disk_t *)bdev->bdev_gd;

    if (disk == NULL)
    {
        return;
    }

    if (dir == BIO_DIR_WRITE)
    {
        if (disk->use_workqueue != USE_QUEUE_RQ && disk->use_workqueue != USE_QUEUE_MQ)
        {
# if !(KFIOC_PARTITION_STATS && \
      (defined(CONFIG_PREEMPT_RT) || defined(CONFIG_TREE_PREEMPT_RCU) || defined(CONFIG_PREEMPT_RCU)))
            struct gendisk *gd = disk->gd;
# endif
# if KFIOC_PARTITION_STATS
#  if !defined(CONFIG_PREEMPT_RT) && !defined(CONFIG_TREE_PREEMPT_RCU) && !defined(CONFIG_PREEMPT_RCU)
            int cpu;

            /*
             * part_stat_lock() with defined(CONFIG_PREEMPT_RT) can't be used! It ends up calling
             * rcu_read_update which is GPL in the RT patch set.
             */
            cpu = part_stat_lock();
#   if KFIOC_X_PART_STAT_REQUIRES_CPU
            part_stat_inc(cpu, &gd->part0, ios[1]);
            part_stat_add(cpu, &gd->part0, sectors[1], totalsize >> 9);
#   else
            part_stat_inc(&gd->part0, ios[1]);
            part_stat_add(&gd->part0, sectors[1], totalsize >> 9);
#   endif
#   if KFIOC_HAS_DISK_STATS_NSECS && KFIOC_X_PART_STAT_REQUIRES_CPU
            part_stat_add(cpu, &gd->part0, nsecs[1],   duration * FIO_NSEC_PER_USEC);   // Convert our usec duration to nsecs.
#   elif KFIOC_HAS_DISK_STATS_NSECS && ! KFIOC_X_PART_STAT_REQUIRES_CPU
            part_stat_add(&gd->part0, nsecs[1],   kfio_div64_64(duration * HZ, FIO_USEC_PER_SEC));
#   else
#    if KFIOC_X_PART_STAT_REQUIRES_CPU
            part_stat_add(cpu, &gd->part0, ticks[1],   kfio_div64_64(duration * HZ, 1000000));
#    else
            part_stat_add(&gd->part0, ticks[1],   kfio_div64_64(duration * HZ, 1000000));
#    endif
#   endif /* KFIOC_HAS_DISK_STATS_NSECS */
            part_stat_unlock();
#  endif /* defined(CONFIG_PREEMPT_RT) */
# else /* KFIOC_PARTITION_STATS */

#  if KFIOC_HAS_DISK_STATS_READ_WRITE_ARRAYS
            disk_stat_inc(gd, ios[1]);
            disk_stat_add(gd, sectors[1], totalsize >> 9);
#   if KFIOC_HAS_DISK_STATS_NSECS
            disk_stat_add(gd, nsecs[1], jiffies_to_nsecs(fusion_usectohz(duration)));
#   else
            disk_stat_add(gd, ticks[1], fusion_usectohz(duration));
#   endif
#  else /* KFIOC_HAS_DISK_STATS_READ_WRITE_ARRAYS */
            disk_stat_inc(gd, writes);
            disk_stat_add(gd, write_sectors, totalsize >> 9);
            disk_stat_add(gd, write_ticks, fusion_usectohz(duration));
#  endif /* else ! KFIOC_HAS_DISK_STATS_READ_WRITE_ARRAYS */
            // TODO: #warning Need disk_round_stats() implementation to replace GPL version.
            // disk_round_stats(gd);
            disk_stat_add(gd, time_in_queue, kfio_get_gd_in_flight(disk, BIO_DIR_WRITE));
# endif /* else ! KFIOC_PARTITION_STATS */
        }
    }
    else if (dir == BIO_DIR_READ)
    {
        if (disk->use_workqueue != USE_QUEUE_RQ && disk->use_workqueue != USE_QUEUE_MQ)
        {
#if !(KFIOC_PARTITION_STATS && \
      (defined(CONFIG_PREEMPT_RT) || defined(CONFIG_TREE_PREEMPT_RCU) || defined(CONFIG_PREEMPT_RCU)))
            struct gendisk *gd = disk->gd;
#endif
# if KFIOC_PARTITION_STATS
# if !defined(CONFIG_PREEMPT_RT) && !defined(CONFIG_TREE_PREEMPT_RCU) && !defined(CONFIG_PREEMPT_RCU)
            int cpu;

            /* part_stat_lock() with defined(CONFIG_PREEMPT_RT) can't be used!
               It ends up calling rcu_read_update which is GPL in the RT patch set */
            cpu = part_stat_lock();
#  if KFIOC_X_PART_STAT_REQUIRES_CPU
            part_stat_inc(cpu, &gd->part0, ios[0]);
            part_stat_add(cpu, &gd->part0, sectors[0], totalsize >> 9);
#  else
            part_stat_inc(&gd->part0, ios[0]);
            part_stat_add(&gd->part0, sectors[0], totalsize >> 9);
#  endif
#  if KFIOC_HAS_DISK_STATS_NSECS && KFIOC_X_PART_STAT_REQUIRES_CPU
            part_stat_add(cpu, &gd->part0, nsecs[0],   duration * FIO_NSEC_PER_USEC);
#  elif KFIOC_HAS_DISK_STATS_NSECS
            part_stat_add(&gd->part0, nsecs[0],   kfio_div64_64(duration * HZ, FIO_USEC_PER_SEC));
#  else
#    if KFIOC_X_PART_STAT_REQUIRES_CPU
            part_stat_add(cpu, &gd->part0, ticks[0],   kfio_div64_64(duration * HZ, 1000000));
#    else
            part_stat_add(&gd->part0, ticks[0],   kfio_div64_64(duration * HZ, 1000000));
#    endif
#  endif /* KFIOC_HAS_DISK_STATS_NSECS && KFIOC_X_PART_STAT_REQUIRES_CPU */
            part_stat_unlock();
# endif /* defined(CONFIG_PREEMPT_RT) */
# else /* KFIOC_PARTITION_STATS */
#  if KFIOC_HAS_DISK_STATS_READ_WRITE_ARRAYS
            disk_stat_inc(gd, ios[0]);
            disk_stat_add(gd, sectors[0], totalsize >> 9);
#  if KFIOC_HAS_DISK_STATS_NSECS
            disk_stat_add(gd, nsecs[0], fusion_usectohz(duration));
#  else
            disk_stat_add(gd, ticks[0], fusion_usectohz(duration));
#  endif
#  else /* KFIO_C_HAS_DISK_STATS_READ_WRITE_ARRAYS */
            disk_stat_inc(gd, reads);
            disk_stat_add(gd, read_sectors, totalsize >> 9);
            disk_stat_add(gd, read_ticks, fusion_usectohz(duration));
#  endif /* else ! KFIO_C_HAS_DISK_STATS_READ_WRITE_ARRAYS */

            // TODO: #warning Need disk_round_stats() implementation to replace GPL version.
            // disk_round_stats(gd);
            disk_stat_add(gd, time_in_queue, kfio_get_gd_in_flight(disk, BIO_DIR_READ));
# endif /* else ! KFIOC_PARTITION_STATS */
        }
    }
#endif /* !defined(__VMKLNX__) */
}

#if !defined(__VMKLNX__) && !KFIOC_PARTITION_STATS
static int kfio_get_gd_in_flight(kfio_disk_t *disk, int rw)
{
    struct gendisk *gd = disk->gd;
# if KFIOC_PARTITION_STATS
#  if KFIOC_HAS_INFLIGHT_RW || KFIOC_HAS_INFLIGHT_RW_ATOMIC
    int dir = 0;

    // In the Linux kernel the direction isn't explicitly defined, however
    // in linux/bio.h, you'll notice that its referenced as 1 for write and 0
    // for read.

    if (rw == BIO_DIR_WRITE)
        dir = 1;

#   if KFIOC_HAS_INFLIGHT_RW_ATOMIC
    return atomic_read(&gd->part0.in_flight[dir]);
#   else
    return gd->part0.in_flight[dir];
#   endif /* KFIOC_HAS_INFLIGHT_RW_ATOMIC */
#  elif KFIOC_X_PART0_HAS_IN_FLIGHT
    return gd->part0.in_flight;
#  else
    return part_stat_read(&gd->part0, ios[STAT_WRITE]);
#  endif /* KFIOC_HAS_INFLIGHT_RW */
# else
    return gd->in_flight;
# endif
}
#endif /* !defined(__VMKLNX__) && !KFIOC_PARTITION_STATS */

void linux_bdev_update_inflight(struct fio_bdev *bdev, int rw, int in_flight)
{
    kfio_disk_t *disk = (kfio_disk_t *)bdev->bdev_gd;
    struct gendisk *gd;

    if (disk == NULL || disk->gd == NULL)
    {
        return;
    }

    gd = disk->gd;

    if (disk->use_workqueue != USE_QUEUE_RQ && disk->use_workqueue != USE_QUEUE_MQ)
    {
#if KFIOC_PARTITION_STATS
#if KFIOC_HAS_INFLIGHT_RW || KFIOC_HAS_INFLIGHT_RW_ATOMIC
        // In the Linux kernel the direction isn't explicitly defined, however
        // in linux/bio.h, you'll notice that its referenced as 1 for write and 0
        // for read.
        int dir = 0;

        if (rw == BIO_DIR_WRITE)
            dir = 1;

#if KFIOC_HAS_INFLIGHT_RW_ATOMIC
        atomic_set(&gd->part0.in_flight[dir], in_flight);
#else
        gd->part0.in_flight[dir] = in_flight;
#endif
#elif KFIOC_X_PART0_HAS_IN_FLIGHT
        gd->part0.in_flight = in_flight;
#else
        part_stat_set_all(&gd->part0, in_flight);
#endif
#else
        gd->in_flight = in_flight;
#endif
    }
}

/**
 * @brief returns 1 if bio is O_SYNC priority
 */
#if KFIOC_DISCARD == 1
static int kfio_bio_is_discard(struct bio *bio)
{
#if !KFIOC_HAS_SEPARATE_OP_FLAGS
#if KFIOC_HAS_UNIFIED_BLKTYPES
    /*
     * RHEL6.1 backported a partial set of the unified blktypes, but
     * still has separate bio and req DISCARD flags. If BIO_RW_DISCARD
     * exists, then that is used on the bio.
     */
#if KFIOC_HAS_BIO_RW_DISCARD
    return bio->bi_rw & (1 << BIO_RW_DISCARD);
#else
    return bio->bi_rw & REQ_DISCARD;
#endif
#else
    return bio_rw_flagged(bio, BIO_RW_DISCARD);
#endif
#else
    return bio_op(bio) == REQ_OP_DISCARD;
#endif
}
#endif

// kfio_dump_bio not supported for ESX4
#if !defined(__VMKLNX__)
/// @brief   Dump an OS bio to the log
/// @param   msg   prefix for message
/// @param   bio   the bio to drop
static void kfio_dump_bio(const char *msg, const struct bio * const bio)
{
    uint64_t sector;
    kassert(bio);

    // Use a local conversion to avoid printf format warnings on some platforms
    sector = (uint64_t)BI_SECTOR(bio);
#if KFIOC_HAS_SEPARATE_OP_FLAGS
    infprint("%s: sector: %llx: flags: %lx : op: %x : op_flags: %x : vcnt: %x", msg,
             sector, (unsigned long)bio->bi_flags, bio_op(bio), bio_flags(bio), bio->bi_vcnt);
#else
    infprint("%s: sector: %llx: flags: %lx : rw: %lx : vcnt: %x", msg,
             sector, (unsigned long)bio->bi_flags, bio->bi_rw, bio->bi_vcnt);
#endif
#if KFIOC_X_BIO_HAS_BIO_SEGMENTS
    // need to put our own segment count here...
    infprint("%s : idx: %x : phys_segments: %x : size: %x",
             msg, BI_IDX(bio), bio_segments(bio), BI_SIZE(bio));
#else
    infprint("%s : idx: %x : phys_segments: %x : size: %x",
             msg, BI_IDX(bio), bio->bi_phys_segments, BI_SIZE(bio));
#endif
#if KFIOC_BIO_HAS_HW_SEGMENTS
    infprint("%s: hw_segments: %x : hw_front_size: %x : hw_back_size %x", msg,
             bio->bi_hw_segments, bio->bi_hw_front_size, bio->bi_hw_back_size);
#endif
#if KFIOC_BIO_HAS_SEG_SIZE
    infprint("%s: seg_front_size %x : seg_back_size %x", msg,
             bio->bi_seg_front_size, bio->bi_seg_back_size);
#endif
#if KFIOC_BIO_HAS_ATOMIC_REMAINING
    infprint("%s: remaining %x", msg, atomic_read(&bio->bi_remaining));
#endif
#if KFIOC_HAS_BIO_COMP_CPU
    infprint("%s: comp_cpu %u", msg, bio->bi_comp_cpu);
#endif

    infprint("%s: max_vecs: %x : cnt %x : io_vec %p : end_io: %p : private: %p",
             msg, bio->bi_max_vecs, kfio_bio_cnt(bio), bio->bi_io_vec,
             bio->bi_end_io, bio->bi_private);
#if KFIOC_BIO_HAS_DESTRUCTOR
    infprint("%s: destructor: %p", msg, bio->bi_destructor);
#endif
#if KFIOC_BIO_HAS_INTEGRITY
    // Note that we don't use KFIOC_BIO_HAS_SPECIAL as yet.
    infprint("%s: integrity: %p", msg, bio_integrity(bio) );
#endif
}
#endif // !__VMKLNX__

static inline void kfio_set_comp_cpu(kfio_bio_t *fbio, struct bio *bio)
{
    int cpu;
#if KFIOC_HAS_BIO_COMP_CPU
    cpu = bio->bi_comp_cpu;
    if (cpu == -1)
    {
        cpu = kfio_current_cpu();
    }
#else
    cpu = kfio_current_cpu();
#endif
    kfio_bio_set_cpu(fbio, cpu);
}

#if !defined(__VMKLNX__)

static unsigned long __kfio_bio_sync(struct bio *bio)
{
#if KFIOC_HAS_SEPARATE_OP_FLAGS
    return bio_flags(bio) == REQ_SYNC;
#else
#if KFIOC_HAS_UNIFIED_BLKTYPES
    return bio->bi_rw & REQ_SYNC;
#elif KFIOC_HAS_BIO_RW_FLAGGED
    return bio_rw_flagged(bio, BIO_RW_SYNCIO);
#else
    return bio_sync(bio);
#endif
#endif
}

static unsigned long __kfio_bio_atomic(struct bio *bio)
{
#ifdef REQ_ATOMIC
    return !!(bio->bi_rw & REQ_ATOMIC);
#elif KFIOC_HAS_BIO_RW_ATOMIC
    return bio_rw_flagged(bio, BIO_RW_ATOMIC);
#else
    return 0;
#endif
}

#if KFIOC_BIO_ERROR_CHANGED_TO_STATUS
static blk_status_t kfio_errno_to_blk_status(int error)
{
    // We would use the kernel function of the same name, but they decided to impede us by making it GPL.

    // Translate the possible errno values to blk_status_t values.
    // This is the reverse of the kernel blk_status_to_errno() function.
    blk_status_t blk_status;

    switch (error)
    {
        case 0:
            blk_status = BLK_STS_OK;
            break;
        case -EOPNOTSUPP:
            blk_status = BLK_STS_NOTSUPP;
            break;
        case -ETIMEDOUT:
            blk_status = BLK_STS_TIMEOUT;
            break;
        case -ENOSPC:
            blk_status = BLK_STS_NOSPC;
            break;
        case -ENOLINK:
            blk_status = BLK_STS_TRANSPORT;
            break;
        case -EREMOTEIO:
            blk_status = BLK_STS_TARGET;
            break;
        case -EBADE:
            blk_status = BLK_STS_NEXUS;
            break;
        case -ENODATA:
            blk_status = BLK_STS_MEDIUM;
            break;
        case -EILSEQ:
            blk_status = BLK_STS_PROTECTION;
            break;
        case -ENOMEM:
            blk_status = BLK_STS_RESOURCE;
            break;
        case -EAGAIN:
            blk_status = BLK_STS_AGAIN;
            break;
        default:
            blk_status = BLK_STS_IOERR;
            break;
    }

    return blk_status;
}
#endif /* KFIOC_BIO_ERROR_CHANGED_TO_STATUS */

#if KFIOC_HAS_END_REQUEST
static int errno_to_uptodate(int error)
{
    // Convert an errno value to an uptodate value.
    // uptodate defines 1=success, 0=general error (EIO) and <0=specific error.
    // In this case we'll never have the general error returned, only success (0) or specific errno values.
    return error == 0? 1 : error;
}
#endif  /* KFIOC_HAS_END_REQUEST */

static void __kfio_bio_complete(struct bio *bio, uint32_t bytes_complete, int error)
{
#if KFIOC_BIO_ENDIO_REMOVED_ERROR
#if KFIOC_BIO_ERROR_CHANGED_TO_STATUS
    // bi_status is type blk_status_t, not an int errno, so must translate as necessary.
    blk_status_t bio_status = BLK_STS_OK;

    if (unlikely(error != 0))
    {
        bio_status = kfio_errno_to_blk_status(error);
    }
    bio->bi_status = bio_status;            /* bi_error was changed to bi_status <sigh> */
#else
    bio->bi_error = error;                  /* now a member of the bio struct */
#endif /* KFIOC_BIO_ERROR_CHANGED_TO_STATUS */
#endif /* !KFIOC_BIO_ENDIO_HAS_ERROR */

    bio_endio(bio
#if KFIOC_BIO_ENDIO_HAS_BYTES_DONE
              , bytes_complete
#endif /* KFIOC_BIO_ENDIO_HAS_BYTES_DONE */
#if !KFIOC_BIO_ENDIO_REMOVED_ERROR
              , error
#endif /* KFIOC_BIO_ENDIO_HAS_ERROR */
              );
}

static void kfio_bio_completor(kfio_bio_t *fbio, uint64_t bytes_complete, int error)
{
    struct bio *bio = (struct bio *)fbio->fbio_parameter;
    uint64_t bytes_completed = 0;

    if (unlikely(fbio->fbio_flags & KBIO_FLG_DUMP))
    {
        kfio_dump_fbio(fbio);
    }

    do
    {
        struct bio *next = bio->bi_next;

        bytes_completed += BI_SIZE(bio);

        if (unlikely(fbio->fbio_flags & KBIO_FLG_DUMP))
        {
            kfio_dump_bio(FIO_DRIVER_NAME, bio);
        }

        __kfio_bio_complete(bio, BI_SIZE(bio), error);
        bio = next;
    } while (bio);

    kassert(bytes_complete == bytes_completed);
}

static fio_bid_t linux_bio_get_bid(struct fio_bdev *bdev, struct bio *bio)
{
    kassert(BI_SECTOR(bio) * KERNEL_SECTOR_SIZE % bdev->bdev_block_size == 0);

    return BI_SECTOR(bio) * KERNEL_SECTOR_SIZE / bdev->bdev_block_size;
}

static fio_blen_t linux_bio_get_blen(struct fio_bdev *bdev, struct bio *bio)
{
    kassert(BI_SIZE(bio) % bdev->bdev_block_size == 0);

    return BI_SIZE(bio) / bdev->bdev_block_size;
}

/*
 * Attempt to add a Linux bio to an existing fbio. This is only allowed
 * when the bio is LBA contiguous with the fbio, and it's the same type.
 * We also require enough room in the SG list. A return value of 1 here
 * does not mean error, it just means that the existing fbio should be
 * submitted and a new one allocated for this bio.
 */
static int kfio_kbio_add_bio(kfio_bio_t *fbio, struct bio *bio)
{
    struct fio_bdev  *bdev = fbio->fbio_bdev;
    fio_blen_t blen;
    int error;

#if KFIOC_DISCARD == 1
    if (kfio_bio_is_discard(bio))
    {
        return 1;
    }
#endif

    /*
     * Need matching direction, and sequential offset
     */
    if ((bio_data_dir(bio) == WRITE && fbio->fbio_cmd != KBIO_CMD_WRITE) ||
        (bio_data_dir(bio) == READ && fbio->fbio_cmd != KBIO_CMD_READ))
    {
        return 1;
    }

    // Don't merge atomic requests.
    if (fbio->fbio_flags & KBIO_FLG_ATOMIC)
    {
        return 1;
    }

#if PORT_SUPPORTS_FIO_REQ_ATOMIC
    if ((bio_data_dir(bio) == WRITE && (bio->bi_flags & FIO_REQ_ATOMIC)))
    {
        return 1;
    }
#endif

    if (fio_range_upper_bound(fbio->fbio_range) !=
        linux_bio_get_bid(bdev, bio))
    {
        return 1;
    }

    blen = linux_bio_get_blen(bdev, bio);

    if (fbio->fbio_range.length + blen > FUSION_MAX_SECTORS_PER_OS_RW_REQUEST)
    {
        return 1;
    }

    error = kfio_sgl_map_bio(fbio->fbio_sgl, bio);
    if (error)
    {
        return 1;
    }

    fbio->fbio_range.length += blen;
    return 0;
}

/// @brief Count the size of the bio chain and make sure it will fit in a kfio_bio
///
/// @param bdev        bdev we'll be writing to
/// @param first_bio   The bio chain to count up.
static int kfio_bio_chain_count(struct fio_bdev *bdev,
                                struct bio *first_bio)
{
    struct bio *bio;
    int segments = 0;
    unsigned long long total_length = 0;

    bio = first_bio;
    while (bio)
    {
        segments++;

        if (!__kfio_bio_atomic(bio))
        {
            return -EINVAL;
        }

#if KFIOC_DISCARD == 1
        if (kfio_bio_is_discard(bio))
        {
            bio = bio->bi_next;
            continue;
        }
#endif

        if (bio_data_dir(bio) != WRITE)
        {
            return -EINVAL;
        }

        total_length += BI_SIZE(bio);

        bio = bio->bi_next;
    }

    /* make sure we fit inside all the atomic limits */
    if (segments > kfio_bdev_max_atomic_iovs(bdev))
    {
        dbgprint(DBGS_ATOMIC, "%s: atomic-write: IOV count (%d) exceeds the max count (%d) supported.\n",
                 fio_bdev_get_bus_name(bdev), segments, kfio_bdev_max_atomic_iovs(bdev));
        return -EINVAL;
    }

    if (total_length > kfio_bdev_max_atomic_total_write_size_bytes(bdev))
    {
        dbgprint(DBGS_ATOMIC, "%s: atomic-write: Total write size (%llu) exceeds the max (%d) supported.\n",
                 fio_bdev_get_bus_name(bdev), total_length, kfio_bdev_max_atomic_total_write_size_bytes(bdev));
        return -EINVAL;
    }

    return segments;
}

/// @brief allocate and setup an iov array to represent each bio in a chain
///
/// @param bdev         bdev we'll be writing to
/// @param first_fbio   list of kfio_bios to map onto.
/// @param first_bio    list of linux bios
///
/// @return             Zero for success, < 0 on error
static int kfio_bio_chain_to_fbio_chain(struct fio_bdev *bdev,
                                        kfio_bio_t *first_fbio,
                                        struct bio *first_bio)
{
    struct bio *bio;
    kfio_bio_t *fbio;
    int ret = 0;

    /*
     * we have a 1:1 mapping between bios and kfio_bios here.  Setup
     * the kfio_bios to match the blocks and pages covered in each biovec
     */
    bio = first_bio;
    fbio = first_fbio;
    while (bio != NULL && fbio != NULL)
    {
        fbio->fbio_range.base = linux_bio_get_bid(bdev, bio);
        fbio->fbio_range.length = linux_bio_get_blen(bdev, bio);
        fbio->fbio_flags = KBIO_FLG_ATOMIC;

#if KFIOC_DISCARD == 1
        if (kfio_bio_is_discard(bio))
        {
            fbio->fbio_cmd = KBIO_CMD_DISCARD;
        }
        else
#endif
        {
            fbio->fbio_cmd = KBIO_CMD_WRITE;

            ret = kfio_sgl_map_bio(fbio->fbio_sgl, bio);
            if (ret)
            {
                break;
            }
        }
        bio = bio->bi_next;
        fbio = fbio->fbio_next;
    }

    return ret;
}

/// @brief call the bio endio function for each bio in a chain
/// @return the number of bytes completed
static int complete_bio_chain(struct bio *bio, int error)
{
    uint64_t bytes_completed = 0;
    do
    {
        struct bio *next = bio->bi_next;
        bytes_completed += BI_SIZE(bio);
        __kfio_bio_complete(bio, BI_SIZE(bio), error);
        bio = next;
    } while (bio);

    return bytes_completed;
}

/// @brief completion callback for a chained atomic io
static void fusion_handle_atomic_chain_completor(kfio_bio_t *fbio,
                                                 uint64_t bytes_complete,
                                                 int error)
{
    struct bio *bio = (struct bio *)fbio->fbio_parameter;
    uint64_t bytes_completed = 0;

    /*
     * we pull the atomic ctx out of the fbio parameter and then our bio chain
     * comes out of the atomic parameter in the ctx
     */
    bytes_completed = complete_bio_chain(bio, error);

    kassert(bytes_complete == bytes_completed);
}

/// @brief convert a chained bio into an atomic fbio and submit
///
/// @param queue        The linux request queue
/// @param first_bio    our chain of linux bios
static void kfio_submit_atomic_chain(struct request_queue *queue,
                                     struct bio *first_bio)
{
    struct kfio_disk *disk = queue->queuedata;
    struct fio_bdev  *bdev = disk->bdev;
    kfio_bio_t       *first_fbio;
    int ret = 0;
    int segments = 0;

    ret = kfio_bio_chain_count(bdev, first_bio);
    if (ret < 0)
    {
        goto fail;
    }
    segments = ret;

    first_fbio = kfio_bio_alloc_chain(bdev, segments);
    if (first_fbio == NULL)
    {
        ret = -ENOMEM;
        goto fail;
    }

    ret = kfio_bio_chain_to_fbio_chain(bdev, first_fbio, first_bio);
    if (ret < 0)
    {
        goto fail_bio;
    }
    segments = ret;

    first_fbio->fbio_completor = fusion_handle_atomic_chain_completor;
    first_fbio->fbio_parameter = (fio_uintptr_t)first_bio;
    kfio_bio_submit(first_fbio);
    return;

fail_bio:
    kfio_bio_free(first_fbio);

fail:
    complete_bio_chain(first_bio, ret);
}

static kfio_bio_t *kfio_map_to_fbio(struct request_queue *queue, struct bio *bio)
{
    struct kfio_disk *disk = queue->queuedata;
    struct fio_bdev  *bdev = disk->bdev;
    kfio_bio_t       *fbio;
    int error;
#if ENABLE_LAT_RECORD
    uint64_t         ts = kfio_rdtsc();
#endif

    if ((BI_SECTOR(bio) * KERNEL_SECTOR_SIZE % bdev->bdev_block_size != 0) ||
        (BI_SIZE(bio) % bdev->bdev_block_size != 0))
    {
#if KFIOC_HAS_SEPARATE_OP_FLAGS
        engprint("Rejecting malformed bio %p sector %lu size 0x%08x flags 0x%08lx op 0x%08x op_flags 0x%04x\n", bio,
                 (unsigned long)BI_SECTOR(bio), BI_SIZE(bio), (unsigned long) bio->bi_flags, bio_op(bio), bio_flags(bio));
#else
        engprint("Rejecting malformed bio %p sector %lu size 0x%08x flags 0x%08lx rw 0x%08lx\n", bio,
                 (unsigned long)BI_SECTOR(bio), BI_SIZE(bio), (unsigned long) bio->bi_flags, bio->bi_rw);
#endif
        return NULL;
    }

    /*
     * This should use kfio_bio_try_alloc and should automatically
     * retry later when some requests become available.
     */
    fbio = kfio_bio_alloc(bdev);
    if (fbio == NULL)
    {
        return NULL;
    }

#if ENABLE_LAT_RECORD
    fusion_lat_set_bio(fbio, 0, ts);
#endif
    fusion_lat_record_bio(fbio, 1);

    // Convert Linux bio to Fusion-io bio and send it down to processing.
    fbio->fbio_flags = 0;

    fbio->fbio_range.base = linux_bio_get_bid(bdev, bio);
    fbio->fbio_range.length = linux_bio_get_blen(bdev, bio);

    fbio->fbio_completor = kfio_bio_completor;
    fbio->fbio_parameter = (fio_uintptr_t)bio;

    kfio_set_comp_cpu(fbio, bio);

#if KFIOC_DISCARD == 1
    if (kfio_bio_is_discard(bio))
    {
        fbio->fbio_cmd = KBIO_CMD_DISCARD;
    }
    else
#endif
    {
        if (bio_data_dir(bio) == WRITE)
        {
            fbio->fbio_cmd = KBIO_CMD_WRITE;

            if (__kfio_bio_sync(bio) != 0)
            {
                fbio->fbio_flags |= KBIO_FLG_SYNC;
            }

#if PORT_SUPPORTS_FIO_REQ_ATOMIC
            if (bio->bi_flags & FIO_REQ_ATOMIC ||
                __kfio_bio_atomic(bio))
            {
                fbio->fbio_flags |= KBIO_FLG_ATOMIC;
            }
#endif // PORT_SUPPORTS_FIO_REQ_ATOMIC
        }
        else
        {
            fbio->fbio_cmd = KBIO_CMD_READ;
        }

        error = kfio_sgl_map_bio(fbio->fbio_sgl, bio);
        if (error != 0)
        {
            /* This should not happen. */
            kfio_bio_free(fbio);
            return NULL;
        }
    }

    return fbio;
}

static void kfio_kickoff_plugged_io(struct request_queue *q, struct bio *bio)
{
    struct bio *tail, *next;
    kfio_bio_t *fbio;

    tail = NULL;
    fbio = NULL;
    do
    {
        next = bio->bi_next;
        bio->bi_next = NULL;

        if (fbio)
        {
            if (!kfio_kbio_add_bio(fbio, bio))
            {
                /*
                 * Keep track of the tail, until we submit
                 */
                if (tail)
                {
                    tail->bi_next = bio;
                }
                tail = bio;
                continue;
            }

            kfio_bio_submit(fbio);
            fbio = NULL;
            tail = NULL;
        }

        fbio = kfio_map_to_fbio(q, bio);
        if (!fbio)
        {
            __kfio_bio_complete(bio,  0, -EIO);
            continue;
        }

        tail = bio;
    } while ((bio = next) != NULL);

    if (fbio)
    {
        kfio_bio_submit(fbio);
    }
}

static int kfio_bio_should_submit_now(struct bio *bio)
{
    /*
     * Some kernels have a bug where it forgets to mark a discard
     * as sync (or appropriately unplug). Check this here and ensure
     * that we run it now instead of deferring. Check for a zero sized
     * requests too, as some "special" requests (such as a FLUSH) suffer
     * from the same bug.
     */
    if (!BI_SIZE(bio))
    {
        return 1;
    }
#if KFIOC_HAS_BIO_RW_SYNC == 1
    if (bio->bi_rw & (1 << BIO_RW_SYNC))
    {
        return 1;
    }
#endif

#if KFIOC_HAS_BIO_RW_UNPLUG == 1
    if (bio->bi_rw & (1 << BIO_RW_UNPLUG))
    {
        return 1;
    }
#endif

#if KFIOC_HAS_REQ_UNPLUG == 1
    if (bio->bi_rw & REQ_UNPLUG)
    {
        return 1;
    }
#endif
#if KFIOC_DISCARD == 1
    return kfio_bio_is_discard(bio);
#else
    return 0;
#endif
}

#if KFIOC_REQUEST_QUEUE_HAS_UNPLUG_FN
static void kfio_unplug(struct request_queue *q)
{
    struct kfio_disk *disk = q->queuedata;
    struct bio *bio = NULL;

    spin_lock_irq(q->queue_lock);
    if (blk_remove_plug(q))
    {
        bio = disk->bio_head;
        disk->bio_head = NULL;
        disk->bio_tail = NULL;
    }
    spin_unlock_irq(q->queue_lock);

    if (bio)
    {
        kfio_kickoff_plugged_io(q, bio);
    }
}

static void test_safe_plugging(void)
{
    /* empty */
}

#else

/* some 3.0 kernels call our unplug callback with
 * irqs off.  We use this variable to track it and
 * avoid plugging on those kernels.
 */
static int dangerous_plugging_callback = -1;

struct kfio_plug {
    struct blk_plug_cb cb;
    struct kfio_disk *disk;
    struct bio *bio_head;
    struct bio *bio_tail;
    struct work_struct work;
};

#if KFIOC_REQUEST_QUEUE_UNPLUG_FN_HAS_EXTRA_BOOL_PARAM
static void kfio_unplug_do_cb(struct work_struct *work)
{
    struct kfio_plug *plug = container_of(work, struct kfio_plug, work);
    struct kfio_disk *disk = plug->disk;
    struct bio *bio;

    bio = plug->bio_head;
    kfree(plug);

    if (bio)
    {
        kfio_kickoff_plugged_io(disk->rq, bio);
    }
}

static void kfio_unplug_cb(struct blk_plug_cb *cb, bool from_schedule)
{
    struct kfio_plug *plug = container_of(cb, struct kfio_plug, cb);
#if KFIOC_KBLOCKD_SCHEDULE_HAS_QUEUE_ARG
    struct kfio_disk *disk = plug->disk;
#endif /* KFIOC_KBLOCKD_SCHEDULE_HAS_QUEUE_ARG */

    if (from_schedule)
    {
        INIT_WORK(&plug->work, kfio_unplug_do_cb);
#if KFIOC_KBLOCKD_SCHEDULE_HAS_QUEUE_ARG
        kblockd_schedule_work(disk->rq, &plug->work);
#else
        kblockd_schedule_work(&plug->work);
#endif /* KFIOC_KBLOCKD_SCHEDULE_HAS_QUEUE_ARG */
        return;
    }

    kfio_unplug_do_cb(&plug->work);
}
#else
static void kfio_unplug_cb(struct blk_plug_cb *cb)
{
    BUG();
}
#endif

/*
 * this is used once while we create the block device,
 * just to make sure unplug callbacks aren't run with irqs off
 */
struct test_plug
{
    struct blk_plug_cb cb;
    int safe;
};


#if KFIOC_REQUEST_QUEUE_UNPLUG_FN_HAS_EXTRA_BOOL_PARAM
static void safe_unplug_cb(struct blk_plug_cb *cb, bool from_schedule)
#else
static void safe_unplug_cb(struct blk_plug_cb *cb)
#endif
{
    struct test_plug *test_plug = container_of(cb, struct test_plug, cb);

    if (irqs_disabled())
        test_plug->safe = 0;
    else
        test_plug->safe = 1;
}

/*
 * trigger an unplug callback without any IO.  This tests
 * if irqs are on when the callback runs, it is safe
 * to use our plugging code.  Otherwise we have to turn it
 * off.  Some 3.0 based kernels have irqs off during the
 * unplug callback.
 */
static void test_safe_plugging(void)
{
    struct blk_plug plug;
    struct test_plug test_plug;

    /* we only need to do this probe once */
    if (dangerous_plugging_callback != -1)
        return;

#if !KFIOC_REQUEST_QUEUE_UNPLUG_FN_HAS_EXTRA_BOOL_PARAM
    /* plug callback should not sleep if called in scheduler. We need this
     * parameter to avoid sleep in the callback
     */
    dangerous_plugging_callback = 1;
    infprint("Unplug callbacks are run without from_schedule parameter.  Plugging disabled\n");
    return;
#endif

    /* setup a plug.  We don't need to worry about
     * the block device being ready because we won't do any IO
     */
    blk_start_plug(&plug);

    test_plug.safe = -1;

    /* add ourselves to the list of callbacks */
    test_plug.cb.callback = safe_unplug_cb;
    list_add_tail(&test_plug.cb.list, &plug.cb_list);

    /*
     * force a schedule.  Later kernels won't run the callbacks
     * if our state is TASK_RUNNING at the time of the schedule
     */
    set_current_state(TASK_UNINTERRUPTIBLE);
    schedule_timeout(1);
    set_current_state(TASK_RUNNING);

    /* remove our plug */
    blk_finish_plug(&plug);

    /* if we failed the safety test, set our global to disable plugging */
    if (test_plug.safe == 0) {
        infprint("Unplug callbacks are run with irqs off.  Plugging disabled\n");
        dangerous_plugging_callback = 1;
    } else {
        dangerous_plugging_callback = 0;
    }
}
#endif

static struct request_queue *kfio_alloc_queue(struct kfio_disk *dp,
                                              kfio_numa_node_t node)
{
    struct request_queue *rq;

    test_safe_plugging();

#if KFIOC_HAS_BLK_ALLOC_QUEUE_NODE
    rq = blk_alloc_queue_node(GFP_NOIO, node);
#else
    rq = blk_alloc_queue(GFP_NOIO);
#endif
    if (rq != NULL)
    {
        rq->queuedata = dp;
        blk_queue_make_request(rq, kfio_make_request);
#if KFIOC_X_REQUEST_QUEUE_HAS_QUEUE_LOCK_POINTER
        rq->queue_lock = (spinlock_t *)&dp->queue_lock;
#else
        memcpy(&dp->queue_lock, &rq->queue_lock, sizeof(dp->queue_lock));
#endif
#if KFIOC_REQUEST_QUEUE_HAS_UNPLUG_FN
        rq->unplug_fn = kfio_unplug;
#endif
    }
    return rq;
}

static int should_holdoff_writes(struct kfio_disk *disk)
{
    return fio_test_bit_atomic(KFIO_DISK_HOLDOFF_BIT, &disk->disk_state);
}
#endif /* !defined(__VMKLNX__) */

static void linux_bdev_backpressure(struct fio_bdev *bdev, int on)
{
    struct kfio_disk *disk = (struct kfio_disk *)bdev->bdev_gd;

    if (disk == NULL)
    {
        return;
    }

    if (on)
    {
        fio_set_bit_atomic(KFIO_DISK_HOLDOFF_BIT, &disk->disk_state);
    }
    else
    {
#if KFIOC_X_REQUEST_QUEUE_HAS_REQUEST_FN
        struct request_queue *q = disk->gd->queue;
#endif
        fusion_cv_lock_irq(&disk->state_lk);
        fio_clear_bit_atomic(KFIO_DISK_HOLDOFF_BIT, &disk->disk_state);
        fusion_condvar_broadcast(&disk->state_cv);
        fusion_cv_unlock_irq(&disk->state_lk);

        /*
         * Re-kick the queue, if we stopped handing out writes
         * due to log pressure. Do this out-of-line, since we can
         * be called here with internal locks held and with IRQs
         * disabled.
         */
#if KFIOC_X_REQUEST_QUEUE_HAS_REQUEST_FN
        if (q->request_fn)
        {
            /*
             * The VMKernel initializes the request_queue in their implementation of
             * blk_queue_init().  However, they both neglect to initialize the
             * unplug handler, and also attempt to call it.  This badness is worked
             * around here and also in kfio_init_queue().
             */
#if defined(__VMKLNX__)
            if (q->unplug_work.work.pending & __WORK_OLD_COMPAT_BIT)
            {
                if (q->unplug_work.work.func.old == NULL)
                {
                    engprint("Null callback avoided; (__WORK_OLD_COMPAT_BIT)\n");
                    return;
                }
            }
            else
            {
                if (q->unplug_work.work.func.new == NULL)
                {
                    engprint("Null callback avoided; (!__WORK_OLD_COMPAT_BIT)\n");
                    return;
                }
            }
#endif /* defined(__VMKLNX__) */

            kfio_restart_queue(q);
        }
#endif /* KFIOC_X_REQUEST_QUEUE_HAS_REQUEST_FN */
    }
}

/*
 * Increment/decrement the exclusive lock pending count
 */
void linux_bdev_lock_pending(struct fio_bdev *bdev, int pending)
{
#if !defined(__VMKLNX__)
    kfio_disk_t *disk = bdev->bdev_gd;
    struct gendisk *gd;
    struct request_queue *q;

    if (disk == NULL || disk->gd == NULL || disk->gd->queue == NULL)
    {
        return;
    }

    gd = disk->gd;
    q = gd->queue;

#if KFIOC_X_REQUEST_QUEUE_HAS_REQUEST_FN
    /*
     * Only the request_fn driven model issues requests in a non-blocking
     * manner. The direct queued model does not need this.
     */
    if (q->request_fn == NULL)
    {
        return;
    }

    if (pending)
    {
        atomic_inc(&disk->lock_pending);
    }
    else
    {
        /*
         * Reset pending back to zero, since we now successfully dropped
         * the lock. If the pending value was bigger than 1, then somebody
         * else likely tried and failed to get the lock. In that case,
         * kick off the queue.
         */
        if (atomic_dec_return(&disk->lock_pending) > 0)
        {
            atomic_set(&disk->lock_pending, 0);
            kfio_restart_queue(q);
        }
    }
#endif /* defined(KFIOC_REQUEST_QUEUE_HAS_REQUEST_FN) */
#endif
}


#if !defined(__VMKLNX__)
static int holdoff_writes_under_pressure(struct kfio_disk *disk)
{
    int count = 0;

    while (should_holdoff_writes(disk))
    {
        int reason = FUSION_WAIT_TRIGGERED;
        fusion_cv_lock_irq(&disk->state_lk);
        // wait until unstalled. Hard-coded 1 minute timeout.
        while (should_holdoff_writes(disk))
        {
            reason = fusion_condvar_timedwait(&disk->state_cv,
                                              &disk->state_lk,
                                              5 * 1000 * 1000);
            if (reason == FUSION_WAIT_TIMEDOUT)
            {
                break;
            }
        }

        fusion_cv_unlock_irq(&disk->state_lk);

        if (reason == FUSION_WAIT_TIMEDOUT)
        {
            count++;
            engprint("%s %d: seconds %d %s\n", __FUNCTION__, __LINE__, count * 5,
                     should_holdoff_writes(disk) ? "stalled" : "unstalled");

            if (count >= 12)
            {
                return !should_holdoff_writes(disk);
            }
        }
    }

    return 1;
}

#if KFIOC_REQUEST_QUEUE_HAS_UNPLUG_FN
static inline void *kfio_should_plug(struct request_queue *q)
{
    if (use_workqueue == USE_QUEUE_NONE)
    {
        return q;
    }

    return NULL;
}
static struct bio *kfio_add_bio_to_plugged_list(void *data, struct bio *bio)
{
    struct request_queue *q = data;
    struct kfio_disk *disk = q->queuedata;
    struct bio *ret = NULL;

    spin_lock_irq(q->queue_lock);
    if (disk->bio_tail)
    {
        disk->bio_tail->bi_next = bio;
    }
    else
    {
        disk->bio_head = bio;
    }
    disk->bio_tail = bio;
    if (kfio_bio_should_submit_now(bio) && use_workqueue == USE_QUEUE_NONE)
    {
        ret = disk->bio_head;
        disk->bio_head = disk->bio_tail = NULL;
    }
    blk_plug_device(q);
    spin_unlock_irq(q->queue_lock);

    return ret;
}
#else
static void *kfio_should_plug(struct request_queue *q)
{
    struct kfio_disk *disk = q->queuedata;
    struct kfio_plug *kplug;
    struct blk_plug *plug;

    if (use_workqueue != USE_QUEUE_NONE)
    {
        return NULL;
    }

    /* this might be -1, 0, or 1 */
    if (dangerous_plugging_callback == 1)
        return NULL;

    plug = current->plug;
    if (!plug)
    {
        return NULL;
    }

    list_for_each_entry(kplug, &plug->cb_list, cb.list)
    {
        if (kplug->cb.callback == kfio_unplug_cb && kplug->disk == disk)
        {
            return kplug;
        }
    }

    kplug = kmalloc(sizeof(*kplug), GFP_ATOMIC);
    if (!kplug)
    {
        return NULL;
    }

    kplug->disk = disk;
    kplug->cb.callback = kfio_unplug_cb;
    kplug->bio_head = kplug->bio_tail = NULL;
    list_add_tail(&kplug->cb.list, &plug->cb_list);
    return kplug;
}
static struct bio *kfio_add_bio_to_plugged_list(void *data, struct bio *bio)
{
    struct kfio_plug *plug = data;
    struct bio *ret = NULL;

    if (plug->bio_tail)
    {
        plug->bio_tail->bi_next = bio;
    }
    else
    {
        plug->bio_head = bio;
    }
    plug->bio_tail = bio;

    if (kfio_bio_should_submit_now(bio) && use_workqueue == USE_QUEUE_NONE)
    {
        ret = plug->bio_head;
        plug->bio_head = plug->bio_tail = NULL;
    }

    return ret;
}
#endif

#if KFIOC_MAKE_REQUEST_FN_VOID
static void kfio_make_request(struct request_queue *queue, struct bio *bio)
#define FIO_MFN_RET
#elif KFIOC_MAKE_REQUEST_FN_UINT
static unsigned int kfio_make_request(struct request_queue *queue, struct bio *bio)
#define FIO_MFN_RET 0
#else
static int kfio_make_request(struct request_queue *queue, struct bio *bio)
#define FIO_MFN_RET 0
#endif
{
    struct kfio_disk *disk = queue->queuedata;
    void *plug_data;

    if (bio_data_dir(bio) == WRITE && !holdoff_writes_under_pressure(disk))
    {
        kassert_once(!"timed out waiting for queue to unstall.");
        __kfio_bio_complete(bio, 0, -EIO);
        return FIO_MFN_RET;
    }

#if KFIOC_HAS_BLK_QUEUE_SPLIT2
    // Split the incomming bio if it has more segments than we have scatter-gather DMA vectors,
    //   and re-submit the remainder to the request queue. blk_queue_split() does all that for us.
    // It appears the kernel quit honoring the blk_queue_max_segments() in about 4.13.

# if KFIOC_X_BIO_HAS_BIO_SEGMENTS
    if (bio_segments(bio) >= queue_max_segments(queue))
# elif KFIOC_X_BIO_HAS_BI_PHYS_SEGMENTS
    if (bio->bi_phys_segments >= queue_max_segments(queue))
# else
    if (bio_phys_segments(queue, bio) >= queue_max_segments(queue))
# endif
    {
        blk_queue_split(queue, &bio);
    }
#endif

#if KFIOC_HAS_BIO_COMP_CPU
    if (bio->bi_comp_cpu == -1)
    {
        bio_set_completion_cpu(bio, kfio_current_cpu());
    }
#endif

#if KFIOC_HAS_BLK_QUEUE_BOUNCE
    blk_queue_bounce(queue, &bio);
#endif /* KFIOC_HAS_BLK_QUEUE_BOUNCE */

    /*
     * The atomic chains have more overhead (using atomic contexts etc) so
     * only create them we have a chain of bios.  A single bio atomic is
     * flagged as atomic in kfio_map_to_fbio
     */
    if (bio->bi_next && __kfio_bio_atomic(bio) && bio_data_dir(bio) == WRITE)
    {
        kfio_submit_atomic_chain(queue, bio);
        return FIO_MFN_RET;
    }

    plug_data = kfio_should_plug(queue);
    if (!plug_data)
    {
        /*
         * No posting/deferral, kick off now
         */
        kfio_bio_t *fbio;

        fbio = kfio_map_to_fbio(queue, bio);
        if (fbio)
        {
            kfio_bio_submit(fbio);
        }
        else
        {
            __kfio_bio_complete(bio,  0, -EIO);
        }
    }
    else
    {
        struct bio *ret;

        /*
         * Queue up
         */
        ret = kfio_add_bio_to_plugged_list(plug_data, bio);

        if (ret != NULL)
        {
            /*
             * If kfio_add_bio_to_plugged_list() returned us work to do,
             * kick that off now.
             */
            kfio_kickoff_plugged_io(queue, ret);
        }
    }

    return FIO_MFN_RET;
}

#endif /* !defined(__VMKLNX__) */

#if KFIOC_BARRIER == 1
static void kfio_prepare_flush(struct request_queue *q, struct request *req)
{
}
#endif

#if KFIOC_USE_IO_SCHED

static unsigned long kfio_get_req_hard_nr_sectors(struct request *req)
{
    kassert(use_workqueue == USE_QUEUE_RQ || use_workqueue == USE_QUEUE_MQ);

# if KFIOC_USE_NEW_IO_SCHED
    return blk_rq_sectors(req);
# else
    return req->hard_nr_sectors;
# endif
}

static int kfio_end_that_request_first(struct request *req, int error, int count)
{
# if defined(__VMKLNX__)
    return 0;
# else

    kassert(use_workqueue == USE_QUEUE_RQ);

#  if KFIOC_HAS_END_REQUEST
    // end_that_request_first() takes an 'uptodate' parameter where 1=success, 0=generic error, <0=specific error.
    // Convert our incoming error value to an 'uptodate' value first.
    return end_that_request_first(req, errno_to_uptodate(error), count);
#  else
    {
#   if KFIOC_BIO_ERROR_CHANGED_TO_STATUS
        // bi_status is type blk_status_t, not an int errno, so must translate as necessary.
        blk_status_t bio_status = BLK_STS_OK;

        if (unlikely(error != 0))
        {
            bio_status = kfio_errno_to_blk_status(error);
        }
#   else
        int bio_status = error;
#   endif /* KFIOC_BIO_ERROR_CHANGED_TO_STATUS */

        // We are already holding the queue lock so we call __blk_end_request
        // If we ever decide to call this _without_ holding the lock, we should
        // use blk_end_request() instead.
        kassert(spin_is_locked(req->q->queue_lock));
        return __blk_end_request(req, bio_status, blk_rq_bytes(req));
    }
#  endif /* KFIOC_HAS_END_REQUEST */
# endif /* __VMKLNX__ */
}

static void kfio_end_that_request_last(struct request *req, int error)
{
# if !KFIOC_USE_NEW_IO_SCHED
    kassert(use_workqueue == USE_QUEUE_RQ || use_workqueue == USE_QUEUE_MQ);

#  if KFIOC_HAS_END_REQUEST
    // end_that_request_last() takes an 'uptodate' value, which is <=0 is failure, 1=success.
    //  Must convert our error value to an uptodate value first.
    end_that_request_last(req, errno_to_uptodate(error));
#  endif
# endif
}

static void kfio_end_request(struct request *req, int error)
{
    if (kfio_end_that_request_first(req, error, kfio_get_req_hard_nr_sectors(req)))
    {
        kfail();
    }
    kfio_end_that_request_last(req, error);
}

static void kfio_restart_queue(struct request_queue *q)
{
# if KFIOC_HAS_BLK_DELAY_QUEUE
    blk_delay_queue(q, 0);
# else
    if (!test_and_set_bit(QUEUE_FLAG_PLUGGED, &q->queue_flags))
    {
#  if KFIOC_KBLOCKD_SCHEDULE_HAS_QUEUE_ARG
        kblockd_schedule_work(q, &q->unplug_work);
#  else
        kblockd_schedule_work(&q->unplug_work);
#  endif /* KFIOC_KBLOCKD_SCHEDULE_HAS_QUEUE_ARG */
    }
# endif /* KFIOC_HAS_BLK_DELAY_QUEUE */
}

# if !defined(__VMKLNX__)
/*
 * Returns non-zero if we have pending requests, either at the OS level
 * or on our internal retry list
 */
static int kfio_has_pending_requests(struct request_queue *q)
{
        kfio_disk_t *disk = q->queuedata;
        int ret;

#  if KFIOC_USE_NEW_IO_SCHED
        ret = blk_peek_request(q) != NULL;
#  else
        ret = !elv_queue_empty(q);
#  endif
        return ret || disk->retry_cnt;
}

/*
 * Typeless container_of(), since we are abusing a different type for
 * our atomic list entry.
 */
#define void_container(e, t, f) (t*)(((fio_uintptr_t)(t*)((char *)(e))-((fio_uintptr_t)&((t*)0)->f)))

/*
 * Splice completion list to local list and handle them
 */
static int complete_list_entries(struct request_queue *q, int error, struct kfio_disk *dp)
{
    struct request *req;
    struct fio_atomic_list list;
    struct fio_atomic_list *entry, *tmp;
    int completed = 0;

    fusion_atomic_list_init(&list);
    fusion_atomic_list_splice(&dp->comp_list, &list);
    if (fusion_atomic_list_empty(&list))
        return completed;

    fusion_atomic_list_for_each(entry, tmp, &list)
    {
        req = void_container(entry, struct request, special);
        kfio_end_request(req, error);
        completed++;
    }

    kassert(dp->pending >= completed);
    dp->pending -= completed;
    return completed;
}

static void kfio_blk_complete_request(struct request *req, int error)
{
    struct request_queue *q = req->q;
    struct kfio_disk *dp = q->queuedata;
    struct fio_atomic_list *entry;
    sector_t last_sector;
    int      last_rw;
    int i;

    /*
     * Save a local copy of the last sector in the request before putting
     * it onto atomic completion list. Once it is there, requests is up for
     * grabs for others completors and we cannot legally access its fields
     * anymore.
     */
    last_sector = blk_rq_pos(req) + (blk_rq_bytes(req) >> 9);
    last_rw = rq_data_dir(req);

    /*
     * Add this completion entry atomically to the shared completion
     * list.
     */
    entry = (struct fio_atomic_list *) &req->special;
    fusion_atomic_list_init(entry);
    fusion_atomic_list_add(entry, &dp->comp_list);

    /*
     * Next try and grab the queue_lock, which we need for completion.
     * If we succeed, set that we are now doing completions with the
     * KFIO_DISK_COMPLETION bit. If we fail, check if someone is already
     * doing completions. If they are, we are golden. They will see our
     * completion entry and do it in due time. If not, repeat the lock
     * check and completion bit check.
     */
    while (!fusion_spin_trylock_irqsave(&dp->queue_lock))
    {
        if (fio_test_bit_atomic(KFIO_DISK_COMPLETION, &dp->disk_state))
            return;

        cpu_relax();
    }

    if (dp->last_dispatch == last_sector && dp->last_dispatch_rw == last_rw)
    {
        dp->last_dispatch = 0;
        dp->last_dispatch_rw = -1;
    }

    /*
     * Great, lets do completions. Mark us as doing that, and complete
     * the list for up to 8 runs (or until it was empty).
     */
    fio_set_bit_atomic(KFIO_DISK_COMPLETION, &dp->disk_state);
    for (i = 0; i < 8; i++)
    {
        if (!complete_list_entries(q, error, dp))
        {
            break;
        }
    }

    /*
     * Complete the list after clearing the bit, in case we raced with
     * someone adding entries.
     */
    fio_clear_bit_atomic(KFIO_DISK_COMPLETION, &dp->disk_state);
    complete_list_entries(q, error, dp);

    /*
     * We usually don't have to re-kick the queue, it happens automatically.
     * But if we dropped out of the queueing loop, we do have to guarantee
     * that we kick things into gear on our own. So do that if we end up
     * in the situation where we have pending IO from the OS but nothing
     * left internally.
     */
    if (!dp->pending && kfio_has_pending_requests(q))
    {
        kfio_restart_queue(q);
    }

    fusion_spin_unlock_irqrestore(&dp->queue_lock);
}

# else // #if !defined(__VMKLNX__)

static void kfio_blk_do_softirq(struct request *req)
{
    struct request_queue *rq;
    struct kfio_disk     *dp;

    rq = req->q;
    dp = rq->queuedata;

    fusion_spin_lock_irqsave(&dp->queue_lock);
    kfio_end_request(req, error);
    fusion_spin_unlock_irqrestore(&dp->queue_lock);
}

static void kfio_blk_complete_request(struct request *req, int error)
{
    // On ESX completions must be done through the softirq handler.
    blk_complete_request(req);
}
#endif

static void kfio_elevator_change(struct request_queue *q, char *name)
{
    // Turns out that the kernel developers don't actually want drivers to change the I/O scheduler, and so
    //  in 4.18 they prohibit use of elevator_init() and _exit() as well as elevator_change().
    // So now, if the noop scheduler isn't set by default for our driver (which it should be),
    //  then it must be set by an admin using either a udev rule or an init script, executing something like:
    //      echo noop > /sys/block/fioX/queue/scheduler
#if KFIOC_HAS_ELEVATOR_INIT_EXIT == 1

// We don't use the real elevator_change since it isn't in the RedHat Whitelist
// see FH-14626 for the gory details.
# if !defined(__VMKLNX__)
#  if KFIOC_ELEVATOR_EXIT_HAS_REQQ_PARAM
    elevator_exit(q, q->elevator);
#  else
    elevator_exit(q->elevator);
#  endif
#  if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,32)
    q->elevator = NULL;
#  endif
    if (elevator_init(q, name))
    {
        errprint_all(ERRID_LINUX_KBLK_INIT_SCHED, "Failed to initialize noop io scheduler\n");
    }
# endif
#endif // KFIOC_HAS_ELEVATOR_INIT_EXIT
}

#if KFIOC_HAS_BIO_COMP_CPU
# if KFIOC_MAKE_REQUEST_FN_VOID
static void kfio_rq_make_request_fn(struct request_queue *q, struct bio *bio)
# else
static int kfio_rq_make_request_fn(struct request_queue *q, struct bio *bio)
# endif
{
    kfio_disk_t *disk = q->queuedata;

    if (bio->bi_comp_cpu == -1)
    {
        bio_set_completion_cpu(bio, kfio_current_cpu());
    }

# if KFIOC_MAKE_REQUEST_FN_VOID
    disk->make_request_fn(q, bio);
# else
    return disk->make_request_fn(q, bio);
# endif
}
#endif

static void kfio_set_queue_depth(struct request_queue *q, unsigned int depth)
{
    int cong;

    q->nr_requests = depth;

    /* set watermark for congestion on */
    cong = q->nr_requests - (q->nr_requests / 8) + 1;
    if (cong > q->nr_requests)
        cong = q->nr_requests;

    q->nr_congestion_on = cong;

    /* set watermark for congestion off */
    cong = q->nr_requests - (q->nr_requests / 8) - (q->nr_requests / 16) - 1;
    if (cong < 1)
        cong = 1;

    q->nr_congestion_off = cong;
}

#if defined(__VMKLNX__)
static void dummy_unplug(struct work_struct *p)
{
    engprint("Null callback avoided in dummy_unplug()\n");
}
#endif /* #if defined(__VMKLNX__) */

static struct request_queue *kfio_init_queue(struct kfio_disk *dp,
                                             kfio_numa_node_t node)
{
    struct request_queue *rq;
    static char elevator_name[] = "noop";

    kassert(use_workqueue == USE_QUEUE_RQ);

    rq = blk_init_queue_node(kfio_do_request, (spinlock_t *)&dp->queue_lock, node);
    if (rq != NULL)
    {
        rq->queuedata = dp;

#if KFIOC_HAS_BIO_COMP_CPU
        dp->make_request_fn = rq->make_request_fn;
        rq->make_request_fn = kfio_rq_make_request_fn;
#endif

        /* Change out the default io scheduler, and use noop instead */
        kfio_elevator_change(rq, elevator_name);
#if defined(__VMKLNX__)
        // ESX expects completions to be done in the softirq handler.
        // Otherwise we get mysterious hangs with no error messages.
        blk_queue_softirq_done(rq, kfio_blk_do_softirq);
#endif

        /* Increase queue depth  */
        kfio_set_queue_depth(rq, DEFAULT_LINUX_MAX_NR_REQUESTS);
    }

    /*
     * The VMKernel initializes the request_queue in their implementation of
     * blk_queue_init().  However, they both neglect to initialize the
     * unplug handler, and also attempt to call it.  This badness is worked
     * around here and also in linux_bdev_backpressure()
     */
#if defined(__VMKLNX__)
    if (rq->unplug_work.work.pending & __WORK_OLD_COMPAT_BIT)
    {
        if (rq->unplug_work.work.func.old == NULL)
        {
            engprint("Null work func callback found & fixed (__WORK_OLD_COMPAT_BIT)\n");
            rq->unplug_work.work.func.old = (old_work_func_t) dummy_unplug;
        }
    }
    else
    {
        if (rq->unplug_work.work.func.new == NULL)
        {
            engprint("Null work func callback found & fixed (!__WORK_OLD_COMPAT_BIT)\n");
            rq->unplug_work.work.func.new = (work_func_t) dummy_unplug;
        }
    }
#endif /* #if defined(__VMKLNX__) */

    return rq;
}

static struct request *kfio_blk_fetch_request(struct request_queue *q)
{
    struct request *req;

    kassert(use_workqueue == USE_QUEUE_RQ);
    kassert(spin_is_locked(q->queue_lock));

#if KFIOC_USE_NEW_IO_SCHED
    req = blk_fetch_request(q);
#else
    req = elv_next_request(q);
    if (req != NULL)
    {
        blkdev_dequeue_request(req);
    }
#endif

    return req;
}

static void kfio_req_completor(kfio_bio_t *fbio, uint64_t bytes_done, int error)
{
    struct request *req = (struct request *)fbio->fbio_parameter;

    if (unlikely(fbio->fbio_flags & KBIO_FLG_DUMP))
    {
        kfio_dump_fbio(fbio);
#if !defined(__VMKLNX__)
        blk_dump_rq_flags(req, FIO_DRIVER_NAME);
#endif
    }

#if KFIOC_HAS_BLK_MQ
    if (use_workqueue == USE_QUEUE_MQ)
    {
# if KFIOC_BLKMQ_COMPLETE_NO_ERROR
        blk_mq_complete_request(req);
# else
        blk_mq_complete_request(req, error);
# endif
#endif
    } else {
      kfio_blk_complete_request(req, error);
    }
}

#if defined(__VMKLNX__) || KFIOC_HAS_RQ_FOR_EACH_BIO == 0
#  define __rq_for_each_bio(lbio, req) \
           if ((req->bio)) \
               for (lbio = (req)->bio; lbio; lbio = lbio->bi_next)
#endif
#if defined(__VMKLNX__) || KFIOC_HAS_RQ_IS_SYNC == 0
# if KFIOC_HAS_REQ_RW_SYNC == 1
#    define rq_is_sync(rq)  (((rq)->flags & REQ_RW_SYNC) != 0)
# else
#    define rq_is_sync(rq)  (0)
# endif
#endif

/// @brief determine if a block request is an empty flush.
///
/// NB: any write request may have the flush bit set, but we do not treat
/// those as special, since all writes are powercut safe and may not be
/// reordered. This function detects only zero-length requests with the
/// flush bit set, which do require special handling.
static inline bool rq_is_empty_flush(const struct request *req)
{
#if KFIOC_NEW_BARRIER_SCHEME == 1
    if (blk_rq_bytes(req) == 0 && req->cmd_flags & REQ_FLUSH)
    {
        return true;
    }
#elif KFIOC_BARRIER_USES_QUEUE_FLAGS == 1
    if (blk_rq_bytes(req) == 0 && req_op(req) == REQ_OP_FLUSH)
    {
        return true;
    }
#elif KFIOC_BARRIER == 1
    if (blk_rq_bytes(req) == 0 && blk_barrier_rq(req))
    {
        return true;
    }
#endif
    return false;
}

static kfio_bio_t *kfio_request_to_bio(kfio_disk_t *disk, struct request *req,
                                       bool can_block)
{
    struct fio_bdev *bdev = disk->bdev;
    kfio_bio_t *fbio;

    if (can_block)
    {
        fbio = kfio_bio_alloc(bdev);
    }
    else
    {
        fbio = kfio_bio_try_alloc(bdev);
    }
    if (fbio == NULL)
    {
        return NULL;
    }

    kassert(blk_rq_bytes(req) % bdev->bdev_block_size == 0);

    fbio->fbio_range.base = (blk_rq_pos(req) << 9) / bdev->bdev_block_size;
    fbio->fbio_range.length = blk_rq_bytes(req) / bdev->bdev_block_size;

    fbio->fbio_completor = kfio_req_completor;
    fbio->fbio_parameter = (fio_uintptr_t)req;

    /* Detect flush barrier requests. */
    if (rq_is_empty_flush(req))
    {
        fbio->fbio_cmd = KBIO_CMD_FLUSH;

        // Zero-length flush requests sometimes have uninitialized blk_rq_pos
        // when received from the OS. Force a valid address to prevent woe
        // elsewhere in the driver.
        fbio->fbio_range.base = 0;

        /* Actually flush on non-powercut-safe cards */
        fbio->fbio_flags |= KBIO_FLG_SYNC;
    }
    else
    /* Detect trim requests. */
#if KFIOC_DISCARD == 1
    if (enable_discard &&
# if KFIOC_HAS_SEPARATE_OP_FLAGS
        (req_op(req) == REQ_OP_DISCARD))
# else
        (req->cmd_flags & REQ_DISCARD))
# endif
    {
        kassert((blk_rq_pos(req) << 9) % bdev->bdev_block_size == 0);
        fbio->fbio_cmd = KBIO_CMD_DISCARD;
    }
    else
#endif
    /* This is a read or write request. */
    {
        struct bio *lbio;
        int error = 0;

        kassert((blk_rq_pos(req) << 9) % bdev->bdev_block_size == 0);

        kfio_set_comp_cpu(fbio, req->bio);

        if (rq_data_dir(req) == WRITE)
        {
            int sync_write = rq_is_sync(req);

            fbio->fbio_cmd = KBIO_CMD_WRITE;

#if KFIOC_NEW_BARRIER_SCHEME == 1 || KFIOC_BARRIER_USES_QUEUE_FLAGS == 1
            sync_write |= req->cmd_flags & REQ_FUA;
#endif

            if (sync_write)
            {
                fbio->fbio_flags |= KBIO_FLG_SYNC;
            }
        }
        else
        {
            fbio->fbio_cmd = KBIO_CMD_READ;
        }

        __rq_for_each_bio(lbio, req)
        {
            error = kfio_sgl_map_bio(fbio->fbio_sgl, lbio);
            if (error != 0)
            {
                break;
            }
        }

        /*
         * Sanity checking: combined length of all bios should cover request
         * size. Only makes sense if above iteration over bios was successful.
         */
        if (error == 0 && kfio_bio_chain_size_bytes(fbio) != kfio_sgl_size(fbio->fbio_sgl))
        {
            // ESX 40u1 wins this time...
            errprint_all(ERRID_LINUX_KBLK_REQ_MISMATCH, "Request size mismatch. Request size %llu dma size %u\n",
                         kfio_bio_chain_size_bytes(fbio), kfio_sgl_size(fbio->fbio_sgl));
#if KFIOC_DISCARD == 1
            infprint("Request cmd 0x%016llx\n", (uint64_t)req->cmd_flags);
#endif
            __rq_for_each_bio(lbio, req)
            {
#if KFIOC_HAS_BIOVEC_ITERATORS
                struct bio_vec vec;
                struct bvec_iter bv_i;
#else
                struct bio_vec *vec;
                int bv_i;
#endif
#if KFIOC_HAS_SEPARATE_OP_FLAGS
                infprint("\tbio %p sector %lu size 0x%08x flags 0x%08lx op 0x%08x op_flags 0x%04x\n", lbio,
                         (unsigned long)BI_SECTOR(lbio), BI_SIZE(lbio), (unsigned long)lbio->bi_flags,
                         bio_op(lbio), bio_flags(lbio));
#else
                infprint("\tbio %p sector %lu size 0x%08x flags 0x%08lx rw 0x%08lx\n", lbio,
                         (unsigned long)BI_SECTOR(lbio), BI_SIZE(lbio), (unsigned long)lbio->bi_flags, lbio->bi_rw);
#endif
                infprint("\t\tvcnt %u idx %u\n", lbio->bi_vcnt, BI_IDX(lbio));

                bio_for_each_segment(vec, lbio, bv_i)
                {
#if KFIOC_HAS_BIOVEC_ITERATORS
                    infprint("vec %d: page %p offset %u len %u\n", bv_i.bi_idx, vec.bv_page,
                             vec.bv_offset, vec.bv_len);
#else
                    infprint("vec %d: page %p offset %u len %u\n", bv_i, vec->bv_page,
                             vec->bv_offset, vec->bv_len);
#endif
                }
            }
            infprint("SGL content:\n");
            kfio_sgl_dump(fbio->fbio_sgl, NULL, "\t", 0);
            error = -EIO; /* Any non-zero value will serve. */
        }

        if (error != 0)
        {
            /* This should not happen. */
            kfail();
            kfio_bio_free(fbio);
            return NULL;
        }

        kassert(kfio_bio_chain_size_bytes(fbio) == kfio_sgl_size(fbio->fbio_sgl));
    }

    return fbio;
}

#if !defined(__VMKLNX__)

/*
 * Pull off as many requests as we can from the IO scheduler, then
 * drop the linux block device queue lock and map/submit them. If we
 * fail allocating internal fbios, requeue the leftovers.
 */
static void kfio_do_request(struct request_queue *q)
{
    kfio_disk_t *disk = q->queuedata;
    struct request *req;
    LIST_HEAD(list);
    int rc, queued, requeued, rw, wait_for_merge;
    struct list_head *entry, *tmp;

    /*
     * Guard against unwanted recursion.
     * While we drop the request lock to allow more requests to be enqueued,
     * we don't allow another thread to process requests in the driver until
     * we process the ones we removed in order to maintain ordering requirements.
     */
    if (disk->in_do_request)
    {
        return;
    }
    disk->in_do_request = 1;

    for ( ; ; )
    {
        wait_for_merge = rw = queued = requeued = 0;
        /*
         * Grab requests that we queued internally for retry first before
         * diving into the OS pending queue.
         */
        if (!list_empty(&disk->retry_list))
        {
            list_splice_init(&disk->retry_list, &list);
            rw = disk->retry_cnt;
            disk->retry_cnt = 0;
        }
        else
        {
            while ((req = kfio_blk_fetch_request(q)))
            {
                // If the OS ever hands us a request with 'special' set, we'll blindly
                // trust that it is a fbio pointer. That would be bad. Hence the
                // following assert.
                kassert(req->special == 0);

#if KFIOC_HAS_BLK_FS_REQUEST
                if (blk_fs_request(req))
#elif KFIOC_REQUEST_HAS_CMD_TYPE
                if (req->cmd_type == REQ_TYPE_FS)
#else
                if (!blk_rq_is_passthrough(req))
#endif
                {
                    // Do not allow improperly aligned requests to go through. OS should
                    // not really allow these to reach our entry point, but be paranoid
                    // and fail ill-formed requests before they get to do any damage at
                    // lower layers.
                    if (((blk_rq_pos(req) << 9) & disk->sector_mask) != 0 ||
                        (blk_rq_bytes(req) & disk->sector_mask) != 0)
                    {
                        if (!rq_is_empty_flush(req))
                        {
                            errprint_all(ERRID_LINUX_KBLK_REQ_REJ,
                                         "Rejecting unaligned request %llu:%lu, device sector size is %u\n",
                                         (unsigned long long)(blk_rq_pos(req) << 9), (unsigned long)blk_rq_bytes(req),
                                         disk->sector_mask + 1);

                            kfio_end_request(req, -EIO);
                            continue;
                        }
                    }

                    if (disk->pending && disk->last_dispatch == blk_rq_pos(req) &&
                        disk->last_dispatch_rw == rq_data_dir(req) &&
                        req->bio != 0 &&
                        req->bio->bi_vcnt == 1)
                    {
                        blk_requeue_request(q, req);
                        wait_for_merge = 1;
                        break;
                    }
                }
                list_add_tail(&req->queuelist, &list);
                disk->last_dispatch = blk_rq_pos(req) + (blk_rq_bytes(req) >> 9);
                disk->last_dispatch_rw = rq_data_dir(req);
                rw++;
            }
        }

        /*
         * By and large we always queue everything we pull off, so
         * add the total sum here. We'll decrement if we need to
         * requeue again
         */
        disk->pending += rw;
        spin_unlock_irq(q->queue_lock);

        list_for_each_safe(entry, tmp, &list)
        {
            kfio_bio_t *fbio;

            req = list_entry(entry, struct request, queuelist);

            fbio = req->special;
            if (!fbio)
            {
                fbio = kfio_request_to_bio(disk, req, false);
            }
            if (!fbio)
            {
                if (unlikely(fio_bdev_self_check(disk->bdev)))
                {
                    // Underlying hardware has failed. Retrying will not improve matters.
                    list_del_init(&req->queuelist);
                    spin_lock_irq(q->queue_lock);
                    kfio_end_request(req, -EIO);
                    spin_unlock_irq(q->queue_lock);
                    queued++;
                    disk->pending--;
                    continue;
                }

                break;
            }

            list_del_init(&req->queuelist);
            fbio->fbio_flags |= KBIO_FLG_NONBLOCK;

            rc = kfio_bio_submit_handle_retryable(fbio);
            if (rc)
            {
                if (kfio_bio_failure_is_retryable(rc))
                {
                    /*
                     * "busy error" conditions. Store the prepped part
                     * for faster retry, and exit.
                     */
                    req->special = fbio;
                    list_add(&req->queuelist, &list);
                    break;
                }
                // Bio is already finished, do not touch it.
                continue;
            }
            queued++;
        }

        // Give up a little time to keep the Soft Lockup complaints a rest
        fusion_cond_resched();

        spin_lock_irq(q->queue_lock);
        if (!list_empty(&list))
        {
            LIST_HEAD(tmp_list);

            /*
             * Splice not-done requests to our internal retry list.
             * Old kernels have neither list_splice_tail() nor a
             * __list_splice() we can use, so splice the old list out
             * first to maintain ordering.
             * The initial count was 'rw', 'queued' is how many we
             * actually sent off. So remaining count is rw - queued.
             */
            list_splice_init(&disk->retry_list, &tmp_list);
            list_splice_init(&list, &disk->retry_list);
            list_splice(&tmp_list, &disk->retry_list);
            disk->retry_cnt += (rw - queued);
            requeued = (rw - queued);
        }

        if (requeued)
        {
            /*
             * Subtract what we did not hand off to the hardware
             */
            disk->pending -= requeued;
        }

        /*
         * If we have pending IO, just let some of that restart us. If not,
         * then ensure that we get reentered. This is essentially a
         * "should not happen" situation.
         */
        if (!disk->pending)
        {
            /*
             * IO must have completed already, re-loop to potentially
             * avoid punting to kblockd
             */
            if (wait_for_merge)
            {
                continue;
            }
#if KFIOC_HAS_BLK_DELAY_QUEUE
            blk_delay_queue(q, 1);
#else
            blk_plug_device(q);
#endif
        }

        if (!queued || requeued || wait_for_merge)
        {
            break;
        }
    }
    disk->in_do_request = 0;
}
#endif /* __VMKLNX__ */

#endif /* KFIOC_USE_IO_SCHED */

// FIXME This interface is not implemented yet. It needs to be a callback for block/scsi interface co-existence.
// void kfio_sq_item_cancel(struct bio *bio)
// {
// #if !defined(__VMKLNX__)
//     __kfio_bio_complete(bio, 0, -EIO);
// #endif
// }

/******************************************************************************
 *   Kernel Atomic Write API
 *
 ******************************************************************************/
/// @brief extracts fio_bdev pointer and passes control to kfio_handle_atomic
///
int kfio_vectored_atomic(struct block_device *linux_bdev,
                         const struct kfio_iovec *iov,
                         uint32_t iovcnt,
                         bool user_pages)
{
#if PORT_SUPPORTS_ATOMIC_WRITE
    uint32_t sectors_written;
    struct fio_bdev *bdev;
    int retval;

    if (!linux_bdev || !iov)
    {
        return -EFAULT;
    }

    bdev = linux_bdev->bd_disk->private_data;

    retval = kfio_handle_atomic(bdev, iov, iovcnt, &sectors_written,
                                user_pages ? FIO_ATOMIC_WRITE_USER_PAGES : 0);
    return retval == 0 ? (int)sectors_written : retval;

#else
    return  -EIO;
#endif

}

int kfio_count_sectors_inuse(struct block_device *linux_bdev,
                           uint64_t base,
                           uint64_t length,
                           uint64_t *count)
{
    struct fio_bdev *bdev = linux_bdev->bd_disk->private_data;

    if (!bdev)
    {
        return -EFAULT;
    }

    return fio_bdev_sectors_inuse(bdev, base, length, count);
}

#endif // KFIO_BLOCK_DEVICE
