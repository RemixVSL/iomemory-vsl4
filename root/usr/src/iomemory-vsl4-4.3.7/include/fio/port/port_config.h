/* -----------------------------------------------------------------------------
 * Copyright (c) 2019, Fusion-io, Inc. (acquired by SanDisk Corp. 2014)
 * Copyright (c) 2014-2015 SanDisk Corp. and/or all its affiliates.
 * All rights reserved.
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * * Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 * * Neither the name of the SanDisk Corp. nor the names of its contributors
 *   may be used to endorse or promote products derived from this software
 *   without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * ---------------------------------------------------------------------------*/

#ifndef __GENERATED_PORT_CONFIG_H__
#define __GENERATED_PORT_CONFIG_H__
#include <fio/port/port_config_vars_externs.h>


/* Automatically attach drive during driver initialization: 0 = disable attach, 1 = enable attach (default). Note for Windows only: The driver will only attach if there was a clean shutdown, otherwise the fiochkdrv utility will perform the full scan attach except when 2 or 3. 2 forces the driver to do a full rescan before the OS GUI boot. 3 forces the driver to rescan in a thread and allows the OS to continue to boot. */
#define AUTO_ATTACH 1

/* Timeout in ms for data log compaction while shutting down. */
#define COMPACTION_TIMEOUT_MS 600000

/* For use only under the direction of Customer Support. */
#define COMPLETION_POLL_MS 10

/* Maximum derating of the core clock. */
#define CORECLK_MAX_DERATING 31744

/* Turn off all debug settings - they won't be available to turn on at run-time. */
#define DBGSET_COMPILE_DBGPRINT_OUT 1

/* The max number of requests the Linux request queue will accept. OS exposes as /sys/block/fioa/queue/nr_requests. */
#define DEFAULT_LINUX_MAX_NR_REQUESTS 1024

/* N/A */
#define DISABLE_MSI 0

/* N/A */
#define DISABLE_MSIX 0

/* For use only under the direction of Customer Support. */
#define DISABLE_SCANNER 0

/* For use only under the direction of Customer Support. */
#define ENABLE_DISCARD 1

/* Compile-in and enable the error injection subsystem. Use fio-error-inject utility to cause errors. */
#define ENABLE_ERROR_INJECTION 0

/* Enable recording of request latencies.  See https://confluence.int.fusionio.com/display/SW/Read+latency+analyzer */
#define ENABLE_LAT_RECORD 0

/* For use only under the direction of Customer Support. */
#define ENABLE_MULTQ_AT_LOAD 1

/* List of cards to exclude from driver initialization (comma separated list of <domain>:<bus>:<slot>.<func>) */
#define EXCLUDE_DEVICES ""

/* Timeout for data log compaction while shutting down. */
#define EXPECTED_IO_SIZE 0

/* Set up affinity options */
#define FIO_AFFINITY ""

/* Binary installation directory */
#define FIO_BINDIR "/usr/bin"

/* Datadir (platform independent data) installation directory */
#define FIO_DATADIR "/usr/share"

/* Optimal block size hint for the linux block layer. */
#define FIO_DEV_BLK_OPTIMAL_BLK_SIZE 32768

/* Number of seconds to wait for device file creation before continuing. */
#define FIO_DEV_WAIT_TIMEOUT_SECS 30

/* Documentation installation directory */
#define FIO_DOCDIR "/usr/share/doc"

/* The name of the driver */
#define FIO_DRIVER_NAME "iomemory-vsl4"

/* Override external power requirement on boards that normally require it. (comma-separated list of adapter serial numbers) */
#define FIO_EXTERNAL_POWER_OVERRIDE ""

/* Causes the driver to suspend operation on failure */
#define FIO_FAIL_FAST 1

/* For use only under the direction of Customer Support. */
#define FIO_GROOM_HARDER_THRESHOLD 5

/* Enables logging of LBA(s) resident in an LEB that fails grooming */
#define FIO_GROOM_LOG_LBA_IN_FAILED_LEB 0

/* Include installation directory */
#define FIO_INCLUDEDIR "/usr/include"

/* Library installation directory */
#define FIO_LIBDIR "/usr/lib"

/* Manual installation directory */
#define FIO_MANDIR "/usr/share/man"

/* Maximum number of bio structures allocated per device */
#define FIO_MAX_NUM_BIOS 5000

/* Number of bio structures pre-allocated per device */
#define FIO_MIN_NUM_BIOS 5000

/* Datadir (platform independent data) installation directory */
#define FIO_OEM_DATADIR "/usr/share/fio"

/* Library installation directory */
#define FIO_OEM_LIBDIR "/usr/lib/fio"

/* OEM name simplified for file naming */
#define FIO_OEM_NAME "fusionio"

/* OEM formal name */
#define FIO_OEM_NAME_FORMAL "Fusion-io"

/* OEM short name simplified for file naming */
#define FIO_OEM_NAME_SHORT "fio"

/* Source installation directory */
#define FIO_OEM_SOURCEDIR "/usr/src/fio"

/* The name of the driverkit package */
#define FIO_PACKAGE_NAME "iomemory-vsl4"

/* Causes the driver to pre-allocate the RAM it needs */
#define FIO_PREALLOCATE_MEMORY ""

/* installation prefix */
#define FIO_PREFIX "/usr"

/* Create a thread for each SCSI Logical Unit. */
#define FIO_SCSI_PORT_NEEDS_LU_THREAD 1

/* Enable UNMAP support. */
#define FIO_SCSI_UNMAP_SUPPORTED 0

/* Source installation directory */
#define FIO_SOURCEDIR "/usr/src"

/* Sysconfig installation directory */
#define FIO_SYSCONFDIR "/etc"

/* The major version of the utilities build */
#define FIO_UTILITIES_MAJOR_VERSION "4"

/* The micro version of the utilities build */
#define FIO_UTILITIES_MICRO_VERSION "7"

/* The minor version of the utilities build */
#define FIO_UTILITIES_MINOR_VERSION "3"

/* N/A */
#define FORCE_MINIMAL_MODE 0

/* Turn on code coverage */
#define FUSION_COVERAGE 0

/* Turn on debugging */
#define FUSION_DEBUG 0

/* Counts allocs and frees in cache pools */
#define FUSION_DEBUG_CACHE 0

/* Something to do with debugging memory */
#define FUSION_DEBUG_MEMORY 0

/* Turn on internal-only features */
#define FUSION_INTERNAL 0

/* Turn on debugging statements */
#define FUSION_DEBUG 1

/* The major version of this build */
#define FUSION_MAJOR_VERSION "4"

/* Turn on release symbol mangling */
#define FUSION_MANGLE_RELEASE_SYMBOLS 0

/* VSL Build for in-field media testing tool. */
#define FUSION_MEDIA_TEST_TOOL 0

/* The micro version of this build */
#define FUSION_MICRO_VERSION "7"

/* The minor version of this build */
#define FUSION_MINOR_VERSION "3"

/* The full version of this release */
#define FUSION_VERSION "4.3.7"

/* For use only under the direction of Customer Support. */
#define GROOMER_BUFFER_SIZE 16384

/* For use only under the direction of Customer Support. */
#define GROOMER_DISABLE 0

/* The proportion of logical space over the low watermark where grooming starts (in ten-thousandths) */
#define GROOMER_HIGH_WATER_DELTA_HPCNT 160

/* The proportion of logical space free over the runway that represents 'the wall' */
#define GROOMER_LOW_WATER_DELTA_HPCNT 1

/* Whitelist of cards to include in driver initialization (comma separated list of <domain>:<bus>:<slot>.<func>) */
#define INCLUDE_DEVICES ""

/* For use only under the direction of Customer Support. */
#define IODRIVE_LOAD_EB_MAP 1

/* For use only under the direction of Customer Support. */
#define IODRIVE_MAKE_ASSERT_NONFATAL 0

/* Maximum number of erase blocks to search for the LEB map. */
#define IODRIVE_MAX_EBMAP_PROBE 100

/* How many requests pending in iodrive */
#define IODRIVE_MAX_NUM_REQUEST_STRUCTURES 5000

/* How many requests pre-allocated in iodrive */
#define IODRIVE_MIN_NUM_REQUEST_STRUCTURES 5000

/* N/A */
#define IODRIVE_SYNC_DELAY_US 2000

/* N/A */
#define IODRIVE_SYNC_WAIT_US 100000

/* If the reserve space is below this threshold (in hundredths of percent), warnings will be issued. */
#define IODRIVE_THROTTLE_CAPACITY_WARNING_THRESHOLD 1000

/* Use the command timeout registers */
#define IODRIVE_USE_COMMAND_TIMEOUTS 1

/* Include block storage interface driver. */
#define KFIO_BLOCK_DEVICE 1

/* Include SCSI storage interface driver. */
#define KFIO_SCSI_DEVICE 0

/* The queue depth that is advertised to the OS SCSI interface. */
#define KFIO_SCSI_QUEUE_DEPTH 256

/* For use only under the direction of Customer Support. */
#define MAX_MD_BLOCKS_PER_DEVICE 0

/* For use only under the direction of Customer Support. */
#define PARALLEL_ATTACH 1

/* Global PCIe slot power limit in milliwatts. Performance will be throttled to not exceed this limit in any PCIe slot. */
#define PCIe_GLOBAL_SLOT_POWER_LIMIT -1

/* Internal */
#define PORT_SUPPORTS_ATOMIC_WRITE 1

/* Internal */
#define PORT_SUPPORTS_DEVFS_HANDLE 0

/* Allows allocation of NUMA node cache memory for requests. */
#define PORT_SUPPORTS_NUMA_NODE_CACHE_ALLOCATIONS 1

/* Internal */
#define PORT_SUPPORTS_NUMA_NODE_OVERRIDE 1

/* Internal */
#define PORT_SUPPORTS_PCI_NUMA_INFO 1

/* Internal */
#define PORT_SUPPORTS_PER_CPU 1

/* Internal */
#define PORT_SUPPORTS_SGLIST_COPY 1

/* Internal */
#define PORT_SUPPORTS_USER_PAGES 1

/* The megabyte limit for FIO_PREALLOCATE_MEMORY. This will prevent the driver from potentially using all of the system's non-paged memory. */
#define PREALLOCATE_MB 0

/* pad to use for parity (if possible, i.e. if not mapped out) */
#define PREFERRED_PARITY_PAD -1

/* For use only under the direction of Customer Support. */
#define READ_RETRY_LOGGING 0

/* Memory limit in MiBytes for rmap rescan. */
#define RMAP_MEMORY_LIMIT_MiB 3100

/* Memory limit in MiBytes for rsort rescan. */
#define RSORT_MEMORY_LIMIT_MiB 0

/* Use the KTR mechanism to track mem leaks. */
#define TRACE_MEM_ALLOCS 0

/* If true, use 1024 byte PCIe rx buffer. This improves performance but causes NMIs on some specific hardware. */
#define USE_LARGE_PCIE_RX_BUFFER 0

#endif /* __GENERATED_PORT_CONFIG_H__ */
