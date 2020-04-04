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

#ifndef __FIO_PORT_MESSAGE_ID_RANGES__
#define __FIO_PORT_MESSAGE_ID_RANGES__

/// @defgroup FIO_MSGID_PORT Porting Layer Message Enumerations
/// @ingroup FIO_MSGID
///
/// Porting layer ID ranges span 0x0000-0x2FFF, inclusive.
/// Generic porting messages may be placed in the 'GENERIC' category.
/// New ports should be assigned the next available sub-layer.
/// The maximum assignable porting layer range is FIO_MSGID_MIN(5,3)


/// @defgroup FIO_MSGID_LIMITS Message Enumeration Limits
/// @ingroup FIO_MSGID
/// @{

/// Each message ID layer reserves bits [10:0] for sub-layers and unique IDs
#define FIO_MSGID_LAYER_BIT          11

/// Sub-layers are identified by the two most significant bits [10:9] of the ID layer
#define FIO_MSGID_SUBLAYER_BIT       (FIO_MSGID_LAYER_BIT - 2)

/// Calculates minimum message ID value provided layer and sub-layer
#define FIO_MSGID_MIN(layer, sublayer) \
    (((layer) << FIO_MSGID_LAYER_BIT) | ((sublayer) << FIO_MSGID_SUBLAYER_BIT))

/// Number of layer enumerations reserved for the porting layer
#define FIO_MSGID_PORT_LAYER_MAX     6

/// Get the maximum (exclusive) ID in a layer given the minimum ID in that layer
#define FIO_MSGID_LAYER_MAX(x)       ((x) + (1 << FIO_MSGID_LAYER_BIT))

/// Get the maximum (exclusive) ID in a sub-layer given the minimum ID in that layer
#define FIO_MSGID_SUBLAYER_MAX(x)    ((x) + (1 << FIO_MSGID_SUBLAYER_BIT))

/// @}


/// @defgroup FIO_MSGID_PORT_LIMITS Porting Layer Enumeration Limits
/// @ingroup FIO_MSGID_PORT
/// @{
#define FIO_MSGID_PORT_GENERIC_MIN          FIO_MSGID_MIN(0, 0)
#define FIO_MSGID_PORT_COMMON_LINUX_MIN     FIO_MSGID_MIN(0, 1)
#define FIO_MSGID_PORT_COMMON_VMWARE_MIN    FIO_MSGID_MIN(0, 2)
#define FIO_MSGID_PORT_ESX_MIN              FIO_MSGID_MIN(0, 3)
#define FIO_MSGID_PORT_ESXI5_MIN            FIO_MSGID_MIN(1, 0)
#define FIO_MSGID_PORT_FREEBSD_MIN          FIO_MSGID_MIN(1, 1)
#define FIO_MSGID_PORT_LINUX_MIN            FIO_MSGID_MIN(1, 2)
#define FIO_MSGID_PORT_OSX_MIN              FIO_MSGID_MIN(1, 3)
#define FIO_MSGID_PORT_SOLARIS_MIN          FIO_MSGID_MIN(2, 0)
#define FIO_MSGID_PORT_TEST_MIN             FIO_MSGID_MIN(2, 1)
#define FIO_MSGID_PORT_UEFI_MIN             FIO_MSGID_MIN(2, 2)
#define FIO_MSGID_PORT_USD_MIN              FIO_MSGID_MIN(2, 3)
#define FIO_MSGID_PORT_WINDOWS_MIN          FIO_MSGID_MIN(3, 0)
#define FIO_MSGID_PORT_VMKAPI_MIN           FIO_MSGID_MIN(3, 1)

/// @}

#endif // __FIO_PORT_MESSAGE_ID_RANGES___
