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

#ifndef _DBGSET_H_
#define _DBGSET_H_

#include <fio/port/kmsg.h>
#if defined(__KERNEL__)
#include <fio/port/kfio.h>
#else
#include <fio/port/ufio.h>
#endif

/// @cond GENERATED_CODE
#include <fio/port/port_config.h>
/// @endcond

enum _DBGSET_FLAGS {

/// @cond GENERATED_CODE
#include <fio/port/port_config_macros_clear.h>
/// @endcond

#define FIO_PORT_CONFIG_SET_DBGFLAG
#define FIO_PORT_CONFIG_MACRO(cfg) _DF_ ## cfg,

/// @cond GENERATED_CODE
#include <fio/port/port_config_macros.h>
#if (FUSION_INTERNAL==1)
#include <fio/internal/config_macros_clear.h>
#define FIO_CONFIG_SET_DBGFLAG
#define FIO_CONFIG_MACRO(cfg) _DF_ ## cfg,
#include <fio/internal/config_macros.h>
#endif
/// @endcond

    _DF_END
};

#undef FIO_PORT_CONFIG_MACRO

#if FUSION_INTERNAL==1
# undef FIO_CONFIG_MACRO
#endif

#if defined(__KERNEL__)

struct dbgflag {
    const char            *name;
    fio_mode_t             mode;
    unsigned int           value;
};

struct dbgset {
    struct kfio_info_node *flag_dir;
    const char            *flag_dir_name;
    struct dbgflag        *flags;
};

extern struct dbgset _dbgset;

extern int dbgs_create_flags_dir(struct dbgset *ds);
extern int dbgs_delete_flags_dir(struct dbgset *ds);

#endif

/* convert DBGSET_COMPILE_DBGPRINT_OUT to xDBGSET_COMPILE_DBGPRINT_OUT for
 * a quick disable of the section.
 */
#define xDBGSET_COMPILE_DBGPRINT_OUT 1


#if (defined(DBGSET_COMPILE_DBGPRINT_OUT) && DBGSET_COMPILE_DBGPRINT_OUT) || \
       FUSION_INTERNAL == 0 || FUSION_DEBUG == 0
#define FUSION_DBGPRINT 0
#else
#define FUSION_DBGPRINT 1
#endif

#if FUSION_DBGPRINT == 0

#define dbgflag_state(x)                   (0)
#define dbgprint(flag_def, ...)            do {} while(0)
#define dbgprint_nc(flag_def, nand_chan, fmt, ...) do {} while(0)
#define dbgprint_ioc(flag_def, chan, fmt, ...) do {} while(0)
#define dbgprint_log(flag_def, log, fmt, ...) do {} while(0)
#define dbgprint_cond(flag_def, cond, ...) do {} while(0)

#elif defined(__KERNEL__)

/* Do the full debug state machine stuff, unless overriden above */

/* Get the state of a flag from a dbgset */
#define dbgflag_state(df_flag)    \
    (_dbgset.flags[_DF_ ## df_flag].value)

#define dbgflag_set_state(df_flag, x)             \
    (_dbgset.flags[_DF_ ## df_flag].value = (x))

/// @brief Print output if flag_def is set to 1, don't print if flag_def is set to 0.
#define dbgprint(df_flag, ...)                              \
    do {                                                    \
        if (unlikely(_dbgset.flags[_DF_ ## df_flag].value)) \
            dbgkprint(__VA_ARGS__);                         \
    } while (0)

/// @brief A version of dbgprint() that adds an additional, run-time condition cond.
#define dbgprint_cond(df_flag, cond, ...)                         \
    do {                                                          \
        if (unlikely(_dbgset.flags[_DF_ ## df_flag].value &&      \
                    (cond)))                                      \
            dbgkprint(__VA_ARGS__);                               \
    } while (0)

/// @brief A version of dbgprint that uses the nand_chan pointer to add the bus name to the output.
#define dbgprint_nc(df_flag, nand_chan, fmt, ...)           \
    do {                                                    \
        if (unlikely(_dbgset.flags[_DF_ ## df_flag].value)) \
            dbgkprint("%s: " fmt, fusion_channel_get_bus_name(nand_chan), ##__VA_ARGS__); \
    } while (0)

/// @brief A version of dbgprint that uses the chan pointer to add the bus name to the output.
#define dbgprint_ioc(df_flag, chan, fmt, ...)           \
    do {                                                    \
        if (unlikely(_dbgset.flags[_DF_ ## df_flag].value)) \
            dbgkprint("%s: " fmt, iodrive_channel_get_bus_name(chan), ##__VA_ARGS__); \
    } while (0)

/// @brief A version of dbgprint that uses the log pointer to add the bus name to the output.
#define dbgprint_log(df_flag, log, fmt, ...)           \
    do {                                                    \
        if (unlikely(_dbgset.flags[_DF_ ## df_flag].value)) \
            dbgkprint("%s: " fmt, fio_log_get_bus_name(log), ##__VA_ARGS__); \
    } while (0)

#else

/* In userland, we just print unconditionally unless DBGSET_COMPILE_DBGPRINT_OUT is 1 or !INTERNAL */

#define dbgprint(df_flag, ...)                              \
    do {                                                    \
        dbgkprint(__VA_ARGS__);                             \
    } while (0)


#define dbgprint_cond(df_flag, cond, ...)                   \
    do {                                                    \
        if (cond)                                           \
            dbgkprint(__VA_ARGS__);                         \
    } while (0)

#define dbgprint_nc(flag_def, nand_chan, fmt, ...)          \
    do {                                                    \
        dbgkprint("%s: " fmt, fusion_channel_get_bus_name(nand_chan), ##__VA_ARGS__); \
    } while (0)

#define dbgprint_ioc(df_flag, chan, fmt, ...)           \
    do {                                                    \
        dbgkprint("%s: " fmt, iodrive_channel_get_bus_name(chan), ##__VA_ARGS__); \
    } while (0)

#define dbgprint_log(df_flag, log, fmt, ...)           \
    do {                                                    \
       dbgkprint("%s: " fmt, fio_log_get_bus_name(log), ##__VA_ARGS__); \
    } while (0)


#endif /* DBGSET_COMPILE_DBGPRINT_OUT */

/// @brief A version of errprint using an id and nand_chan pointer to add the bus name to the output
#define errprint_nc(nand_chan, id, fmt, ...)                             \
    kmsg_filter(MSG_LEVEL_ERR, fusion_channel_get_bus_name(nand_chan), id, fmt, ##__VA_ARGS__)

/// @brief A version of errprint that uses the chan pointer to add the bus name to the output.
#define errprint_ioc(chan, id, fmt, ...)                                 \
    kmsg_filter(MSG_LEVEL_ERR, iodrive_channel_get_bus_name(chan), id, fmt, ##__VA_ARGS__)

/// @brief A version of errprint for messages that directly provide the device label
#define errprint_lbl(label, id, fmt, ...)                        \
    kmsg_filter(MSG_LEVEL_ERR, label, id, fmt, ##__VA_ARGS__)

/// @brief A version of errprint for messages not specific to a channel (e.g. driver load)
#define errprint_all(id, fmt, ...)                                \
    kmsg_filter(MSG_LEVEL_ERR, "ioMemory driver", id, fmt, ##__VA_ARGS__)

/// @brief A print that produces errprint on INTERNAL or DEBUG builds
#if FUSION_INTERNAL || FUSION_DEBUG
#define sqaprint(fmt, ...)  kmsg_filter(MSG_LEVEL_ERR, "", NO_MSG_ID, fmt, ##__VA_ARGS__)
#else
#define sqaprint(...)
#endif

/// @brief A version of wrnprint for messages not specific to a channel (e.g. driver load)
#define wrnprint_all(dummy, fmt, ...)                                  \
    kmsg_filter(MSG_LEVEL_WARN, "Fusion-io driver", NO_MSG_ID, fmt, ##__VA_ARGS__)

#endif /* _DBGSET_H_ */
