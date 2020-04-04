//-----------------------------------------------------------------------------
// Copyright (c) 2006-2014, Fusion-io, Inc.(acquired by SanDisk Corp. 2014)
// Copyright (c) 2014-2019 SanDisk Corp. and/or all its affiliates. All rights reserved.
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

/* FIXME - this should be added to some general include so these are globally available
 * without specifically including this file.
 */

#ifndef __FIO_PORT_COMPILER_H__
#define __FIO_PORT_COMPILER_H__

#if defined(USERSPACE_KERNEL)
# include <fio/port/userspace/compiler.h>
#elif defined(__linux__) || defined(__VMKLNX__)
# include <fio/port/gcc/compiler.h>
#elif defined(__SVR4) && defined(__sun)
# include <fio/port/solaris/compiler.h>
#elif defined(__FreeBSD__)
# include <fio/port/freebsd/compiler.h>
#elif defined(__OSX__)
# include <fio/port/osx/compiler.h>
#elif defined(WIN32) || defined(WINNT)
# include <fio/port/windows/compiler.h>
#elif defined(UEFI)
# include <fio/port/uefi/compiler.h>
#else
# error Unsupported OS - please define a fio/port/<platform>/compiler.h and include it in fio/port/compiler.h
#endif

#if (defined(WIN32) || defined(WINNT))
#include <sal.h>
#define FIO_ACQUIRES_LOCK(lock) _Acquires_lock_(lock)
#define FIO_RELEASES_LOCK(lock) _Releases_lock_(lock)
#define FIO_REQUIRES_LOCK_HELD(lock) _Requires_lock_held_(lock)
#define FIO_REQUIRES_LOCK_NOT_HELD(lock) _Requires_lock_not_held_(lock)

// pfsl stands for pointer to fusion_spinlock_t
// osl  stands for an opaque data type that has a spin lock

/// @brief Used for forward declarations of an unspecified
///        struct or typedef (opaque) which contains a spinlock
#define FIO_OPAQUE_STRUCT_HAS_SPINLOCK _Has_lock_kind_(_Lock_kind_spinlock_)

#define FIO_ACQUIRES_ISR_SPINLOCK           FIO_ACQUIRES_SPINLOCK // Linux ISR SLs map to windows regular SLs. No Windows Interrupt SLs used.
#define FIO_ACQUIRES_CV_LOCK                FIO_ACQUIRES_SPINLOCK

/// @brief Used for a function that acquires a fusion_spinlock_t
///        that is visible to the caller.
#define FIO_ACQUIRES_SPINLOCK(pfsl)             \
    _At_((pfsl), _Inout_)                       \
    _Acquires_nonreentrant_lock_((pfsl)->lock)  \
    _IRQL_requires_max_(DISPATCH_LEVEL)         \
    _At_((pfsl)->irql, _IRQL_saves_)            \
    _IRQL_raises_(DISPATCH_LEVEL)

/// @brief Used for a function that acquires a spinlock contained
///        in a data type that is opaque to the caller.
#define FIO_ACQUIRES_SPINLOCK_OPAQUE(osl)       \
    _Acquires_nonreentrant_lock_((osl))         \
    _IRQL_requires_max_(DISPATCH_LEVEL)         \
    _IRQL_saves_global_(OldIrql, (osl))         \
    _IRQL_raises_(DISPATCH_LEVEL)

#define FIO_RELEASES_ISR_SPINLOCK FIO_RELEASES_SPINLOCK
#define FIO_RELEASES_CV_LOCK      FIO_RELEASES_SPINLOCK

#define FIO_RELEASES_SPINLOCK(pfsl)             \
    _At_((pfsl), _Inout_)                       \
    _Releases_nonreentrant_lock_((pfsl)->lock)  \
    _IRQL_requires_(DISPATCH_LEVEL)             \
    _At_((pfsl)->irql, _IRQL_restores_)

#define FIO_RELEASES_SPINLOCK_OPAQUE(osl)       \
    _Releases_nonreentrant_lock_((osl))         \
    _IRQL_requires_(DISPATCH_LEVEL)             \
    _IRQL_restores_global_(OldIrql, (osl))

#define FIO_REQUIRES_ISR_SPINLOCK_HELD FIO_REQUIRES_SPINLOCK_HELD
#define FIO_REQUIRES_CV_LOCK_HELD      FIO_REQUIRES_SPINLOCK_HELD

/// @brief Used for a function that requires holding a
///        visible fusion_spinlock_t.
#define FIO_REQUIRES_SPINLOCK_HELD(pfsl)        \
    _At_((pfsl), _In_)                          \
    _Requires_lock_held_((pfsl)->lock)          \
    _IRQL_requires_(DISPATCH_LEVEL)

/// @brief Used for a function that acquires a visible
///        fusion_spinlock_t upon returning 1 or true.
#define FIO_ACQUIRES_SPINLOCK_WHEN_RETURNS_ONE(pfsl)    \
    _Must_inspect_result_                               \
    _Post_satisfies_(return == 1 || return == 0)        \
    _Success_(return != 0)                              \
    /* All post annotations only apply on success */    \
    FIO_ACQUIRES_SPINLOCK(pfsl)                         \
    _On_failure_(_IRQL_requires_same_)

/// @brief Used for a function that acquires an opaqued
///        fusion_spinlock_t upon returning 1 or true.
#define FIO_ACQUIRES_SPINLOCK_OPAQUE_WHEN_RETURNS_ONE(osl)  \
    _Must_inspect_result_                                   \
    _Post_satisfies_(return == 1 || return == 0)            \
    _Success_(return != 0)                                  \
    /* All post annotations only apply on success */        \
    FIO_ACQUIRES_SPINLOCK_OPAQUE(osl)                       \
    _On_failure_(_IRQL_requires_same_)

/// @brief Used for a function that acquires a visible
///        fusion_spinlock_t if its pointer is not NULL.
#define FIO_ACQUIRES_SPINLOCK_WHEN_NOT_NULL(pfsl)       \
    _IRQL_requires_max_(DISPATCH_LEVEL)                 \
    _At_((pfsl), _Inout_opt_)                           \
    _When_((pfsl) != NULL, FIO_ACQUIRES_SPINLOCK(pfsl))

#define FIO_RELEASES_SPINLOCK_WHEN_NOT_NULL(pfsl)       \
    _At_((pfsl), _Inout_opt_)                           \
    _When_((pfsl) != NULL, FIO_RELEASES_SPINLOCK(pfsl))

#define FIO_ACQUIRES_ISR_SPINLOCK_WHEN_NOT_NULL FIO_ACQUIRES_SPINLOCK_WHEN_NOT_NULL
#define FIO_RELEASES_ISR_SPINLOCK_WHEN_NOT_NULL FIO_RELEASES_SPINLOCK_WHEN_NOT_NULL

/// @brief Used for a function called by the implementation of opaque spinlock interface.
///        You only need to use this if you want to associate an opaque spinlock
///        with a visible (at some other scope) fusion_spinlock_t.
#define FIO_OPAQUE_SPINLOCK_TARGET(osl,pfsl) _Post_same_lock_((osl), (pfsl))

/// @brief Used for a function that acquires a mutex contained
///        in a data type that is opaque to the caller.
#define FIO_ACQUIRES_MUTEX_OPAQUE(oml)  \
    _Acquires_nonreentrant_lock_((oml)) \
    _IRQL_requires_max_(APC_LEVEL)      \
    _IRQL_saves_global_(OldIrql, (oml)) \
    _IRQL_raises_(APC_LEVEL)

#define FIO_RELEASES_MUTEX_OPAQUE(oml)  \
    _Releases_nonreentrant_lock_(oml)   \
    _IRQL_requires_(APC_LEVEL)          \
    _IRQL_restores_global_(OldIrql, oml)

/// @brief Used for a function that enters critical region with lock
#define FIO_ENTER_CRITICAL_REGION_LOCKED(crl)   \
    _Acquires_lock_(_Global_critical_region_)   \
    _Acquires_lock_(crl)                        \
    _IRQL_requires_max_(APC_LEVEL)

#define FIO_LEAVE_CRITICAL_REGION_UNLOCKED(crl) \
    _Releases_lock_(_Global_critical_region_)   \
    _Releases_lock_(crl)                        \
    _IRQL_requires_max_(DISPATCH_LEVEL)

/// @brief Used for a function that requires lock l to be held
///        by the caller and reacuires the lock before returning
#define FIO_HOLDS_LOCK_FOR_CV(l)        \
    _IRQL_requires_max_(DISPATCH_LEVEL) \
    _Releases_exclusive_lock_(l)        \
    _Acquires_nonreentrant_lock_(l)     \
    _IRQL_raises_(DISPATCH_LEVEL)

#define FIO_ANALYZER_ASSUME(p)                      __analysis_assume(p)

// It is difficult (if not impossible) to explain to the windows analyzer that a lock
// was obtained by a called function--unless the lock is global or based on a parameter.
#define FIO_ANALYZER_ASSUME_LOCK_HELD(sl)           _Analysis_assume_lock_held_(sl)

#define FIO_SUPPRESS_MISSING_ANNOTATION_IN_HEADER   __pragma(warning(suppress:28301))
#define FIO_WORK_AROUND_MUTEX_ANALYZER_BUG          __pragma(warning(suppress:28167))   // Not restoring irql before return
#define FIO_SUPPRESS_FUNC_CALL_NOT_PERMITTED_WARN   __pragma(warning(suppress: 28121))
#define FIO_SUPPRESS_INCONSISTENT_IRQL_WARNING      __pragma(warning(suppress: 28156))

#define FIO_SUPPRESS_OVERFLOW_ON_SHIFT_WARNING      __pragma(warning(suppress:6297))    // Arithmetic overflow - 32-bit shift and then cast to 64-bit
#define FIO_SUPPRESS_OVERFLOW_ON_OP_WARNING         __pragma(warning(suppress:26451))   // Arithmetic overflow - operation on smaller # of bytes and then cast to larger # of bytes
#define FIO_SUPPRESS_BUFFER_OVERFLOW_WARNING        __pragma(warning(suppress:6386))    // Buffer overflow
#define FIO_NULL_DEREFERENCE_WARNING                __pragma(warning(suppress:6011))    // Dereferencing NULL pointer
#define FIO_SUPPRESS_NON_ZERO_CONSTANT_WARNING      __pragma(warning(suppress:6235))    // Expression is always a non-zero constant

#define FIO_SUPPRESS_FAILED_TO_ACQUIRE_LOCK         __pragma(warning(suppress:26166))   // Possibly failing to acquire or to hold lock <lock> in function <func>
#define FIO_SUPPRESS_FAILED_TO_RELEASE_LOCK         __pragma(warning(suppress:26165))   // Possibly failing to release lock <lock> in function <func>.

#define FIO_IN_PARAMETER                            _In_                                // Implies it cannot be NULL
#define FIO_OUT_PARAMETER                           _Out_
#else

#define FIO_ACQUIRES_LOCK(lock)
#define FIO_RELEASES_LOCK(lock)
#define FIO_REQUIRES_LOCK_HELD(lock)
#define FIO_REQUIRES_LOCK_NOT_HELD(lock)
#define FIO_OPAQUE_STRUCT_HAS_SPINLOCK
#define FIO_ACQUIRES_ISR_SPINLOCK FIO_ACQUIRES_SPINLOCK
#define FIO_ACQUIRES_CV_LOCK      FIO_ACQUIRES_SPINLOCK
#define FIO_ACQUIRES_SPINLOCK(l)
#define FIO_RELEASES_ISR_SPINLOCK FIO_RELEASES_SPINLOCK
#define FIO_RELEASES_CV_LOCK      FIO_RELEASES_SPINLOCK
#define FIO_RELEASES_SPINLOCK(l)
#define FIO_REQUIRES_ISR_SPINLOCK_HELD FIO_REQUIRES_SPINLOCK_HELD
#define FIO_REQUIRES_CV_LOCK_HELD      FIO_REQUIRES_SPINLOCK_HELD
#define FIO_REQUIRES_SPINLOCK_HELD(l)
#define FIO_ACQUIRES_SPINLOCK_OPAQUE(osl)
#define FIO_RELEASES_SPINLOCK_OPAQUE(osl)
#define FIO_ACQUIRES_SPINLOCK_WHEN_RETURNS_ONE(l)
#define FIO_ACQUIRES_SPINLOCK_OPAQUE_WHEN_RETURNS_ONE(osl)
#define FIO_ACQUIRES_SPINLOCK_WHEN_NOT_NULL(l)
#define FIO_RELEASES_SPINLOCK_WHEN_NOT_NULL(l)
#define FIO_ACQUIRES_ISR_SPINLOCK_WHEN_NOT_NULL FIO_ACQUIRES_SPINLOCK_WHEN_NOT_NULL
#define FIO_RELEASES_ISR_SPINLOCK_WHEN_NOT_NULL FIO_RELEASES_SPINLOCK_WHEN_NOT_NULL
#define FIO_OPAQUE_SPINLOCK_TARGET(osl,pfsl)
#define FIO_ACQUIRES_MUTEX_OPAQUE(oml)
#define FIO_RELEASES_MUTEX_OPAQUE(oml)
#define FIO_ENTER_CRITICAL_REGION_LOCKED(crl)
#define FIO_LEAVE_CRITICAL_REGION_UNLOCKED(crl)
#define FIO_HOLDS_LOCK_FOR_CV(l)
#define FIO_ANALYZER_ASSUME(p)
#define FIO_SUPPRESS_MISSING_ANNOTATION_IN_HEADER
#define FIO_WORK_AROUND_MUTEX_ANALYZER_BUG
#define FIO_ANALYZER_ASSUME_LOCK_HELD(sl)
#define FIO_SUPPRESS_FUNC_CALL_NOT_PERMITTED_WARN
#define FIO_SUPPRESS_INCONSISTENT_IRQL_WARNING
#define FIO_SUPPRESS_OVERFLOW_ON_SHIFT_WARNING
#define FIO_SUPPRESS_OVERFLOW_ON_OP_WARNING
#define FIO_SUPPRESS_BUFFER_OVERFLOW_WARNING
#define FIO_NULL_DEREFERENCE_WARNING
#define FIO_SUPPRESS_NON_ZERO_CONSTANT_WARNING
#define FIO_IN_PARAMETER
#define FIO_OUT_PARAMETER
#endif

#ifdef __COVERITY__
# define FUSION_COVERITY_ONLY(exp) (exp)
#else
# define FUSION_COVERITY_ONLY(exp) ((void)0)
#endif

#ifndef __has_attribute
#define __has_attribute(x) 0
#endif

#if ((!defined(__clang__) && ((__GNUC__ == 4 && __GNUC_MINOR__ >= 9) || (__GNUC__ > 4))) || \
     (defined(__clang__) && __has_attribute(returns_nonnull)))
#define FIO_RETURNS_NONNULL __attribute__((returns_nonnull))
#else
#define FIO_RETURNS_NONNULL
#endif

// Note: This function attribute compile time checks pointers for NULL values
// and also permits compiler optimization on the assumption that the pointer
// will not be null at runtime.
#if ((!defined(__clang__) && defined __GNUC__) || (defined(__clang__) && __has_attribute(nonnull)))
#define FIO_NONNULL_PARAMS __attribute__((nonnull))
#define FIO_NONNULL_PARAMS_DEFINED 1
#else
#define FIO_NONNULL_PARAMS
#define FIO_NONNULL_PARAMS_DEFINED 0
#endif

#if ((!defined(__clang__) && ((__GNUC__ == 2 && __GNUC_MINOR__ >= 4) || (__GNUC__ > 2))) || \
     (defined(__clang__) && __has_attribute(unused)))
#define FIO_UNUSED __attribute__((unused))
#else
#define FIO_UNUSED
#endif

#endif
