// -----------------------------------------------------------------------------
// Copyright (c) 2014-2016 SanDisk Corp. and/or all its affiliates. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
// * Redistributions of source code must retain the above copyright notice,
//   this list of conditions and the following disclaimer.
// * Redistributions in binary form must reproduce the above copyright notice,
//   this list of conditions and the following disclaimer in the documentation
//   and/or other materials provided with the distribution.
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
//----------------------------------------------------------------------------

#ifndef __FIO_CMN_LINUX_MESSAGE_IDS__
#define __FIO_CMN_LINUX_MESSAGE_IDS__

#include <fio/port/message_id_ranges.h>

/// @brief Enumerates the unique message IDs for the fio-port/common-linux layer ID range.
///
/// @note New message IDs should be appended to the end of the enum.
/// @warning DO NOT MODIFY ORDERING OR REMOVE ID ENTRIES.
///          Removed values must be retained and retired to preserve ID mapping.
///          For retired IDs, append _DEPRECATED to the key
///          For IDs that are used in multiple places, append _DUP to the key
///
/// @ingroup FIO_MSGID_PORT
enum fio_port_common_linux_error_ids {
    ERRID_CMN_LINUX_CDEV_INVALID_INODE = FIO_MSGID_PORT_COMMON_LINUX_MIN,
    ERRID_CMN_LINUX_CDEV_INIT_FAIL,
    ERRID_CMN_LINUX_MAIN_LOAD_FAIL,
    ERRID_CMN_LINUX_PCI_MALFORMED_PARAM,
    ERRID_CMN_LINUX_PCI_MAP_BIO,
    ERRID_CMN_LINUX_PCI_MAP_BAR_DUP,
    ERRID_CMN_LINUX_PCI_ENABLE_DEV,
    ERRID_CMN_LINUX_PCI_MEM_REGION,
    ERRID_CMN_LINUX_PCI_ERR,
    ERRID_CMN_LINUX_PCI_LINK_RESET_DEPRECATED,
    ERRID_CMN_LINUX_PCI_SLOT_RESET,
    FIO_MSGID_PORT_CMN_LINUX_CURRENT_MAX ///< Current layer maximum. New entries above.
};
// Triggering this assert indicates this ID range is exhausted and a new range
// must be allocated.
C_ASSERT(FIO_MSGID_PORT_CMN_LINUX_CURRENT_MAX <= FIO_MSGID_SUBLAYER_MAX(FIO_MSGID_PORT_COMMON_LINUX_MIN));

// ====================================================

/// @brief Enumerates the unique message IDs for the fio-port/linux layer ID range.
///
/// @note New message IDs should be appended to the end of the enum.
/// @warning DO NOT MODIFY ORDERING OR REMOVE ID ENTRIES.
///          Removed values must be retained and retired to preserve ID mapping.
/// Removed values must be retained and retired to preserve ID mapping.
///
/// @ingroup FIO_MSGID_PORT
enum fio_port_linux_error_ids {
    ERRID_LINUX_KBLK_INIT_SCHED = FIO_MSGID_PORT_LINUX_MIN,
    ERRID_LINUX_KBLK_REQ_MISMATCH,
    ERRID_LINUX_KBLK_REQ_REJ,
    ERRID_LINUX_KMEM_CREATE_POOL,
    ERRID_LINUX_KMEM_POOL_SIZE,
    ERRID_LINUX_KMEM_EXPAND_POOL,
    ERRID_LINUX_KMEM_ALLOC_PAGE,
    ERRID_LINUX_KMEM_MAP_VA,
    ERRID_LINUX_KMEM_CREATE_HEAP,
    ERRID_LINUX_KMEM_TRIM,
    ERRID_LINUX_KMEM_INIT_MALLOC,
    ERRID_LINUX_KMEM_INCREASE_POOL,
    ERRID_LINUX_KMEM_OUT_OF_PAGE,
    ERRID_LINUX_KMEM_MEM_ALLOC,
    ERRID_LINUX_KSCATTER_UNSUPPORTED_USER_MEM,
    ERRID_LINUX_KSCATTER_TOO_GREAT_SPAN,
    ERRID_LINUX_KSCATTER_DMA_DETAILS,
    ERRID_LINUX_KSCATTER_UNALIGNED_DMA,
    ERRID_LINUX_KSCATTER_TOO_FEW_ENTRIES,
    FIO_MSGID_PORT_LINUX_CURRENT_MAX ///< Current layer maximum. New entries above.
};
// Triggering this assert indicates this ID range is exhausted and a new range
// must be allocated.
C_ASSERT(FIO_MSGID_PORT_LINUX_CURRENT_MAX <= FIO_MSGID_SUBLAYER_MAX(FIO_MSGID_PORT_LINUX_MIN));

// ====================================================

/// @brief Enumerates the unique message IDs for the fio-port/esx layer ID range.
///
/// @note New message IDs should be appended to the end of the enum.
/// @warning DO NOT MODIFY ORDERING OR REMOVE ID ENTRIES.
///          Removed values must be retained and retired to preserve ID mapping.
/// Removed values must be retained and retired to preserve ID mapping.
///
/// @ingroup FIO_MSGID_PORT
enum fio_port_esx_error_ids {
    ERRID_ESX_KCACHE_NULL_HEADER_EXPANSION_DEPRECATED = FIO_MSGID_PORT_ESX_MIN,
    ERRID_ESX_KCACHE_NULL_HEADER_AVAIL_DEPRECATED,
    ERRID_ESX_KCACHE_POP_FREE_LIST_DEPRECATED,
    ERRID_ESX_KCACHE_NULL_HEADER_FREE_DEPRECATED,
    ERRID_ESX_KCACHE_CACHE_ALLOC_DEPRECATED,
    ERRID_ESX_KCACHE_CREATE_CACHE_DEPRECATED,
    FIO_MSGID_PORT_ESX_CURRENT_MAX ///< Current layer maximum. New entries above.
};
// Triggering this assert indicates this ID range is exhausted and a new range
// must be allocated.
C_ASSERT(FIO_MSGID_PORT_ESX_CURRENT_MAX <= FIO_MSGID_SUBLAYER_MAX(FIO_MSGID_PORT_ESX_MIN));

// ====================================================

/// @brief Enumerates the unique message IDs for the fio-port/esxi5 layer ID range.
///
/// @note New message IDs should be appended to the end of the enum.
/// @warning DO NOT MODIFY ORDERING OR REMOVE ID ENTRIES.
///          Removed values must be retained and retired to preserve ID mapping.
/// Removed values must be retained and retired to preserve ID mapping.
///
/// @ingroup FIO_MSGID_PORT
enum fio_port_esxi5_error_ids {
    ERRID_ESXI5_KBLK_INIT_SCHED_DEPRECATED = FIO_MSGID_PORT_ESXI5_MIN,
    ERRID_ESXI5_KCACHE_NULL_HEADER_EXPANSION,
    ERRID_ESXI5_KCACHE_NULL_HEADER_AVAIL,
    ERRID_ESXI5_KCACHE_FREE_LIST,
    ERRID_ESXI5_KCACHE_NULL_HEADER_FREE,
    ERRID_ESXI5_KCACHE_CACHE_ALLOC,
    ERRID_ESXI5_KCACHE_CREATE_CACHE,
    ERRID_ESXI5_KMEM_CREATE_POOL_DUP,
    ERRID_ESXI5_KMEM_POOL_SIZE_DUP,
    ERRID_ESXI5_KMEM_EXPAND_POOL_DUP,
    ERRID_ESXI5_KMEM_ALLOC_PAGE_DUP,
    ERRID_ESXI5_KMEM_VMK_MAP_DUP,
    ERRID_ESXI5_KMEM_CREATE_HEAP_DUP,
    ERRID_ESXI5_KMEM_TRIM_DUP,
    ERRID_ESXI5_KMEM_INIT_MALLOC_DUP,
    ERRID_ESXI5_KMEM_INCREASE_POOL,
    ERRID_ESXI5_KMEM_OUT_OF_PAGE_DUP,
    ERRID_ESXI5_KSCATTER_UNSUPPORTED_USER_MEM,
    ERRID_ESXI5_KSCATTER_UNALIGNED_DMA,
    ERRID_ESXI5_KSCATTER_FIXED_UNALIGNED,
    ERRID_ESXI5_KSCSI_REGISTER_HOST,
    ERRID_ESXI5_KFIO_ASSERT_STATS,
    ERRID_ESXI5_KMEM_INIT_MALLOC_HEAP,
    ERRID_ESXI5_KMEM_INVAL_PREALLOC,
    FIO_MSGID_PORT_ESXI5_CURRENT_MAX ///< Current layer maximum. New entries above.
};
// Triggering this assert indicates this ID range is exhausted and a new range
// must be allocated.
C_ASSERT(FIO_MSGID_PORT_ESXI5_CURRENT_MAX <= FIO_MSGID_SUBLAYER_MAX(FIO_MSGID_PORT_ESXI5_MIN));


#endif // __FIO_CMN_LINUX_MESSAGE_IDS__
