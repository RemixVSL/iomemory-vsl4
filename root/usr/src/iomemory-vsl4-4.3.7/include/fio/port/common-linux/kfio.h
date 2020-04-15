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
//-----------------------------------------------------------------------------

#ifndef __FIO_PORT_LINUX_KFIO_H__
#define __FIO_PORT_LINUX_KFIO_H__

#ifndef __FIO_PORT_KFIO_H__
#error Do not include this file directly - instead include <fio/port/kfio.h>
#endif

/* The strategy is to migrate to a more general portability layer.  There
 * should be very few OS-specifics in this file.
 */

#if defined(__KERNEL__)

#include <fio/port/ktypes.h>

#endif

#include <fio/port/commontypes.h>



// this include comes from gcc, not from linux kernel
#include <stdarg.h>

/**
 * @ingroup PORT_COMMON_LINUX
 * @{
 */
#if !defined(FIO_UNIT_TEST)
#define KFIO_PORT_MSG_LEVEL_DEFINED 1
#endif

extern const char *kfio_print_prefix[];
#define KERN_PRINT_ERR    0
#define KERN_PRINT_INFO   1
#define KERN_PRINT_DEBUG  2
#define KERN_PRINT_CONT   3

#ifndef FIO_UNIT_TEST
# define kfio_kprint(prefix, fmt, level, ...) \
     kfio_print("%s" prefix fmt, kfio_print_prefix[level], ## __VA_ARGS__)
#else
# define kfio_kprint(prefix, fmt, level, ...) \
     kfio_print(prefix fmt,  ## __VA_ARGS__)
#endif

#define errprint(fmt, ...)       C_ASSERT(0); // errprint has been deprecated
#define infprint(fmt, args...)   kfio_kprint("fioinf ", fmt, KERN_PRINT_INFO, ##args)

#define dbgkprint(fmt, args...)  kfio_kprint("fiodbg ", fmt, KERN_PRINT_DEBUG, ##args)
#define cdbgkprint(fmt, args...) kfio_kprint("",        fmt, KERN_PRINT_CONT, ##args)

#if FUSION_INTERNAL
# define engprint(fmt, args...)  kfio_kprint("fioeng ", fmt, KERN_PRINT_INFO, ##args)
# define cengprint(fmt, args...) kfio_kprint("",        fmt, KERN_PRINT_CONT, ##args)
#endif

extern void *kfio_ioremap_nocache (unsigned long offset, unsigned long size);
extern void  kfio_iounmap(void *addr);

/* disk / block stuff */

#define kfio_cmpxchg(dst, old, newv)    cmpxchg((dst), (old), (newv))
#define kfio_xchg(dst, value)           xchg((dst), (value))

#if defined(__KERNEL__)

#include <fio/port/common-linux/div64.h>

# define kfio_do_div(n, rem, base) do {                       \
            uint32_t __base = (base);                         \
            (rem) = kfio_mod64_64((uint64_t)(n), __base);     \
            (n) = kfio_div64_64((uint64_t)(n), __base);       \
        } while (0)

#endif

#if FUSION_INTERNAL

// Debug interface for saving a call stack and printing it at a later point.
// This implementation is mostly gcc-specific; however, the symbol decode
// in kfio_print_saved_call_stack() is linux-specific.
//
// Sample usage:
//
// {
//      KFIO_DECLARE_SAVED_CALL_STACK(my_caller_stack);
//
//      kfio_save_call_stack(&my_caller_stack);
//
//     ... time passes ...
//
//     infprint("saved call stack:\n");
//     kfio_print_saved_call_stack(&my_caller_stack);
// }

#define MAX_SAVED_STACK_FRAMES 16 // If you change this, must also change kfio_save_call_stack().

typedef struct
{
    void *addr[MAX_SAVED_STACK_FRAMES];
} kfio_saved_call_stack;

#define KFIO_DECLARE_SAVED_CALL_STACK(name) kfio_saved_call_stack (name);

/// @brief zero a saved call stack.
static inline void kfio_reset_saved_call_stack(kfio_saved_call_stack *stack)
{
    unsigned i;
    for (i = 0; i < MAX_SAVED_STACK_FRAMES; ++i)
    {
        stack->addr[i] = NULL;
    }
}

/// @brief save the current call stack into the given state buffer.
///
/// Note that gcc requires the parameter to __builtin_*_address to
/// be a compile time constant, which is why this is not a simple loop.
static inline void kfio_save_call_stack(kfio_saved_call_stack *stack)
{
    kfio_reset_saved_call_stack(stack);

    if (!__builtin_frame_address(0)) return;
    stack->addr[0] = __builtin_return_address(0);
    if (!__builtin_frame_address(1)) return;
    stack->addr[1] = __builtin_return_address(1);
    if (!__builtin_frame_address(2)) return;
    stack->addr[2] = __builtin_return_address(2);
    if (!__builtin_frame_address(3)) return;
    stack->addr[3] = __builtin_return_address(3);
    if (!__builtin_frame_address(4)) return;
    stack->addr[4] = __builtin_return_address(4);
    if (!__builtin_frame_address(5)) return;
    stack->addr[5] = __builtin_return_address(5);
    if (!__builtin_frame_address(6)) return;
    stack->addr[6] = __builtin_return_address(6);
    if (!__builtin_frame_address(7)) return;
    stack->addr[7] = __builtin_return_address(7);
    if (!__builtin_frame_address(8)) return;
    stack->addr[8] = __builtin_return_address(8);
    if (!__builtin_frame_address(9)) return;
    stack->addr[9] = __builtin_return_address(9);
    if (!__builtin_frame_address(10)) return;
    stack->addr[10] = __builtin_return_address(10);
    if (!__builtin_frame_address(11)) return;
    stack->addr[11] = __builtin_return_address(11);
    if (!__builtin_frame_address(12)) return;
    stack->addr[12] = __builtin_return_address(12);
    if (!__builtin_frame_address(13)) return;
    stack->addr[13] = __builtin_return_address(13);
    if (!__builtin_frame_address(14)) return;
    stack->addr[14] = __builtin_return_address(14);
    if (!__builtin_frame_address(15)) return;
    stack->addr[15] = __builtin_return_address(15);
}

C_ASSERT(MAX_SAVED_STACK_FRAMES == 16); // Else must fix implementation of kfio_save_call_stack() and update this assert.

/// @brief print the given saved call stack.
///
/// uses printk() for output.
extern void kfio_print_saved_call_stack(const kfio_saved_call_stack *stack);

#endif // FUSION_INTERNAL

/**
 * @}
 */

#endif /* __FIO_PORT_LINUX_KFIO_H__ */
