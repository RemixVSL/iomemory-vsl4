//-----------------------------------------------------------------------------
// Copyright (c) 2006-2014, Fusion-io, Inc.(acquired by SanDisk Corp. 2014)
// Copyright (c) 2014-2015, SanDisk Corp. and/or all its affiliates. All rights reserved.
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
#error This file supports linux only
#endif

#include <linux/types.h>
#include <linux/module.h>
#include <linux/kthread.h>

#include <asm/uaccess.h>

#include <fio/port/dbgset.h>
#include <fio/port/kfio_config.h>
#if !defined(__VMKLNX__)
#include <linux/kallsyms.h>
#endif

/**
 * @ingroup PORT_LINUX
 * @{
 */

typedef struct FUSION_STRUCT_ALIGN(8) _linux_spinlock
{
    spinlock_t lock;
    unsigned long flags;
} linux_spinlock_t;
#define FUSION_SPINLOCK_NOT_IRQSAVED ~0UL

/*
 * Spinlock wrappers
 *
 * If the flags member of the linuc_spinlock_t structure is set to
 *  FUSION_SPINLOCK_NOT_IRQSAVED then the spinlock is held without
 *  IRQ saving.
 */
void noinline fusion_init_spin(fusion_spinlock_t *s, const char *name)
{
    linux_spinlock_t *ps = (linux_spinlock_t *) s;

    spin_lock_init(&ps->lock);
}

// spin_lock must always be called in a consistent context, i.e. always in
// user context or always in interrupt context. The following two macros
// make this distinction. Use fusion_spin_lock() if your lock is always
// obtained in user context; use fusion_spin_lock_irqdisabled if your
// lock is always obtained in interrupt contect (OR with interrupts
// disabled); and use fusion_spin_trylock_irqsave if it can be called in
// both or if you don't know.
void noinline fusion_spin_lock(fusion_spinlock_t *s)
{
    linux_spinlock_t *ps = (linux_spinlock_t *) s;
#if FUSION_DEBUG && !defined(CONFIG_PREEMPT_RT) && !defined(__VMKLNX__)
    kassert(!irqs_disabled());
#endif
    spin_lock(&ps->lock);
    ps->flags = FUSION_SPINLOCK_NOT_IRQSAVED;
}

void fusion_destroy_spin(fusion_spinlock_t *s)
{
}

int noinline fusion_spin_is_locked(fusion_spinlock_t *s)
{
    return spin_is_locked(&((linux_spinlock_t *)s)->lock);
}

void noinline fusion_spin_lock_irqdisabled(fusion_spinlock_t *s)
{
#if FUSION_DEBUG && !defined(CONFIG_PREEMPT_RT)
    kassert(irqs_disabled());
#endif
    spin_lock(&((linux_spinlock_t *)s)->lock);
}

int noinline fusion_spin_trylock(fusion_spinlock_t *s)
{
    linux_spinlock_t *ps = (linux_spinlock_t *) s;
    int ret;

    ret = spin_trylock(&ps->lock);
    if (ret) /* we have the lock */
    {
        ps->flags = FUSION_SPINLOCK_NOT_IRQSAVED;
    }
    return ret ? 1 : 0;
}

void noinline fusion_spin_unlock(fusion_spinlock_t *s)
{
    linux_spinlock_t *ps = (linux_spinlock_t *) s;
    spin_unlock(&ps->lock);
}

/* The flags parameter is modified before the lock is locked
 * so we only modify the linux_spinlock_t.flags after acquiring
 * the spinlock.
 */
void noinline fusion_spin_lock_irqsave(fusion_spinlock_t *s)
{
    linux_spinlock_t *ps = (linux_spinlock_t *) s;
    unsigned long flags;

    spin_lock_irqsave(&ps->lock, flags);
    ps->flags = flags;
}

void noinline fusion_spin_lock_irq(fusion_spinlock_t *s)
{
    spin_lock_irq(&((linux_spinlock_t *)s)->lock);
}

/* The flags parameter is modified before the lock is tried
 * so we only modify the linux_spinlock_t.flags if we have
 * the spinlock held.
 */
int noinline fusion_spin_trylock_irqsave(fusion_spinlock_t *s)
{
    linux_spinlock_t *ps = (linux_spinlock_t *) s;
    unsigned long flags;
    int ret ;

    ret = spin_trylock_irqsave(&ps->lock, flags);
    if (ret) /* lock is held */
    {
        ps->flags = flags;
    }
    return ret ? 1 : 0;
}

void noinline fusion_spin_unlock_irqrestore(fusion_spinlock_t *s)
{
    linux_spinlock_t *ps = (linux_spinlock_t *) s;

#if defined(__VMKLINUX__)
    /* VMware defines their irqrestore implementation as a macro; acting on the
     * actual flags in the lock, rather than a copy (or one passed-by-value).
     * Here, we correct this manually. VMware fixed this for ESXi 5.0 only.
     */
    unsigned long flags = ps->flags;
    spin_unlock_irqrestore(&ps->lock, flags);
#else
    spin_unlock_irqrestore(&ps->lock, ps->flags);
#endif
}

void noinline fusion_spin_unlock_irq(fusion_spinlock_t *s)
{
    spin_unlock_irq(&((linux_spinlock_t *)s)->lock);
}

/**
 * If the lock is not held the following function's return is meaningless.
 */
int noinline fusion_spin_is_irqsaved(fusion_spinlock_t *s)
{
    linux_spinlock_t *ps = (linux_spinlock_t *) s;
    return (ps->flags == FUSION_SPINLOCK_NOT_IRQSAVED ? 0 : 1);
}

/*
 * Mutex wrappers
 */

#ifdef DEFINE_MUTEX
# define KFIOC_HAS_MUTEX_SUBSYSTEM 1
#else
# define KFIOC_HAS_MUTEX_SUBSYSTEM 0
#endif

#if !KFIOC_HAS_MUTEX_SUBSYSTEM
#include <asm/semaphore.h>
#endif

void fusion_mutex_init(fusion_mutex_t *lock, const char *name)
{
#if KFIOC_HAS_MUTEX_SUBSYSTEM
    mutex_init((struct mutex *)lock);
#else
    init_MUTEX((struct semaphore *)lock);
#endif
}

void fusion_mutex_destroy(fusion_mutex_t *lock)
{
#if KFIOC_HAS_MUTEX_SUBSYSTEM
    //  mutex_destroy((struct mutex *)lock);
#else
    do { } while(0);
#endif
}

int fusion_mutex_trylock(fusion_mutex_t *lock)
{
#if KFIOC_HAS_MUTEX_SUBSYSTEM
    return mutex_trylock((struct mutex *)lock) ? 1 : 0;
#else
    return !down_trylock((struct semaphore *)lock);
#endif
}
void fusion_mutex_lock(fusion_mutex_t *lock)
{
#if KFIOC_HAS_MUTEX_SUBSYSTEM
    mutex_lock((struct mutex *)lock);
#else
    down((struct semaphore *)lock);
#endif
}

void fusion_mutex_unlock(fusion_mutex_t *lock)
{
#if KFIOC_HAS_MUTEX_SUBSYSTEM
    mutex_unlock((struct mutex *)lock);
#else
    up((struct semaphore *)lock);
#endif
}

/*
 * Sempahore wrappers
 */

#if !defined(__VMKLNX__)
void fusion_rwsem_init(fusion_rwsem_t *x, const char *name)
{
    init_rwsem((struct rw_semaphore *)x);
}
void fusion_rwsem_destroy(fusion_rwsem_t *x)
{
    (void) x;
}
void fusion_rwsem_down_read(fusion_rwsem_t *x)
{
    down_read((struct rw_semaphore *)x);
}
/* Returns 1 if we got the lock */
int fusion_rwsem_down_read_trylock(fusion_rwsem_t *x)
{
    return down_read_trylock((struct rw_semaphore *)x);
}
void fusion_rwsem_up_read(fusion_rwsem_t *x)
{
    up_read((struct rw_semaphore *)x);
}
void fusion_rwsem_down_write(fusion_rwsem_t *x)
{
    down_write((struct rw_semaphore *)x);
}
/* Returns 1 if we got the lock */
int fusion_rwsem_down_write_trylock(fusion_rwsem_t *x)
{
    return down_write_trylock((struct rw_semaphore *)x);
}
void fusion_rwsem_up_write(fusion_rwsem_t *x)
{
    up_write((struct rw_semaphore *)x);
}
#else  // rw semaphore routines for VMKLNX
void fusion_rwsem_init(fusion_rwsem_t *x, const char *name)
{
    sema_init((struct semaphore *)x, 1);
}
void fusion_rwsem_destroy(fusion_rwsem_t *x)
{
    (void) x;
}
void fusion_rwsem_down_read(fusion_rwsem_t *x)
{
    down((struct semaphore *)x);
}
void fusion_rwsem_up_read(fusion_rwsem_t *x)
{
    up((struct semaphore *)x);
}
void fusion_rwsem_down_write(fusion_rwsem_t *x)
{
    down((struct semaphore *)x);
}
void fusion_rwsem_up_write(fusion_rwsem_t *x)
{
    up((struct semaphore *)x);
}
/* Returns 1 if we got the lock */
int fusion_rwsem_down_write_trylock(fusion_rwsem_t *x)
{
    return !down_trylock((struct semaphore *)x);
}
/* Returns 1 if we got the lock */
int fusion_rwsem_down_read_trylock(fusion_rwsem_t *x)
{
    return !down_trylock((struct semaphore *)x);
}
#endif

#if defined(__VMKLNX__)
// Version of fusion_create_kthread() that returns the thread task_struct pointer
struct task_struct *fusion_esx_create_kthread(fusion_kthread_func_t func, void *data,
                                              void *fusion_nand_device, const char *fmt, ...)
{
    va_list ap;
    char buffer[MAX_KTHREAD_NAME_LENGTH];
    struct task_struct *ts;

    va_start(ap, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, ap);
    va_end(ap);

    ts = kthread_run(func, data, "%s", buffer);

    return IS_ERR(ts)? NULL : ts;
}

void fusion_esx_wakeup_thread(struct task_struct *ts)
{
    wake_up_process(ts);
}
#endif

/*
 * Platform specific allocations and structure size checks
 */
C_ASSERT(sizeof(fusion_spinlock_t) >= sizeof(linux_spinlock_t));
C_ASSERT(sizeof(fusion_rwspin_t) >= sizeof(rwlock_t));
C_ASSERT(sizeof(fusion_rwsem_t) >= sizeof(struct rw_semaphore));
#if KFIOC_HAS_MUTEX_SUBSYSTEM
C_ASSERT(sizeof(fusion_mutex_t) >= sizeof(struct mutex));
#endif

/**
 * @}
 */
