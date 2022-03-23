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
// -----------------------------------------------------------------------------
#ifndef __FIO_PORT_TYPES_H__
#define __FIO_PORT_TYPES_H__

#ifndef ATTRIBUTE_PACKED_ALIGNED
#define ATTRIBUTE_PACKED_ALIGNED(x) ERROR You_must_include_fio_pshpack1_h_before_and_fio_poppack_h_after_any_use_of_ATTRIBUTE_PACKED_ALIGNED_or_UNALIGNED
#endif

#ifndef ATTRIBUTE_PACKED_UNALIGNED
#define ATTRIBUTE_PACKED_UNALIGNED  ATTRIBUTE_PACKED_ALIGNED(1)
#endif

// Preprocessor scripting madness
#if defined(__KERNEL__)
# include <fio/port/kfio.h>
# include <fio/port/ktime.h>
#else // We're in user-space now
# include <fio/port/ufio.h>
#endif // __KERNEL__

#include <fio/port/fio-port.h>

///
/// Macro to provide a count of the number of elements in a statically defined array x.
///
#define FIO_STATIC_ARRAY_COUNT(x)    ((uint32_t)(sizeof(x)/sizeof(x[0])))

#ifndef MIN
#define MIN(a, b)   (((a) < (b)) ? (a) : (b))
#endif

#ifndef MAX
#define MAX(a, b)   (((a) > (b)) ? (a) : (b))
#endif

#endif // __FIO_PORT_TYPES_H__
