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
// -----------------------------------------------------------------------------

#ifndef _FIO_PORT_UUID_H_
#define _FIO_PORT_UUID_H_

#include <fio/public/fioapi.h>
#include <fio/port/kglobal.h>

#ifdef __cplusplus
extern "C" {
#endif

// Used when we really know that casting off a const is ok.
#define FIO_DECONST(x) ((void *)(fio_intptr_t)(x))

#define FIO_UUID_STRING_LEN 37
typedef char fio_uuid_str_t[FIO_UUID_STRING_LEN];

#define FIO_UUID_LEN 16
typedef unsigned char fio_uuid_t[FIO_UUID_LEN];

DllExport
void fio_uuid_copy(fio_uuid_t dest, const fio_uuid_t src);

void fio_uuid_clear(fio_uuid_t uu);

int fio_uuid_is_null(const fio_uuid_t uu);

DllExport
void fio_uuid_generate(fio_uuid_t uu);

DllExport
void fio_uuid_unparse(const fio_uuid_t uu, char *);

DllExport
int fio_uuid_parse(const char *in, fio_uuid_t uu);

int fio_uuid_compare(const fio_uuid_t uu1, const fio_uuid_t uu2);

// Homegrown functions that deal with the string version of the uuid.
DllExport
void fio_uuid_str_copy(fio_uuid_str_t dest, const fio_uuid_str_t src);

DllExport
int fio_uuid_str_compare(const fio_uuid_str_t u1, const fio_uuid_str_t u2);

#ifdef __cplusplus
}
#endif

#endif /* _FIO_PORT_UUID_H_ */
