//-----------------------------------------------------------------------------
// Copyright (c) 2014 Fusion-io, Inc. (acquired by SanDisk Corp. 2014)
// Copyright (c) 2014-2015, SanDisk Corp. and/or all its affiliates.
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
// -----------------------------------------------------------------

#include <fio/port/kfio.h>

#if !defined(KFIO_PORT_MSG_LEVEL_DEFINED)
const char *MSG_LEVEL_STR[] = { "fioerr", "fiowrn", "fioinf", "fioeng", "fiodbg" };
#endif

/**
 * @brief filtering messages based on configuration
 * @param msg_lvl Message level, MSG_LEVEL_ERR - MSG_LEVEL_INFO
 * @param msg_ctx The device context that this message is originated from
 * @param id integer ID of associated message - use NO_MSG_ID for lack thereof.
 * @param fmt Message format string
 * @return 0 on success, -1 on failure
 */
int FIO_EFIAPI
kmsg_filter(msg_level_t msg_lvl, const char *msg_ctx, int32_t id,
#if defined(WINNT) || defined(WIN32)
                _In_z_ _Printf_format_string_
#endif
                const char *fmt, ...)
{
    va_list ap;
    int rc;
    char complete_fmt[MSG_FMT_LEN_MAX];

    char pad_id[MSG_ID_LEN_MAX];
    //TODO: some filtering here ...

    if (id != NO_MSG_ID)
    {
        // zero pad if the id is short
        kfio_snprintf(pad_id, MSG_ID_LEN_MAX, "%05d ", id);
    }
    else
    {
        pad_id[0] = '\0'; // don't print anything for the ID
    }

    // assemble a complete format string as "fioerr <dev name> <original fmt>"
    if (msg_ctx && kfio_strlen(msg_ctx) > 0)
    {
        rc = kfio_snprintf(complete_fmt, MSG_FMT_LEN_MAX, "%s %s: %s%s",
                           MSG_LEVEL_STR[msg_lvl], msg_ctx, pad_id, fmt);
    }
    else
    {
        // coming from the original errprint, context is null or ""
        rc = kfio_snprintf(complete_fmt, MSG_FMT_LEN_MAX, "%s %s%s",
                           MSG_LEVEL_STR[msg_lvl], pad_id, fmt);
    }

    va_start(ap, fmt);
    if (rc < 0)
    {
        // fail to generate the complete format, use the passed-in
        rc = kmsg_subscription(msg_lvl, fmt, ap);
    }
    else
    {
        rc = kmsg_subscription(msg_lvl, complete_fmt, ap);
    }
    va_end(ap);

    return rc;
}

/**
 * @brief forwarding message to one or more logging mechanisms
 * @param msg_lvl Message level, MSG_LEVEL_ERR - MSG_LEVEL_INFO
 * @param fmt Complete message format string. It includes "fioerr" "dev context" "fmt"
 * @param ap a variable argument list
 * @return >=0 on success, <0 on failure
 */
int kmsg_subscription(msg_level_t msg_lvl,
#if defined(WINNT) || defined(WIN32)
                      _In_z_ _Printf_format_string_
#endif
                      const char *fmt, va_list ap)
{
    va_list ap_local;
    int rc;

    va_copy(ap_local, ap);
    rc = kfio_kvprint(msg_lvl, fmt, ap_local);
    va_end(ap_local);

    // forwarding to other logging mechanisms ...

    return rc;
}

