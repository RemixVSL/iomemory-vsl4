//-----------------------------------------------------------------------------
// Copyright (c) 2014 Fusion-io, Inc. (acquired by SanDisk Corp. 2014)
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

#ifndef __FIO_COMS_PORT_CDEV_H__
#define __FIO_COMS_PORT_CDEV_H__

#include <fio/port/kfio.h>

/// @defgroup CDEVAPI-publicport CDEV PUBLICPORT API (public-client or port => cdev interfaces)
/// @{

struct coms_cdev;
typedef struct __fusion_poll_struct coms_poll_struct;   /// @todo why is this here when the __ version is in port?

extern uint32_t coms_cdev_get_dev_number(const struct coms_cdev *cdev);
extern const char *coms_cdev_get_name(const struct coms_cdev *cdev);
extern const char *coms_cdev_get_bus_name(const struct coms_cdev *cdev);

typedef int (*coms_cdev_enum_fn)(void *caller_handle, void *param);
extern struct coms_cdev *coms_port_cdev_enumerate(coms_cdev_enum_fn enum_proc, int fail_on_locked, void *param);

extern int coms_cdev_is_alerted(struct coms_cdev *cdev);
extern coms_poll_struct *coms_cdev_get_poll_struct(struct coms_cdev *cdev);
extern int coms_cdev_ioctl(struct coms_cdev *cdev, unsigned int cmd, fio_uintptr_t arg);

// Function to wait for device file creation
extern int coms_wait_for_dev(const char *devname);
/// @}

#endif //__FIO_COMS_PORT_CDEV_H__
