//-----------------------------------------------------------------------------
// Copyright (c) 2012-2014, Fusion-io, Inc.(acquired by SanDisk Corp. 2014)
// Copyright (c) 2014 SanDisk Corp. and/or all its affiliates. All rights reserved.
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

/* This implementation of multiple granularity locking
 * is based in part on the code in Ch. 8.3 of:
 *
 * Gray, Jim, and Andreas Reuter. Transaction Processing: Concepts and Techniques.
 * San Mateo, CA: Morgan Kaufmann, 1993. Print.
 *
 * and the "rwsem" implementation in linux kernel v3.5.
 */

#ifndef __SIX_LOCK_H__
#define __SIX_LOCK_H__

#include <fio/port/types.h>

// BIG WARNING
// Notation and *order* is taken from Gray and Reuter (1993)
// This makes it easy to verify the tables
typedef enum
{
    LOCK_NULL = 0,
    LOCK_IS   = 1,                 // Intent-shared lock
    LOCK_IX   = 2,                 // Intent-exclusive lock
    LOCK_S    = 3,                 // Shared lock
    LOCK_SIX  = 4,                 // Shared + intent-exclusive lock
    LOCK_U    = 5,                 // Update lock (p.408)
    LOCK_X    = 6,                 // Exclusive lock
    NUM_SIX_LOCK_CLASSES = 7
} fusion_sixsem_lock_t;

typedef int8_t   fusion_sixsem_mask_size_t;
typedef uint16_t fusion_sixsem_wait_size_t;
typedef uint16_t fusion_sixsem_held_size_t;

typedef struct
{
    fusion_spinlock_t            sixsem_lock;
    fusion_condvar_t             sixsem_cv;

    fusion_sixsem_mask_size_t    cur_locks_mask;
    fusion_sixsem_held_size_t    lock_counts[NUM_SIX_LOCK_CLASSES];
    fusion_sixsem_wait_size_t    wait_count;
    uint64_t                     wake_serial;
    uint64_t                     wait_serial;
} fusion_sixsem_t;

// These five functions represent the full external API.
extern void fusion_sixsem_init(fusion_sixsem_t *sem, const char* sem_name);
extern int  fusion_sixsem_down(fusion_sixsem_t *sem, fusion_sixsem_lock_t mode);
extern bool fusion_sixsem_down_trylock(fusion_sixsem_t *sem, fusion_sixsem_lock_t mode);
extern int  fusion_sixsem_upgrade(fusion_sixsem_t *sem, fusion_sixsem_lock_t old_mode, fusion_sixsem_lock_t mode);
extern void fusion_sixsem_up(fusion_sixsem_t *sem, fusion_sixsem_lock_t mode);
extern void fusion_sixsem_destroy(fusion_sixsem_t *sem);

#endif //__SIX_LOCK_H__
