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

#ifndef __FIO_PORT_BITOPS_H__
# define __FIO_PORT_BITOPS_H__

# if defined(__x86_64__) || defined(__i386__) || defined(_M_AMD64) || defined(_M_IX86)
#  include <fio/port/arch/x86_common/atomic.h>
#  include <fio/port/arch/x86_common/bits.h>
#  if !defined(WIN32) && !defined(WINNT)
#   include <fio/port/arch/x86_common/cache.h>
#  endif
# elif defined(__PPC64__) || defined(__PPC__)
#  include <fio/port/arch/ppc/atomic.h>
#  include <fio/port/arch/ppc/bits.h>
#  include <fio/port/arch/ppc/cache.h>

# elif defined(__mips64)
#  include <fio/port/arch/mips/atomic.h>
#  include <fio/port/arch/mips/bits.h>
#  include <fio/port/arch/mips/cache.h>

// Stubs for platforms of interest to which we will not port
# elif defined(__xtensa__) || defined(__aarch64__) || defined(_M_ARM)
#  include <fio/port/arch/stubbed/atomic.h>
#  include <fio/port/arch/stubbed/bits.h>
#  include <fio/port/arch/stubbed/cache.h>

# else
# error "You must define bit operations for your platform."
# endif

/*---------------------------------------------------------------------------*/

// If we are using GCC, there are builtins for bitcounting which can exploit
// magic processor instructions. However these require library support which
// is not available in-kernel.
# if defined(__GNUC__) && !defined(__KERNEL__)

/// @brief return number of bits set in the given 8 bit quantity.
static inline unsigned fio_bitcount8(uint8_t val)
{
    return (uint32_t)__builtin_popcount(val);
}

/// @brief return number of bits set in the given 32 bit quantity.
static inline unsigned fio_bitcount32(uint32_t val)
{
    return (uint32_t)__builtin_popcount(val);
}

/// @brief return number of bits set in the given 64 bit quantity.
static inline unsigned fio_bitcount64(uint64_t val)
{
    return (uint32_t)__builtin_popcountl(val);
}

# else // Not using GCC bitcount builtins.

/// @brief return number of bits set in the given 8 bit quantity.
///
/// Algorithm available in the public domain, see http://www-graphics.stanford.edu/~seander/bithacks.html#CountBitsSetParallel.
static inline unsigned fio_bitcount8(uint8_t val)
{
    val = val - ((val >> 1) & 0x55);
    val = (val & 0x33) + ((val >> 2) & 0x33);
    return (val + (val >> 4)) & 0x0F;
}
/// @brief return number of bits set in the given 32 bit quantity.
///
/// Algorithm available in the public domain, see http://www-graphics.stanford.edu/~seander/bithacks.html#CountBitsSetParallel.
static inline unsigned fio_bitcount32(uint32_t val)
{
    val = val - ((val >> 1) & 0x55555555);
    val = (val & 0x33333333) + ((val >> 2) & 0x33333333);
    return (((val + (val >> 4)) & 0xF0F0F0F) * 0x1010101) >> 24;
}

/// @brief return number of bits set in the given 64 bit quantity.
///
/// Algorithm available in the public domain, see http://www-graphics.stanford.edu/~seander/bithacks.html#CountBitsSetParallel.
static inline unsigned fio_bitcount64(uint64_t val)
{
    val = val - ((val >> 1) & 0x5555555555555555ULL);
    val = (val & 0x3333333333333333ULL) + ((val >> 2) & 0x3333333333333333ULL);
    return (((val + (val >> 4)) & 0xf0f0f0f0f0f0f0fULL) * 0x101010101010101ULL) >> 56;
}
# endif // __GNUC__

/// @brief return the index of the 'n'th set bit in the given 32-bit value.
///
/// 'n' is one based and index is zero based, i.e. fio_get_nth_bit_set_index32(x, 1)
/// returns the zero-based index of the first set bit in x. It is an error to invoke
/// with n less than 1 or greater than 32.
///
/// Returns 32 if no such bit found.
static inline uint32_t fio_get_nth_bit_set_index32(const uint32_t val, uint32_t n)
{
    uint32_t found = 0;
    uint32_t index = 0;

    while (index < 32)
    {
        if (val & (1 << index))
        {
            if (++found == n)
            {
                break;
            }
        }
        index++;
    }

    return index;
}

/// @brief return the index of the top set bit in the given 32-bit value.
///
/// Returns -1 if no such bit found.
static inline int32_t fio_get_top_bit_set_index32(const uint32_t val)
{
    int32_t index = 31;

    while (index >= 0)
    {
        if (val & (1 << index))
        {
            break;
        }
        --index;
    }

    return index;
}
/// @brief Round up to the next highest power of 2 for the given uint32_t
///
/// Returns 1 in case of 0, since 0 is not a power of 2.
static inline uint32_t fio_round_up_power_of_2_u32(uint32_t v)
{
    v += (v == 0);
    v--;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    v++;
    return v;
}

#endif /* __FIO_PORT_BITOPS_H__  */

