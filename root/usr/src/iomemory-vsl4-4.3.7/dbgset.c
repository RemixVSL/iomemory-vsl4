//-----------------------------------------------------------------------------
// Copyright (c) 2014 Fusion-io, Inc.(acquired by SanDisk Corp. 2014)
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
// -----------------------------------------------------------------------------
#if defined (__linux__)
#include <fio/port/compiler.h>
#include "port-internal-boss.h"
#endif
/// @cond GENERATED_CODE
#include <fio/port/port_config.h>
/// @endcond
#if (FUSION_INTERNAL==1)
#include <fio/internal/config.h>
#endif
#include <fio/port/dbgset.h>

#include <fio/port/port_config_vars_externs.h>
#if (FUSION_INTERNAL==1)
#include <fio/internal/config_vars_externs.h>
#endif

/* Create storage for the debug flags */
/// @cond GENERATED_CODE
#include <fio/port/port_config_macros_clear.h>
/// @endcond

/**
 * @ingroup PORT_COMMON_LINUX
 * @{
 */

#define FIO_PORT_CONFIG_SET_DBGFLAG
#define FIO_PORT_CONFIG_MACRO(dbgf) [_DF_ ## dbgf] = { .name = #dbgf, .mode = S_IRUGO | S_IWUSR, .value = dbgf },

#if (FUSION_INTERNAL==1)
#include <fio/internal/config_macros_clear.h>
#define FIO_CONFIG_SET_DBGFLAG
#define FIO_CONFIG_MACRO(dbgf) [_DF_ ## dbgf] = { .name = #dbgf, .mode = S_IRUGO | S_IWUSR, .value = dbgf },
#endif

static struct dbgflag _dbgflags[] = {
/// @cond GENERATED_CODE
#include <fio/port/port_config_macros.h>
#if (FUSION_INTERNAL==1)
#include <fio/internal/config_macros.h>
#endif
/// @endcond
    [_DF_END] = { .name = NULL, .mode = 0, .value =0 }
};

struct dbgset _dbgset = {
    .flag_dir_name = "debug",
    .flags         = _dbgflags
};

/* Make debug flags module parameters so they can be set at module load-time */
/// @cond GENERATED_CODE
#include <fio/port/port_config_macros_clear.h>
/// @endcond
#define FIO_PORT_CONFIG_SET_DBGFLAG
#define FIO_PORT_CONFIG_MACRO(dbgf) module_param_named(debug_ ## dbgf, _dbgflags[_DF_ ## dbgf].value, uint, S_IRUGO | S_IWUSR);
/// @cond GENERATED_CODE
#include <fio/port/port_config_macros.h>
/// @endcond
#if (FUSION_INTERNAL==1)
/// @cond GENERATED_CODE
#include <fio/internal/config_macros_clear.h>
/// @endcond
#define FIO_CONFIG_SET_DBGFLAG
#define FIO_CONFIG_MACRO(dbgf) module_param_named(debug_ ## dbgf, _dbgflags[_DF_ ## dbgf].value, uint, S_IRUGO | S_IWUSR);
/// @cond GENERATED_CODE
#include <fio/internal/config_macros.h>
/// @endcond
#endif

/**
 * @}
 */
