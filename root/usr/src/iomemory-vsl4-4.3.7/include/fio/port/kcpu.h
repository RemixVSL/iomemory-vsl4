//-----------------------------------------------------------------------------
// Copyright (c) 2011-2014, Fusion-io, Inc.(acquired by SanDisk Corp. 2014)
// Copyright (c) 2014-2016 SanDisk Corp. and/or all its affiliates. All rights reserved.
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

#ifndef __FIO_PORT_KTYPES_H__
#error "Please include <fio/port/ktypes.h> before this file."
#endif

#ifndef __FIO_PORT_KCPU_H__
#define __FIO_PORT_KCPU_H__

#include <fio/port/errno.h>
#include <fio/port/kpci.h>

/**
 * @brief CPU topology map
 * @note  This structure is used to map CPUs to NUMA nodes. It
 *        provides an OS agnostic map from CPU to NUMA node.
 */
struct kfio_cpu_topology {
    unsigned int numnodes;          ///< Number of NUMA nodes
    unsigned int numcpus;           ///< Maximum number of CPUs
    kfio_numa_node_t *cpu_to_node;  ///< Mapping of CPU to NUMA node
};

struct hw_comp_queue;
typedef void (kfio_cpu_notify_fn)(int new_cpu_flag, kfio_cpu_t cpu);

#if PORT_SUPPORTS_PER_CPU

extern kfio_cpu_t kfio_current_cpu(void);
extern kfio_cpu_t kfio_get_cpu(kfio_get_cpu_t *flags);
extern void kfio_put_cpu(kfio_get_cpu_t *flags);
extern unsigned int kfio_max_cpus(void);
extern int kfio_cpu_online(kfio_cpu_t cpu);
extern kfio_numa_node_t kfio_cpu_to_node(kfio_cpu_t cpu);

extern int kfio_create_kthread_on_cpu(fusion_kthread_func_t func, void *data,
                                      void *fusion_nand_device, kfio_cpu_t cpu,
                                      const char *fmt, ...);

extern void kfio_unregister_cpu_notifier(kfio_cpu_notify_fn *func);
extern int kfio_register_cpu_notifier(kfio_cpu_notify_fn *func);
extern int kfio_fill_cpu_topology(struct kfio_cpu_topology *);
extern void kfio_free_cpu_topology(struct kfio_cpu_topology *);
extern void kfio_bind_kthread_to_node(kfio_numa_node_t node);

extern int kfio_map_cpus_to_read_queues(struct hw_comp_queue *read_queues,
                                        uint32_t read_queue_count,
                                        size_t struct_hw_comp_queue_size,
                                        const struct kfio_cpu_topology *cpu_topology,
                                        kfio_pci_dev_t *pci_dev,
                                        struct hw_comp_queue **cpu_to_read_queue);

#else

static inline kfio_cpu_t kfio_current_cpu(void)
{
    return 0;
}

static inline kfio_cpu_t kfio_get_cpu(kfio_get_cpu_t *flags)
{
    return kfio_current_cpu();
}

static inline void kfio_put_cpu(kfio_get_cpu_t *flags)
{
}

static inline unsigned int kfio_max_cpus(void)
{
    return 1;
}

static inline int kfio_cpu_online(kfio_cpu_t cpu)
{
    return cpu == kfio_current_cpu() ? 1 : 0;
}

static inline kfio_numa_node_t kfio_cpu_to_node(kfio_cpu_t cpu)
{
    return FIO_NUMA_NODE_NONE;
}

#define kfio_create_kthread_on_cpu(func, data, fusion_nand_device, cpu, fmt, ...) \
    fusion_create_kthread(func, data, fusion_nand_device, fmt, ##__VA_ARGS__)

static inline void kfio_unregister_cpu_notifier(kfio_cpu_notify_fn *func)
{
}

static inline int kfio_register_cpu_notifier(kfio_cpu_notify_fn *func)
{
    return 0;
}

static inline int kfio_fill_cpu_topology(struct kfio_cpu_topology *top)
{
    top->numnodes = top->numcpus = 1;
    top->cpu_to_node = NULL;
    return 0;
}

static inline void kfio_free_cpu_topology(struct kfio_cpu_topology *top)
{
    (void)top;
}

static inline void kfio_bind_kthread_to_node(kfio_numa_node_t node)
{
}

static inline int kfio_map_cpus_to_read_queues(struct hw_comp_queue *read_queues,
                                               uint32_t read_queue_count,
                                               size_t struct_hw_comp_queue_size,
                                               const struct kfio_cpu_topology *cpu_topology,
                                               kfio_pci_dev_t *pci_dev,
                                               struct hw_comp_queue **cpu_to_read_queue)
{
    return 0;
}

#endif

#define IODRIVE_MAX_PIPELINES 1
#define IODRIVE_MAX_ADMIN_QUEUES 1
#define IODRIVE_MAX_WRITE_QUEUES 1
#define IODRIVE_MAX_COMP_QUEUES 16

#endif
