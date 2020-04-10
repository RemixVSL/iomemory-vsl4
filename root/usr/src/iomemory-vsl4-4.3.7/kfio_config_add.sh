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
KFIOC_X_HAS_BLK_MQ_DELAY_QUEUE
KFIOC_X_HAS_BLK_STOP_QUEUE
KFIOC_X_REQUEST_QUEUE_HAS_SPECIAL
KFIOC_X_REQUEST_QUEUE_HAS_QUEUEDATA
KFIOC_X_REQUEST_QUEUE_HAS_QUEUE_LOCK_POINTER
KFIOC_X_PART_STAT_REQUIRES_CPU
KFIOC_X_TASK_HAS_CPUS_ALLOWED
KFIOC_X_BIO_HAS_BIO_SEGMENTS
KFIOC_X_BIO_HAS_BI_PHYS_SEGMENTS
KFIOC_X_HAS_BLK_QUEUE_FLAG_OPS
KFIOC_X_BIO_HAS_ERROR
KFIOC_X_REQ_HAS_ERRORS
KFIOC_X_REQ_HAS_ERROR_COUNT
KFIOC_X_BOUNCE_H
KFIOC_X_HAS_TIMER_SETUP
KFIOC_X_HAS_DISK_STATS_NSECS
KFIOC_X_HAS_COARSE_REAL_TS
KFIOC_X_HAS_ELEVATOR_INIT
KFIOC_X_PART0_HAS_IN_FLIGHT
"
KFIOC_REMOVE_TESTS="KFIOC_BIO_HAS_SPECIAL
KFIOC_HAS_BLK_DELAY_QUEUE
KFIOC_USE_IO_SCHED
KFIOC_X_TASK_HAS_CPUS_ALLOWED
KFIOC_X_HAS_BLK_STOP_QUEUE
KFIOC_MAKE_REQUEST_FN_VOID
KFIOC_X_REQUEST_QUEUE_HAS_QUEUE_LOCK_POINTER
KFIOC_BIOSET_CREATE_HAS_THIRD_ARG
KFIOC_HAS_REQ_UNPLUG
KFIOC_X_HAS_ELEVATOR_INIT
KFIOC_CONFIG_PREEMPT_RT
KFIOC_HAS_BIO_RW_FLAGGED
KFIOC_HAS_ELV_DEQUEUE_REQUEST
KFIOC_MISSING_WORK_FUNC_T
KFIOC_CONFIG_TREE_PREEMPT_RCU
KFIOC_HAS_ELEVATOR_INIT_EXIT
KFIOC_KBLOCKD_SCHEDULE_HAS_QUEUE_ARG
KFIOC_BLOCK_DEVICE_RELEASE_RETURNS_INT
KFIOC_NEEDS_VIRT_TO_PHYS
#IS_IMPLICIT_FROM_4_20
KFIOC_REQUEST_QUEUE_UNPLUG_FN_HAS_EXTRA_BOOL_PARAM
KFIOC_REQUEST_HAS_CMD_TYPE
KFIOC_X_BOUNCE_H
KFIOC_X_HAS_BLK_MQ_DELAY_QUEUE
KFIOC_X_BIO_HAS_BI_PHYS_SEGMENTS
KFIOC_HAS_INFLIGHT_RW
KFIOC_HAS_INFLIGHT_RW_ATOMIC
KFIOC_BIO_ENDIO_HAS_BYTES_DONE
#NO_SUCH_THING_DUDE
KFIOC_BIO_ENDIO_REMOVED_ERROR
KFIOC_BIO_ERROR_CHANGED_TO_STATUS
KFIOC_X_REQ_HAS_ERRORS
KFIOC_BIO_HAS_DESTRUCTOR
KFIOC_TASK_HAS_NR_CPUS_ALLOWED_RT
KFIOC_TASK_HAS_NR_CPUS_ALLOWED_DIRECT
KFIOC_HAS_COMPAT_IOCTL_METHOD
"

for remove in KFIOC_REMOVE_TESTS; do
    echo "remove $remove"
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
            sleep .1
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

# flag:          KFIOC_X_HAS_BLK_FS_REQUEST
# usage:         1   Kernel has obsolete blk_fs_request macro
#                0   It does not
# kernel version 2.6.36 removed macro.
KFIOC_X_HAS_BLK_STOP_QUEUE()
{
    local test_flag="$1"
    local test_code='
#include <linux/blkdev.h>
int kfioc_has_blk_stop_queue(struct request_queue *rq)
{
    return blk_stop_queue(rq);
}
'
    kfioc_test "$test_code" "$test_flag" 1 -Werror
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


# flag:           KFIOC_X_REQUEST_QUEUE_HAS_QUEUEDATA
# values:
#                 0
#                 1     5.3 kernels have queuedata as part of the "do what you want"
# git commit:     NA
# comments:
# iomemory-vsl:   4
KFIOC_X_REQUEST_QUEUE_HAS_QUEUEDATA()
{
    local test_flag="$1"
    local test_code='
#include <linux/blkdev.h>

void kfioc_test_request_queue_has_queue_lock_pointer(void) {
    struct request_queue *q;
    void *x;
    x = q->queuedata;
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

# flag:           KFIOC_X_REQUEST_QUEUE_HAS_QUEUE_LOCK_POINTER
# values:
#                 0     starting 5.0 the request_queue has a spinlock_t for queue_lock
#                 1     pre 5.0 kernels have a spinlock_t *queue_lock in request_queue.
# git commit:     NA
# comments:
KFIOC_X_REQUEST_QUEUE_HAS_QUEUE_LOCK_POINTER()
{
    local test_flag="$1"
    local test_code='
#include <linux/blkdev.h>

void kfioc_test_request_queue_has_queue_lock_pointer(void) {
    spinlock_t *l;
    struct request_queue *q;
    q->queue_lock = l;
}
'

    kfioc_test "$test_code" "$test_flag" 1
}

# flag:           KFIOC_X_PART_STAT_REQUIRES_CPU
# values:
#                 0     newer kernels don't need the cpu for stats
#                 1     older kernels need the cpu for stats
# git commit:
# comments:       in newer
KFIOC_X_PART_STAT_REQUIRES_CPU()
{
    local test_flag="$1"
    local test_code='
#include <linux/genhd.h>
struct gendisk *gd;
void kfioc_test_part_stat_requires_cpu(void) {
  part_stat_inc(1, &gd->part0, ios[0]);
}
'

    kfioc_test "$test_code" "$test_flag" 1
}

# flag:           KFIOC_REQUEST_QUEUE_HAS_REQUEST_FN
# values:
#                 0     for newer kernels that don't have request_fn member in struct request_queue.
#                 1     for older kernels that have request_fn member in struct request_queue.
# git commit:     NA
# comments:
KFIOC_X_REQUEST_QUEUE_HAS_REQUEST_FN()
{
    local test_flag="$1"
    local test_code='
#include <linux/blkdev.h>

void kfioc_test_request_queue_request_fn(void) {
    struct request_queue q = { .request_fn = NULL };
    (void)q;
}
'

    kfioc_test "$test_code" "$test_flag" 1
}

# flag:          KFIOC_X_TASK_HAS_CPUS_ALLOWED
# usage:         1   Task struct has CPUs allowed as mask
#                0   It does not
KFIOC_X_TASK_HAS_CPUS_ALLOWED()
{
    local test_flag="$1"
    local test_code='
#include <linux/sched.h>
void kfioc_check_task_has_cpus_allowed(void)
{
    cpumask_t *cpu_mask = NULL;
    struct task_struct *tsk = NULL;
    tsk->cpus_allowed = *cpu_mask;
}
'
    kfioc_test "$test_code" "$test_flag" 1 -Werror
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

# flag:           KFIOC_BIO_HAS_BI_PHYS_SEGMENTS
# usage:          0     if bio has no bi_phys_segments
#                 1     if bio has bi_phys_segments
KFIOC_X_BIO_HAS_BI_PHYS_SEGMENTS()
{
    local test_flag="$1"
    local test_code='
#include <linux/bio.h>

void kfioc_test_bio_has_bi_phys_segments(void) {
	struct bio bio;
	void *test = &(bio.bi_phys_segments);
}
'
    kfioc_test "$test_code" "$test_flag" 1 -Werror-implicit-function-declaration
}

# flag:          KFIOC_X_HAS_BLK_QUEUE_FLAG_OPS
# usage:         1   request queue limits structure has 'queue_flag_clear_unlocked' function.
#                0   It does not
KFIOC_X_HAS_BLK_QUEUE_FLAG_OPS()
{
    local test_flag="$1"
    local test_code='
#include <linux/blkdev.h>
void has_queue_blk_queue_flag_ops(void)
{
     struct request_queue q;
     blk_queue_flag_clear(0, &q);
}
'
    kfioc_test "$test_code" "$test_flag" 1 "-Werror -Werror=frame-larger-than=4096"
}

# flag:           KFIOC_X_BIO_HAS_ERROR
# usage:          1 if bio.bi_error does not exist, 0 if instead
# git commit:
# kernel version: v4.14-rc4
KFIOC_X_BIO_HAS_ERROR()
{
    local test_flag="$1"
    local test_code='
#include <linux/bio.h>

void kfioc_bio_has_error(void) {
    struct bio test_bio;
    (void) test_bio.error;
}
'
    kfioc_test "$test_code" "$test_flag" 1
}

# flag:           KFIOC_X_REQ_HAS_ERRORS
# usage:          1 if req.errors does not exist, 0 if instead
# git commit:
# kernel version: v4.10
KFIOC_X_REQ_HAS_ERRORS()
{
    local test_flag="$1"
    local test_code='
#include <linux/blkdev.h>

void kfioc_req_has_errors(void) {
    struct request *req;
    req->errors = 1;
}
'
    kfioc_test "$test_code" "$test_flag" 1
}

# flag:           KFIOC_X_REQ_HAS_ERROR_COUNT
# usage:          1 if req.error_count does not exist, 0 if instead
# git commit:
# kernel version: v4.14-rc4
KFIOC_X_REQ_HAS_ERROR_COUNT()
{
    local test_flag="$1"
    local test_code='
#include <linux/blkdev.h>

void kfioc_req_has_error_count(void) {
    struct request *req;
    req->error_count = 1;
}
'
    kfioc_test "$test_code" "$test_flag" 1
}

# flag:           KFIOC_X_BOUNCE_H
# usage:          1 if no bounce.h, 0 has bounce.h
#                 bounce was seperated out from highmem
# git commit:
# kernel version: v4.14-rc4
KFIOC_X_BOUNCE_H()
{
    local test_flag="$1"
    local test_code='
#include <linux/bounce.h>

void kfioc_bio_has_error(void) {
    struct bio test_bio;
    (void) test_bio.error;
}
'
    kfioc_test "$test_code" "$test_flag" 1
}

# flag:           KFIOC_X_HAS_TIMER_SETUP
# usage:          1   linux/time.h has `timer_setup((struct timer_list *)  timer, fusion_timer_callback, 0)
#                 0   not timer_setup, use function and data of timer instead
# git commit:
# kernel version: v4.15
# iomemory-vsl:   3
KFIOC_X_HAS_TIMER_SETUP() {
    local test_flag="$1"
    local test_code='
#include <linux/time.h>

static void timer_callback(struct timer_list *t) {
}

void kfioc_has_timer_setup(void) {
    struct timer_list *timer;
    timer_setup((struct timer_list *)  timer, timer_callback, 0);
}
'
    kfioc_test "$test_code" "$test_flag" 1

}

# flag:            KFIOC_X_HAS_DISK_STATS_NSECS
# usage:           1 struct disk_stats has nsecs member
#                  0 struct is still uses ticks member
# kernel version:  Added in 4.19 to log disk stats with nanoseconds
#                  commit "block: use nanosecond resolution for iostat"
KFIOC_X_HAS_DISK_STATS_NSECS()
{
    local test_flag="$1"
    local test_code='
#include <linux/genhd.h>

void test_has_disk_stats_nsecs(void)
{
    struct disk_stats stat = { .nsecs = 0 };
    (void)stat;
}
'
    kfioc_test "$test_code" "$test_flag" 1
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

# flag:            KFIOC_X_HAS_ELEVATOR_INIT
# usage:           1 kernel has 2 parameter elevator_init()
#                  0 newer kernel without that function
# kernel version:  Symbol export removed in 4.18. Since then only internal
#                  commit: "block: unexport elevator_init/exit"
KFIOC_X_HAS_ELEVATOR_INIT()
{
    local test_flag="$1"
    local test_code='
#include <linux/elevator.h>

test_has_elevator_init(void)
{
    struct request_queue *q;
    elevator_init(q, "noop");
}
'
    kfioc_test "$test_code" "$test_flag" 1
}

# flag:           KFIOC_X_PART0_HAS_IN_FLIGHT
# values:
#                 0     beyond 5 the struct has no in_flight
#                 1     older kernels do have in_flight
# git commit:
# comments:       yada
KFIOC_X_PART0_HAS_IN_FLIGHT()
{
    local test_flag="$1"
    local test_code='
#include <linux/genhd.h>
struct gendisk *gd;
void kfioc_test_part0_has_in_flight(void) {
  return gd->part0.in_flight;
}
'

    kfioc_test "$test_code" "$test_flag" 1
}
