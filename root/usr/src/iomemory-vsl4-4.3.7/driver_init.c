//-----------------------------------------------------------------------------
// Copyright (c) 2006-2014, Fusion-io, Inc.(acquired by SanDisk Corp. 2014)
// Copyright (c) 2014-2016 SanDisk Corp. and/or all its affiliates. All rights reserved.
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
#include <linux/module.h>

#include <fio/port/kfio_config.h>

#include "fio/port/kfio.h"

/**
 * @ingroup PORT_COMMON_LINUX
 * @{
 */

// Global driver options
char       *numa_node_override[MAX_PCI_DEVICES];
int         num_numa_node_override;


MODULE_PARM_DESC(numa_node_override, "Override device to NUMA node binding");
module_param_array (numa_node_override, charp, &num_numa_node_override, S_IRUGO | S_IWUSR);


int use_workqueue = USE_QUEUE_NONE;
module_param (use_workqueue, int, S_IRUGO | S_IWUSR);

// TODO: do we need any of this?
#if FUSION_MEDIA_TEST_TOOL
#if FUSION_INTERNAL || FUSION_DEBUG
#error FUSION_MEDIA_TEST_TOOL must only be used only with a release build.
#endif
// Evil Hackery to enable module parameters required by the media test tool
// without using an internal build

int bypass_whitening = 0; // BYPASS_WHITENING;
module_param(bypass_whitening, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(bypass_whitening, "N/A");

int bypass_ecc = 0; // BYPASS_ECC;
module_param(bypass_ecc, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(bypass_ecc, "N/A");

int iodrive_scan_nv_data = 1; // IODRIVE_SCAN_NV_DATA;
module_param(iodrive_scan_nv_data, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(iodrive_scan_nv_data, "N/A");

int disable_rle = 0; // DISABLE_RLE;
module_param(disable_rle, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(disable_rle, "N/A");

int use_parity_if_optional = 1; // USE_PARITY_IF_OPTIONAL
module_param(use_parity_if_optional, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(use_parity_if_optional, "N/A");

#endif

/**
 * @}
 */
