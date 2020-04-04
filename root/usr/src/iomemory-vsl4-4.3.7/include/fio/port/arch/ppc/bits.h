//-----------------------------------------------------------------------------
// Copyright (c) 2013-2014, Fusion-io, Inc.(acquired by SanDisk Corp. 2014)
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

#ifndef _FIO_PORT_ARCH_PPC_BITS_H
#define _FIO_PORT_ARCH_PPC_BITS_H

#if !defined(__PPC64__) && !defined(__PPC__)
#error "This file is for PPC CPUs."
#endif

typedef uint32_t fusion_bits_t;

/**
 * @brief: returns 1 iff bit # v is set in *p else returns 0.
 */
static inline int fio_test_bit_atomic(int v, const volatile fusion_bits_t *p)
{
    __asm__ volatile__ ("\n\t   eieio"
                        "\n\t   sync"
                        : : : );
    return ((1U << (v & 31)) & *p) != 0;
}

/**
 * @brief atomic set of bit # v of *p.
 */
static inline void fio_set_bit_atomic(int v, volatile fusion_bits_t *p)
{
    fusion_bits_t mask = (1U << (v & 31) );
    fusion_bits_t tmp;

    __asm__ volatile__ ("\n\t1: lwarx %0, 0, %2"
                        "\n\t   or %0, %0, %4"
                        "\n\t   stwcx. %0, 0, %2"
                        "\n\t   bne- 1"
                        "\n\t   eieio"
                        "\n\t   sync"
                        :"=&r"(tmp), "=m"(*p)
                        :"r"(p), "m"(*p), "m"(mask)
                        :"cc", "memory" );
}

/**
 * @brief atomic clear of bit # v of *p.
 */
static inline void fio_clear_bit_atomic(int v, volatile fusion_bits_t *p)
{
    fusion_bits_t mask = (1U << (v & 31) );
    fusion_bits_t tmp;

    __asm__ volatile__ ("\n\t1: lwarx %0, 0, %2"
                        "\n\t   andc %0, %0, %4"
                        "\n\t   stwcx. %0, 0, %2"
                        "\n\t   bne- 1"
                        "\n\t   eieio"
                        "\n\t   sync"
                        :"=&r"(tmp), "=m"(*p)
                        :"r"(p), "m"(*p), "m"(mask)
                        :"cc", "memory" );
}

/**
 * @brief atomic set of bit # v of *p, returns old value.
 */
static inline unsigned char fio_test_and_set_bit_atomic(int v, volatile fusion_bits_t *p)
{
    fusion_bits_t mask = (1U << (v & 31) );
    fusion_bits_t tmp;
    fusion_bits_t old;

    __asm__ volatile__ ("\n\t1: lwarx %0, 0, %3"
                        "\n\t   stwx %0, 0, %2"
                        "\n\t   or %0, %0, %5"
                        "\n\t   stwcx. %0, 0, %3"
                        "\n\t   bne- 1"
                        "\n\t   eieio"
                        "\n\t   sync"
                        :"=&r"(tmp), "=m"(*p), "=m"(old)
                        :"r"(p), "m"(*p), "m"(mask)
                        :"cc", "memory" );
    return !!(old & ~mask);
}

/**
 * @brief atomic clear of bit # v of *p, returns old value.
 */
static inline unsigned char fio_test_and_clear_bit_atomic(int v, volatile fusion_bits_t *p)
{
    fusion_bits_t mask = (1U << (v & 31) );
    fusion_bits_t tmp;
    fusion_bits_t old;

    __asm__ volatile__ ("\n\t1: lwarx %0, 0, %3"
                        "\n\t   stwx %0, 0, %2"
                        "\n\t   andc %0, %0, %5"
                        "\n\t   stwcx. %0, 0, %3"
                        "\n\t   bne- 1"
                        "\n\t   eieio"
                        "\n\t   sync"
                        :"=&r"(tmp), "=m"(*p), "=m"(old)
                        :"r"(p), "m"(*p), "m"(mask)
                        :"cc", "memory" );
    return !!(old & ~mask);
}

/**
 * @brief atomic clear of bit # v of *p.
 */
static inline void fio_clear_bit_atomic64(int v, volatile uint64_t *p)
{
    fusion_bits_t mask = (1U << (v & 63) );
    uint64_t tmp;

    __asm__ volatile__ ("\n\t1: ldarx %0, 0, %2"
                        "\n\t   andc %0, %0, %4"
                        "\n\t   stdcx. %0, 0, %2"
                        "\n\t   bne- 1"
                        "\n\t   eieio"
                        "\n\t   sync"
                        :"=&r"(tmp), "=m"(*p)
                        :"r"(p), "m"(*p), "m"(mask)
                        :"cc", "memory" );
}

#endif //_FIO_PORT_ARCH_PPC_BITS_H
