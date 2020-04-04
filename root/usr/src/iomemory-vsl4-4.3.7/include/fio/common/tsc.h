//-----------------------------------------------------------------------------
// Copyright (c) 2015 SanDisk Corp. and/or all its affiliates. All rights reserved.
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

#ifndef _FIO_COMMON_TSC_H_
#define _FIO_COMMON_TSC_H_

#include <fio/port/ktypes.h>
#include <fio/port/ktime.h>

#define FIO_TSC_CORES_NOT_SYNCED 0

#if FIO_TSC_CORES_NOT_SYNCED
#include <fio/port/kcpu.h>
#endif

/// @brief Time Stamp Counter
/// @note The Time Stamp Counter is potentially unique for each CPU, so if we are going
///       to compare them, we had better remember which CPU the value was taken from,
///       unless the TSCs are guaranteed to be synchronized between CPUs
///       (which we think is true in all modern processors).
struct fio_tsc
{
    uint64_t   tsc_value;
#if FIO_TSC_CORES_NOT_SYNCED
    kfio_cpu_t tsc_cpu;
#endif
};

/// @brief Get the tsc now.
static inline void fio_tsc_now(struct fio_tsc *tsc)
{
#if FIO_TSC_CORES_NOT_SYNCED
    tsc->tsc_cpu = kfio_current_cpu();
#endif
    tsc->tsc_value = kfio_rdtsc();
}

/// @brief Find the difference between TSCs.
/// @note If there is a problem finding it, return 0. We can use 0 as an error
///       value because it is impossible to get two TSCs on exactly the same CPU clock.
/// @note If the difference is <0, return 0.
static inline uint64_t fio_tsc_diff(struct fio_tsc *t1, struct fio_tsc *t2)
{
    int64_t diff = t2->tsc_value - t1->tsc_value;

#if FIO_TSC_CORES_NOT_SYNCED
    if (t2->tsc_cpu != t1->tsc_cpu)
    {
        return 0;
    }
#endif

    return diff > 0 ? (uint64_t)diff : 0;
}

/// @brief Find the difference from a TSC and now.
/// @note If there is a problem finding it, return 0.
static inline uint64_t fio_tsc_diff_from_now(struct fio_tsc *tsc1)
{
    struct fio_tsc now;

    fio_tsc_now(&now);
    return fio_tsc_diff(tsc1, &now);
}

extern uint64_t fio_tsc_per_us(void);

#endif  // _FIO_COMMON_TSC_H_
