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

#ifndef __FIO_PORT_KFIO_H__
#define __FIO_PORT_KFIO_H__

#include <fio/port/ktypes.h>
#include <fio/port/kmem.h>
#include <fio/port/kmsg.h>

#if defined(USERSPACE_KERNEL)
#include <fio/port/userspace/kfio.h>
#elif defined(UEFI)
#include <fio/port/uefi/kfio.h>
#elif defined(__linux__)
#include <fio/port/common-linux/kfio.h>
#elif defined(__SVR4) && defined(__sun)
#include <fio/port/bitops.h>
#include <fio/port/solaris/kfio.h>
#elif defined(__FreeBSD__)
#include <fio/port/freebsd/kfio.h>
#else
#error Unsupported OS - if you are porting, try starting with a copy of stubs
#endif

// Default maximum formatted volume capacity in bytes
#ifndef PORT_FORMATTED_CAPACITY_BYTES_MAX
#define PORT_FORMATTED_CAPACITY_BYTES_MAX KFIO_UINT64_MAX
#endif

#ifndef KFIO_DECLARE_SAVED_CALL_STACK

/// If OS doesn't implement call stack save interface, define as NOOPs.
# define KFIO_DECLARE_SAVED_CALL_STACK(name)
# define kfio_reset_saved_call_stack(s)
# define kfio_save_call_stack(s)
# define kfio_print_saved_call_stack(s)
#endif

/**
 * OS-independent declarations follow:
 */
/* Memory Functions */
extern int FIO_EFIAPI kfio_print(const char *format, ...)
#ifdef __GNUC__
__attribute__((format(printf,1,2)))
#endif
;
extern int FIO_EFIAPI
kfio_snprintf(char *buffer, fio_size_t n, const char *format, ...)
#ifdef __GNUC__
__attribute__((format(printf,3,4)))
#endif
;

/** @brief send a message to OS specific logging mechanism
 *  @param msg_lvl Message level
 *  @param fmt Complete message format string. It includes "fioerr" "dev context" "fmt"
 *  @param ap a variable argument list
 *  @return >=0 on success, <0 on failure
 */
extern int kfio_kvprint(msg_level_t msg_lvl, const char *fmt, va_list ap);

// USD has a separate definition
#ifndef USERSPACE_KERNEL
#define wrnprint(fmt, ...)       kmsg_filter(MSG_LEVEL_WARN, "", NO_MSG_ID, fmt, ##__VA_ARGS__)
#endif

/// @brief   Append a formatted string to another string.
/// @note    We might choose to use this when building up a string
///          as opposed to using the return value from kfio_snprintf
///          to increment the pointer to the next append spot because
///          the return from kfio_snprintf may be different for different
///          ports when truncation occurs.
/// @returns None.
#define kfio_snprintf_cat(buffer, n, format, ...) \
    do { \
        kassert(buffer != NULL); \
        kfio_snprintf(buffer + kfio_strlen(buffer), n - kfio_strlen(buffer), format, ## __VA_ARGS__); \
    } while(0)

#if !FUSION_INTERNAL
# define engprint(...)  ((void)0)
# define cengprint(...) ((void)0)
#endif

extern int kfio_vprint(const char *format, va_list args);
extern int kfio_vsnprintf(char *buffer, fio_size_t n, const char *format, va_list ap);
extern int kfio_strncmp(const char *s1, const char *s2, fio_size_t n);
extern int kfio_strcmp(const char *s1, const char *s2);
extern char *kfio_strncpy(char *dst, const char *src, fio_size_t n);
extern char *kfio_strcat(char *dst, const char *src);
extern char *kfio_strncat(char *dst, const char *src, int size);
extern fio_size_t kfio_strlen(const char *s);
extern void *kfio_memset(void *dst, int c, fio_size_t n);
extern int kfio_memcmp(const void *m1, const void *m2, fio_size_t n);
extern void *kfio_memcpy(void *dst, const void *src, fio_size_t n);
extern void *kfio_memmove(void *dst, const void *src, fio_size_t n);
extern int kfio_stoi(const char *p, int *nchar);
extern long int kfio_strtol(const char *nptr, char **endptr, int base);
extern unsigned long int kfio_strtoul(const char *nptr, char **endptr, int base);
extern unsigned long long int kfio_strtoull(const char *nptr, char **endptr, int base);
extern int kfio_strspn (const char *s, const char *accept);
extern char *kfio_strpbrk(char *cs, const char *ct);
extern char *kfio_strchr(const char *s, int c);
extern char *kfio_strtok_r(char *s, const char *del, char **sv_ptr);

extern void infprintbuf8(const uint8_t *buffer, unsigned buffer_size, unsigned num_entries_per_line,
                         const char *entry_format, const char *address_format, const char *leader_format, ...);
extern void infprintbuf32(const uint32_t *buffer, unsigned buffer_size, unsigned num_entries_per_line,
                          const char *entry_format, const char *address_format, const char *leader_format, ...);

extern int kfio_copy_from_user(void *to, const void *from, unsigned len);
extern int kfio_copy_to_user(void *to, const void *from, unsigned len);

#if !defined(WIN32) && !defined(WINNT)

extern void     kfio_dump_stack(void);
extern uint32_t kfio_random_seed(void);

#endif /* !defined(WIN32) */

#endif /* __FIO_PORT_KFIO_H__ */
