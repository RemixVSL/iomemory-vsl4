//-----------------------------------------------------------------------------
// Copyright (c) 2006-2014, Fusion-io, Inc.(acquired by SanDisk Corp. 2014)
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
/** @file
 *     NO OS-SPECIFIC REFERENCES ARE TO BE IN THIS FILE
 *
 */
#ifndef __FIO_PORT_KBLOCK_H__
#define __FIO_PORT_KBLOCK_H__

#include <fio/port/ktypes.h>
#include <fio/port/kcpu.h>
#include <fio/port/kbio.h>
#include <fio/common/uuid.h>

struct fio_bio_pool;

#define FIO_BDEV_NAME_BYTES       32

#define FIO_BDEV_FLAG_READ_ONLY_BIT    0
#define FIO_BDEV_FLAG_ONLINE_BIT       1
#define FIO_BDEV_FLAG_REGISTERED_BIT   2
#define FIO_BDEV_FLAG_EXPOSED_BIT      3

#define FIO_BDEV_NOTIFY_START          1
#define FIO_BDEV_NOTIFY_HALT           2

#define FIO_RETRY_REASON_IOCTX_REQ_ALLOC  0
#define FIO_RETRY_REASON_WRITE_SPACE      1
#define FIO_RETRY_REASON_WRITE_RETARD     2
#define FIO_RETRY_REASON_NONE             3

/// @brief Possible values for fio_bdev_storage_interface.interface_type.
/// @note For now, this is selected by the default_storage_interface parameter.
///       If the parameter tries to select an interface which is not registered, the registered type
///       with the lowest value will be used.
#define FIO_BDEV_STORAGE_INTERFACE_NBD   0  ///< Network Block Device protocol (implemented by USD)
#define FIO_BDEV_STORAGE_INTERFACE_BLOCK 1  ///< Block device interface (implemented by fio-port)
#define FIO_BDEV_STORAGE_INTERFACE_SCSI  2  ///< SCSI device interface (implemented by fio-scsi)
#define FIO_BDEV_STORAGE_INTERFACE_MAX   2  // Must match the highest value
#define FIO_BDEV_STORAGE_INTERFACE_NONE  FIO_BDEV_STORAGE_INTERFACE_MAX + 1

#if defined(__KERNEL__)

/// @brief Encapsulation of the interface which will send IOs to a fio_bdev.
/// @note The available interface drivers register their existence using
///       fio_bdev_storage_interface_register().
///       Up to one interface of each type may register.
/// @note Shortly after the fio_bdev is created, it is assigned to use one of the registered interfaces using
///       fio_bdev_select_storage_interface().
struct fio_bdev_storage_interface
{
    uint32_t interface_type;                                                                       ///< FIO_BDEV_STORAGE_INTERFACE_...
    void (*bdev_name_disk)(struct fio_bdev *bdev);                                                 ///< Name a disk
    int  (*bdev_create_disk)(struct fio_bdev *bdev);                                               ///< Create a new disk
    int  (*bdev_expose_disk)(struct fio_bdev *bdev);                                               ///< Expose the disk that was created
    int  (*bdev_hide_disk)(struct fio_bdev *bdev, uint32_t op_flags);                              ///< Hide the disk that was exposed
    void (*bdev_destroy_disk)(struct fio_bdev *bdev);                                              ///< Destroy the disk that was created
    void (*bdev_backpressure)(struct fio_bdev *bdev, int on);                                      ///< Start or stop backpressure. This can be NULL if not supported.
    void (*bdev_lock_pending)(struct fio_bdev *bdev, int on);                                      ///< Increment or decrement the excl lock pending count. This can be NULL if not supported.
    void (*bdev_update_inflight)(struct fio_bdev *bdev, int rw, int in_flight);                    ///< Update in flight read or write value. This can be NULL if not supported.
    void (*bdev_update_stats)(struct fio_bdev *bdev, int dir, uint64_t total_size, uint64_t dur);  ///< Update read or write statistics. This can be NULL if not supported.
};

// Access to the members of this structure are protected by the included spinlock.
// Reads_outstanding was implemented as an atomic because it was updated during read
// completion which was in interrupt context. It looks as though read completion now
// defaults to being done from a worker thread and total_read_bytes is now also updated
// during read completion and since completions are no longer done in interrupt context,
// we can use simple spinlock protection. If read completion is enabled in interrupt
// context then we'll have to use irqsave spinlock protection to avoid eventual deadlock.
struct fio_bdev_stats
{
    fusion_spinlock_t lock;

    uint64_t *total_reads;             // Array of per cpu read counters.
    uint64_t *total_writes;            // Array of per cpu write counters.
    uint64_t discards_outstanding;
    uint64_t reads_outstanding;
    uint64_t writes_outstanding;
    uint64_t outstanding_write_packets;
    uint64_t total_read_bytes;
    uint64_t total_write_bytes;
    uint64_t total_discards;
    uint64_t total_discard_sectors;
    uint64_t total_read_zero_bytes;
    /** The interval_* members are only used in fio_proc_read_interval_request_stats(),
       and are protected by the same structure lock */
    uint64_t *interval_total_reads;
    uint64_t *interval_total_writes;
    uint64_t interval_reads_outstanding;
    uint64_t interval_writes_outstanding;
    uint64_t interval_total_read_bytes;
    uint64_t interval_total_write_bytes;
#if PORT_SUPPORTS_ATOMIC_WRITE
    uint64_t total_atomics;
    uint64_t interval_total_atomics;
    uint64_t total_atomic_write_bytes;
    uint64_t interval_total_atomic_write_bytes;
#if FUSION_INTERNAL
    uint64_t failed_atomics;
    uint64_t interval_failed_atomics;
#endif /* FUSION_INTERNAL */
#endif /* PORT_SUPPORTS_ATOMIC_WRITE */
};

struct fio_bdev
{
    void                  *bdev_gd;

    fusion_bits_t         bdev_flags;

    char                  bdev_name[FIO_BDEV_NAME_BYTES + 1];
    char                  bdev_node[FIO_BDEV_NAME_BYTES + 1];

    struct kfio_info_node *bdev_info_base;
    struct kfio_info_node *bdev_info_discard_base;

    uint64_t              bdev_num_blocks;     // Logical Number of blocks in this bdev
    uint32_t              bdev_block_size;     // Size of each block
    uint32_t              bdev_mem_alignment;  // Minimum data unit / alignment required for this device/arch.

    struct fio_bdev_stats bdev_stats;
    struct fio_devstats   *bdev_devstats;

    int                   bdev_index;          // Unique number of the device within system
    int                   bdev_dev_number;     // Parent ioDrive device (unit) number
    int                   bdev_vsu_number;     // VSU number, unique per bdev_dev_number
    int                   padding_one_int;

    fusion_condvar_t      bdev_bio_done_cv;    // if you can't get one hang out here.
    fusion_cv_lock_t      bdev_bio_done_lock;  // protects all "request_xxxx" fields

    struct fio_bdev       *bdev_parent;        // Parent block device
    uint64_t               bdev_offset;        // Our offset relative to the log.

    /// bio pool to use for bio allocation. Can be shared by many different bdevs
    struct fio_bio_pool   *bdev_bio_pool;

    /// These fields are for use by the creator of the block device
    /// to interpret the commands sent to it
    void                  *bdev_opaque;

    /// These fields are used by the porting layer for device name
    /// creation.
    void                  *bdev_devfs_handle;

    // May not be enforced on all platforms
    int                   bdev_max_sg_entries;
    kfio_numa_node_t      bdev_numa_node;

    int  (*bdev_do_open)(struct fio_bdev *);
    void (*bdev_do_release)(struct fio_bdev *);
    int  (*bdev_do_ioctl)(struct fio_bdev *, unsigned cmd, uintptr_t arg);
    int  (*bdev_do_read)(struct fio_bdev *, struct kfio_bio *);
    int  (*bdev_do_flush)(struct fio_bdev *, struct kfio_bio *);
    int  (*bdev_do_modify)(struct fio_bdev *, struct kfio_bio *);
    int  (*bdev_do_notify)(struct fio_bdev *, int, void *);
    int  (*bdev_do_supports)(struct fio_bdev *bdev, const fio_uuid_t uuid);
    fio_uuid_t *(*bdev_do_get_identifier)(struct fio_bdev *bdev);

    const struct fio_bdev_storage_interface *bdev_sif;
};

///@brief Returns the sum of the per cpu statistic in fio_bdev_stats (e.g. total_reads)
static inline uint64_t get_per_cpu_stat_sum(uint64_t *array)
{
    uint64_t ret = 0;
    uint32_t i;

    for (i = 0 ; i < kfio_max_cpus(); i++)
    {
        ret += array[i];
    }
    return ret;
}

extern int kfio_bdev_max_atomic_iovs(struct fio_bdev *bdev);
extern int kfio_bdev_max_atomic_total_write_size_bytes(struct fio_bdev *bdev);
extern int kfio_bdev_atomic_writes_enabled(struct fio_bdev *bdev);

extern int fio_init_bdev_subsystem(void);
extern int fio_teardown_bdev_subsystem(void);
extern uint32_t fio_bdev_storage_interface_get_type_to_use(void);
extern void fio_bdev_select_storage_interface(struct fio_bdev *bdev);
extern uint32_t fio_bdev_ptrim_available(struct fio_bdev *bdev);

/// @defgroup PLATSTORAPI VSL Platform Storage API

/// @defgroup PLATSTORAPI-port core => port interfaces
/// @note Functions implemented in OS-specific kblock code and called from parts of the core that are not OS-aware.
/// @ingroup PLATSTORAPI
/// @{

/// @brief One-time initializer for the storage interface.
extern int  kfio_platform_init_storage_interface(void);

/// @brief One-time teardown for the storage interface.
extern int  kfio_platform_teardown_storage_interface(void);
/// @}

/// @defgroup PLATSTORAPI-port-hideflags bdev_plaform_hide_disk bdev_op_flags
/// @ingroup PLATSTORAPI-port
/// @{
#define KFIO_DISK_OP_DESTROY   0x01  ///< The disk is being destroyed completely and won't come back.
#define KFIO_DISK_OP_SHUTDOWN  0x02  ///< The disk is being hidden for power-off or shutdown.
#define KFIO_DISK_OP_FORCE     0x04  ///< The operation has to succeed even on open device.
/// @}

/// @defgroup PLATSTORAPI-core port => core interfaces
/// @note Generic kblock-related services provided by core and available to OS-specific block code.
/// @ingroup PLATSTORAPI
/// @{

extern void fio_bdev_storage_interface_register(struct fio_bdev_storage_interface *bdev_sif);

/// @brief Return the string representation of the bus name being used
extern const char *fio_bdev_get_bus_name(struct fio_bdev *bdev);

/// @brief Disk namer which the bdev_name_disk interface can use if it likes.
extern void fio_bdev_name_disk(struct fio_bdev *bdev, const char *prefix, int use_letter_suffix);

extern int  fio_bdev_sectors_inuse(struct fio_bdev *bdev, uint64_t base, uint64_t length, uint64_t *count);
extern void fio_bdev_drain_wait(struct fio_bdev *bdev);
extern void fio_bdev_stop_draining(struct fio_bdev *bdev);

extern int fio_handle_read_bio(struct fio_bdev *bdev, struct kfio_bio *bio);
extern int fio_handle_flush_bio(struct fio_bdev *bdev, struct kfio_bio *bio);
extern int fio_handle_modify_bio(struct fio_bdev *bdev, struct kfio_bio *bio);

/// @brief Open the specified device
static inline int fio_bdev_open(struct fio_bdev *bdev)
{
    if (bdev->bdev_do_open == NULL)
    {
        return 0;
    }

    return bdev->bdev_do_open(bdev);
}

/// @brief Release the device
static inline void fio_bdev_release(struct fio_bdev *bdev)
{
    if (bdev->bdev_do_release != NULL)
    {
        bdev->bdev_do_release(bdev);
    }
}

/// @brief Perform an ioctl on the specified device
static inline int fio_bdev_ioctl(struct fio_bdev *bdev, unsigned cmd, uintptr_t arg)
{
    if (bdev->bdev_do_ioctl == NULL)
    {
        return -ENOTTY;
    }

    return bdev->bdev_do_ioctl(bdev, cmd, arg);
}

/// @brief Return the number of bytes to be transferred by the specified operation.
static inline uint64_t kfio_bio_chain_size_bytes(struct kfio_bio *bio)
{
    return kfio_bio_chain_size_blocks(bio) * bio->fbio_bdev->bdev_block_size;
}

/// @brief Convert a local block range relative to this bdev into log space.
static inline fio_range_t fio_bdev_range_to_global(struct fio_bdev *bdev, fio_range_t local)
{
    fio_range_t range = local;
    range.base += bdev->bdev_offset;
    return range;
}

/// @brief Convert a global block range relative to log space into this bdev.
static inline fio_range_t fio_bdev_range_from_global(struct fio_bdev *bdev, fio_range_t global)
{
    fio_range_t range = global;
    range.base -= bdev->bdev_offset;
    return range;
}

/// @}

/// @defgroup PLATSTORAPI-core-uuid Interfaces for accessing a VSU type and uuid
/// @ingroup PLATSTORAPI-core
/// @{

/// @brief Determines whether a device supports the capability with the given well-known UUID.
static inline int fio_bdev_supports(struct fio_bdev *bdev, const fio_uuid_t uuid)
{
    if (bdev->bdev_do_supports == NULL)
        return 0;
    return bdev->bdev_do_supports(bdev, uuid);
}

/// @brief Return a device's UUID identifier.
static inline fio_uuid_t *fio_bdev_get_identifier(struct fio_bdev *bdev)
{
    if (bdev->bdev_do_get_identifier == NULL)
        return NULL;
    return bdev->bdev_do_get_identifier(bdev);
}

/// @}

static inline int fio_bdev_notify_start(struct fio_bdev *bdev)
{
    if (bdev->bdev_do_notify == NULL)
    {
        return 0;
    }

    return bdev->bdev_do_notify(bdev, FIO_BDEV_NOTIFY_START, NULL);
}

static inline int fio_bdev_notify_halt(struct fio_bdev *bdev)
{
    if (bdev->bdev_do_notify == NULL)
    {
        return 0;
    }

    return bdev->bdev_do_notify(bdev, FIO_BDEV_NOTIFY_HALT, NULL);
}

extern void fio_restart_read_bio(struct kfio_bio *bio, int error);

static inline void fio_bdev_on_backpressure(struct fio_bdev *bdev, int enter)
{
    if (bdev->bdev_sif->bdev_backpressure)
    {
        bdev->bdev_sif->bdev_backpressure(bdev, enter);
    }
}

static inline void fio_bdev_lock_begin(struct fio_bdev *bdev)
{
    if (bdev->bdev_sif->bdev_lock_pending)
    {
        bdev->bdev_sif->bdev_lock_pending(bdev, 1);
    }
}

static inline void fio_bdev_lock_end(struct fio_bdev *bdev)
{
    if (bdev->bdev_sif->bdev_lock_pending)
    {
        bdev->bdev_sif->bdev_lock_pending(bdev, 0);
    }
}

extern int fio_bdev_self_check(struct fio_bdev *bdev);

#endif // __KERNEL__
#endif // __FIO_PORT_KBLOCK_H__
