//-----------------------------------------------------------------------------
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

#ifndef __FIO_PORT_MESSAGE_IDS_H__
#define __FIO_PORT_MESSAGE_IDS_H__

#include <fio/port/message_id_ranges.h>
#include <fio/port/commontypes.h>

#if defined(UEFI)
#include <fio/port/uefi/message_ids.h>
#elif defined(__FreeBSD__)
#include <fio/port/freebsd/message_ids.h>
#elif defined(__OSX__)
#include <fio/port/osx/message_ids.h>
#elif defined(__SVR4) && defined(__sun)
#include <fio/port/solaris/message_ids.h>
#elif defined(__VMKAPI__)
#include <fio/port/esxi6/message_ids.h>
#elif defined(__linux__) || defined(__VMKLNX__)
#include <fio/port/common-linux/message_ids.h>
#elif defined(WINNT) || defined(WIN32)
#include <fio/port/windows/message_ids.h>
#elif !defined(USERSPACE_KERNEL) // Userspace does not have error IDs
#error "Unsupported OS"
#endif

/// @brief Enumerates the unique message IDs for the generic porting sub-layer ID range.
///
/// @note New message IDs should be appended to the end of the enum.
///
/// @warning DO NOT MODIFY ORDERING OR REMOVE ID ENTRIES
///          Removed values must be retained and retired to preserve ID mapping.
///          For retired IDs, append _DEPRECATED to the key
///          For IDs that are used in multiple places, append _DUP to the key
///
/// @ingroup FIO_MSGID_PORT
enum fio_port_generic_error_ids {
    ERRID_PORT_GENERIC_UNK_ERRNO = FIO_MSGID_PORT_GENERIC_MIN,
    FIO_MSGID_PORT_GENERIC_CURRENT_MAX ///< Current layer maximum. New entries above.
};
// Triggering this assert indicates this ID range is exhausted and a new range
// must be allocated.
C_ASSERT(FIO_MSGID_PORT_GENERIC_CURRENT_MAX <= FIO_MSGID_SUBLAYER_MAX(FIO_MSGID_PORT_GENERIC_MIN));

#endif /* __FIO_PORT_MESSAGE_IDS_H__ */
