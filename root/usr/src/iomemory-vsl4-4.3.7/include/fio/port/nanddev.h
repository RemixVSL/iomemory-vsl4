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
//-----------------------------------------------------------------------------
#ifndef __FIO_PORT_NANDDEV_H__
#define __FIO_PORT_NANDDEV_H__

#include <fio/port/cdev.h>

struct fusion_nand_device;

/**
 * @brief Get the device number of the nand device
 */
extern uint32_t fusion_nand_get_devnum(struct fusion_nand_device *entry);

/**
 * @brief Get the PCI bus name of the device
 */
extern const char *fusion_nand_get_bus_name(struct fusion_nand_device *entry);

/**
 * @brief Get the device name of the fusion_nand_device
 */
extern const char *fusion_nand_get_dev_name(struct fusion_nand_device *entry);

/**
 * @brief get kfio_pci_dev_t * given fusion_nand_device
 */
extern void *fusion_nand_get_pci_dev(const struct fusion_nand_device *entry);

/**
 * @brief get back the pointer specified by porting layer as contest
 *        argument in iodrive_pci_create_pipeline call.
 */
extern void *fusion_nand_get_driver_data(struct fusion_nand_device *entry);

/**
 * @brief get the cdev handle given fusion_nand_device
 */
extern void *fusion_nand_get_cdev_handle(struct fusion_nand_device *entry);

/**
 * @brief The wrapper will call this function to get ioctl information to the
 *        kernel.
 */
extern int fusion_control_ioctl(void *nand, unsigned int cmd, fio_uintptr_t arg);

 /**
 * @brief The fusion_control_ioctl will call this function to get fusion
 *        internal ioctls.
 */
extern int fusion_control_internal_ioctl(void *nand, unsigned int cmd, fio_uintptr_t arg);

/**
 * @brief get kfio_poll_struct * given fusion_nand_device
 */
extern coms_poll_struct *nand_device_get_poll_struct(struct fusion_nand_device *entry);

/**
 * @brief device enumeration callback.
 */
typedef int (*fusion_nand_device_enum_proc_t)(struct fusion_nand_device *nand_dev, void *param);

/**
 * @brief enumerate all known devices, invoking enum_proc for each one.
 * when fail_on_locked == 0, it may sleep and wait for mutex.
 * when fail_on_locked == 1, it fails out with -EBUSY if can't get mutex. No sleep occurs. Used when a lock is already held
 */
extern int fusion_nand_device_enumerate(fusion_nand_device_enum_proc_t enum_proc, int fail_on_locked, void *param);

#endif // __FIO_PORT_NANDDEV_H__
