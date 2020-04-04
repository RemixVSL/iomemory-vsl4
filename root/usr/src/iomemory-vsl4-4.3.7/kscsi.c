// --------------------------------------------------------------------------
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
// --------------------------------------------------------------------------

// This file implements the functions which interact with the "regular" linux SCSI mid-layer.

#include <fio/port/port_config.h>

// Ignore this whole file if the SCSI device is not being used.
#if KFIO_SCSI_DEVICE

#include <scsi/scsi_host.h>
#include <scsi/scsi_cmnd.h>
#include <fio/port/kscsi.h>
#include <fio/port/kscatter.h>
#include <fio/port/kbio.h>
#include <fio/port/common-linux/kscsi_config.h>

/**
 * @ingroup PORT_LINUX
 * @{
 */

/*
 * Register the host with the operating system
 */
int fio_port_scsi_host_register_host(struct Scsi_Host *shost, struct pci_dev *pdev)
{
    return scsi_add_host(shost, NULL);
}

/*
 * Unregister the host with the operating system
 */
void fio_port_scsi_host_unregister_host(struct Scsi_Host *shost)
{
    scsi_remove_host(shost);
}

int kfio_port_scsi_cmd_copy_to_sg(void *port_cmd, void *src_buffer, uint32_t len)
{
    struct scsi_cmnd *scmd = (struct scsi_cmnd *)port_cmd;
    uint32_t n;

    n = scsi_sg_copy_from_buffer(scmd, src_buffer, len);

    dbgprint(DBGS_SCSI_IO2, "kfio_port_scsi_cmd_copy_to_sg copied %u bytes\n", n);

    return n == len ? 0 : -EIO;
}

int kfio_port_scsi_cmd_copy_from_sg(void *port_cmd, void *dst_buffer, uint32_t len)
{
    struct scsi_cmnd *scmd = (struct scsi_cmnd *)port_cmd;
    uint32_t n;

    n = scsi_sg_copy_to_buffer(scmd, dst_buffer, len);

    dbgprint(DBGS_SCSI_IO2, "kfio_port_scsi_cmd_copy_from_sg copied %u bytes\n", n);

    return n == len ? 0 : -EIO;
}

int kfio_port_scsi_cmd_map_kfio_bio(void *port_cmd, struct kfio_bio *fbio, uint32_t alignment)
{
    struct scsi_cmnd *scmd = (struct scsi_cmnd *)port_cmd;
    struct scatterlist *sg;
    unsigned i;

    for_each_sg(scsi_sglist(scmd), sg, scsi_sg_count(scmd), i)
    {
        int ret = kfio_sgl_map_page(fbio->fbio_sgl, (fusion_page_t)sg_page(sg), sg->offset, sg->length);
        if (ret != 0)
        {
            return ret;
        }
    }

    return 0;
}

/**
 * @}
 */

#endif  // KFIO_SCSI_DEVICE
