//-----------------------------------------------------------------------------
// Copyright (c) 2013-2014, Fusion-io, Inc.(acquired by SanDisk Corp. 2014)
// Copyright (c) 2014-2015, SanDisk Corp. and/or all its affiliates. All rights reserved.
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
#error "This file supports Linux only"
#endif

#include <fio/port/kfio_config.h>

#if KFIO_BLOCK_DEVICE
#include <fio/port/common-linux/kblock.h>
#endif

// pretty sure we don't need a scsi interface for a PCIE card.
#if KFIO_SCSI_DEVICE
#include <fio/port/common-linux/kscsi_config.h>
#include <fio/port/kscsi.h>
#endif

int kfio_platform_init_storage_interface(void);
int kfio_platform_teardown_storage_interface(void);

/**
 * @ingroup PORT_COMMON_LINUX
 * @{
 */

int kfio_platform_init_storage_interface(void)
{
    int rc = 0;

#if KFIO_BLOCK_DEVICE
    rc = kfio_platform_init_block_interface();
    if (rc < 0)
    {
        return rc;
    }
#endif

#if KFIO_SCSI_DEVICE && KFIOC_PORT_SUPPORTS_SCSI
    rc = kfio_platform_init_scsi_interface();
    if (rc < 0)
    {
        return rc;
    }
#endif

    return rc;
}

int kfio_platform_teardown_storage_interface(void)
{
    int rc = 0;

#if KFIO_SCSI_DEVICE && KFIOC_PORT_SUPPORTS_SCSI
    rc = kfio_platform_teardown_scsi_interface();
    if (rc < 0)
    {
        return rc;
    }
#endif

#if KFIO_BLOCK_DEVICE
    rc = kfio_platform_teardown_block_interface();
    if (rc < 0)
    {
        return rc;
    }
#endif

    return rc;
}

/**
 * @}
 */
