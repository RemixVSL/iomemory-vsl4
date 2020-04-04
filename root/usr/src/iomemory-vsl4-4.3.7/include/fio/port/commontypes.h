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

#ifndef _FIO_PORT_COMMONTYPES_H_
#define _FIO_PORT_COMMONTYPES_H_

#include <fio/port/port_config.h>

#if defined(USERSPACE_KERNEL)
#include <fio/port/userspace/commontypes.h>
#elif defined(UEFI)
#include <fio/port/uefi/commontypes.h>
#elif defined(__VMKAPI__)
#include <fio/port/esxi6/commontypes.h>
#elif defined(__linux__) || defined(__VMKLNX__)
#include <fio/port/common-linux/commontypes.h>
#elif defined(__SVR4) && defined(__sun)
#include <fio/port/solaris/commontypes.h>
#elif defined(__FreeBSD__)
#include <fio/port/freebsd/commontypes.h>
#elif defined(__OSX__)
#include <fio/port/osx/commontypes.h>
#elif defined(WINNT) || defined(WIN32)
#include <fio/port/windows/commontypes.h>
#else
#error Unsupported OS - if you are porting, try starting with a copy of stubs
#endif

//============================================================================
// Useful pre-processor string manipulation macros
//============================================================================

#define FIO_UNICODE_HELPER(str) L##str
#define FIO_UNICODE(str)  FIO_UNICODE_HELPER(str)

#define FIO_STRINGIZE_HELPER(str) #str
#define FIO_STRINGIZE(str) FIO_STRINGIZE_HELPER(str)

#if !defined(C_ASSERT)
// Assert a compile time condition in a global context
//
// Unfortunately this definition of C_ASSERT is mutually exclusive with
// using -Wredundant-decls. That's the lessor of two evils.
//
#ifdef __cplusplus
// c/c++ linkage errors eliminated
#define C_ASSERT(x) extern "C" int __CPP_ASSERT__ [(x)?1:-1]
#else
#define C_ASSERT(x) extern int __C_ASSERT__ [(x)?1:-1]
#endif
#endif

#if !defined(STATIC_ASSERT)
// Assert a compile time condition in a function context
#if defined(__cplusplus) && __cplusplus >= 201103l
#define STATIC_ASSERT(x) static_assert(x, "Assertion Failed: " #x)
#else
#define STATIC_ASSERT(x)                            \
    do                                              \
    {                                               \
        enum { static_assert = 1/(x) };             \
    } while (0)
#endif
#endif /* !STATIC_ASSERT */

#endif /* _FIO_PORT_COMMONTYPES_H_ */
