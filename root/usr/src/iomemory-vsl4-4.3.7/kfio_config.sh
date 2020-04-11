#!/bin/sh

#-----------------------------------------------------------------------------
# Copyright (c) 2006-2014, Fusion-io, Inc.(acquired by SanDisk Corp. 2014)
# Copyright (c) 2014-2018 SanDisk Corp. and/or all its affiliates. (acquired by Western Digital Corp. 2016)
# Copyright (c) 2016-2019 Western Digital Technologies, Inc. All rights reserved.
#
#  Redistribution and use in source and binary forms, with or without
#  modification, are permitted provided that the following conditions are met:
#  * Redistributions of source code must retain the above copyright notice,
#    this list of conditions and the following disclaimer.
#  * Redistributions in binary form must reproduce the above copyright notice,
#    this list of conditions and the following disclaimer in the documentation
#    and/or other materials provided with the distribution.
#  * Neither the name of the SanDisk Corp. nor the names of its contributors
#    may be used to endorse or promote products derived from this software
#    without specific prior written permission.
#  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
#  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
#  THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
#  A PARTICULAR PURPOSE ARE DISCLAIMED.
#  IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
#  FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
#  OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
#  PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
#  OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
#  WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
#  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
#  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#-----------------------------------------------------------------------------

set -e
set -u


VERBOSE=0
PRESERVE=0
FIOARCH="$(uname -m)"
KERNELDIR="/lib/modules/$(uname -r)/build"
KERNELSOURCEDIR=""
OUTPUTFILE=""
CONFIGDIR=""
CREATED_CONFIGDIR=0
TMP_OUTPUTFILE=""
KFIOC_PROCS=""
FAILED_TESTS=""
TIMEOUT_DELTA=${TIMEOUT_DELTA:-120}
TIMEOUT_TIME=$(($(date "+%s")+$TIMEOUT_DELTA))
FUSION_DEBUG=0


# These are exit codes from include/sysexits.h
EX_OK=0
EX_USAGE=64
EX_SOFTWARE=70
EX_OSFILE=72


# NOTE: test_list is obviously the names of the flags that must be
# tested and set.  Even though it's poor bourne shell style to use
# capitalized function names, the test flags and the test functions
# have identical names so that a look-up-table is not needed (it just
# makes a few things simpler).

# Several of the compile time tests include linux/slab.h, which eventually include
# linux/types.h.  This has the side effect of including kernel space definitions
# of integral types which may cause issues with print format specifiers.  Later
# Linux kernels include asm-generic/int-ll64.h unconditionally for kernel space
# integral type definitions, while the Linux porting layer stdint.h conditionally
# defines 64 bit integral types with one or two "long" statements dependent on
# architecture.

KFIOC_TEST_LIST="KFIOC_HAS_BIOVEC_ITERATORS
KFIOC_HAS_PCI_ERROR_HANDLERS
KFIOC_HAS_GLOBAL_REGS_POINTER
KFIOC_HAS_SYSRQ_KEY_OP_ENABLE_MASK
KFIOC_HAS_LINUX_SCATTERLIST_H
KFIOC_KMEM_CACHE_CREATE_REMOVED_DTOR
KFIOC_HAS_KMEM_CACHE
KFIOC_STRUCT_FILE_HAS_PATH
KFIOC_UNREGISTER_BLKDEV_RETURNS_VOID
KFIOC_USE_LINUX_UACCESS_H
KFIOC_MODULE_PARAM_ARRAY_NUMP
KFIOC_HAS_BLK_LIMITS_IO_MIN
KFIOC_HAS_BLK_LIMITS_IO_OPT
KFIOC_PCI_REQUEST_REGIONS_CONST_CHAR
KFIOC_FOPS_USE_LOCKED_IOCTL
KFIOC_HAS_RQ_POS_BYTES
KFIOC_QUEUE_HAS_NONROT_FLAG
KFIOC_QUEUE_HAS_RANDOM_FLAG
KFIOC_NUMA_MAPS
KFIOC_PCI_HAS_NUMA_INFO
KFIOC_CACHE_ALLOC_NODE_TAKES_FLAGS
KFIOC_HAS_QUEUE_FLAG_CLUSTER
KFIOC_HAS_SCSI_SG_FNS
KFIOC_HAS_SCSI_SG_COPY_FNS
KFIOC_HAS_SCSI_RESID_FNS
KFIOC_HAS_SCSI_QD_CHANGE_FN
KFIOC_HAS_PROCFS_PDE_DATA
KFIOC_HAS_PROC_CREATE_DATA
KFIOC_SGLIST_NEW_API
KFIOC_ACPI_EVAL_INT_TAKES_UNSIGNED_LONG_LONG
KFIOC_BIO_HAS_SEG_SIZE
KFIOC_HAS_FILE_INODE_HELPER
KFIOC_GET_USER_PAGES_HAS_GUP_FLAGS
KFIOC_BLK_MQ_OPS_HAS_MAP_QUEUES
KFIOC_HAS_PCI_ENABLE_MSIX_EXACT
KFIOC_HAS_BLK_QUEUE_SPLIT2
KFIOC_MISSING_WORK_FUNC_T
KFIOC_USE_BLK_QUEUE_FLAGS_FUNCTIONS
"


#
# General functions
#

# Pass an argument as a delta from "now" time for the timeout.  If an
# argument is not passed then default to adding TIMEOUT_DELTA to "now".
update_timeout()
{
    TIMEOUT_TIME=$(($(date "+%s")+${1:-$TIMEOUT_DELTA}))
}


# This is a way to log the output of this script in a merged stdout+stderr log file
# while still having stdout and stderr sent to . . . stdout and stderr!
open_log()
{
    # The tee processes will die when this process exits.
    rm -f "$CONFIGDIR/kfio_config.stdout" "$CONFIGDIR/kfio_config.stderr" "$CONFIGDIR/kfio_config.log"
    exec 3>&1 4>&2
    touch "$CONFIGDIR/kfio_config.log"
    mkfifo "$CONFIGDIR/kfio_config.stdout"
    tee -a "$CONFIGDIR/kfio_config.log" <"$CONFIGDIR/kfio_config.stdout" >&3 &
    mkfifo "$CONFIGDIR/kfio_config.stderr"
    tee -a "$CONFIGDIR/kfio_config.log" <"$CONFIGDIR/kfio_config.stderr" >&4 &
    exec >"$CONFIGDIR/kfio_config.stdout"  2>"$CONFIGDIR/kfio_config.stderr"
    # Don't need these now that everything is connected
    rm -f "$CONFIGDIR/kfio_config.stdout" "$CONFIGDIR/kfio_config.stderr"
}


kfioc_open_config()
{
    cat <<EOF > "${TMP_OUTPUTFILE}"
//-----------------------------------------------------------------------------
// Copyright (c) 2011-2014, Fusion-io, Inc. (acquired by SanDisk Corp. 2014)
// Copyright (c) 2014-2015, SanDisk Corp. and/or all its affiliates.
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

#ifndef _FIO_PORT_LINUX_KFIO_CONFIG_H_
#define _FIO_PORT_LINUX_KFIO_CONFIG_H_

EOF

}


kfioc_close_config()
{
    cat <<EOF >> "${TMP_OUTPUTFILE}"
#include <linux/slab.h>
#include <linux/gfp.h>

#ifndef GFP_NOWAIT
#define GFP_NOWAIT  (GFP_ATOMIC & ~__GFP_HIGH)
#endif

#endif /* _FIO_PORT_LINUX_KFIO_CONFIG_H_ */
EOF

}


collect_kfioc_results()
{
    local flag=
    local pid=
    local status=
    local result=
    local test_fail=
    local rc=0

    for flag in $KFIOC_PROCS; do
        pid=${flag##*:}
        flag=${flag%%:*}
        test_fail=
        status=$(get_kfioc_status $flag exit)
        result=$(get_kfioc_status $flag result)

        if [ -z "$status" ]; then
            printf "ERROR: missing test exit status $flag\n" >&2
            test_fail=1
        elif [ "$status" -ge 128 ]; then
            printf "ERROR: test exited by signal $flag $status\n" >&2
            test_fail=1
        elif [ "$status" -gt 2 ]; then
            printf "ERROR: unexpected test exit status $flag $status\n" >&2
            test_fail=1
        fi

        if [ -z "$result" ]; then
            printf "ERROR: missing test result $flag\n" >&2
            test_fail=1
        elif [ "$result" -gt 1 ]; then
            printf "ERROR: unexpected test value $flag $result\n" >&2
            test_fail=1
        fi

        if [ -n "$test_fail" ]; then
            FAILED_TESTS="$FAILED_TESTS $flag"
            rc=1
        else
            printf "  %.14s  $flag=$result\n" $(date "+%s.%N")
            generate_kfioc_define $flag >>"$TMP_OUTPUTFILE"
        fi
    done

    return $rc
}


fio_create_makefile()
{
    local kfioc_flag="$1"
    local extra_cflags="${2:-}"

    cat <<EOF >"${CONFIGDIR}/${kfioc_flag}/Makefile"
KERNEL_SRC := ${KERNELDIR}

ifneq (\$(KERNELRELEASE),)

obj-m := kfioc_test.o

else

all: modules

modules clean:
	\$(MAKE) -C \$(KERNEL_SRC) M=\$(CURDIR) EXTRA_CFLAGS='-Wall ${extra_cflags}' \$@

endif

EOF
}


sig_exit()
{
    printf "%.14s  Exiting\n" $(date "+%s.%N")
    # Keep the output files if "PRESERVE" is used or if there are failures.
    if [ -d "$CONFIGDIR" -a 0 -eq "$PRESERVE" -a -z "$FAILED_TESTS" ]; then
        printf "Deleting temporary files (use '-p' to preserve temporary files)\n"
        if [ 1 -eq "$CREATED_CONFIGDIR" ]; then
            rm -rf "${CONFIGDIR}"
        else
            rm -rf "$TMP_OUTPUTFILE" "${CONFIGDIR}/kfio"*
        fi
    elif [ -n "$FAILED_TESTS" ]; then
        printf "Preserving configdir for failure analysis: $CONFIGDIR\n" >&2
        tar \
            --exclude="*kfio_config.tar.gz" \
            -C "$(dirname $CONFIGDIR)" \
            -c -z \
            -f "$CONFIGDIR/kfio_config.tar.gz" \
            "$(basename $CONFIGDIR)"
        printf "Submit tar file to support for analysis: '$CONFIGDIR/kfio_config.tar.gz'\n" >&2
    else
        printf "Preserving configdir due to '-p' option: $CONFIGDIR\n"
    fi
}


# start_test() wraps each test so that script debug can be recorded
start_test()
{
    local test_name="$1"

    mkdir -p "$CONFIGDIR/$test_name"
    exec >"$CONFIGDIR/$test_name/kfio_config.log" 2>&1
    set -x
    $test_name $test_name
}


start_tests()
{
    local kfioc_test=

    # Clean out any old cruft in case building in a previously used directory
    (
        cd "$CONFIGDIR"
        rm -rf KFIOC_* kfio_config.h kfio_config.tar.gz
    )

    printf "Starting tests:\n"
    for kfioc_test in $KFIOC_TEST_LIST; do
        printf "  %.14s  $kfioc_test...\n" $(date "+%s.%N")

        # Each test has an absolute time deadline for completion from when it is started.
        # A test can depend on another test so it needs a timeout to decide that the other
        # test may have failed.
        update_timeout
        start_test $kfioc_test &
        KFIOC_PROCS="$KFIOC_PROCS $kfioc_test:$!"
    done

    printf "Started tests, waiting for completions...\n"

    # We want more time for ourselves than the child tests
    TIMEOUT_DELTA=$(($TIMEOUT_DELTA+$TIMEOUT_DELTA/2))
    update_timeout
}


finished()
{
    local rc=0

    kfioc_open_config || rc=1

    collect_kfioc_results || rc=1

    kfioc_close_config || rc=1

    if [ -z "$FAILED_TESTS" -a 0 = "$rc" ]; then
        cp "${TMP_OUTPUTFILE}" "$OUTPUTFILE"
        printf "Finished\n"
    else
        printf "ERROR: Failures detected\n" >&2
        if [ -n "$FAILED_TESTS" ]; then
            printf "Failed tests: $FAILED_TESTS\n" >&2
            rc=1
        fi
    fi

    return $rc
}


set_kfioc_status()
{
    local kfioc_flag="$1"
    local value="$2"
    local file="$3"
    shift 3

    mkdir -p "$CONFIGDIR/$kfioc_flag"
    # It appears there's a nice race condition where a file is created by
    # the test and the parent reads the data from the file before the write
    # is flushed - resulting in an empty read.  Creating the ".tmp" file
    # and then moving it into place should guarantee that the contents are
    # flushed prior to the file existing.
    printf "%s\n" "$value" >"$CONFIGDIR/$kfioc_flag/$file.tmp"
    mv "$CONFIGDIR/$kfioc_flag/$file.tmp" "$CONFIGDIR/$kfioc_flag/$file"
}


get_kfioc_status()
{
    local kfioc_flag="$1"
    local file="$2"
    local last_status_count="$(ls "$CONFIGDIR/"*/* 2>/dev/null | wc -l)"
    local this_status_count=0

    while [ ! -s "$CONFIGDIR/$kfioc_flag/$file" -a $(date "+%s") -lt "$TIMEOUT_TIME" ]; do
        this_status_count="$(ls "$CONFIGDIR/"*/* 2>/dev/null | wc -l)"
        if [ "$this_status_count" -gt "$last_status_count" ]; then
            last_status_count="$this_status_count"
            update_timeout
        fi
        sleep 1
    done

    if [ -s "$CONFIGDIR/$kfioc_flag/$file" ]; then
        local result="$(cat "$CONFIGDIR/$kfioc_flag/$file")"
        if [ -z "$result" ]; then
            printf "ERROR: empty result in $CONFIGDIR/$kfioc_flag/$file\n" >&2
            exit $EX_SOFTWARE
        fi
        printf "$result"
        update_timeout
    else
        printf "%.14s  ERROR: timed out waiting for $kfioc_flag/$file $$ $last_status_count:$this_status_count $TIMEOUT_TIME\n" $(date "+%s.%N") >&2
        exit $EX_SOFTWARE
    fi
}


generate_kfioc_define()
{
    local kfioc_flag="$1"
    local result=$(get_kfioc_status $kfioc_flag result)

    if [ -n "$result" ]; then
        printf "#define $kfioc_flag ($result)\n"
    fi
}


kfioc_test()
{
    local code="
#include <linux/module.h>
$1
"
    local kfioc_flag="$2"
    # POSITIVE_RESULT is what should be returned when a test compiles successfully.
    # This value is inverted when the compile test fails
    local positive_result="$3"
    local cflags="${4:-}"
    local test_dir="${CONFIGDIR}/${kfioc_flag}"
    local result=0
    local license="
MODULE_LICENSE(\"Proprietary\");
"

    mkdir -p "$test_dir"
    fio_create_makefile "$kfioc_flag" "$cflags"

    echo "$code" > "$test_dir/kfioc_test.c"

    if [ 1 -eq "$FUSION_DEBUG" ]; then
        license="
MODULE_LICENSE(\"GPL\");
"
    fi
    echo "$license" >> "$test_dir/kfioc_test.c"

    if [ 1 -eq "$VERBOSE" ]; then
        env -i PATH="${PATH}" make -C "$test_dir" V=1 2>&1 | tee "$test_dir/kfioc_test.log" || result=$?
    else
        env -i PATH="${PATH}" make -C "$test_dir" V=1 >"$test_dir/kfioc_test.log" 2>&1 || result=$?
    fi

    # Save the exit status
    set_kfioc_status $kfioc_flag $result exit

    # Interpret the result
    local myflag=$positive_result
    # Return code of 0 indicates success
    if [ $result != 0 ]; then
        myflag=$((! $positive_result))
    fi

    # Save the interpreted result
    set_kfioc_status $kfioc_flag $myflag result
}


kfioc_has_include()
{
    local include_file="$1"
    local kfioc_flag="$2"

    local test_code="
#include <$include_file>
"

    kfioc_test "$test_code" "$kfioc_flag" 1
}


#
# Actual test procedures for determining Kernel capabilities
#


# flag:           KFIOC_MISSING_WORK_FUNC_T
# usage:          undef for automatic selection by kernel version
#                 0     for non stupid kernels
#                 1     for 2.6.18-92.el5.h
# git commit:
# comments:
KFIOC_MISSING_WORK_FUNC_T()
{
    local test_flag="$1"
    local test_code='
#include <linux/workqueue.h>

void kfio_test_work_func_t(void) {
    work_func_t *fn;
    fn = NULL;
}
'

    kfioc_test "$test_code" "$test_flag" 0
}

# flag:           KFIOC_HAS_PCI_ERROR_HANDLERS
# values:
#                 0     for older kernels
#                 1     For kernels that have pci error handlers.
# git commit:     392a1ce761bc3b3a5d642ee341c1ff082cbb71f0
# kernel version: >= 2.6.18
KFIOC_HAS_PCI_ERROR_HANDLERS()
{
    local test_flag="$1"
    local test_code='
#include <linux/pci.h>

void kfioc_test_pci_error_handlers(void) {
    struct pci_error_handlers e;
    (void)e;
}
'

    kfioc_test "$test_code" "$test_flag" 1
}


# flag:           KFIOC_HAS_GLOBAL_REGS_POINTER
# values:
#                 0     for older kernel that pass pt_regs arguments to handlers
#                 1     for kernels that have a global regs pointer.
# git commit:     7d12e780e003f93433d49ce78cfedf4b4c52adc5
# comments:       The change in IRQ arguments causes an ABI incompatibility.
KFIOC_HAS_GLOBAL_REGS_POINTER()
{
    local test_flag="$1"
    local test_code='
#include <asm/irq_regs.h>

struct pt_regs *kfioc_set_irq_regs(void) {
    return get_irq_regs();
}
'

    kfioc_test "$test_code" "$test_flag" 1
}


# flag:           KFIOC_HAS_SYSRQ_KEY_OP_ENABLE_MASK
# values:
#                 0     for older kernels that removed enable_mask in struct sysrq_key_op
#                 1     for kernels that have enable_mask member in struct sysrq_key_op
#                 in struct sysrq_key_op.
# git commit:     NA
# kernel version: >= 2.6.12
# comments:       Structure size change causes ABI incompatibility.
KFIOC_HAS_SYSRQ_KEY_OP_ENABLE_MASK()
{
    local test_flag="$1"
    local test_code='
#include <linux/sysrq.h>

int kfioc_test_sysrq_key_op(void) {
    struct sysrq_key_op op = {};
    return op.enable_mask;
}
'

    kfioc_test "$test_code" "$test_flag" 1
}


# flag:           KFIOC_HAS_LINUX_SCATTERLIST_H
# values:
#                 0     for older kernels that had asm/scatterlist.h
#                 1     for kernels that have linux/scatterlist.h
# git commit:     NA
# comments:
KFIOC_HAS_LINUX_SCATTERLIST_H()
{
    local test_flag="$1"
    local test_code='
#include <asm/scatterlist.h>
#include <linux/mm.h>
#include <linux/scatterlist.h>
'

    kfioc_test "$test_code" "$test_flag" 1
}


# flag:           KFIOC_KMEM_CACHE_CREATE_REMOVED_DTOR
# values:
#                 0     for older kernels that have the dtor argument to kmem_cache_create
#                 1     for kernels that removed the dtor argument to kmem_cache_create
# git commit:     c59def9f222d44bb7e2f0a559f2906191a0862d7
# comments:       API and ABI incompatible.
KFIOC_KMEM_CACHE_CREATE_REMOVED_DTOR()
{
    local test_flag="$1"
    local test_code='
#include <linux/slab.h>

void kfioc_test_kmem_cache_create(void) {
    kmem_cache_create("foo", 0, 0, 0, NULL);
}
'

    kfioc_test "$test_code" "$test_flag" 1
}

# flag:           KFIOC_HAS_KMEM_CACHE
# values:
#                 0     for older kernels that use kmem_cache_s
#                 1     for kernels that have kmem_cache
# git commit:     2109a2d1b175dfcffbfdac693bdbe4c4ab62f11f
# comments:       minor name change of the structure.
KFIOC_HAS_KMEM_CACHE()
{
    local test_flag="$1"
    local test_code='
#include <linux/slab.h>

struct kfioc_test_kmem_cache {
    int junk;
};

void kfioc_test_kmem_cache(void) {
    struct kfioc_test_kmem_cache c;
    kmem_cache_destroy((struct kmem_cache *) &c);
    return;
}
'

    kfioc_test "$test_code" "$test_flag" 1 "-Werror"
}

# flag:           KFIOC_STRUCT_FILE_HAS_PATH
# values:
#                 0     for older kernels that had separate struct dentry and
#                       struct vfsmount.
#                 1     for newer kernels with struct path in struct file.
# git commit:     0f7fc9e4d03987fe29f6dd4aa67e4c56eb7ecb05
# comments:       API portability.  ABI portability problems because struct vfsmount
#                 and struct dentry are reversed in struct path.
KFIOC_STRUCT_FILE_HAS_PATH()
{
    local test_flag="$1"
    local test_code='
#include <linux/fs.h>

void kfioc_test_file_path(void) {
    struct file f = { .f_path = { [0] = 0, [1] = 0 } };
    (void)f;
}
'

    kfioc_test "$test_code" "$test_flag" 1
}

# flag:           KFIOC_UNREGISTER_BLKDEV_RETURNS_VOID
# values:
#                 0     for kernels that return int from unregister_blkdev()
#                 1     for newer kernels that return void from unregister_blkdev()
# git commit:     f4480240f700587c15507b7815e75989b16825b2
# comments:
KFIOC_UNREGISTER_BLKDEV_RETURNS_VOID()
{
    local test_flag="$1"
    local test_code='
#include <linux/fs.h>

int kfioc_test_unregister_blkdev(void) {
    int rc = 0;
    rc = unregister_blkdev(0, NULL);
    return rc;
}
'

    kfioc_test "$test_code" "$test_flag" 0
}

# flag:           KFIOC_HAS_BIOVEC_ITERATORS
# values:
#                 0     for kernel doesn't support biovec_iter and immuatable biovecs
#                 1     for kernel that supports biovec_iter and immutable biovecs
# comments:       Introduced in 3.14
KFIOC_HAS_BIOVEC_ITERATORS()
{
    local test_flag="$1"
    local test_code='
#include <linux/bio.h>

void kfioc_test_has_biovec_iterators(void) {
        struct bvec_iter bvec_i;
        bvec_i.bi_sector = 0;
}
'

    kfioc_test "$test_code" "$test_flag" 1 -Werror
}

# flag:          KFIOC_USE_BLK_QUEUE_FLAGS_FUNCTIONS
# usage:         1   Kernel uses newer blk_queue_flag_set/clear functions.
#                0   It uses older queue_flag_set/clear functions.
# comments:       Changed in kernel 4.18 so that the queue lock is taken when manipulating queue flags.
KFIOC_USE_BLK_QUEUE_FLAGS_FUNCTIONS()
{
    local test_flag="$1"
    local test_code='
#include <linux/blkdev.h>
void kfioc_use_blk_queue_functions(void)
{
    struct request_queue *rq = NULL;
    int flags = 1;
    blk_queue_flag_set(flags, rq);
}
'
    kfioc_test "$test_code" KFIOC_USE_BLK_QUEUE_FLAGS_FUNCTIONS 1 -Werror
}

# flag:           KFIOC_USE_LINUX_UACCESS_H
# values:
#                 0     for kernels that use asm/uaccess.h
#                 1     for kernels that use linux/uaccess.h
KFIOC_USE_LINUX_UACCESS_H()
{
    local test_flag="$1"
    kfioc_has_include "linux/uaccess.h" "$test_flag"
}

# flag:           KFIOC_MODULE_PARAM_ARRAY_NUMP
# values:
#                 0     for kernels that don't take a nump pointer as the fourth argument
#                       to the module_param_array_named() macro (and module_param_array())
#                 1     for kernels that take nump pointer parameters.
# git commit:     Pre-dates git
# comments:
KFIOC_MODULE_PARAM_ARRAY_NUMP()
{
    local test_flag="$1"
    local test_code='
char *test[10];

module_param_array(test, charp, NULL, 0);
'

    kfioc_test "$test_code" "$test_flag" 1
}

# flag:           KFIOC_HAS_BLK_LIMITS_IO_MIN
#                 1     if the kernel defines blk_limits_io_min
#                 0     if the kernel does not
KFIOC_HAS_BLK_LIMITS_IO_MIN()
{
    local test_flag="$1"
    local test_code='
#include <linux/blkdev.h>
void kfioc_has_blk_limits_io_min(void)
{
    blk_limits_io_min(NULL, 0);
}
'

    kfioc_test "$test_code" "$test_flag" 1 -Werror-implicit-function-declaration
}

# flag:           KFIOC_HAS_BLK_LIMITS_IO_OPT
#                 1     if the kernel defines blk_limits_io_opt
#                 0     if the kernel does not
KFIOC_HAS_BLK_LIMITS_IO_OPT()
{
    local test_flag="$1"
    local test_code='
#include <linux/blkdev.h>
void kfioc_has_blk_limits_io_opt(void)
{
    blk_limits_io_opt(NULL, 0);
}
'

    kfioc_test "$test_code" "$test_flag" 1 -Werror-implicit-function-declaration
}

# flag:           KFIOC_FOPS_USE_LOCKED_IOCTL
#                 1     if the driver should use fops->ioctl()
#                 0     if the driver should use fops->unlocked_ioctl()
#                 Change introduced in 2.6.36
KFIOC_FOPS_USE_LOCKED_IOCTL()
{
    local test_flag="$1"
    local test_code='
#include <linux/fs.h>
void foo(void)
{
    struct file_operations fops;
    fops.ioctl = NULL;
}
'
    kfioc_test "$test_code" "$test_flag" 1 -Werror
}

# flag:           KFIOC_HAS_RQ_POS_BYTES
#                 1     if the driver should use
#                 0     if the driver should use
#                 Change introduced in 2e46e8b27aa57c6bd34b3102b40ee4d0144b4fab
KFIOC_HAS_RQ_POS_BYTES()
{
    local test_flag="$1"
    local test_code='
#include <linux/blkdev.h>
void foo(void)
{
    struct request *req = NULL;
    sector_t foo;

    foo = blk_rq_pos(req);
}
'
    kfioc_test "$test_code" "$test_flag" 1 -Werror-implicit-function-declaration
}

# flag:          KFIOC_PCI_REQUEST_REGIONS_CONST_CHAR
# usage:         1   pci_request_regions(..) has a const second parameter.
#                0   pci_request_regions() has no const second parameter.
KFIOC_PCI_REQUEST_REGIONS_CONST_CHAR()
{
    local test_flag="$1"
    local test_code='
#include <linux/pci.h>
int kfioc_pci_request_regions_const_param(void)
{
    return pci_request_regions(NULL, (const char *) "foo");
}
'
    kfioc_test "$test_code" KFIOC_PCI_REQUEST_REGIONS_CONST_CHAR 1 -Werror
}

# flag:          KFIOC_QUEUE_HAS_NONROT_FLAG
# usage:         1   Kernel supports flag for non-rotational device
#                0   It does not
KFIOC_QUEUE_HAS_NONROT_FLAG()
{
    local test_flag="$1"
    local test_code='
#include <linux/blkdev.h>
int kfioc_check_nonrot_flag(void)
{
    return QUEUE_FLAG_NONROT;
}
'
    kfioc_test "$test_code" KFIOC_QUEUE_HAS_NONROT_FLAG 1 -Werror
}

# flag:          KFIOC_QUEUE_HAS_RANDOM_FLAG
# usage:         1   Kernel supports flag for disabling random entropy
#                0   It does not
KFIOC_QUEUE_HAS_RANDOM_FLAG()
{
    local test_flag="$1"
    local test_code='
#include <linux/blkdev.h>
int kfioc_check_random_flag(void)
{
    return QUEUE_FLAG_ADD_RANDOM;
}
'
    kfioc_test "$test_code" KFIOC_QUEUE_HAS_RANDOM_FLAG 1 -Werror
}

# flag:          KFIOC_NUMA_MAPS
# usage:         1   Kernel exports enough NUMA knowledge for us to bind
#                0   It does not
KFIOC_NUMA_MAPS()
{
    local test_flag="$1"
    local test_code='
#include <linux/sched.h>
#include <linux/cpumask.h>
const cpumask_t *kfioc_check_numa_maps(void)
{
    return cpumask_of_node(0);
}
'
    kfioc_test "$test_code" "$test_flag" 1 -Werror
}

# flag:          KFIOC_PCI_HAS_NUMA_INFO
# usage:         1   Kernel populates NUMA info for PCI devices
#                0   It does not
KFIOC_PCI_HAS_NUMA_INFO()
{
    local test_flag="$1"
    local test_code='
#include <linux/pci.h>
#include <linux/device.h>
int kfioc_pci_has_numa_info(void)
{
    struct pci_dev pdev = { };

    return dev_to_node(&pdev.dev);
}
'
    kfioc_test "$test_code" "$test_flag" 1 -Werror
}

# flag:          KFIOC_CACHE_ALLOC_NODE_TAKES_FLAGS
# usage:         1   kmem_cache_alloc_node takes a 'flags' parameter.
#                0   It does not
KFIOC_CACHE_ALLOC_NODE_TAKES_FLAGS()
{
    local test_flag="$1"
    local test_code='
#include <linux/slab.h>
void kfioc_kmem_cache_alloc_node_takes_flags(void)
{
    kmem_cache_alloc_node(0, GFP_KERNEL, 0);
}
'
    kfioc_test "$test_code" "$test_flag" 1 -Werror
}

# flag:          KFIOC_HAS_QUEUE_FLAG_CLUSTER
# usage:         1   Kernel has QUEUE_FLAG_CLUSTER
#                0   It does not
KFIOC_HAS_QUEUE_FLAG_CLUSTER()
{
    local test_flag="$1"
    local test_code='
#include <linux/blkdev.h>
int has_queue_flag_cluster(void)
{
    return QUEUE_FLAG_CLUSTER ? 1 : 0;
}
'
    kfioc_test "$test_code" KFIOC_HAS_QUEUE_FLAG_CLUSTER 1 -Werror
}

# flag:          KFIOC_SGLIST_NEW_API
# usage:         1   Kernel supports new sglist API
#                0   It does not
KFIOC_SGLIST_NEW_API()
{
    local test_flag="$1"
    local test_code='
#include <linux/scatterlist.h>
void support_sglist_new_api(void)
{
    struct scatterlist *sl = NULL;

    sg_set_page(sl, sg_page(sl), 0, 0);
}
'
    kfioc_test "$test_code" KFIOC_SGLIST_NEW_API 1 -Werror
}

# flag:           KFIOC_HAS_SCSI_QD_CHANGE_FN
# usage:          undef for automatic selection by kernel version
#                 0     if the kernel does not have the SCSI change queue depth function
#                 1     if the kernel has the functions
KFIOC_HAS_SCSI_QD_CHANGE_FN()
{
    local test_flag="$1"
    local test_code='
#include <scsi/scsi_device.h>
struct scsi_device *sdev;

void kfioc_has_scsi_qd_fns(void)
{
    scsi_change_queue_depth(sdev, 64);
}
'

    kfioc_test "$test_code" "$test_flag" 1 -Werror-implicit-function-declaration
}


# flag:           KFIOC_HAS_SCSI_SG_FNS
# usage:          undef for automatic selection by kernel version
#                 0     if the kernel does not have the SCSI sg functions
#                 1     if the kernel has the functions
KFIOC_HAS_SCSI_SG_FNS()
{
    local test_flag="$1"
    local test_code='
#include <scsi/scsi_cmnd.h>
struct scatterlist;

void kfioc_has_scsi_sg_fns(void)
{
    struct scsi_cmnd *scmd = NULL;
    struct scatterlist *one = scsi_sglist(scmd);
    unsigned two = scsi_sg_count(scmd);
}
'

    kfioc_test "$test_code" "$test_flag" 1 -Werror-implicit-function-declaration
}

# flag:           KFIOC_HAS_SCSI_SG_COPY_FNS
# usage:          undef for automatic selection by kernel version
#                 0     if the kernel does not have the SCSI sg copy functions
#                 1     if the kernel has the functions
KFIOC_HAS_SCSI_SG_COPY_FNS()
{
    local test_flag="$1"
    local test_code='
#include <scsi/scsi_cmnd.h>

void kfioc_has_scsi_sg_copy_fns(void)
{
    struct scsi_cmnd *scmd = NULL;
    int one = scsi_sg_copy_from_buffer(scmd, NULL, 0);
    int two = scsi_sg_copy_to_buffer(scmd, NULL, 0);
}
'

    kfioc_test "$test_code" "$test_flag" 1 -Werror-implicit-function-declaration
}

# flag:           KFIOC_HAS_SCSI_RESID_FNS
# usage:          undef for automatic selection by kernel version
#                 0     if the kernel does not have the SCSI resid functions
#                 1     if the kernel has the functions
KFIOC_HAS_SCSI_RESID_FNS()
{
    local test_flag="$1"
    local test_code='
#include <scsi/scsi_cmnd.h>
struct scatterlist;

void kfioc_has_scsi_resid_fns(void)
{
    struct scsi_cmnd *scmd = NULL;
    scsi_set_resid(scmd, 0);
}
'

    kfioc_test "$test_code" "$test_flag" 1 -Werror-implicit-function-declaration
}

# flag:           KFIOC_HAS_PROCFS_PDE_DATA
# usage:          undef for automatic selection by kernel version
#                 0     if the kernel does not have the PDE_DATA helper
#                 1     if the kernel has the PDE_DATA function
# git commit:     d9dda78bad879595d8c4220a067fc029d6484a16
# kernel version: >= 3.10
KFIOC_HAS_PROCFS_PDE_DATA()
{
    local test_flag="$1"
    local test_code='
#include <linux/proc_fs.h>

void *kfioc_has_procfs_pde_data(struct inode *inode)
{
    return PDE_DATA(inode);
}
'
    kfioc_test "$test_code" "$test_flag" 1 -Werror-implicit-function-declaration
}

# flag:           KFIOC_HAS_PROC_CREATE_DATA
# usage:          undef for automatic selection by kernel version
#                 0     if the kernel does not have the proc_create_data function
#                 1     if the kernel has the function
# git commit:     59b7435149eab2dd06dd678742faff6049cb655f
KFIOC_HAS_PROC_CREATE_DATA()
{
    local test_flag="$1"
    local test_code='
#include <linux/proc_fs.h>

void *kfioc_has_proc_create_data(struct inode *inode)
{
    return proc_create_data(NULL, 0, NULL, NULL, NULL);
}
'
    kfioc_test "$test_code" "$test_flag" 1 -Werror-implicit-function-declaration
}

# flag:           KFIOC_ACPI_EVAL_INT_TAKES_UNSIGNED_LONG_LONG
# values:
#                 0     32-bit version of acpi_evaluate_integer present
#                 1     64-bit version of acpi_evaluate_integer present
# git commit:     27663c5855b10af9ec67bc7dfba001426ba21222
# kernel version: >= 2.6.27.15
KFIOC_ACPI_EVAL_INT_TAKES_UNSIGNED_LONG_LONG()
{
    local test_flag="$1"
    local test_code='
#include <linux/acpi.h>

void kfioc_acpi_eval_int_takes_unsigned_long_long(void)
{
    unsigned long long data = 0;
    acpi_evaluate_integer(NULL, NULL, NULL, &data);
}
'

    kfioc_test "$test_code" "$test_flag" 1 -Werror
}

# flag:           KFIOC_BIO_HAS_SEG_SIZE
# usage:          undef for automatic selection by kernel version
#                 0     if the kernel does not have bio seg_front/back_size
#                 1     if the kernel has structure elements
# kernel version: >= 2.6.27.15
KFIOC_BIO_HAS_SEG_SIZE()
{
    local test_flag="$1"
    local test_code='
#include <linux/bio.h>

void kfioc_test_bio_seg_size(void) {
	struct bio bio;
	bio.bi_seg_front_size=0;
	bio.bi_seg_back_size=0;
}
'
    kfioc_test "$test_code" "$test_flag" 1 -Werror-implicit-function-declaration
}

# flag:           KFIOC_HAS_FILE_INODE_HELPER
# usage:          undef for automatic selection by kernel version
#                 0     if the kernel does not have file_inode() helper
#                 1     if the kernel has file_inode() helper
KFIOC_HAS_FILE_INODE_HELPER()
{
    local test_flag="$1"
    local test_code='
#include <linux/fs.h>

void kfioc_test_file_inode(void) {
        struct file f;
        struct inode *test = file_inode(&f);
        (void)test;
}
'
    kfioc_test "$test_code" "$test_flag" 1 -Werror-implicit-function-declaration
}

# flag:            KFIOC_GET_USER_PAGES_HAS_GUP_FLAGS
# usage:           1 get_user_pages has a combined gup_flags parameter
#                  0 get_user_pages has separate write and force parameters
# git commit:      c164154f66f0c9b02673f07aa4f044f1d9c70274 <- changed API
#
# kernel version: v4.10
KFIOC_GET_USER_PAGES_HAS_GUP_FLAGS()
{
    local test_flag="$1"
    local result=0

    grep -A3 -E "(long|int) get_user_pages\(" "$KERNELSOURCEDIR/include/linux/mm.h" | grep "gup_flags" || result=$?
    result=$((! $result))

    set_kfioc_status "$test_flag" 0 exit
    set_kfioc_status "$test_flag" "$result" result
}

# flag:            KFIOC_BLK_MQ_OPS_HAS_MAP_QUEUES
# usage:           1 blk_mq_ops has a 'map_queues' field
#                  0 blk_mq_ops has the older 'map_queue' field
# git commit:      da695ba236b993f07a540d35c17f271ef08c89f3 <- added new API
#                  xxx <- removed old API
# kernel version: v4.10
KFIOC_BLK_MQ_OPS_HAS_MAP_QUEUES()
{
    local test_flag="$1"
    local test_code='
#include <linux/blk-mq.h>

void test_blk_map_queues(void)
{
    struct blk_mq_ops ops;

    ops.map_queues = NULL;
}
'
    kfioc_test "$test_code" "$test_flag" 1 -Werror
}


# flag:            KFIOC_HAS_PCI_ENABLE_MSIX_EXACT
# usage:           1 pci_enable_msix_exact() function exists
#                  0 Use the older pci_enable_msix() function.
# git commit:      kernel 4.8 added new API
#                  kernel 4.12 removed old function
# kernel version: v4.8 and 4.12
KFIOC_HAS_PCI_ENABLE_MSIX_EXACT()
{
    local test_flag="$1"
    local test_code='
#include <linux/pci.h>

void test_pcie_enable_msix_exact(struct pci_dev* pdev, struct msix_entry* msi)
{
    unsigned int nr_vecs = 1;

    pci_enable_msix_exact(pdev, msi, nr_vecs);
}
'
    kfioc_test "$test_code" "$test_flag" 1 -Werror
}

# flag:            KFIOC_HAS_BLK_QUEUE_SPLIT2
# usage:           1 blk_queue_split() with two parameters exists
#                  0 function does not exist, or is the older 3-parameter version.
# kernel version:  Added in 4.3 with three parameters, changed to 2 parameters in 4.13.
#                   This checks for the two-parameter version, since it appears that
#                   the kernel quit honoring segment limits in about 4.13, requiring us
#                   to have to split bios given to us with too many segments.
KFIOC_HAS_BLK_QUEUE_SPLIT2()
{
    local test_flag="$1"
    local test_code='
#include <linux/blkdev.h>

void test_has_blk_queue_split2(struct request_queue *rq, struct bio **bio)
{
    blk_queue_split(rq, bio);
}
'
    kfioc_test "$test_code" "$test_flag" 1 -Werror
}


###############################################################################

usage()
{
    local prog="$1"
    shift

    cat <<EOF
Usage: ${prog} [ -a <arch> ] [ -h ] [ -k <kernel_dir> ] [ -p ] [ -v ] -o <output_file>

-a <arch>            Architecture to build
-d <tmp_dir>         Temporary directory to use for config tests
-h                   Usage information (yep, you're reading it now)
-k <kernel_dir>      Kernel "build" directory or the "O" output directory
-o <output_file>     Output file for the config settings (REQUIRED)
-p                   Preserve temporary files in the temporary directory
-s <kernel_src_dir>  Kernel "source" directory if it is different then the "-k <kernel_dir>" directory
-v                   Verbose output
EOF
}

#
# Main code block
#
main()
{
    local rc=0
    local option=
    while getopts a:d:hk:o:ps:vl: option; do
        case $option in
            (a)
                FIOARCH="$OPTARG"
                ;;
            (d)
                CONFIGDIR="$OPTARG"
                ;;
            (h)
                usage "$0"
                exit $EX_OK
                ;;
            (k)
                KERNELDIR="$OPTARG"
                ;;
            (o)
                OUTPUTFILE="$OPTARG"
                ;;
            (p)
                PRESERVE=1
                ;;
            (s)
                KERNELSOURCEDIR="$OPTARG"
                ;;
            (v)
                VERBOSE=1
                ;;
            (l)
                FUSION_DEBUG="$OPTARG"
                ;;
        esac
    done
    shift $(($OPTIND - 1))

    if [ -z "$OUTPUTFILE" ]; then
        printf "ERROR: '-o' output file argument is required\n\n" >&2
        usage "$0" >&2
        exit $EX_USAGE
    fi

    trap sig_exit EXIT

    if [ -z "$CONFIGDIR" ]; then
        CREATED_CONFIGDIR=1
        : "${CONFIGDIR:=$(mktemp -t -d kfio_config.XXXXXXXX)}"
    elif ! [ -d "$CONFIGDIR" ]; then
        CREATED_CONFIGDIR=1
        mkdir -p "$CONFIGDIR"
    fi

    # This should be opened as soon as the CONFIGDIR is available
    open_log

    : "${TMP_OUTPUTFILE:=${CONFIGDIR}/kfio_config.h}"

    cat <<EOF
Detecting Kernel Flags
Config dir         : ${CONFIGDIR}
Output file        : ${OUTPUTFILE}
Kernel output dir  : ${KERNELDIR}
EOF

    # Check to make sure the kernel build directory exists
    if [ ! -d "$KERNELDIR" ]; then
        echo "Unable to locate the kernel build dir '$KERNELDIR'"
        echo "Please make sure your kernel development package is installed."
        exit $EX_OSFILE;
    fi

    # For some reason we used to unilaterally set KERNELSOURCEDIR to KERNELDIR. Maybe because they were the same in many
    #  cases and it didn't seem to matter, the way we used them.
    # With CRT-1114 however, we had to change the way we detect the API changes to get_user_pages(), which requires
    #  that the KERNELDIR point to the kernel build directory and the KERNELSOURCEDIR point to...well, the kernel source.
    # Attempt to correctly set the KERNELSOURCEDIR by checking for the existence of a file. (mm.h because, why not?).
    INCLUDEFILEPATH="include/linux/mm.h"
    if [ ! -f "$KERNELSOURCEDIR/$INCLUDEFILEPATH" ]; then
        KERNELSOURCEDIR=${KERNELDIR}
    fi

    cat <<EOF
Kernel source dir  : ${KERNELSOURCEDIR}
EOF
    # Check again to make sure the kernel souce directory and a file exists
    if [ ! -f "$KERNELSOURCEDIR/$INCLUDEFILEPATH" ]; then
        echo "Unable to locate the kernel source in '$KERNELSOURCEDIR'"
        echo "Please make sure your kernel development package is installed."
        exit $EX_OSFILE;
    fi

    . ./kfio_config_add.sh

    start_tests

    finished || rc=$?

    return $rc
}

if [ "$#" -gt 0 ]; then
    main "$@"
else
    main
fi
