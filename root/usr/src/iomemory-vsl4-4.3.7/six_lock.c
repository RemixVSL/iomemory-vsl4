//-----------------------------------------------------------------------------
// Copyright (c) 2012-2014, Fusion-io, Inc.(acquired by SanDisk Corp. 2014)
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

/* This implementation of multiple granularity locking
 * is based in part on the code in Ch. 8.3 of:
 *
 * Gray, Jim, and Andreas Reuter. Transaction Processing: Concepts and Techniques.
 * San Mateo, CA: Morgan Kaufmann, 1993. Print.
 *
 * and the "rwsem" implementation in linux kernel v3.5.
 *
 * BIG SCARY WARNING:  ALL THE TABLES GIVEN BELOW DEPEND ON THE ORDER OF
 * THE fusion_sixsem_lock_t ENUM.
 *
 */

#include <fio/port/six_lock.h>
#include <fio/port/dbgset.h>
#include <fio/port/errno.h>

#define FOR_ALL_LOCK_CLASSES(ind)                       \
    for (ind = 0; ind < NUM_SIX_LOCK_CLASSES; ind++)

#define INIT_LOCK_COUNTS(ind, sem)                              \
    FOR_ALL_LOCK_CLASSES(ind) { sem->lock_counts[ind] = 0; }

/*
 * The COMPAT_MASK/INCOMPAT_MASK definitions encode the SIX-lock lock table.
 * Ex. Request for IS lock => check incompat_masks[LOCK_IS] against currently held locks.
 */
#define MASK_NULL  (1 << LOCK_NULL)   // Unlocked lock
#define MASK_IS    (1 << LOCK_IS)     // Intent-shared lock
#define MASK_IX    (1 << LOCK_IX)     // Intent-exclusive lock
#define MASK_S     (1 << LOCK_S)      // Shared lock
#define MASK_SIX   (1 << LOCK_SIX)    // Shared + intent-exclusive lock
#define MASK_U     (1 << LOCK_U)      // Update lock
#define MASK_X     (1 << LOCK_X)      // Exclusive lock

static const fusion_sixsem_mask_size_t lock_mode_masks[] =
{
    MASK_NULL,
    MASK_IS,
    MASK_IX,
    MASK_S,
    MASK_SIX,
    MASK_U,
    MASK_X,
};

C_ASSERT(sizeof(lock_mode_masks) / sizeof(fusion_sixsem_mask_size_t) == NUM_SIX_LOCK_CLASSES);

#define COMPAT_MASK_NULL           (MASK_NULL | MASK_S  | MASK_IS | MASK_IX | MASK_SIX | MASK_X | MASK_U)
#define COMPAT_MASK_IS             (MASK_NULL | MASK_S  | MASK_IS | MASK_IX | MASK_SIX)
#define COMPAT_MASK_IX             (MASK_NULL | MASK_IS | MASK_IX)
#define COMPAT_MASK_S              (MASK_NULL | MASK_S  | MASK_IS)
#define COMPAT_MASK_SIX            (MASK_NULL | MASK_IS)
#define COMPAT_MASK_U              (MASK_NULL | MASK_S)
#define COMPAT_MASK_X              (MASK_NULL)

static const fusion_sixsem_mask_size_t incompat_masks[] =
{
    (~COMPAT_MASK_NULL),
    (~COMPAT_MASK_IS),
    (~COMPAT_MASK_IX),
    (~COMPAT_MASK_S),
    (~COMPAT_MASK_SIX),
    (~COMPAT_MASK_U),
    (~COMPAT_MASK_X),
};

C_ASSERT(sizeof(incompat_masks) / sizeof(fusion_sixsem_mask_size_t) == NUM_SIX_LOCK_CLASSES);

// [new_mode][old_mode]
static const fusion_sixsem_lock_t upgrade_conversion[NUM_SIX_LOCK_CLASSES][NUM_SIX_LOCK_CLASSES] =
{
    // LOCK_NULL
    { LOCK_NULL, LOCK_IS,  LOCK_IX,  LOCK_S,   LOCK_SIX, LOCK_U,   LOCK_X },
    // LOCK_IS
    { LOCK_IS,   LOCK_IS,  LOCK_IX,  LOCK_S,   LOCK_SIX, LOCK_U,   LOCK_X },
    // LOCK_IX
    { LOCK_IX,   LOCK_IX,  LOCK_IX,  LOCK_SIX, LOCK_SIX, LOCK_X,   LOCK_X },
    // LOCK_S
    { LOCK_S,    LOCK_S,   LOCK_SIX, LOCK_S,   LOCK_SIX, LOCK_U,   LOCK_X },
    // LOCK_SIX
    { LOCK_SIX,  LOCK_SIX, LOCK_SIX, LOCK_SIX, LOCK_SIX, LOCK_SIX, LOCK_X },
    // LOCK_U
    { LOCK_U,    LOCK_U,   LOCK_X,   LOCK_U,   LOCK_SIX, LOCK_U,   LOCK_X },
    // LOCK_X
    { LOCK_X,    LOCK_X,   LOCK_X,   LOCK_X,   LOCK_X,   LOCK_X,   LOCK_X },
};

// Internal functions
/// @brief Return true if lock request is compatible with currently held locks
static inline bool six_lock_is_compatible(fusion_sixsem_t *sem, fusion_sixsem_lock_t old_mode, fusion_sixsem_lock_t new_mode)
{
    fusion_sixsem_mask_size_t locks_mask = sem->cur_locks_mask;

    kassert(fusion_spin_is_locked(&sem->sixsem_lock));

    // No locks are held
    if (sem->cur_locks_mask == 0)
    {
        return true;
    }

    if (sem->lock_counts[old_mode] == 1)
    {
        locks_mask &= ~lock_mode_masks[old_mode];
    }

    return !(incompat_masks[new_mode] & locks_mask);
}

// API functions

/// @brief Initialize a SIX-semaphore
void fusion_sixsem_init(fusion_sixsem_t *sem, const char *sem_name)
{
    int i;

    // Condvar initialization
    fusion_cv_lock_init(&sem->sixsem_lock, sem_name);
    fusion_condvar_init(&sem->sixsem_cv, sem_name);

    // Initialize an empty semaphore
    INIT_LOCK_COUNTS(i, sem);
    sem->cur_locks_mask = 0;
    sem->wait_count = 0;
    sem->wake_serial = 0;
    sem->wait_serial = 0;
}

/**
   @brief Compare wrapping 32 bit serial numbers
   @return  -1 if s1 < s2
             0 if s1 == s2
             1 if s1 > s2
 */

static inline int serial64_compare(uint64_t s1, uint64_t s2)
{
    if (s1 == s2)
    {
        return 0;
    }

    if (s1 < s2)
    {
        if (s2 - s1 < (1ULL << 63))
        {
            return -1;
        }

        return 1;
    }

    if (s2 - s1 < (1ULL << 63))
    {
        return 1;
    }

    return -1;
}

/*
 * @brief Request a lock in mode new_mode while holding it in mode
 * old_mode. If not immediately granted, the request is placed in a
 * wait queue. If the wait queue or lock is full, then drop the
 * request and return with error -EAGAIN.
 */
int fusion_sixsem_upgrade(fusion_sixsem_t *sem, fusion_sixsem_lock_t old_mode, fusion_sixsem_lock_t new_mode)
{
    int      retval = 0;
    bool     init_flag = true;
    uint64_t serial;

    new_mode = upgrade_conversion[new_mode][old_mode];

    fusion_cv_lock(&sem->sixsem_lock);

    dbgprint(DBGS_SIX_LOCK, "Attempt to take a lock of class %d (from %d) on a sixsem.\n", new_mode, old_mode);
    kassert(old_mode >= 0 && old_mode < NUM_SIX_LOCK_CLASSES);
    kassert(new_mode >= 0 && new_mode < NUM_SIX_LOCK_CLASSES);

    // Increment wait count and check whether wait queue is full
    if (unlikely(++sem->wait_count == 0))
    {
        sem->wait_count--;
        retval = -EAGAIN;
        goto exit;
    }

    serial = sem->wait_serial;

    if (old_mode != LOCK_NULL)
    {
        sem->wait_serial++;
    }

    /*
     * Wait for compatibility
     *
     * When there are other waiters, the init_flag prevents
     * threads from cutting in line, and therefore prevents starvation.
     */
    while ((sem->wait_count > 1 && init_flag) ||
           (serial64_compare(serial, sem->wake_serial) < 0) ||
           !six_lock_is_compatible(sem, old_mode, new_mode))
    {
        init_flag = false;
        fusion_condvar_wait(&sem->sixsem_cv, &sem->sixsem_lock);
    }

    // Lock is granted - let the next batch through
    if (old_mode != LOCK_NULL)
    {
        sem->wake_serial++;
    }

    if (old_mode != LOCK_NULL)
    {
        kassert(sem->lock_counts[old_mode]);
        if (!--sem->lock_counts[old_mode])
        {
            sem->cur_locks_mask &= (~lock_mode_masks[old_mode]);
        }
    }

    // Increment relevant lock count and check for overflow
    if(unlikely(++sem->lock_counts[new_mode] == 0))
    {
        sem->lock_counts[new_mode]--;
        retval = -EAGAIN;
        goto exit;
    }

    // Bookkeeping. Also wake next waiter.
    sem->cur_locks_mask |= lock_mode_masks[new_mode];
    if (--sem->wait_count > 0)
    {
        fusion_condvar_signal(&sem->sixsem_cv);
    }

 exit:
    fusion_cv_unlock(&sem->sixsem_lock);
    return retval;
}

/*
 * @brief Request a lock in some mode while not holding it at all. If
 * not immediately granted, the request is placed in a wait queue. If
 * the wait queue or lock is full, then drop the request and return
 * with error -EAGAIN.
 */
int fusion_sixsem_down(fusion_sixsem_t *sem, fusion_sixsem_lock_t new_mode)
{
    return fusion_sixsem_upgrade(sem, LOCK_NULL, new_mode);
}

/*
 * @brief Request a lock in some mode. If immediately granted, the lock
 * is acquired and the function returns true.
 * Otherwise, the lock request is dropped and the function returns with false.
 */
bool fusion_sixsem_down_trylock(fusion_sixsem_t *sem, fusion_sixsem_lock_t mode)
{
    bool got_lock = false;

    fusion_cv_lock(&sem->sixsem_lock);

    dbgprint(DBGS_SIX_LOCK, "Attempt to trylock a lock of class %d on a sixsem.\n", mode);
    kassert(mode >= 0 && mode < NUM_SIX_LOCK_CLASSES);

    // There are waiters; can't immediately get the lock
    if (sem->wait_count > 0)
    {
        goto exit;
    }

    // Check if lock is compatible and there are no pending conversions
    if (sem->wait_serial == sem->wake_serial &&
        six_lock_is_compatible(sem, LOCK_NULL, mode))
    {
        // Increment relevant lock count and check for overflow
        if (++sem->lock_counts[mode] == 0)
        {
            sem->lock_counts[mode]--;
            goto exit;
        }
        sem->cur_locks_mask |= lock_mode_masks[mode];
        got_lock = true;
    }

 exit:
    fusion_cv_unlock(&sem->sixsem_lock);
    return got_lock;
}

/// @brief Release a lock in some mode.
void fusion_sixsem_up(fusion_sixsem_t *sem, fusion_sixsem_lock_t mode)
{
    fusion_cv_lock(&sem->sixsem_lock);

    dbgprint(DBGS_SIX_LOCK, "Attempt to unlock a lock of class %d on a sixsem.\n", mode);
    kassert(sem->lock_counts[mode] > 0
            && (sem->cur_locks_mask & lock_mode_masks[mode]) != 0);

    // Bookkeeping
    if (--sem->lock_counts[mode] == 0)
    {
        sem->cur_locks_mask &= (~lock_mode_masks[mode]);
    }

    // Check whether we can pull something off the wait queue.
    fusion_condvar_signal(&sem->sixsem_cv);

    fusion_cv_unlock(&sem->sixsem_lock);
}

/// @brief Cleanup and destroy
void fusion_sixsem_destroy(fusion_sixsem_t *sem)
{
    kassert(sem->cur_locks_mask == 0 && sem->wait_count == 0);

    fusion_cv_lock_destroy(&sem->sixsem_lock);
    fusion_condvar_destroy(&sem->sixsem_cv);
}
