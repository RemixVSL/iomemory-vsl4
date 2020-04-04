//-----------------------------------------------------------------------------
// Copyright (c) 2015, SanDisk Corp. and/or all its affiliates. All rights reserved.
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

#ifndef __SRC_FIO_COMS_PORT_PORTING_CDEV_H__
#define __SRC_FIO_COMS_PORT_PORTING_CDEV_H__

#if !defined(UEFI)
#define PORT_SUPPORTS_CDEV
#endif

#include <fio/port/cdev.h>

/// @defgroup CDEVAPI-porting CDEV PORTING API (cdev => port interfaces)
/// @ingroup CDEVAPI
/// @{

struct coms_cdev;

#ifdef PORT_SUPPORTS_CDEV

/// @brief Create the port's cdev companion
extern int coms_port_cdev_create(struct coms_cdev *cdev, void *port_param, void **handlep);

/// @brief Destroy the port's cdev companion
extern void coms_port_cdev_destroy(void *port_cdev);

/// @brief Wake up anything waiting on the control device (e.g., a select)
extern void coms_port_cdev_wake(void *port_cdev, coms_poll_struct *poll_struct);

#else

static inline int coms_port_cdev_create(struct coms_cdev *cdev, void *port_param, void **handlep) { return 0; }
static inline void coms_port_cdev_destroy(void *port_cdev) { return; }
static inline void coms_port_cdev_wake(void *port_cdev, coms_poll_struct *ps) { return; }

#endif

/// @}

#endif //__SRC_FIO_COMS_PORT_PORTING_CDEV_H__
