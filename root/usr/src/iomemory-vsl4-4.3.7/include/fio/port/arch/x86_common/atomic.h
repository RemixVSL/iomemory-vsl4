//-----------------------------------------------------------------------------
// Copyright (c) 2006-2014, Fusion-io, Inc.(acquired by SanDisk Corp. 2014)
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
#ifndef _FIO_PORT_ARCH_X86_ATOMIC_H
#define _FIO_PORT_ARCH_X86_ATOMIC_H

typedef struct fusion_atomic32 {
    volatile int32_t value;
} fusion_atomic32_t;

typedef struct fusion_atomic64 {
    volatile int64_t value;
} fusion_atomic64_t;

/* x86 does reads and sets atomically */

/** @brief set of 32-bit atomic
**/
FIO_NONNULL_PARAMS static inline void fusion_atomic32_set(fusion_atomic32_t *atomp, int32_t val)
{
    atomp->value = val;
}

/** @brief read of 32-bit atomic
**/
FIO_NONNULL_PARAMS static inline int32_t fusion_atomic32_read(fusion_atomic32_t *atomp)
{
    return atomp->value;
}

/** @brief set of 64-bit atomic
**/
FIO_NONNULL_PARAMS static inline void fusion_atomic64_set(fusion_atomic64_t *atomp, int64_t val)
{
    atomp->value = val;
}

/** @brief read of 64-bit atomic
**/
FIO_NONNULL_PARAMS static inline int64_t fusion_atomic64_read(fusion_atomic64_t *atomp)
{
    return atomp->value;
}

#if defined(_MSC_VER)  /* MS compiler for MS Windows and UEFI */

#if defined(UEFI)
#define InterlockedIncrement(v)               _InterlockedIncrement(v)
#define InterlockedDecrement(v)               _InterlockedDecrement(v)
#define InterlockedExchangeAdd(v, x)          _InterlockedExchangeAdd(v, x)
#define InterlockedExchange(v, x)             _InterlockedExchange(v, x)
#define InterlockedCompareExchange(v, x, y)   _InterlockedCompareExchange(v, x, y)
#define InterlockedIncrement64(v)             _InterlockedIncrement64(v)
#define InterlockedDecrement64(v)             _InterlockedDecrement64(v)
#define InterlockedExchangeAdd64(v, x)        _InterlockedExchangeAdd64(v, x)
#define InterlockedExchange64(v, x)           _InterlockedExchange64(v, x)
#define InterlockedCompareExchange64(v, x, y) _InterlockedCompareExchange64(v, x, y)
#endif

/** @brief atomic increment of 32-bit atomic.
**/
static inline void fusion_atomic32_inc(fusion_atomic32_t *x)
{
    (void)InterlockedIncrement((volatile LONG *) &x->value);
}

/** @brief atomic increment of 32-bit atomic.
    @return value after increment
**/
static inline int32_t fusion_atomic32_incr(fusion_atomic32_t *x)
{
    return InterlockedIncrement((volatile LONG *) &x->value);
}

/** @brief atomic decrement of 32-bit atomic.
**/
static inline void fusion_atomic32_dec(fusion_atomic32_t *x)
{
    (void)InterlockedDecrement((volatile LONG *) &x->value);
}

/** @brief atomic decrement of 32-bit atomic.
    @return value after decrement
**/
static inline int32_t fusion_atomic32_decr(fusion_atomic32_t *x)
{
    return InterlockedDecrement((volatile LONG *) &x->value);
}

/** @brief atomic add of 32-bit atomic.
**/
static inline void fusion_atomic32_add(fusion_atomic32_t *x, int32_t val)
{
    (void)InterlockedExchangeAdd((volatile LONG *) &x->value, (LONG) val);
}

/** @brief atomic add of 32-bit atomic.
    @return value after addition
**/
static inline int32_t fusion_atomic32_add_return(fusion_atomic32_t *x, int32_t val)
{
    return InterlockedExchangeAdd((volatile LONG *) &x->value, (LONG) val) + val;
}

/** @brief atomic subtraction of 32-bit atomic.
**/
static inline void fusion_atomic32_sub(fusion_atomic32_t *x, int32_t val)
{
    (void)InterlockedExchangeAdd((volatile LONG *) &x->value, (LONG) -val);
}

/** @brief atomic subtraction of 32-bit atomic.
    @return value after subtraction
**/
static inline int32_t fusion_atomic32_sub_return (fusion_atomic32_t *x, int32_t val)
{
    return InterlockedExchangeAdd((volatile LONG *) &x->value, (LONG) -val) - val;
}

/** @brief atomic test and set of 32-bit atomic
    @return original value before the set
**/
static inline int32_t fusion_atomic32_exchange(fusion_atomic32_t *x, volatile int32_t val)
{
    return InterlockedExchange((volatile LONG *) &x->value, (LONG) val);
}

/** @brief atomic compare and swap of 32-bit atomic
    If *dest == oldval, *dest = newval.
    @return the value of *dest before the operation
**/
static inline int32_t fusion_atomic32_cas(fusion_atomic32_t *dest, int32_t oldval, int32_t newval)
{
    return InterlockedCompareExchange((volatile LONG *) &dest->value, (LONG) newval, (LONG) oldval);
}

/** @brief atomic compare and swap of 32-bit atomic
    If *dest == oldval, *dest = newval.
    @return true if the exchange took place
**/
static inline int fusion_atomic32_cas_bool(fusion_atomic32_t *dest, int32_t oldval, int32_t newval)
{
    return (InterlockedCompareExchange((volatile LONG *) &dest->value, (LONG) newval, (LONG) oldval) == oldval);
}

/** @brief atomic increment of 64-bit atomic.
**/
static inline void fusion_atomic64_inc(fusion_atomic64_t *atomp)
{
    (void)InterlockedIncrement64(&atomp->value);
}

/** @brief atomic increment of 64-bit atomic.
    @return value after increment
**/
static inline int64_t fusion_atomic64_incr(fusion_atomic64_t *atomp)
{
    return InterlockedIncrement64(&atomp->value);
}

/** @brief atomic decrement of 64-bit atomic.
**/
static inline void fusion_atomic64_dec(fusion_atomic64_t *atomp)
{
    (void)InterlockedDecrement64(&atomp->value);
}

/** @brief atomic decrement of 64-bit atomic.
    @return value after decrement
**/
static inline int64_t fusion_atomic64_decr(fusion_atomic64_t *atomp)
{
    return InterlockedDecrement64(&atomp->value);
}

/** @brief atomic add of 64-bit atomic.
**/
static inline void fusion_atomic64_add(fusion_atomic64_t *atomp, int64_t val)
{
    (void)InterlockedExchangeAdd64(&atomp->value, val);
}

/** @brief atomic add of 64-bit atomic.
    @return value after addition
**/
static inline int64_t fusion_atomic64_add_return(fusion_atomic64_t *atomp, int64_t val)
{
    return InterlockedExchangeAdd64(&atomp->value, val) + val;
}

/** @brief atomic subtraction of 64-bit atomic.
**/
static inline void fusion_atomic64_sub(fusion_atomic64_t *atomp, int64_t val)
{
    (void)InterlockedExchangeAdd64(&atomp->value, -val);
}

/** @brief atomic subtraction of 64-bit atomic.
    @return value after subtraction
**/
static inline int64_t fusion_atomic64_sub_return(fusion_atomic64_t *atomp, int64_t val)
{
    return InterlockedExchangeAdd64(&atomp->value, -val) - val;
}

/** @brief atomic test and set of 64-bit atomic
    @return original value before the set
**/
static inline int64_t fusion_atomic64_exchange(fusion_atomic64_t *atomp, int64_t val)
{
    return InterlockedExchange64(&atomp->value, val);
}

/** @brief atomic compare and swap of 64-bit atomic
    If *dest == oldval, *dest = newval.
    @return the value of *dest before the operation
**/
static inline int64_t fusion_atomic64_cas(fusion_atomic64_t *dest, int64_t oldval, int64_t newval)
{
    return InterlockedCompareExchange64((volatile int64_t *) &dest->value, newval, oldval);
}

/** @brief atomic compare and swap of 64-bit atomic
    If *dest == oldval, *dest = newval.
    @return true if the exchange took place
**/
static inline int fusion_atomic64_cas_bool(fusion_atomic64_t *dest, int64_t oldval, int64_t newval)
{
    return (InterlockedCompareExchange64((volatile int64_t *) &dest->value, newval, oldval) == oldval);
}

#elif defined(__GNUC__)  /* GCC for everything else*/

// Use the sync builtins if our gcc version ( >= 4.0 ) supports it.  If not
// the assembly functions following these will used instead.
#if __GNUC__ >= 4 && __GNUC_MINOR__ >= 2

/** @brief atomic increment of 32-bit atomic.
**/
static inline void fusion_atomic32_inc(fusion_atomic32_t *atomp)
{
    (void)__sync_add_and_fetch(&atomp->value, 1);
}

/** @brief atomic increment of 32-bit atomic.
    @return value after increment
**/
static inline int32_t fusion_atomic32_incr(fusion_atomic32_t *atomp)
{
    return __sync_add_and_fetch(&atomp->value, 1);
}

/** @brief atomic decrement of 32-bit atomic.
**/
static inline void fusion_atomic32_dec(fusion_atomic32_t *atomp)
{
    (void)__sync_sub_and_fetch(&atomp->value, 1);
}

/** @brief atomic decrement of 32-bit atomic.
    @return value after decrement
**/
static inline int32_t fusion_atomic32_decr(fusion_atomic32_t *atomp)
{
    return __sync_sub_and_fetch(&atomp->value, 1);
}

/** @brief atomic add of 32-bit atomic.
**/
static inline void fusion_atomic32_add(fusion_atomic32_t *atomp, int32_t val)
{
    (void)__sync_add_and_fetch(&atomp->value, val);
}

/** @brief atomic add of 32-bit atomic.
    @return value after addition
**/
static inline int32_t fusion_atomic32_add_return(fusion_atomic32_t *atomp, int32_t val)
{
    return __sync_add_and_fetch(&atomp->value, val);
}

/** @brief atomic subtraction of 32-bit atomic.
**/
static inline void fusion_atomic32_sub(fusion_atomic32_t *atomp, int32_t val)
{
    (void)__sync_sub_and_fetch(&atomp->value, val);
}

/** @brief atomic subtraction of 32-bit atomic.
    @return value after subtraction
**/
static inline int32_t fusion_atomic32_sub_return(fusion_atomic32_t *atomp, int32_t val)
{
    return __sync_sub_and_fetch(&atomp->value, val);
}

/** @brief atomic test and set of 32-bit atomic
    @return original value before the set
**/
static inline int32_t fusion_atomic32_exchange(fusion_atomic32_t *atomp, int32_t val)
{
    return __sync_lock_test_and_set(&atomp->value, val);
}

/** @brief atomic compare and swap of 32-bit atomic
    If *dest == oldval, *dest = newval.
    @return the value of *dest before the operation
**/
static inline int32_t fusion_atomic32_cas(fusion_atomic32_t *dest, int32_t oldval, int32_t newval)
{
    return __sync_val_compare_and_swap(&dest->value, oldval, newval);
}

/** @brief atomic compare and swap of 32-bit atomic
    If *dest == oldval, *dest = newval.
    @return true if the exchange took place
**/
static inline int fusion_atomic32_cas_bool(fusion_atomic32_t *dest, int32_t oldval, int32_t newval)
{
    return __sync_bool_compare_and_swap(&dest->value, oldval, newval);
}

/** @brief atomic increment of 64-bit atomic.
**/
static inline void fusion_atomic64_inc(fusion_atomic64_t *atomp)
{
    (void)__sync_add_and_fetch(&atomp->value, 1);
}

/** @brief atomic increment of 64-bit atomic.
    @return value after increment
**/
static inline int64_t fusion_atomic64_incr(fusion_atomic64_t *atomp)
{
    return __sync_add_and_fetch(&atomp->value, 1);
}

/** @brief atomic decrement of 64-bit atomic.
**/
static inline void fusion_atomic64_dec(fusion_atomic64_t *atomp)
{
    (void)__sync_sub_and_fetch(&atomp->value, 1);
}

/** @brief atomic decrement of 64-bit atomic.
    @return value after decrement
**/
static inline int64_t fusion_atomic64_decr(fusion_atomic64_t *atomp)
{
    return __sync_sub_and_fetch(&atomp->value, 1);
}

/** @brief atomic add of 64-bit atomic.
**/
static inline void fusion_atomic64_add(fusion_atomic64_t *atomp, int64_t val)
{
    (void)__sync_add_and_fetch(&atomp->value, val);
}

/** @brief atomic add of 64-bit atomic.
    @return value after addition
**/
static inline int64_t fusion_atomic64_add_return(fusion_atomic64_t *atomp, int64_t val)
{
    return __sync_add_and_fetch(&atomp->value, val);
}

/** @brief atomic subtraction of 64-bit atomic.
**/
static inline void fusion_atomic64_sub(fusion_atomic64_t *atomp, int64_t val)
{
    (void)__sync_sub_and_fetch(&atomp->value, val);
}

/** @brief atomic subtraction of 64-bit atomic.
    @return value after subtraction
**/
static inline int64_t fusion_atomic64_sub_return(fusion_atomic64_t *atomp, int64_t val)
{
    return __sync_sub_and_fetch(&atomp->value, val);
}

/** @brief atomic compare and swap of 64-bit atomic
    If *dest == oldval, *dest = newval.
    @return the value of *dest before the operation
**/
static inline int64_t fusion_atomic64_cas(fusion_atomic64_t *dest, int64_t oldval, int64_t newval)
{
    return __sync_val_compare_and_swap(&dest->value, oldval, newval);
}

/** @brief atomic compare and swap of 64-bit atomic
    If *dest == oldval, *dest = newval.
    @return true if the exchange took place
**/
static inline int fusion_atomic64_cas_bool(fusion_atomic64_t *dest, int64_t oldval, int64_t newval)
{
    return __sync_bool_compare_and_swap(&dest->value, oldval, newval);
}

/*
  Taken from http://gcc.gnu.org/onlinedocs/gcc-4.1.0/gcc/Atomic-Builtins.html

  This builtin (__sync_lock_test_and_set), as described by Intel, is not a
  traditional test-and-set operation, but rather an atomic exchange operation.
  It writes value into *ptr, and returns the previous contents of *ptr.

  Many targets have only minimal support for such locks, and do not support a full
  exchange operation. In this case, a target may support reduced functionality here
  by which the only valid value to store is the immediate constant 1. The exact value
  actually stored in *ptr is implementation defined.

  This builtin is not a full barrier, but rather an acquire barrier. This means that
  references after the builtin cannot move to (or be speculated to) before the builtin,
  but previous memory stores may not be globally visible yet, and previous memory loads
  may not yet be satisfied.
*/

/** @brief atomic test and set of 64-bit atomic
    @return original value before the set
**/
static inline int64_t fusion_atomic64_exchange(fusion_atomic64_t *atomp, int64_t val)
{
    return __sync_lock_test_and_set(&atomp->value, val);
}

#else
// Older gcc compiler here.  Hopefully the following ugly assembly code will someday disappear.
// ___ This is INTEL/AMD specific and requires 32-bit integers ONLY ___

/*

Since I'm very likely to forget how this inline assembly syntax is formatted, heres
a quick explanation for those who are unfortunate enough to have to look at it.
Inline assembly is not typical assembly syntax because most of this stuff gives the compiler a
idea of what variables change, which ones to use where, and how to protect them.
I don't use real registers in my inline so the compiler can better optimize their
use.

__asm__ __volatile__ ( "<instruction> <src reg, ie. %n>, <dst reg %n>"
: output reg/addr (1st parameter), %2, .. %n
: input reg/addr %n+1, %n+2, ... (last parameter)
: clobber list register/memory/etc. );

The output/input contents are specified with "constraint", (c code).
I used the following contrains:
"m"  memory, implied "&" (reference)
"r" register
"0" same as first output variable
"q" only registers a,b,c,d (intel only)

The following are constraint modifiers:
"=" write only operand
"i" integer type
"+" read and write operand
"memory" code changes the contents of memory

__volatile__ to make sure the compiler doesn't move the generated code
out of order in an attempt to optimize it.  With atomics we very much
need them modified _only_ when we want them modified.

- Jer

PS:  The "lock" instruction works, but is pointless on a uniprocessor machine.
So it takes another instruction, I didn't think it was worth optimizing
out for UP kernels.
*/

/** @brief atomic increment 32-bit atomic
**/
static inline void fusion_atomic32_inc(fusion_atomic32_t *x)
{
    __asm__ __volatile__("\n\tlock; incl %0"
                         :"=m"(x->value) :"m"(x->value));
}

#define fusion_atomic32_incr(x)       fusion_atomic32_add_return(x,1)

/** @brief atomic decrement 32-bit atomic
**/
static inline void fusion_atomic32_dec(fusion_atomic32_t *x)
{
    __asm__ __volatile__("\n\tlock; decl %0"
                         :"=m"(x->value) :"m"(x->value));
}

#define fusion_atomic32_decr(x)       fusion_atomic32_add_return(x,-1)

/** @brief atomic addition of 32-bit atomic and immediate value
**/
static inline void fusion_atomic32_add(fusion_atomic32_t *atomp, int32_t val)
{
    __asm__ __volatile__("\n\tlock; addl %1, %0"
                         :"=m"(atomp->value)
                         :"ir"(val), "m"(atomp->value));
}

/** @brief atomic addition of 32-bit atomic and immediate value
    @return value after addition
**/
static inline int32_t fusion_atomic32_add_return(fusion_atomic32_t *atomp, int32_t val)
{
    int i = val;

    __asm__ __volatile__("\n\tlock; xaddl %0, %1"
                         :"+r"(val), "+m"(atomp->value)
                         :
                         :"memory");
    return val + i;
}

/** @brief atomic subtraction of immediate value from 32-bit atomic
**/
static inline void fusion_atomic32_sub(fusion_atomic32_t *atomp, int32_t val)
{
    __asm__ __volatile__("\n\tlock; subl %1, %0"
                         :"=m"(atomp->value)
                         :"ir"(val), "m"(atomp->value));
}

/** @brief atomic subtraction of immediate value from 32-bit atomic
    @return value after subtraction
**/
#define fusion_atomic32_sub_return(atomp,val)  fusion_atomic32_add_return(atomp, -val)

static inline int32_t fusion_atomic32_exchange(fusion_atomic32_t *atomp, volatile int32_t val)
{
    __asm__ __volatile__("xchgl %k0,%1"
                         :"=r" (val)
                         :"m" (atomp->value), "0" (val)
                         :"memory");
    return val;
}

/** @brief atomic compare and swap of 32-bit atomic
    If *dest == oldval, *dest = newval.
    @return the value of *dest before the operation
**/
static inline int32_t fusion_atomic32_cas(fusion_atomic32_t *dest, int32_t oldval, int32_t newval)
{
    volatile int32_t rc = oldval;

    __asm__ __volatile__("\n\tlock cmpxchgl %2, %1"
                         : "+a" (rc), "+m" (dest->value)
                         : "r" (newval)
                         :"memory");
    return rc;
}

/** @brief atomic compare and swap of 32-bit atomic
    If *dest == oldval, *dest = newval.
    @return true if the exchange took place
**/
static inline int fusion_atomic32_cas_bool(fusion_atomic32_t *dest, int32_t oldval, int32_t newval)
{
    volatile int32_t rc = oldval;

    __asm__ __volatile__("\n\tlock cmpxchgl %2, %1"
                         : "+a" (rc), "+m" (dest->value)
                         : "r" (newval)
                         :"memory");
    return rc == oldval;
}

/** @brief atomic increment 64-bit atomic
**/
static inline void fusion_atomic64_inc(fusion_atomic64_t *x)
{
    __asm__ __volatile__("\n\tlock; incq %0"
                         :"=m"(x->value)
                         :"m"(x->value));
}

/** @brief atomic increment 64-bit atomic
    @return value after increment
**/
#define fusion_atomic64_incr(x)       fusion_atomic64_add_return(x,1)

/** @brief atomic decrement 64-bit atomic
**/
static inline void fusion_atomic64_dec(fusion_atomic64_t *x)
{
    __asm__ __volatile__("\n\tlock; decq %0"
                         :"=m"(x->value)
                         :"m"(x->value));
}

/** @brief atomic decrement 64-bit atomic
    @return value after decrement
**/
#define fusion_atomic64_decr(x)       fusion_atomic64_add_return(x,-1)

/** @brief atomic addition of 64-bit atomic and immediate value
**/
static inline void fusion_atomic64_add(fusion_atomic64_t *atomp, int64_t val)
{
    __asm__ __volatile__("\n\tlock; addq %1, %0"
                         :"=m"(atomp->value)
                         :"ir"(val), "m"(atomp->value));
}

/** @brief atomic addition of 64-bit atomic and immediate value
    @return value after addition
**/
static inline int64_t fusion_atomic64_add_return(fusion_atomic64_t *atomp, int64_t val)
{
    int64_t i = val;

    __asm__ __volatile__("\n\tlock; xaddq %0, %1"
                         :"+r"(val), "+m"(atomp->value)
                         :
                         :"memory");
    return val + i;
}

/** @brief atomic subtraction of immediate value from 64-bit atomic
**/
static inline void fusion_atomic64_sub(fusion_atomic64_t *atomp, int64_t val)
{
    __asm__ __volatile__("\n\tlock; subq %1, %0"
                         :"=m"(atomp->value)
                         :"ir"(val), "m"(atomp->value));
}

/** @brief atomic subtraction of immediate value from 64-bit atomic
    @return value after subtraction
**/
#define fusion_atomic64_sub_return(atomp,val)  fusion_atomic64_add_return(atomp, -val)

/** @brief atomic test and set of 64-bit atomic
    @return orignial value before set
 **/
static inline int64_t fusion_atomic64_exchange(fusion_atomic64_t *atomp, volatile int64_t val)
{
    __asm__ __volatile__("xchgq %0, %1"
                         :"=r" (val)
                         :"m" (atomp->value), "0" (val)
                         :"memory");
    return val;
}

/** @brief atomic compare and swap of 64-bit atomic
    If *dest == oldval, *dest = newval.
    @return the value of *dest before the operation
**/
static inline int64_t fusion_atomic64_cas(fusion_atomic64_t *dest, int64_t oldval, int64_t newval)
{
    volatile int64_t rc = oldval;

    __asm__ __volatile__("\n\tlock cmpxchgq %2, %1"
                         : "+a" (rc), "+m" (dest->value)
                         : "r" (newval)
                         :"memory");
    return rc;
}

/** @brief atomic compare and swap of 64-bit atomic
    If *dest == oldval, *dest = newval.
    @return true if the exchange took place
**/
static inline int fusion_atomic64_cas_bool(fusion_atomic64_t *dest, int64_t oldval, int64_t newval)
{
    volatile int64_t rc = oldval;

    __asm__ __volatile__("\n\tlock cmpxchgq %2, %1"
                         : "+a" (rc), "+m" (dest->value)
                         : "r" (newval)
                         :"memory");
    return rc == oldval;
}


#endif /* __GNUC__ */
#else /* Not GCC nor MS compiler */
#error "Please implement atomic functions for your compiler."
#endif

#endif /* _FIO_PORT_ARCH_X86_ATOMIC_H */
