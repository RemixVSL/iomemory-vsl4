
//-----------------------------------------------------------------------------
// Copyright (c) 2006-2014 Fusion-io, Inc. (acquired by SanDisk Corp. 2014)
// Copyright (c) 2014 SanDisk Corp. and/or all its affiliates. All rights reserved.
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
//-----------------------------------------------------------------------------
#include <linux/module.h>

#include <fio/port/kfio_config.h>

#include "fio/port/kfio.h"

module_param(auto_attach, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(auto_attach, "Automatically attach drive during driver initialization: 0 = disable attach, 1 = enable attach (default). Note for Windows only: The driver will only attach if there was a clean shutdown, otherwise the fiochkdrv utility will perform the full scan attach except when 2 or 3. 2 forces the driver to do a full rescan before the OS GUI boot. 3 forces the driver to rescan in a thread and allows the OS to continue to boot.");
module_param(compaction_timeout_ms, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(compaction_timeout_ms, "Timeout in ms for data log compaction while shutting down.");
module_param(completion_poll_ms, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(completion_poll_ms, "For use only under the direction of Customer Support.");
module_param(coreclk_max_derating, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(coreclk_max_derating, "Maximum derating of the core clock.");
module_param(disable_msi, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(disable_msi, "N/A");
module_param(disable_msix, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(disable_msix, "N/A");
module_param(disable_scanner, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(disable_scanner, "For use only under the direction of Customer Support.");
module_param(enable_discard, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(enable_discard, "For use only under the direction of Customer Support.");
module_param(enable_multq_at_load, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(enable_multq_at_load, "For use only under the direction of Customer Support.");
module_param_string(exclude_devices, exclude_devices, sizeof(exclude_devices), S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(exclude_devices, "List of cards to exclude from driver initialization (comma separated list of <domain>:<bus>:<slot>.<func>)");
module_param(expected_io_size, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(expected_io_size, "Timeout for data log compaction while shutting down.");
module_param(fio_dev_optimal_blk_size, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(fio_dev_optimal_blk_size, "Optimal block size hint for the linux block layer.");
module_param(fio_dev_wait_timeout_secs, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(fio_dev_wait_timeout_secs, "Number of seconds to wait for device file creation before continuing.");
module_param_string(external_power_override, external_power_override, sizeof(external_power_override), S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(external_power_override, "Override external power requirement on boards that normally require it. (comma-separated list of adapter serial numbers)");
module_param(groom_harder_threshold, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(groom_harder_threshold, "For use only under the direction of Customer Support.");
module_param(fio_max_num_bios, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(fio_max_num_bios, "Maximum number of bio structures allocated per device");
module_param(fio_min_num_bios, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(fio_min_num_bios, "Number of bio structures pre-allocated per device");
module_param_string(preallocate_memory, preallocate_memory, sizeof(preallocate_memory), S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(preallocate_memory, "Causes the driver to pre-allocate the RAM it needs");
module_param(enable_unmap, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(enable_unmap, "Enable UNMAP support.");
module_param(force_minimal_mode, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(force_minimal_mode, "N/A");
module_param(groomer_buffer_size, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(groomer_buffer_size, "For use only under the direction of Customer Support.");
module_param(disable_groomer, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(disable_groomer, "For use only under the direction of Customer Support.");
module_param(groomer_high_water_delta_hpcnt, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(groomer_high_water_delta_hpcnt, "The proportion of logical space over the low watermark where grooming starts (in ten-thousandths)");
module_param(groomer_low_water_delta_hpcnt, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(groomer_low_water_delta_hpcnt, "The proportion of logical space free over the runway that represents 'the wall'");
module_param_string(include_devices, include_devices, sizeof(include_devices), S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(include_devices, "Whitelist of cards to include in driver initialization (comma separated list of <domain>:<bus>:<slot>.<func>)");
module_param(iodrive_load_eb_map, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(iodrive_load_eb_map, "For use only under the direction of Customer Support.");
module_param(make_assert_nonfatal, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(make_assert_nonfatal, "For use only under the direction of Customer Support.");
module_param(max_requests, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(max_requests, "How many requests pending in iodrive");
module_param(min_requests, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(min_requests, "How many requests pre-allocated in iodrive");
module_param(capacity_warning_threshold, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(capacity_warning_threshold, "If the reserve space is below this threshold (in hundredths of percent), warnings will be issued.");
module_param(use_command_timeouts, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(use_command_timeouts, "Use the command timeout registers");
module_param(scsi_queue_depth, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(scsi_queue_depth, "The queue depth that is advertised to the OS SCSI interface.");
module_param(max_md_blocks_per_device, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(max_md_blocks_per_device, "For use only under the direction of Customer Support.");
module_param(parallel_attach, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(parallel_attach, "For use only under the direction of Customer Support.");
module_param(global_slot_power_limit_mw, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(global_slot_power_limit_mw, "Global PCIe slot power limit in milliwatts. Performance will be throttled to not exceed this limit in any PCIe slot.");
module_param(preallocate_mb, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(preallocate_mb, "The megabyte limit for FIO_PREALLOCATE_MEMORY. This will prevent the driver from potentially using all of the system's non-paged memory.");
module_param(preferred_ppad, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(preferred_ppad, "pad to use for parity (if possible, i.e. if not mapped out)");
module_param(read_retry_logging, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(read_retry_logging, "For use only under the direction of Customer Support.");
module_param(rmap_memory_limit_MiB, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(rmap_memory_limit_MiB, "Memory limit in MiBytes for rmap rescan.");
module_param(rsort_memory_limit_MiB, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(rsort_memory_limit_MiB, "Memory limit in MiBytes for rsort rescan.");
module_param(use_large_pcie_rx_buffer, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(use_large_pcie_rx_buffer, "If true, use 1024 byte PCIe rx buffer. This improves performance but causes NMIs on some specific hardware.");
