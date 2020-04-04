//-----------------------------------------------------------------------------
// Copyright (c) 2014 Fusion-io, Inc. (acquired by SanDisk Corp. 2014)
// Copyright (c) 2014-2015 SanDisk Corp. and/or all its affiliates.
// All rights reserved.
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

#ifndef __FIO_PORT_KMSG_H__
#define __FIO_PORT_KMSG_H__

#ifdef __KERNEL__

#include <fio/port/ktypes.h>
#include "fio/port/vararg.h"

typedef enum
{
    MSG_LEVEL_ERR = 0,
    MSG_LEVEL_WARN,
    MSG_LEVEL_INFO,
    MSG_LEVEL_ENG,
    MSG_LEVEL_DBG
} msg_level_t;

extern const char *MSG_LEVEL_STR[];

// No message ID was provided, do not print one
#define NO_MSG_ID   -1

// max length of the complete format string "fioerr <dev context> fmt"
#define MSG_FMT_LEN_MAX  512

// max characters in message id string (including terminating space and null)
#define MSG_ID_LEN_MAX     7

#if defined(WINNT) || defined(WIN32)

extern int kmsg_filter(msg_level_t msg_lvl, const char *msg_ctx, int32_t id,
                       _In_z_ _Printf_format_string_ const char *fmt, ...);
extern int kmsg_subscription(msg_level_t msg_lvl,
                             _In_z_ _Printf_format_string_ const char *fmt,
                             va_list ap);
#else
extern int FIO_EFIAPI
kmsg_filter(msg_level_t msg_lvl, const char *msg_ctx, int32_t id, const char *fmt, ...)
#ifdef __GNUC__
__attribute__((format(printf,4,5)))
#endif
;

extern int kmsg_subscription(msg_level_t msg_lvl, const char *fmt, va_list ap);
#endif /* defined WINNT/WIN32 */

#endif /* __KERNEL__ */

#endif /* __FIO_PORT_KMSG_H__ */
