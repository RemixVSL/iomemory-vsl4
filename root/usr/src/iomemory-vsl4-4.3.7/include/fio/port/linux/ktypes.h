//-----------------------------------------------------------------------------
// Copyright (c) 2006-2014, Fusion-io, Inc.(acquired by SanDisk Corp. 2014)
// Copyright (c) 2014-2016 SanDisk Corp. and/or all its affiliates. (acquired by Western Digital Corp. 2016)
// Copyright (c) 2016-2018 Western Digital Technologies, Inc. All rights reserved.
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

#ifndef __FUSION_LINUX_KTYPES_H__
#define __FUSION_LINUX_KTYPES_H__

#ifndef __FIO_PORT_KTYPES_H__
#error Do not include this file directly - instead include <fio/port/ktypes.h>
#endif

#include <fio/port/common-linux/kassert.h>
#ifndef BUILDING_MODULE
#include <fio/port/stdint.h>
#else
#include <linux/types.h>
#endif
#include <fio/port/bitops.h>
#include <fio/port/common-linux/commontypes.h>

/**
 * @ingroup PORT_LINUX
 * @{
 */

/**
 * brief @struct __fusion_poll_struct
 * OS-independent structure for poll support:
 * Linux wait_queue_head_t
 * Solaris struct pollhead
 * FreeBSD struct selinfo
 *
 */
struct FUSION_STRUCT_ALIGN(8) __fusion_poll_struct {
    DECLARE_RESERVE(196);
};

#define KERNEL_SECTOR_SIZE   512 /* ARGH */

typedef struct kfio_disk kfio_disk_t;

// Atomic write flag for linux bios
// [31] collides with BIO_POOL_* bits in 4.3 kernel resizing of bio.bi_flags
#define FIO_REQ_ATOMIC      (1UL<<31)
#define PORT_SUPPORTS_FIO_REQ_ATOMIC 1 // cannot be enabled until kernel support is provided

typedef unsigned long kfio_get_cpu_t;
typedef int kfio_numa_node_t;

#define FIO_NUMA_NODE_NONE      -1


#define FIO_WORD_SIZE 8
#define FIO_BITS_PER_LONG 64
#define FUSION_PAGE_SHIFT           12
#define FUSION_PAGE_SIZE            (1UL<<FUSION_PAGE_SHIFT)
#define FUSION_PAGE_MASK            (~(FUSION_PAGE_SIZE-1))
#define FUSION_PAGE_ALIGN_UP(addr)  ((char*)((((fio_uintptr_t)(addr))+FUSION_PAGE_SIZE-1)&FUSION_PAGE_MASK))
#define FUSION_ROUND_TO_PAGES(size) ((fio_uintptr_t)FUSION_PAGE_ALIGN_UP(size))

#if defined(__KERNEL__)
#define S_IRWXU 00700
#define S_IRUSR 00400
#define S_IWUSR 00200
#define S_IXUSR 00100

#define S_IRWXG 00070
#define S_IRGRP 00040
#define S_IWGRP 00020
#define S_IXGRP 00010

#define S_IRWXO 00007
#define S_IROTH 00004
#define S_IWOTH 00002
#define S_IXOTH 00001

#define S_IRUGO         (S_IRUSR|S_IRGRP|S_IROTH)
#define S_IWUGO         (S_IWUSR|S_IWGRP|S_IWOTH)
#define S_IXUGO         (S_IXUSR|S_IXGRP|S_IXOTH)

#endif // __KERNEL

// Valid values for use_workqueue. Exposed to users, do not alter.
#define USE_QUEUE_NONE   0 ///< directly submit requests.
#define USE_QUEUE_RQ     3 ///< use single kernel thread and blk_init_queue style queueing.
#define USE_QUEUE_MQ     4 ///< use blk-mq. (Currently disabled FH-24331).

struct __fusion_sema {
// This is horribly ugly, but this needs to be large enough
// to accommodate any platforms semaphore structure.  We cast it
// in the wrapper layer to whatever it needs to be in the
// host OS.  In this case, the Linux kernel uses rw_sema structs
// that are 24 bytes long, and normal semaphore structs that are
// 32 bytes long, so we will use the maximum.
    DECLARE_RESERVE(184);
};

typedef volatile struct __fusion_sema     fusion_rwsem_t;

/* DO NOT USE INTERRUPTIBLE FUNCTIONS */
#define __int_compile_error() C_ASSERT(0)

extern void fusion_rwsem_init(fusion_rwsem_t *x, const char *);
extern void fusion_rwsem_destroy(fusion_rwsem_t *x);
extern void fusion_rwsem_down_read(fusion_rwsem_t *x);
extern int fusion_rwsem_down_read_trylock(fusion_rwsem_t *x);
extern void fusion_rwsem_up_read(fusion_rwsem_t *x);
extern void fusion_rwsem_down_write(fusion_rwsem_t *x);
extern void fusion_rwsem_up_write(fusion_rwsem_t *x);
extern int fusion_rwsem_down_write_trylock(fusion_rwsem_t *x);

#define PORT_SUPPORTS_TRYLOCK_RWSEM

//#define fusion_set_relative_timer(o,t)   mod_timer((o),jiffies + (t))
//#define fusion_del_timer(t)              del_timer_sync(t)

struct __fusion_spinlock
{
// This must be large enough to handle all spinlock port
// implementations.
    DECLARE_RESERVE(192);

};

typedef volatile struct __fusion_spinlock  fusion_spinlock_t;

// spin_lock must always be called in a consistent context, i.e. always in
// user context or always in interrupt context. The following two macros
// make this distinction. Use fusion_spin_lock() if your lock is always
// obtained in user context; use fusion_spin_lock_irqdisabled if your
// lock is always obtained in interrupt context (OR with interrupts
// disabled); and use fusion_spin_trylock_irqsave if it can be called in
// both or if you don't know.
extern void fusion_init_spin(fusion_spinlock_t *s, const char *name);
extern void fusion_destroy_spin(fusion_spinlock_t *s);
extern void fusion_spin_lock(fusion_spinlock_t *s);
extern int  fusion_spin_is_locked(fusion_spinlock_t *s);
extern int  fusion_spin_trylock(fusion_spinlock_t *s);
extern void fusion_spin_unlock(fusion_spinlock_t *s);
extern void fusion_spin_lock_irqdisabled(fusion_spinlock_t *s);
extern void fusion_spin_lock_irqsave(fusion_spinlock_t *s);
extern int  fusion_spin_trylock_irqsave(fusion_spinlock_t *s);
extern void fusion_spin_unlock_irqrestore(fusion_spinlock_t *s);
extern int  fusion_spin_is_irqsaved(fusion_spinlock_t *s);

typedef volatile struct __fusion_spinlock  fusion_rwspin_t;

extern void fusion_rwspin_init(fusion_rwspin_t *s, const char *name);
extern void fusion_rwspin_destroy(fusion_rwspin_t *s);
extern void fusion_rwspin_read_lock(fusion_rwspin_t *s);
extern void fusion_rwspin_write_lock(fusion_rwspin_t *s);
extern void fusion_rwspin_read_unlock(fusion_rwspin_t *s);
extern void fusion_rwspin_write_unlock(fusion_rwspin_t *s);

#define PORT_SUPPORTS_RWSPIN

typedef struct __fusion_sema fusion_mutex_t;

extern void fusion_mutex_init(fusion_mutex_t *lock, const char *);
extern void fusion_mutex_destroy(fusion_mutex_t *lock);
extern int  fusion_mutex_trylock(fusion_mutex_t *lock);
extern void fusion_mutex_lock(fusion_mutex_t *lock);
extern void fusion_mutex_unlock(fusion_mutex_t *lock);

#define fusion_mutex_lock_interruptible(x)  __int_compile_error()

typedef void    *fusion_kthread_t;

#define MAX_KTHREAD_NAME_LENGTH 40
typedef int (*fusion_kthread_func_t)(void *);

//nand_device can be NULL in the following fxn call
int fusion_create_kthread(fusion_kthread_func_t func, void *data, void *fusion_nand_device,
                          const char *fmt, ...);

#define kfio_barrier()          asm volatile("mfence":::"memory")
#define kfio_store_barrier()    asm volatile("sfence":::"memory")
#define kfio_load_barrier()     asm volatile("lfence":::"memory")

// Returns true if we're running on what is considered a server OS
#define fusion_is_server_os()     0

static inline uint64_t kfio_rdtsc(void)
{
    unsigned hi, lo;
    __asm__ __volatile__ ("rdtsc" : "=a"(lo), "=d"(hi));
    return ( (uint64_t)lo)|( ((uint64_t)hi)<<32 );
}

/* Linux port implements kinfo backend using sysctl. */
#define KFIO_INFO_USE_OS_BACKEND 1

/**
 * @}
 */

#endif /* __FIO_PORT_KTYPES_H__ */