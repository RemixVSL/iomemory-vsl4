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
// Provides linux/types.h
#include <fio/port/port_config.h>
#include <fio/port/message_ids.h>

// Ignore this whole file, if the block device is not being included in the build.
#if KFIO_BLOCK_DEVICE

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
#include <fio/port/cdev.h>
#include <linux/buffer_head.h>
#include <linux/blk-mq.h>


extern int use_workqueue;
static int fio_major;

static void linux_bdev_name_disk(struct fio_bdev *bdev);
static int  linux_bdev_create_disk(struct fio_bdev *bdev);
static int  linux_bdev_expose_disk(struct fio_bdev *bdev);
static int  linux_bdev_hide_disk(struct fio_bdev *bdev, uint32_t opflags);
static void linux_bdev_destroy_disk(struct fio_bdev *bdev);
static void linux_bdev_backpressure(struct fio_bdev *bdev, int on);
static void linux_bdev_lock_pending(struct fio_bdev *bdev, int pending);
static void linux_bdev_update_stats(struct fio_bdev *bdev, int dir, uint64_t totalsize, uint64_t duration);
static void linux_bdev_update_inflight(struct fio_bdev *bdev, int rw, int in_flight);

#define BI_SIZE(bio) (bio->bi_iter.bi_size)
#define BI_SECTOR(bio) (bio->bi_iter.bi_sector)
#define BI_IDX(bio) (bio->bi_iter.bi_idx)

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
    struct blk_mq_tag_set tag_set;
};

enum {
    KFIO_DISK_HOLDOFF_BIT   = 0,
    KFIO_DISK_COMPLETION    = 1,
};

/*
 * Enable tag flush barriers by default, and default to safer mode of
 * operation on cards that don't have powercut support. Barrier mode can
 * also be QUEUE_ORDERED_TAG, or QUEUE_ORDERED_NONE for no barrier support.
 */
int iodrive_barrier_sync = 0;

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

#if KFIOC_HAS_RQ_POS_BYTES == 0
#define blk_rq_pos(rq)    ((rq)->sector)
#define blk_rq_bytes(rq)  ((rq)->nr_sectors << 9)
#endif

extern int kfio_sgl_map_bio(kfio_sg_list_t *sgl, struct bio *bio);


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

    fio_major = register_blkdev(0, "fio");
    return fio_major <= 0 ? -EBUSY : 0;
}


int kfio_platform_teardown_block_interface(void)
{
    int rc = 0;

#if KFIOC_UNREGISTER_BLKDEV_RETURNS_VOID
    unregister_blkdev(fio_major, "fio");
#else
    rc = unregister_blkdev(fio_major, "fio");
#endif

    return rc;
}

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

static int kfio_open(struct block_device *blk_dev, fmode_t mode)
{
    struct fio_bdev *bdev = blk_dev->bd_disk->private_data;

    return kfio_open_disk(bdev);
}

static void kfio_release(struct gendisk *gd, fmode_t mode)
{
    struct fio_bdev *bdev = gd->private_data;

    kfio_close_disk(bdev);
}

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

static struct block_device_operations fio_bdev_ops =
{
    .owner =        THIS_MODULE,
    .open =         kfio_open,
    .release =      kfio_release,
    .ioctl =        kfio_ioctl,
    .compat_ioctl = kfio_compat_ioctl
};

static struct request_queue *kfio_alloc_queue(struct kfio_disk *dp, kfio_numa_node_t node);
static unsigned int kfio_make_request(struct request_queue *queue, struct bio *bio);
static void __kfio_bio_complete(struct bio *bio, uint32_t bytes_complete, int error);
static void kfio_invalidate_bdev(struct block_device *bdev);

static blk_status_t fio_queue_rq(struct blk_mq_hw_ctx *hctx, const struct blk_mq_queue_data *bd)
{
# if KFIOC_X_REQUEST_QUEUE_HAS_SPECIAL
    struct kfio_disk *disk = hctx->driver_data;
# endif
    struct request *req = bd->rq;
    kfio_bio_t *fbio;
    int rc;

# if KFIOC_X_REQUEST_QUEUE_HAS_SPECIAL
    fbio = req->special;
    if (!fbio)
    {
# else
    // fbio = kfio_request_to_bio(disk, req, false);;
# endif
# if KFIOC_X_REQUEST_QUEUE_HAS_SPECIAL
    }
# endif
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
# if KFIOC_X_REQUEST_QUEUE_HAS_SPECIAL
            req->special = fbio;
# endif
            goto retry;
        }
        /*
         * Bio already completed, we'll just return QUEUE_OK as we should not
         * touch it anymore.
         */
    }

    return BLK_STS_OK;
busy:
    blk_mq_delay_run_hw_queue(hctx, 1);
    return BLK_STS_RESOURCE;
retry:
    blk_mq_run_hw_queues(hctx->queue, true);
    return BLK_STS_RESOURCE;
}

static int fio_init_hctx(struct blk_mq_hw_ctx *hctx, void *data, unsigned int i)
{
    struct kfio_disk *disk = data;

    hctx->driver_data = disk;
    return 0;
}

static struct blk_mq_ops fio_mq_ops = {
    .queue_rq   = fio_queue_rq,
# if KFIOC_BLK_MQ_OPS_HAS_MAP_QUEUES
    /* We want to use blk_mq_map_queues, but it's GPL: however, the block
       driver will call it for us if the function pointer is NULL */
    .map_queues = NULL,
# else
    .map_queue  = blk_mq_map_queue,
# endif
    .init_hctx  = fio_init_hctx,
};


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
    struct kfio_blk_add_disk_param *param;

    disk = bdev->bdev_gd;
    if (disk == NULL)
    {
        return -ENODEV;
    }

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

    if (disk->use_workqueue != USE_QUEUE_RQ)
        disk->rq = kfio_alloc_queue(disk, bdev->bdev_numa_node);

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

blk_queue_max_hw_sectors(rq, FUSION_MAX_SECTORS_PER_OS_RW_REQUEST);
blk_queue_max_segments(rq, bdev->bdev_max_sg_entries);

#if KFIOC_HAS_QUEUE_FLAG_CLUSTER
    blk_queue_flag_clear(QUEUE_FLAG_CLUSTER, rq);
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(5, 0, 0)
// Linux from 5.0 > removed the limits.cluster: https://patchwork.kernel.org/patch/10716231/
#else
#  error "Do not know how to disable request queue clustering for this kernel."
#endif

    blk_queue_max_segment_size(rq, PAGE_SIZE);
    blk_queue_logical_block_size(rq, bdev->bdev_block_size);
#if KFIOC_DISCARD == 1
    if (enable_discard)
    {
        blk_queue_flag_set(QUEUE_FLAG_DISCARD, rq);
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

#if KFIOC_USE_BLK_QUEUE_FLAGS_FUNCTIONS
    blk_queue_flag_set(QUEUE_FLAG_WC, rq);
#else
#error No barrier scheme supported
#endif

#if KFIOC_QUEUE_HAS_NONROT_FLAG
    /* Tell the kernel we are a non-rotational storage device */
    blk_queue_flag_set(QUEUE_FLAG_NONROT, rq);
#endif
#if KFIOC_QUEUE_HAS_RANDOM_FLAG
    /* Disable device global entropy contribution */
    blk_queue_flag_clear(QUEUE_FLAG_ADD_RANDOM, rq);
#endif

    disk->gd = gd = alloc_disk(FIO_NUM_MINORS);
    if (disk->gd == NULL)
    {
        linux_bdev_hide_disk(bdev, KFIO_DISK_OP_SHUTDOWN | KFIO_DISK_OP_FORCE);
        return -ENOMEM;
    }

    gd->major = fio_major;
    gd->first_minor = FIO_NUM_MINORS * bdev->bdev_index;
    gd->minors = FIO_NUM_MINORS;
    gd->fops = &fio_bdev_ops;
    gd->queue = rq;
    gd->private_data = bdev;
#if defined GENHD_FL_EXT_DEVT
    gd->flags = GENHD_FL_EXT_DEVT;
#endif

    fio_bdev_ops.owner = THIS_MODULE;

    strncpy(gd->disk_name, bdev->bdev_name, sizeof(gd->disk_name)-1);
    gd->disk_name[sizeof(gd->disk_name)-1] = 0;

    set_capacity(gd, bdev->bdev_num_blocks * bdev->bdev_block_size / KERNEL_SECTOR_SIZE);

    infprint("%s: Creating block device %s: major: %d minor: %d sector size: %d...\n",
             fio_bdev_get_bus_name(bdev), gd->disk_name, gd->major,
             gd->first_minor, bdev->bdev_block_size);

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
    return 0;
}

static void kfio_kill_requests(struct request_queue *q)
{
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
        if (disk->rq->mq_ops)
        {
            blk_mq_stop_hw_queues(disk->rq);
        }

        /*
         * The queue is stopped and dead and no new user requests will be
         * coming to it anymore. Fetch remaining already queued requests
         * and fail them,
         */
        if (disk->use_workqueue == USE_QUEUE_RQ)
        {
            kfio_kill_requests(disk->rq);
        }
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

        fusion_spin_unlock_irqrestore(&disk->queue_lock);

        /* Wait for all IO against bdev to finish. */
        fio_bdev_drain_wait(bdev);

        /* Tell Linux that disk is gone. */
        del_gendisk(disk->gd);

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
                mutex_lock(&linux_bdev->bd_mutex);
                mutex_unlock(&linux_bdev->bd_mutex);

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

        if (use_workqueue == USE_QUEUE_MQ)
        {
            blk_mq_free_tag_set(&disk->tag_set);
        }

        disk->rq = NULL;
    }

    return 0;
}

static void linux_bdev_destroy_disk(struct fio_bdev *bdev)
{
    struct kfio_disk *disk = bdev->bdev_gd;
    fusion_destroy_spin(&disk->queue_lock);

    fusion_condvar_destroy(&disk->state_cv);
    fusion_cv_lock_destroy(&disk->state_lk);
    kfio_free(disk, sizeof(*disk));

    bdev->bdev_gd = NULL;
}

static void kfio_invalidate_bdev(struct block_device *bdev)
{
    invalidate_bdev(bdev);
}

void linux_bdev_update_stats(struct fio_bdev *bdev, int dir, uint64_t totalsize, uint64_t duration)
{
    kfio_disk_t *disk = (kfio_disk_t *)bdev->bdev_gd;

    if (disk == NULL)
    {
        return;
    }

    if (dir == BIO_DIR_WRITE)
    {
        if (disk->use_workqueue != USE_QUEUE_RQ && disk->use_workqueue != USE_QUEUE_MQ)
        {
            struct gendisk *gd = disk->gd;
            int cpu;

            cpu = part_stat_lock();
            part_stat_inc(&gd->part0, ios[1]);
            part_stat_add(&gd->part0, sectors[1], totalsize >> 9);
            part_stat_add(&gd->part0, nsecs[1],   kfio_div64_64(duration * HZ, FIO_USEC_PER_SEC));
            part_stat_unlock();
        }
    }
    else if (dir == BIO_DIR_READ)
    {
        if (disk->use_workqueue != USE_QUEUE_RQ && disk->use_workqueue != USE_QUEUE_MQ)
        {
            struct gendisk *gd = disk->gd;
            int cpu;

            cpu = part_stat_lock();
            part_stat_inc(&gd->part0, ios[0]);
            part_stat_add(&gd->part0, sectors[0], totalsize >> 9);
            part_stat_add(&gd->part0, nsecs[0],   kfio_div64_64(duration * HZ, FIO_USEC_PER_SEC));
            part_stat_unlock();
        }
    }
}

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
        part_stat_set_all(&gd->part0, in_flight);
    }
}

/**
 * @brief returns 1 if bio is O_SYNC priority
 */
#if KFIOC_DISCARD == 1
static int kfio_bio_is_discard(struct bio *bio)
{
// # if !KFIOC_HAS_SEPARATE_OP_FLAGS
// #  if KFIOC_HAS_UNIFIED_BLKTYPES
    return bio_op(bio) == REQ_OP_DISCARD;
}
#endif

/// @brief   Dump an OS bio to the log
/// @param   msg   prefix for message
/// @param   bio   the bio to drop
static void kfio_dump_bio(const char *msg, const struct bio * const bio)
{
    uint64_t sector;
    kassert(bio);

    // Use a local conversion to avoid printf format warnings on some platforms
    sector = (uint64_t)BI_SECTOR(bio);

    infprint("%s: sector: %llx: flags: %lx : op: %x : op_flags: %x : vcnt: %x", msg,
             sector, (unsigned long)bio->bi_flags, bio_op(bio), bio_flags(bio), bio->bi_vcnt);

    // need to put our own segment count here...
    infprint("%s : idx: %x : phys_segments: %x : size: %x",
             msg, BI_IDX(bio), bio_segments(bio), BI_SIZE(bio));

#if KFIOC_BIO_HAS_SEG_SIZE
    infprint("%s: seg_front_size %x : seg_back_size %x", msg,
             bio->bi_seg_front_size, bio->bi_seg_back_size);
#endif
    infprint("%s: max_vecs: %x : io_vec %p : end_io: %p : private: %p",
             msg, bio->bi_max_vecs, bio->bi_io_vec,
             bio->bi_end_io, bio->bi_private);

    infprint("%s: integrity: %p", msg, bio_integrity(bio) );
}

static inline void kfio_set_comp_cpu(kfio_bio_t *fbio, struct bio *bio)
{
    int cpu;
    cpu = kfio_current_cpu();
    kfio_bio_set_cpu(fbio, cpu);
}

static unsigned long __kfio_bio_sync(struct bio *bio)
{
# if KFIOC_HAS_SEPARATE_OP_FLAGS
    return bio_flags(bio) == REQ_SYNC;
# else
#  if KFIOC_HAS_UNIFIED_BLKTYPES
    return bio->bi_rw & REQ_SYNC;
#  endif
# endif
}

static unsigned long __kfio_bio_atomic(struct bio *bio)
{
    return 0;
}

// TODO: We should probably switch to using the Kernel GPL module.
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

static void __kfio_bio_complete(struct bio *bio, uint32_t bytes_complete, int error)
{
    // bi_status is type blk_status_t, not an int errno, so must translate as necessary.
    blk_status_t bio_status = BLK_STS_OK;

    if (unlikely(error != 0))
    {
        bio_status = kfio_errno_to_blk_status(error);
    }
    bio->bi_status = bio_status;            /* bi_error was changed to bi_status <sigh> */

    bio_endio(bio);
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
            if (bio->bi_flags & FIO_REQ_ATOMIC)
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

#if KFIOC_DISCARD == 1
    return kfio_bio_is_discard(bio);
#else
    return 0;
#endif
}

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

    if (from_schedule)
    {
        INIT_WORK(&plug->work, kfio_unplug_do_cb);
        kblockd_schedule_work(&plug->work);
        return;
    }

    kfio_unplug_do_cb(&plug->work);
}

/*
 * this is used once while we create the block device,
 * just to make sure unplug callbacks aren't run with irqs off
 */
struct test_plug
{
    struct blk_plug_cb cb;
    int safe;
};

static void safe_unplug_cb(struct blk_plug_cb *cb, bool from_schedule)
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

static struct request_queue *kfio_alloc_queue(struct kfio_disk *dp,
                                              kfio_numa_node_t node)
{
    struct request_queue *rq;

    test_safe_plugging();

    rq = blk_alloc_queue_node(GFP_NOIO, node);
    if (rq != NULL)
    {
        rq->queuedata = dp;
        blk_queue_make_request(rq, kfio_make_request);
        memcpy(&dp->queue_lock, &rq->queue_lock, sizeof(dp->queue_lock));
    }
    return rq;
}

static int should_holdoff_writes(struct kfio_disk *disk)
{
    return fio_test_bit_atomic(KFIO_DISK_HOLDOFF_BIT, &disk->disk_state);
}

//TODO: We need to revisit this and do some cleanup.
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
        fusion_cv_lock_irq(&disk->state_lk);
        fio_clear_bit_atomic(KFIO_DISK_HOLDOFF_BIT, &disk->disk_state);
        fusion_condvar_broadcast(&disk->state_cv);
        fusion_cv_unlock_irq(&disk->state_lk);
    }
}

/*
 * Increment/decrement the exclusive lock pending count
 */
void linux_bdev_lock_pending(struct fio_bdev *bdev, int pending)
{
    kfio_disk_t *disk = bdev->bdev_gd;
    struct gendisk *gd;
    struct request_queue *q;

    if (disk == NULL || disk->gd == NULL || disk->gd->queue == NULL)
    {
        return;
    }

    gd = disk->gd;
    q = gd->queue;

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
            //kfio_restart_queue(q); function does nothing.
        }
    }
}

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

static unsigned int kfio_make_request(struct request_queue *queue, struct bio *bio)
#define FIO_MFN_RET 0
{
    struct kfio_disk *disk = queue->queuedata;
    void *plug_data;

    if (bio_data_dir(bio) == WRITE && !holdoff_writes_under_pressure(disk))
    {
        kassert_once(!"timed out waiting for queue to unstall.");
        __kfio_bio_complete(bio, 0, -EIO);
        return FIO_MFN_RET;
    }

    // Split the incomming bio if it has more segments than we have scatter-gather DMA vectors,
    //   and re-submit the remainder to the request queue. blk_queue_split() does all that for us.
    // It appears the kernel quit honoring the blk_queue_max_segments() in about 4.13.
    if (bio_segments(bio) >= queue_max_segments(queue))
        blk_queue_split(queue, &bio);

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
