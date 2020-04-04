//-----------------------------------------------------------------------------
// Copyright (c) 2013-2014, Fusion-io, Inc.(acquired by SanDisk Corp. 2014)
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
//-----------------------------------------------------------------------------

#ifndef __FIO_PORT_RANGES_H__
#define __FIO_PORT_RANGES_H__

#include "fio/port/compiler.h"

// Long story as to why these are here in contrast to another file.
// This file is used in both kernel and user space, and the porting layer
// is not kind to such files.  Hence this madness


/// @brief A generic range class.
typedef struct
{
    uint64_t base;
    uint64_t length;
} fio_range_t;

typedef fio_range_t fio_block_range_t;
typedef fio_range_t fio_mem_range_t;


/// fio_block_range_t is a logical address of a block of data.
/// Block == no header == just the data, not a packet with a header.
/// Do not assume this is 512 bytes, it is whatever size the log specifies for
/// blocks.
typedef uint64_t fio_bid_t;
typedef uint64_t fio_blen_t;

#define FIO_ILLEGAL_LPN 0xFFFFFFFFFFFFFFFFULL // change if the above is changed.
#define FIO_INVALID_BID 0xFFFFFFFFFFFFFFFFULL // Deprecated but still in use.


// Helper functions for working with ranges.

#ifndef FIOMIN
#define FIOMIN(a, b)   (((a) < (b)) ? (a) : (b))
#endif

#ifndef FIOMAX
#define FIOMAX(a, b)   (((a) > (b)) ? (a) : (b))
#endif

/// @brief Construct a block range from its components
static inline fio_block_range_t fio_make_block_range(uint64_t base, uint64_t length)
{
    fio_block_range_t retval;

    retval.base = base;
    retval.length = length;

    return retval;
}

/// @brief Return true if two ranges are exactly equal
static inline int fio_range_equal(fio_range_t r1, fio_range_t r2)
{
    return (r1.base == r2.base && r1.length == r2.length);
}

/// @brief Return true if the given range's length wraps past the 64-bit limit
static inline int fio_range_wrap(fio_range_t r1)
{
    return (r1.base + r1.length < r1.base);
}

/**
   @returns - the sector following the range. (It is NOT the last
   sector of the range.)  For example a range base of 100
   and size of 1 would have an upper bound of 101 even
   though its last sector is 100.
*/
static inline uint64_t fio_range_upper_bound(fio_range_t range)
{
    return range.base + range.length;
}

/// @brief Return the last valid location in the given range.
static inline uint64_t fio_range_last(fio_range_t range)
{
    kassert(range.length > 0);
    return range.base + range.length - 1;
}

/// @brief Returns 1 if there is any overlap in two provided ranges.
static inline int fio_ranges_overlap(fio_range_t r1, fio_range_t r2)
{
    if (r1.base < fio_range_upper_bound(r2) &&
        r2.base < fio_range_upper_bound(r1))
    {
        return 1;
    }

    return 0;
}

/// @brief Create a block range which is the intersection of two given block ranges
/// @return True if the computed intersection is non-empty
static inline int fio_range_intersect(fio_range_t r1, fio_range_t r2,
                                      fio_range_t *r)
{
    fio_bid_t base = FIOMAX(r1.base, r2.base);
    fio_bid_t past = FIOMIN(r1.base + r1.length, r2.base + r2.length);

    if (past > base)
    {
        r->base = base;
        r->length = past - r->base;
        return 1;
    }

    return 0;
}

static inline int fio_range_overlaps_tail(fio_range_t tail, fio_range_t other)
{
    return (tail.base > other.base) && (tail.base < fio_range_upper_bound(other));
}

static inline int fio_range_bisects(fio_range_t bisector, fio_range_t other)
{
    return (bisector.base > other.base) && (fio_range_upper_bound(bisector) < fio_range_upper_bound(other));
}

static inline int fio_range_overlaps_head(fio_range_t head, fio_range_t other)
{
    return (other.base < fio_range_upper_bound(head));
}

static inline fio_range_t fio_range_add_offset(fio_range_t r, uint64_t offset)
{
    fio_range_t retval = r;

    retval.base += offset;

    return retval;
}

static inline fio_range_t fio_range_sub_offset(fio_range_t r, uint64_t offset)
{
    fio_range_t retval = r;

    retval.base -= offset;

    return retval;
}

static inline void fio_range_offset(fio_range_t *r, int64_t offset)
{
    r->base += offset;
}


#endif /* __FIO_PORT_BLOCK_RANGE_H__ */
