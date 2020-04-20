//-----------------------------------------------------------------------------
// Copyright (c) 2006-2014, Fusion-io, Inc.(acquired by SanDisk Corp. 2014)
// Copyright (c) 2014-2016 SanDisk Corp. and/or all its affiliates. (acquired by Western Digital Corp. 2016)
// Copyright (c) 2016-2017 Western Digital Technologies, Inc. All rights reserved.
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

#if !defined (__linux__)
#error This file supports linux only
#endif

#include <linux/types.h>
#include <linux/module.h>
#include <fio/port/dbgset.h>
#include <fio/port/ktypes.h>
#include <fio/port/kcpu.h>
#include <linux/stddef.h>
#include <linux/cpumask.h>
#include <linux/kthread.h>

#include <fio/port/kfio_config.h>
#include <linux/kallsyms.h>

/**
 * @ingroup PORT_COMMON_LINUX
 * @{
 */

#if PORT_SUPPORTS_PER_CPU

/**
 *  @brief returns current CPU.
 *  @note Some core code assumes that returning a non-zero CPU number implies support for
 *    per CPU completions.
 */
kfio_cpu_t kfio_current_cpu(void)
{
    return raw_smp_processor_id();
}

kfio_cpu_t kfio_get_cpu(kfio_get_cpu_t *flags)
{
    local_irq_save(*flags);
    return smp_processor_id();
}

void kfio_put_cpu(kfio_get_cpu_t *flags)
{
    local_irq_restore(*flags);
}

/**
 *  @brief returns maximum number of CPUs.
 *  @note Some code assumes a return value greater than 1 implies support for per
 *    CPU completions.
 */
unsigned int kfio_max_cpus(void)
{
    return num_possible_cpus();
}

/**
 *  @brief returns nonzero if indicated CPU is online.
 */
int kfio_cpu_online(kfio_cpu_t cpu)
{
    return cpu_online(cpu);
}

/**
*  @brief returns numa node this cpu is on
*/
kfio_numa_node_t kfio_cpu_to_node(kfio_cpu_t cpu)
{
    return cpu_to_node(cpu);
}

int kfio_create_kthread_on_cpu(fusion_kthread_func_t func, void *data,
                               void *fusion_nand_device, kfio_cpu_t cpu,
                               const char *fmt, ...)
{
    struct task_struct *task;
    va_list ap;
    char buffer[40];
    int rc = 0;

    va_start(ap, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, ap);
    va_end(ap);

    task = kthread_create(func, data, "%s", buffer);
    if (IS_ERR(task))
    {
        rc = PTR_ERR(task);
        return rc;
    }

    kthread_bind(task, cpu);
    wake_up_process(task);

    return rc;
}

#if KFIOC_NUMA_MAPS
static void __kfio_bind_task_to_cpumask(struct task_struct *tsk, cpumask_t *mask)
{
    tsk->cpus_mask = *mask;
    tsk->nr_cpus_allowed = cpumask_weight(mask);
}
#endif

/*
 * Will take effect on next schedule event
 */
void kfio_bind_kthread_to_node(kfio_numa_node_t node)
{
#if KFIOC_NUMA_MAPS
    if (node != FIO_NUMA_NODE_NONE)
    {
        cpumask_t *cpumask = (cpumask_t *) cpumask_of_node(node);

        if (cpumask && cpumask_weight(cpumask))
            __kfio_bind_task_to_cpumask(current, cpumask);
    }
#endif
}

int kfio_fill_cpu_topology(struct kfio_cpu_topology *top)
{
#if defined(ENABLE_TOPO_DEFINES) || !defined(CONFIG_XEN)
    unsigned int node, cpu;

    top->numnodes = num_possible_nodes();
    top->numcpus = kfio_max_cpus();

    top->cpu_to_node = kfio_malloc(sizeof(kfio_cpu_t) * top->numcpus);
    if (!top->cpu_to_node)
    {
        return -ENOMEM;
    }

    for_each_possible_cpu(cpu)
    {
        node = cpu_to_node(cpu);
        top->cpu_to_node[cpu] = node;
    }
#else
    top->numnodes = num_possible_nodes();
    top->numcpus = kfio_max_cpus();

    top->cpu_to_node = NULL;
#endif
    return 0;
}

void kfio_free_cpu_topology(struct kfio_cpu_topology *top)
{
    kfio_free(top->cpu_to_node, sizeof(kfio_cpu_t) * top->numcpus);
}

#define SET_MAP(cpu_ix, queue_ix) \
    cpu_to_read_queue[cpu_ix] = \
      (struct hw_comp_queue *)(((uint8_t *)read_queues) + struct_hw_comp_queue_size * (queue_ix));

/// @brief map the available read queues to CPUs.
///
/// This function fills in the cpu_to_read_queue array.
///
/// To keep the definition of struct hw_comp_queue opaque but be able to
/// operate on the read_queues array, sizeof the structure is passed in and the
/// above SET_MAP macro is used to update the array.
///
/// @param[in]  read_queues                array of available read queues.
/// @param      read_queue_count           number of elements in read_queues.
/// @param      struct_hw_comp_queue_size  sizeof(struct hw_comp_queue)
/// @param[in]  cpu_topology               NUMA topology.
/// @param[in]  pci_dev                    pci device the mapping is for
/// @param[out] cpu_to_read_queue          map of CPU to read queues.
///
/// @return number of read queues in use, negative on error.
int kfio_map_cpus_to_read_queues(struct hw_comp_queue *read_queues,
                                 uint32_t read_queue_count,
                                 size_t struct_hw_comp_queue_size,
                                 const struct kfio_cpu_topology *cpu_topology,
                                 kfio_pci_dev_t *pci_dev,
                                 struct hw_comp_queue **cpu_to_read_queue)
{
    const uint32_t num_nodes = num_online_nodes();
    const uint32_t nodes_possible = num_possible_nodes();
    uint32_t node_ndx, node_counter;
    uint32_t node_hist[nodes_possible], node_map[nodes_possible];
    kfio_cpu_t cpu;

    dbgprint(DBGS_MULTQ,
             "%s: read_queues:%p read_queue_count:%u struct_hw_comp_queue_size:%lu\n",
             __func__, read_queues, read_queue_count, struct_hw_comp_queue_size);
    dbgprint(DBGS_MULTQ,
             "%s: cpu_topology:%p pci_dev:%p cpu_to_read_queue:%p\n",
             __func__, cpu_topology, pci_dev, cpu_to_read_queue);
    dbgprint(DBGS_MULTQ, "%s: num_online_cpus:%u num_online_nodes:%u\n",
             __func__, num_online_cpus(), num_online_nodes());


    kfio_memset(node_hist, 0, sizeof(uint32_t)*nodes_possible);
    for (cpu = 0; cpu < cpu_topology->numcpus; ++cpu)
    {
        node_hist[cpu_to_node(cpu)]++;
    }

#if FUSION_DEBUG
    for (node_ndx=0; node_ndx<nodes_possible; node_ndx++)
    {
        dbgprint(DBGS_MULTQ, "%s: CPU Node Histogram: %4u : %4u\n",
                 __func__, node_ndx, node_hist[node_ndx]);
    }
#endif

    // Now generate a map of used nodes.  This is to properly accomodate
    // 'missing' nodes which take up space in the NUMA topology, but would mess
    // up our queue allocation if we didn't account for it.
    //
    // NB: It might be useful to initialize this map to -1 (or some other flag
    // value) and test and abort later if we end up with an unmapped CPU.
    // Or perhaps just restart the whole exercise?
    node_counter=0;
    kfio_memset(node_map, 0, sizeof(uint32_t)*nodes_possible);
    for (node_ndx=0; node_ndx<nodes_possible; node_ndx++)
    {
        if (node_hist[node_ndx] != 0)
        {
            node_map[node_ndx] = node_counter;

            // find the next node that has entries
            while (node_hist[++node_counter] == 0 && node_counter < num_nodes)
                ;
        }

        dbgprint(DBGS_MULTQ, "%s: CPU Node Map: %4u : %4u\n",
                 __func__, node_ndx, node_map[node_ndx]);
    }

    /*
     * If we have enough queues to have one per CPU, lets do that.
     */
    if (read_queue_count >= num_online_cpus())
    {
        uint32_t ix = 0;
        uint32_t queues_used = 0;

        for (cpu = 0; cpu < cpu_topology->numcpus; ++cpu)
        {
            SET_MAP(cpu, ix);
            if (cpu_online(cpu))
            {
                // If a cpu came online since we called num_online_cpus(), we
                // might end up finding more CPUs than queues here. In this
                // very unlikely case, just wrap and assign two CPUs to
                // existing queues. Sub-optimal, but this is a very rare case.
                //
                // Currently offline CPUs share a queue with their nearest
                // online neighbor, just in case they come online before we
                // have a change to rebalance.
                ix = (ix + 1) % read_queue_count;
                queues_used++;
            }
        }

        if (queues_used < read_queue_count)
        {
            return (int)queues_used;
        }
        return (int)read_queue_count;
    }
    else
    {
        // We don't have enough queues for all CPUs. We want CPUs which share a
        // queue to be on the same NUMA node, as much as possible.
        //
        // However, it is (theoretically) possible for there to be more NUMA
        // nodes that queue as well, in which case we'll just share a queue
        // between groups of NUMA nodes.
        //
        // So: we organize the NUMA nodes into "clusters" (groups of NUMA
        // nodes) such that we have at least one queue available per "cluster".
        // Then, if we have more than one queue per cluster available (as will
        // happen if we have a small number of NUMA nodes) we round-robin
        // assignment pf the CPUs in the node to the set of queues assigned to
        // the node.
        uint32_t cpus_in_cluster[IODRIVE_MAX_COMP_QUEUES] = {0};
        uint32_t queues_per_cluster = 1;
        uint32_t nodes_per_cluster = 1;
        uint32_t cluster_count = num_nodes;

        if (read_queue_count < num_nodes)
        {
            // Don't have one queue per NUMA node available. Start grouping NUMA
            // nodes into clusters until we have one queue per cluster.
            while (cluster_count > read_queue_count)
            {
                nodes_per_cluster++;
                cluster_count = (num_nodes + nodes_per_cluster - 1) / nodes_per_cluster;
            }
        }
        else
        {
            // We have at least one queue per NUMA node, maybe more.
            queues_per_cluster = read_queue_count / num_nodes;
        }

        dbgprint(DBGS_MULTQ,
                 "MultiQ: configuring for %u clusters of %u nodes each, %u queues per cluster.\n",
                cluster_count, nodes_per_cluster, queues_per_cluster);

        for (cpu = 0; cpu < cpu_topology->numcpus; ++cpu)
        {
            uint32_t node = cpu_to_node(cpu);
            uint32_t cluster;
            uint32_t read_q;

            cluster = node_map[node] / nodes_per_cluster;

            kassert(cluster < IODRIVE_MAX_COMP_QUEUES);

            read_q = (cluster * queues_per_cluster) + (cpus_in_cluster[cluster] % queues_per_cluster);

            dbgprint(DBGS_MULTQ,
                     "%s: cpu:%u node:%u cluster:%u per_cluster:%u read_q:%u\n",
                      __func__, cpu, node, cluster, cpus_in_cluster[cluster], read_q);

            SET_MAP(cpu, read_q);
            if (cpu_online(cpu))
            {
                cpus_in_cluster[cluster]++;
            }
        }

        return (int)(cluster_count * queues_per_cluster);
    }
}

#endif // PORT_SUPPORTS_PER_CPU

/**
 * @}
 */
