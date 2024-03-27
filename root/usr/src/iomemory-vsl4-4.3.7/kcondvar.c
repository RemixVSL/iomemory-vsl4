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

#include "port-internal-boss.h"
#include <fio/port/dbgset.h>
#include <fio/port/kcondvar.h>
#include <fio/port/ktime.h>
#include <linux/sched.h>    // for struct task_struct used in kassert

int noinline __fusion_condvar_timedwait(fusion_condvar_t *cv,
                                        fusion_cv_lock_t *lock,
                                        int64_t timeout_us,
                                        int interruptible);

/**
 * @ingroup PORT_COMMON_LINUX
 * @{
 */
/*
  URGENT!!! Pay special attention to the wait state!

  On Linux, waiting with the uninterruptible flag will cause
  process accounting to consider the task _running_, even
  though it is really sleeping!  This thereby drastically
  increases the amount of CPU the task is recorded as using.

  We still want to block as uninterruptible but ensure we are counted
  accurately as sleeping, so we must set the interruptible flag
  and then ignore any signals which may be received.  The following
  table explains our behavior based on the platform:

  Linux:
  fusion_condvar_timedwait()         - uninterruptible
  fusion_condvar_timedwait_noload()  - interruptible

  Solaris, FreeBSD, etc.:
  fusion_condvar_timedwait()         - uninterruptible
  fusion_condvar_timedwait_noload()  - uninterruptible

*/

/*******************************************************************************
*******************************************************************************/

// "include/fio/port/kcondvar.h" indicates that fusion_condvar_t must be >= wait_queue_head_t, not wait_queue_t,
//      and also the functions that use fusion_condvar_t all typecast to wait_queue_head_t, not wait_queue_t.
//  Thus, this should be compared to wait_queue_head_t.
C_ASSERT(sizeof(fusion_condvar_t) >= sizeof(wait_queue_head_t));

void noinline fusion_schedule_yield(void)
{
    schedule();
}

/**
*/
void noinline fusion_condvar_init (fusion_condvar_t *cv, const char *name)
{
    init_waitqueue_head((wait_queue_head_t *) cv); // public kernel interface
    return;
}

/**
 */
void noinline fusion_condvar_destroy (fusion_condvar_t *cv)
{
    return;
}

/**
 * Signals one waiter on this cv.
 */
void noinline fusion_condvar_signal (fusion_condvar_t *cv)
{
    wait_queue_head_t *q = (wait_queue_head_t *) cv;
    wake_up(q);
}

/**
 *  Signals all waiters on this cv.
 */
void noinline fusion_condvar_broadcast (fusion_condvar_t *cv)
{
    wait_queue_head_t *q = (wait_queue_head_t *) cv;
    wake_up_all(q);
}

/**
 * The function @sa fusion_condvar_signal(cv) must be called when a change to
 * the wait condition occurs.
 *
 * On return the lock is held.
 *
 * N.B. To ensure SMP safety, use the functions @sa fusion_cv_lock()
 *   and @sa fusion_cv_unlock() around writes that changes the condition
 *   protected by this cv.
 */
void noinline fusion_condvar_wait(fusion_condvar_t *cv,
                                  fusion_cv_lock_t *lock)
{   int is_irqsaved;
    DEFINE_WAIT(_wait);
    wait_queue_head_t *q = (wait_queue_head_t *) cv;

    is_irqsaved = fusion_cv_lock_is_irqsaved(lock);
    /* Linux wakes all threads in wake_up() unless
     * prepare_to_wait_exclusive() is called */
    prepare_to_wait_exclusive(q, &_wait, TASK_INTERRUPTIBLE);

    if (is_irqsaved)
    {
        fusion_cv_unlock_irq(lock);
    }
    else
    {
        fusion_cv_unlock(lock);
    }

    schedule();

    if (is_irqsaved)
    {
        fusion_cv_lock_irq(lock);
    }
    else
    {
        fusion_cv_lock(lock);
    }

    finish_wait(q, &_wait);
}

int noinline __fusion_condvar_timedwait(fusion_condvar_t *cv,
                                      fusion_cv_lock_t *lock,
                                      int64_t timeout_us,
                                      int interruptible)
{
    int is_irqsaved;
    DEFINE_WAIT(_wait);
    wait_queue_head_t *q = (wait_queue_head_t *) cv;
    int64_t tmo = fusion_usectohz(timeout_us);
    int ret;

    is_irqsaved = fusion_cv_lock_is_irqsaved(lock);

    /* Linux wakes all threads in wake_up() unless
     * prepare_to_wait_exclusive() is called */
    if (interruptible)
    {
        prepare_to_wait_exclusive(q, &_wait, TASK_INTERRUPTIBLE);
    }
    else
    {
        prepare_to_wait_exclusive(q, &_wait, TASK_UNINTERRUPTIBLE);
    }

    if (is_irqsaved)
    {
        fusion_cv_unlock_irq(lock);
    }
    else
    {
        fusion_cv_unlock(lock);
    }

    ret = schedule_timeout(tmo);

    if (is_irqsaved)
    {
        fusion_cv_lock_irq(lock);
    }
    else
    {
        fusion_cv_lock(lock);
    }

    finish_wait(q, &_wait);

    return (ret == 0) ? FUSION_WAIT_TIMEDOUT : FUSION_WAIT_TRIGGERED;
}

/**
 * @return reason return value:
 * -FUSION_WAIT_TIMEDOUT      (0)
 * -FUSION_WAIT_INTERRUPTED   (-1)
 * -FUSION_WAIT_TRIGGERED     (1)
 */
int noinline fusion_condvar_timedwait(fusion_condvar_t *cv,
                                      fusion_cv_lock_t *lock,
                                      int64_t timeout_us)
{
    return __fusion_condvar_timedwait(cv, lock, timeout_us, 0);
}

/**
 * This is identical to @sa fusion_condvar_timedwait() but the thread is
 *  set as interruptible to avoid adding to the load average.
 */
int noinline fusion_condvar_timedwait_noload(fusion_condvar_t *cv,
                                             fusion_cv_lock_t *lock,
                                             int64_t timeout_us)
{
    /* Only safe from kernel threads! */
    kassert(!current->mm);
    return __fusion_condvar_timedwait(cv, lock, timeout_us, 1);
}

/** Same as fusion_condvar_timedwait_noload() but returns the time elapsed */
/**
 * This is identical to @sa fusion_condvar_timedwait() but the thread is
 *  set as interruptible to avoid adding to the load average.
 */
uint64_t noinline fusion_condvar_timedwait_noload_elapsed(fusion_condvar_t *cv,
                                                          fusion_cv_lock_t *lock,
                                                          int64_t timeout_us)
{
    int is_irqsaved;
    DEFINE_WAIT(_wait);
    wait_queue_head_t *q = (wait_queue_head_t *) cv;
    int64_t tmo = fusion_usectohz(timeout_us);
    uint32_t ret;

    is_irqsaved = fusion_cv_lock_is_irqsaved(lock);
    /* Linux wakes all threads in wake_up() unless
     * prepare_to_wait_exclusive() is called */
    prepare_to_wait_exclusive(q, &_wait, TASK_INTERRUPTIBLE);

    if (is_irqsaved)
    {
        fusion_cv_unlock_irq(lock);
    }
    else
    {
        fusion_cv_unlock(lock);
    }
    ret = schedule_timeout(tmo);

    if (is_irqsaved)
    {
        fusion_cv_lock_irq(lock);
    }
    else
    {
        fusion_cv_lock(lock);
    }

    finish_wait(q, &_wait);

    return (timeout_us - fusion_hztousec(ret));
}


void noinline fusion_cond_resched(void)
{
    cond_resched();
}

/**
 * @}
 */
