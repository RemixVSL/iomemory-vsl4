//-----------------------------------------------------------------------------
// Copyright (c) 2014 Fusion-io, Inc. (acquired by SanDisk Corp. 2014)
// Copyright (c) 2014-2016 SanDisk Corp. and/or all its affiliates. (acquired by Western Digital Corp. 2016)
// Copyright (c) 2016-2017 Western Digital Technologies, Inc. All rights reserved.
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

#if !defined (__linux__)
#error This file supports linux only
#endif

#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/uaccess.h>
#include <fio/port/dbgset.h>

/**
 * @ingroup PORT_LINUX
 * @{
 */

void
__kassert_fail (const char *expr, const char *file, int line,
                const char *func, int good, int bad)
{
    if (make_assert_nonfatal)
    {

#if FUSION_DEBUG || FUSION_INTERNAL
        sqaprint("iodrive: assertion failed %s:%d:%s: %s",
                file, line, func, expr);
        sqaprint("ASSERT STATISTICS: %d bad, %d good\n",
                 bad, good);
        dump_stack ();
#endif
    }
    else
    {
#if FUSION_DEBUG
        panic("iodrive: assertion failed %s:%d:%s: %s",
                             file, line, func, expr);
#else
        BUG();
#endif
    }
}

const char *kfio_print_prefix[] = {
    [KERN_PRINT_ERR]   = KERN_ERR,
    [KERN_PRINT_INFO]  = KERN_INFO,
    [KERN_PRINT_DEBUG] = KERN_DEBUG,
    [KERN_PRINT_CONT]  = ""
};

// linux/esx (when not FIO_UNIT_TEST) needs prepending a header (KERN_ERR, etc)
#if KFIO_PORT_MSG_LEVEL_DEFINED
const char *MSG_LEVEL_STR[] = {
    KERN_ERR "fioerr",
    KERN_WARNING "fiowrn",
    KERN_INFO "fioinf",
    KERN_INFO "fioeng",
    KERN_DEBUG "fiodbg"};
#endif

int kfio_print(const char *format, ...)
{
    va_list ap;
    int rc;

    va_start(ap, format);
    rc = kfio_vprint(format, ap);
    va_end(ap);

    return rc;
}

int kfio_vprint(const char *format, va_list ap)
{
#if !defined(__VMKLNX__)
    return vprintk(format, ap);
#else
    va_list argsCopy;
    int printedLen;

    va_copy(argsCopy, ap);

    vmk_vLogNoLevel(VMK_LOG_URGENCY_NORMAL, format, ap);
    printedLen = vmk_Vsnprintf(NULL, 0, format, argsCopy);

    va_end(argsCopy);

    return printedLen;
#endif
}

int kfio_snprintf(char *buffer, fio_size_t n, const char *format, ...)
{
    va_list ap;
    int rc;

    va_start(ap, format);
    rc = kfio_vsnprintf(buffer, n, format, ap);
    va_end(ap);

    return rc;
}

int kfio_vsnprintf(char *buffer, fio_size_t n, const char *format, va_list ap)
{
    return vsnprintf(buffer, n, format, ap);
}

int kfio_strcmp(const char *s1, const char *s2)
{
    return strcmp(s1, s2);
}

fio_size_t kfio_strlen(const char *s1)
{
    return strlen(s1);
}

int kfio_strncmp(const char *s1, const char *s2, fio_size_t n)
{
    return strncmp(s1, s2, n);
}

char *kfio_strncpy(char *dst, const char *src, fio_size_t n)
{
    return strncpy(dst, src, n);
}

char *kfio_strcat(char *dst, const char *src)
{
    return strcat(dst, src);
}

char *kfio_strncat(char *dst, const char *src, int size)
{
    return strncat(dst, src, size);
}

void *kfio_memset(void *dst, int c, fio_size_t n)
{
    return memset(dst, c, n);
}

int kfio_memcmp(const void *m1, const void *m2, fio_size_t n)
{
    return memcmp(m1, m2, n);
}

void *kfio_memcpy(void *dst, const void *src, fio_size_t n)
{
    return memcpy(dst, src, n);
}

void *kfio_memmove(void *dst, const void *src, fio_size_t n)
{
    return memmove(dst, src, n);
}

unsigned long long kfio_strtoull(const char *nptr, char **endptr, int base)
{
    return simple_strtoull(nptr, endptr, base);
}

unsigned long int kfio_strtoul(const char *nptr, char **endptr, int base)
{
    return simple_strtoul(nptr, endptr, base);
}

long int kfio_strtol(const char *nptr, char **endptr, int base)
{
    return simple_strtol(nptr, endptr, base);
}

/*
 * RW spinlock wrappers
 */

void fusion_rwspin_init(fusion_rwspin_t *s, const char *name)
{
    rwlock_init((rwlock_t *)s);
}

void fusion_rwspin_destroy(fusion_rwspin_t *s)
{
    (void)s;
}

void fusion_rwspin_read_lock(fusion_rwspin_t *s)
{
#if FUSION_DEBUG && !defined(CONFIG_PREEMPT_RT) && !defined(__VMKLNX__)
    kassert(!irqs_disabled());
#endif
    read_lock((rwlock_t *)s);
}
void fusion_rwspin_write_lock(fusion_rwspin_t *s)
{
#if FUSION_DEBUG && !defined(CONFIG_PREEMPT_RT) && !defined(__VMKLNX__)
    kassert(!irqs_disabled());
#endif
    write_lock((rwlock_t *)s);
}

void fusion_rwspin_read_unlock(fusion_rwspin_t *s)
{
    read_unlock((rwlock_t *)s);
}
void fusion_rwspin_write_unlock(fusion_rwspin_t *s)
{
    write_unlock((rwlock_t *)s);
}

int fusion_create_kthread(fusion_kthread_func_t func, void *data, void *fusion_nand_device,
                           const char *fmt, ...)
{
    va_list ap;
    char buffer[MAX_KTHREAD_NAME_LENGTH];
    struct task_struct *t;

    va_start(ap, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, ap);
    va_end(ap);

    t = kthread_run(func, data, "%s", buffer);

    return IS_ERR(t) ? PTR_ERR(t) : 0;
}

/*
 * Userspace <-> Kernelspace routines
 * XXX: all the kfio_[put\get]_user_*() routines are superfluous
 * and ought be replaced with kfio_copy_[to|from]_user()
 */
int kfio_copy_from_user(void *to, const void *from, unsigned len)
{
    return copy_from_user(to, from, len);
}

int kfio_copy_to_user(void *to, const void *from, unsigned len)
{
    return copy_to_user(to, from, len);
}

void kfio_dump_stack()
{
    dump_stack();
}


// TODO: don't think this is needed.
#if FIO_BITS_PER_LONG == 32
/* 64bit divisor, dividend and result. dynamic precision */
uint64_t kfio_div64_64(uint64_t dividend, uint64_t divisor)
{
    uint32_t tmp_divisor = divisor;
    uint32_t upper = divisor >> 32;
    uint32_t shift;

    if(upper)
    {
        shift = fls(upper);
        dividend = dividend >> shift;
        tmp_divisor = divisor >> shift;
    }

    do_div(dividend, tmp_divisor);

    return dividend;
}

uint64_t kfio_mod64_64(uint64_t dividend, uint64_t divisor)
{
    return do_div(dividend, divisor);
}
#endif

int kfio_kvprint(msg_level_t msg_lvl, const char *fmt, va_list ap)
{
    int rc;
    va_list ap_local;
    va_copy(ap_local, ap);
    rc = kfio_vprint(fmt, ap_local);
    va_end(ap_local);
    return rc;
}

/**
 * @}
 */
