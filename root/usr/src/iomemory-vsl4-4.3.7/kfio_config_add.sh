#!/bin/sh
#
# Extra tests that are used for compatibility with newer kernels.
# This file gets sourced by kfio_config.sh, to keep changes to a minimum
#
# TODO:
# FIX SPECIAL: KFIOC_X_REQUEST_QUEUE_HAS_SPECIAL
#
NCPUS=$(grep -c ^processor /proc/cpuinfo)
TEST_RATE=$(expr $NCPUS "*" 2)

KFIOC_TEST_LIST="${KFIOC_TEST_LIST}
KFIOC_X_REQUEST_QUEUE_HAS_SPECIAL
KFIOC_X_BIO_HAS_BIO_SEGMENTS
KFIOC_X_HAS_COARSE_REAL_TS
"

KFIOC_REMOVE_TESTS=""

for remove in $KFIOC_REMOVE_TESTS; do
    echo "Hardcode $remove result to 0"
    eval "${remove}() {
      set_kfioc_status $remove 0 exit
      set_kfioc_status $remove 0 result
    }"
done

##
# Override start_tests with out more efficient
##
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
        start_test $kfioc_test &
        KFIOC_PROCS="$KFIOC_PROCS $kfioc_test:$!"
        KFIOC_COUNT=$( pgrep -fc "kfio_config.sh -a" )
        while [ $KFIOC_COUNT -gt $TEST_RATE ]
        do
            sleep .01
            KFIOC_COUNT=$( pgrep -fc "kfio_config.sh -a" )
        done
    done

    printf "Started tests, waiting for completions...\n"

    # We want more time for ourselves than the child tests
    TIMEOUT_DELTA=$(($TIMEOUT_DELTA+$TIMEOUT_DELTA/2))
    update_timeout
}

# flag:           KFIOC_X_HAS_BLK_MQ_DELAY_QUEUE
# values:
#                 1     for kernels that have the blk_mq_delay_device() helper
#                 0     for kernels that do not
# git commit:
# comments:
KFIOC_X_HAS_BLK_MQ_DELAY_QUEUE()
{
    local test_flag="$1"
    local test_code='
#include <linux/blkdev.h>
#include <linux/blk-mq.h>

void kfioc_test_blk_mq_delay_queue(void)
{
    blk_mq_delay_queue(NULL, 0)
}
'

    kfioc_test "$test_code" "$test_flag" 1 -Werror-implicit-function-declaration
}


# flag:           KFIOC_X_REQUEST_QUEUE_HAS_SPECIAL
# values:
#                 0
#                 1    pre 5.3 kernels have special as part of the "do what you want"
# git commit:     NA
# comments:
# iomemory-vsl:   4
KFIOC_X_REQUEST_QUEUE_HAS_SPECIAL()
{
    local test_flag="$1"
    local test_code='
#include <linux/blkdev.h>

void kfioc_test_request_queue_has_queue_lock_pointer(void) {
    struct request_queue *q;
    void *x;
    x = q->special;
}
'

    kfioc_test "$test_code" "$test_flag" 1
}

# flag:           KFIOC_HAS_BLK_MQ
# usage:          undef for automatic selection by kernel version
#                 0     if the kernel doesn't support blk-mq
#                 1     if the kernel has blk-mq
# kernel version: v4.1-rc1
# A popular distro has backported blk-mq.h into 3.10.0 kernel in a way that does
# not work with the driver
## The original test is broken, and missing linux/version.h
KFIOC_HAS_BLK_MQ()
{
    local test_flag="$1"
    local test_code='
#include <linux/version.h>
#if LINUX_VERSION_CODE > KERNEL_VERSION(3, 13, 0)
#include <linux/blk-mq.h>

int kfioc_has_blk_mq(void)
{
    struct blk_mq_tag_set tag_set;
    tag_set.nr_hw_queues = 1;
    tag_set.cmd_size = 0;
    if (!blk_mq_alloc_tag_set(&tag_set))
    {
        blk_mq_init_queue(&tag_set);
    }
    blk_mq_run_hw_queues(NULL,0);
    return 1;
}
#else
#error blk-mq was added in 3.13.0 kernel
#endif
'
    kfioc_test "$test_code" "$test_flag" 1 -Werror-implicit-function-declaration
}

# flag:           KFIOC_X_BIO_HAS_BIO_SEGMENTS
# usage:          0     if kernel has no bio_segments
#                 1     if kernel has bio_segments
KFIOC_X_BIO_HAS_BIO_SEGMENTS()
{
    local test_flag="$1"
    local test_code='
#include <linux/bio.h>

void kfioc_test_bio_has_bio_segments(void) {
    struct bio *bio = NULL;
    unsigned segs;
    segs = bio_segments(bio);
}
'
    kfioc_test "$test_code" "$test_flag" 1 -Werror-implicit-function-declaration
}

# flag:            KFIOC_X_HAS_COARSE_REAL_TS
# usage:           1 kernel exports ktime_get_coarse_real_ts64()
#                  0 old kernel with current_kernel_time()
# kernel version:  Added in 4.18 to provide a 64 bit time interface
#                  commit: "timekeeping: Standardize on ktime_get_*() naming"
KFIOC_X_HAS_COARSE_REAL_TS()
{
    local test_flag="$1"
    local test_code='
#include <linux/timekeeping.h>

void test_has_coarse_real_ts(void)
{
    struct timespec64 ts;
    ktime_get_coarse_real_ts64(&ts);
}
'
    kfioc_test "$test_code" "$test_flag" 1
}
