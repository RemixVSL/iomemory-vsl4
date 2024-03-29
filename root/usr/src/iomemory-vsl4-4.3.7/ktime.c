//-----------------------------------------------------------------------------
// Copyright (c) 2006-2014, Fusion-io, Inc.(acquired by SanDisk Corp. 2014)
// Copyright (c) 2014-2016 SanDisk Corp. and/or all its affiliates. All rights reserved.
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
// -----------------------------------------------------------------------------

#include "port-internal-boss.h"
#include <fio/port/ktime.h>

#include <linux/jiffies.h>
#include <linux/time.h>
#include <linux/timer.h>
#include <linux/delay.h>

C_ASSERT(sizeof(struct fusion_timer_list) >= sizeof(struct timer_list));

/**
 * @ingroup PORT_COMMON_LINUX
 * @{
 */

/**
 * @brief delays in kernel context
 */
void noinline kfio_udelay(unsigned int usecs)
{
    udelay(usecs);
}

/**
 * @brief delays in kernel context
 */
void noinline kfio_msleep(unsigned int millisecs)
{
    msleep(millisecs);
}

void noinline kfio_msleep_noload(unsigned int millisecs)
{
    unsigned long remainder = msleep_interruptible(millisecs);

    /*
     * FH-22522
     *
     * The msleep_interruptible() function does not add to the task
     * cpu usage accounting, but isn't guaranteed to delay for the
     * full duration.  This has been observed during module load
     * time at boot.
     *
     * Because it's unclear how often a reschedule can occur, if the
     * previous sleep completed early, we'll use the sleep with a
     * guaranteed minimum duration instead.  This will add to the
     * tasks observable load, but won't burn any real cpu time.
     */
    if (remainder)
    {
        msleep(remainder);
    }
}

/**
 * @brief returns number of clock ticks since last system reboot
 */
uint64_t noinline fusion_get_lbolt(void)
{
    return get_jiffies_64();
}

/**
 * @brief returns # ticks per second
 */
uint64_t noinline fusion_HZ(void)
{
    return HZ;
}

/**
 * @brief returns number of seconds since last system reboot
 */
uint32_t noinline kfio_get_seconds(void)
{
    // this hurts...till we can patch the binary .so we just do this
    // means we are toast in 2038 though....
    return (uint32_t) ktime_get_real_seconds();
}

/**
 * @brief returns current time in nanoseconds
 */
uint64_t noinline fusion_getnanotime(void)
{
    return (fusion_get_lbolt() * 1000000000) / fusion_HZ();
}

/**
 * @brief returns current time in microseconds
 */
uint64_t noinline fusion_getmicrotime(void)
{
    return get_jiffies_64() * 1000000 / HZ;
}

/// @brief return current UTC wall clock time in seconds since the Unix epoch (Jan 1 1970).
uint64_t noinline fusion_getwallclocktime(void)
{
    struct timespec64 ts;
    ktime_get_coarse_real_ts64(&ts);
    return (uint64_t)ts.tv_sec;
}

/*******************************************************************************
*******************************************************************************/
uint64_t noinline fusion_usectohz(uint64_t u)
{
    if (u > jiffies_to_usecs(MAX_JIFFY_OFFSET))
        return MAX_JIFFY_OFFSET;
#if HZ <= USEC_PER_SEC && !(USEC_PER_SEC % HZ)
    return kfio_div64_64(u + (USEC_PER_SEC / HZ) - 1, USEC_PER_SEC / HZ);
#elif HZ > USEC_PER_SEC && !(HZ % USEC_PER_SEC)
    return u * (HZ / USEC_PER_SEC);
#else
    return kfio_div64_64(u * HZ + USEC_PER_SEC - 1, USEC_PER_SEC);
#endif
    return (u * HZ / 1000000);
}

/*******************************************************************************
*******************************************************************************/
uint64_t noinline fusion_hztousec(uint64_t hertz)
{
    return (hertz * (1000000 / HZ));
}

/**
 * support for delayed one shots
 */
void noinline fusion_init_timer(struct fusion_timer_list* timer)
{
    // init_timer() interface is very old and deprecated.
    //init_timer((struct timer_list *) timer);
}

/**
 *
 */
void noinline fusion_set_timer_function(struct fusion_timer_list* timer,
    void (*f) (uintptr_t))
{
    // init_timer() interface is very old and deprecated.
    //((struct timer_list *) timer)->function = f;
}

/**
 *
 */
void noinline fusion_set_timer_data(struct fusion_timer_list* timer, uintptr_t d)
{
    // init_timer() interface is very old and deprecated.
    //((struct timer_list *) timer)->data = d;
}

/**
 * @param timer the timer
 * @param t is in ticks
 */
void noinline fusion_set_relative_timer(struct fusion_timer_list* timer, uint64_t t)
{
    // init_timer() interface is very old and deprecated.
    //mod_timer((struct timer_list *) timer, fusion_get_lbolt() + t);
}

/**
 *
 */
void noinline fusion_del_timer(struct fusion_timer_list* timer)
{
    // init_timer() interface is very old and deprecated.
    //del_timer_sync((struct timer_list *) timer);
}

/**
 * @}
 */
