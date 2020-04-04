//-----------------------------------------------------------------------------
// Copyright (c) 2006-2014, Fusion-io, Inc.(acquired by SanDisk Corp. 2014)
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

#ifndef __FIO_PORT_KPCI_H__
#define __FIO_PORT_KPCI_H__

#include <fio/port/ktypes.h>

#if !defined(USERSPACE_KERNEL)
# define __FIO_PORT_PCI_DEV_T_DEFINED
# if defined(UEFI)
#  include <fio/port/uefi/kpci.h>
# elif defined(__VMKAPI__)
#  include <fio/port/esxi6/kpci.h>
# elif defined(__linux__) || defined(__VMKLNX__)
#  include <fio/port/common-linux/kpci.h>
# elif defined(__FreeBSD__)
#  include <fio/port/freebsd/kpci.h>
# elif defined(__OSX__)
#  include <fio/port/osx/kpci.h>
# elif defined(__sun__)
#  include <fio/port/solaris/kpci.h>
# elif defined(WINNT) || defined(WIN32)
#  include <fio/port/windows/kpci.h>
# else
#  undef __FIO_PORT_PCI_DEV_T_DEFINED
# endif
#else
# undef __FIO_PORT_PCI_DEV_T_DEFINED
#endif

#ifndef __FIO_PORT_PCI_DEV_T_DEFINED
typedef void kfio_pci_dev_t;
#endif

extern int kfio_pci_enable_msi(kfio_pci_dev_t *pdev);
extern void kfio_pci_disable_msi(kfio_pci_dev_t *pdev);
extern const char *kfio_pci_name(kfio_pci_dev_t *pdev);

extern void *kfio_pci_get_drvdata(kfio_pci_dev_t *pdev);
extern void kfio_pci_set_drvdata(kfio_pci_dev_t *pdev, void *data);

extern void kfio_iodrive_intx(kfio_pci_dev_t *pci_dev, int enable);

extern int kfio_pci_read_config_byte(kfio_pci_dev_t *pdev, int where, uint8_t *val);
extern int kfio_pci_read_config_word(kfio_pci_dev_t *pdev, int where, uint16_t *val);
extern int kfio_pci_read_config_dword(kfio_pci_dev_t *pdev, int where, uint32_t *val);
extern int kfio_pci_write_config_byte(kfio_pci_dev_t *pdev, int where, uint8_t val);
extern int kfio_pci_write_config_word(kfio_pci_dev_t *pdev, int where, uint16_t val);
extern int kfio_pci_write_config_dword(kfio_pci_dev_t *pdev, int where, uint32_t val);

extern uint8_t kfio_pci_get_bus(kfio_pci_dev_t *pdev);
extern uint8_t kfio_pci_get_devicenum(kfio_pci_dev_t *pdev);
extern uint8_t kfio_pci_get_function(kfio_pci_dev_t *pdev);
extern uint8_t kfio_pci_get_slot(kfio_pci_dev_t *pdev);

extern uint16_t kfio_pci_get_vendor(kfio_pci_dev_t *pdev);
extern uint32_t kfio_pci_get_devnum(kfio_pci_dev_t *pdev);
extern uint16_t kfio_pci_get_subsystem_vendor(kfio_pci_dev_t *pdev);
extern uint16_t kfio_pci_get_subsystem_device(kfio_pci_dev_t *pdev);
extern uint16_t kfio_pci_get_domain(kfio_pci_dev_t *pdev);

extern uint64_t kfio_pci_resource_start(kfio_pci_dev_t *pdev, uint16_t bar);
extern uint32_t kfio_pci_resource_len(kfio_pci_dev_t *pdev, uint16_t bar);

struct kfio_pci_csr_handle
{
    void *csr_virt;
    void *csr_hdl;
    void *bar2_csr_virt;
};

extern int  kfio_pci_map_csr(kfio_pci_dev_t *pdev, const char *device_label, struct kfio_pci_csr_handle *csr);
extern void kfio_pci_unmap_csr(kfio_pci_dev_t *pdev, struct kfio_pci_csr_handle *csr);

// Accommodate per port BAR2 mapping
#define KFIO_PCI_BAR2  2
#ifndef kfio_pci_map_bar
static inline int kfio_pci_map_noop(void)
{
    return 0;
}
# define kfio_pci_map_bar(pci_dev, device_label, bar_virt, barnum) kfio_pci_map_noop()
#endif

#ifndef kfio_pci_unmap_bar
# define kfio_pci_unmap_bar(pci_dev, bar_virt) while(0) { }
#endif

/**
 * @struct fusion_dma_t
 * @brief placeholder struct that is large enough for the @e real OS-specific
 * structure:
 * Linux => dma_addr_t (size 4 for x86, size 8 for x64)
 * Solaris => struct of ddi_dma_handle_t & ddi_acc_handle_t
 * FreeBSD => struct of bus_dma_map_t (8 bytes) & bus_dmamap_t (8 bytes)
 * OSX     => IOBufferMemoryDescriptor * (8)
 * UEFI    => Mapping & EFI_PCI_IO_PROTOCOL
 */
struct fusion_dma_t
{
    uint64_t phys_addr;
    uint64_t _private[96/sizeof(uint64_t)];
};

extern void *kfio_dma_alloc_coherent(kfio_pci_dev_t *pdev, unsigned int size,
                                     struct fusion_dma_t *dma_handle);

extern void kfio_dma_free_coherent(kfio_pci_dev_t *pdev, unsigned int size,
                                   void *vaddr, struct fusion_dma_t *dma_handle);

#define KFIO_DMA_SYNC_FOR_DRIVER 1
#define KFIO_DMA_SYNC_FOR_DEVICE 2
extern int kfio_dma_sync(struct fusion_dma_t *dma_hdl, uint64_t offset, size_t length, unsigned type);

typedef int kfio_irqreturn_t;

// Warning: do not add or modify kfio_irqreturn_t values without consulting the
// 'Subtle code warning' in iodrive_process_interrupts()!!
#define FIO_IRQ_NONE        (0)
#define FIO_IRQ_HANDLED     (1)
#define FIO_IRQ_SKIPPED     (2) // Multiple cpus prepare handling the IRQ. Only one CPU handles it, others skip
#define FIO_IRQ_RETVAL(x)   ((x) != 0)

kfio_irqreturn_t iodrive_intr_fast(int irq, void *dev_id);
kfio_irqreturn_t iodrive_intr_fast_pipeline(int irq, void *dev_id);
kfio_irqreturn_t iodrive_hq_intr(int irq, void *dev_id);

void *iodrive_get_hw_comp_queue(kfio_pci_dev_t* pci_dev, int queue);
void iodrive_intr_hq_disable(void *hq);
int iodrive_is_work_pending_on_hq(void* hq);


#if !defined(WIN32) && !defined(WINNT)

extern int  kfio_request_irq(kfio_pci_dev_t *pdev, const char *devname,
                             void *iodrive_dev, int msi_enabled);
extern void kfio_free_irq(kfio_pci_dev_t *pdev, void *dev);

#endif /* !defined(WIN32) */


#if !defined(WIN32) && !defined(WINNT)

static inline void kfio_pci_device_failed(kfio_pci_dev_t *pdev)
{
}

#else

extern void kfio_pci_device_failed(kfio_pci_dev_t *pdev);

#endif

extern int  kfio_get_irq_number(kfio_pci_dev_t *pdev, uint32_t *irq);

#if defined(PORT_SUPPORTS_MSIX)

#if (!defined(WIN32) && !defined(WINNT)) || defined(UEFI)
extern int  kfio_request_msix(kfio_pci_dev_t *pdev, const char *devname, void *iodrive_dev,
                              kfio_msix_t *msix, unsigned int vector);
extern void kfio_free_msix(kfio_pci_dev_t *pdev, kfio_msix_t *msix, unsigned int vector, void *dev);
extern unsigned int kfio_pci_enable_msix(kfio_pci_dev_t *pdev, kfio_msix_t *msix, unsigned int nr_vecs);
extern void kfio_pci_disable_msix(kfio_pci_dev_t *pdev, kfio_msix_t *msix);
#endif
extern int  kfio_get_msix_number(kfio_pci_dev_t *pdev, kfio_msix_t *msix, uint32_t vec_ix, uint32_t *irq);

#else

typedef struct kfio_msix
{
    DECLARE_RESERVE(1);
} kfio_msix_t;

#if !defined(WIN32) && !defined(WINNT)
static inline int kfio_request_msix(kfio_pci_dev_t *pdev, const char *devname, void *iodrive_dev,
                                    kfio_msix_t *msix, unsigned int vector)
{
    return -EINVAL;
}

static inline int kfio_pci_enable_msix(kfio_pci_dev_t *pdev, kfio_msix_t *msix, unsigned int nr_vecs)
{
    return 0;
}

static inline void kfio_pci_disable_msix(kfio_pci_dev_t *pdev, kfio_msix_t *msix)
{
}

static inline void kfio_free_msix(kfio_pci_dev_t *pdev, kfio_msix_t *msix, unsigned int vector, void *dev)
{
}

#endif /* !defined(WIN32) */

static inline int kfio_get_msix_number(kfio_pci_dev_t *pdev, kfio_msix_t *msix, uint32_t vec_ix, uint32_t *irq)
{
    (void)pdev; (void)msix; (void)vec_ix; (void)irq;
    return -ENOENT;
}
#endif /* PORT_SUPPORTS_MSIX */

struct fusion_nand_device;

extern int  kfio_create_pipeline(kfio_pci_dev_t *pci_dev, struct fusion_nand_device *nand_dev,
                                 int pipeline, kfio_numa_node_t node);
extern void kfio_destroy_pipeline(kfio_pci_dev_t *pci_dev, int pipeline, void *context, int shutdown);

/* Core entry points. */
extern int  iodrive_pci_init_pre_interrupts_enabled(kfio_pci_dev_t *pci_dev);
extern int  iodrive_pci_init_post_interrupts_enabled(kfio_pci_dev_t *pci_dev);
extern void iodrive_pci_fini_pre_interrupts_disabled(kfio_pci_dev_t *pci_dev, int shutdown);
extern void iodrive_pci_fini_post_interrupts_disabled(kfio_pci_dev_t *pci_dev);
extern void iodrive_pci_intr_config(kfio_pci_dev_t *pci_dev, int legacy_mode);

extern int  iodrive_pci_attach(kfio_pci_dev_t *pci_dev);
extern void iodrive_pci_detach(kfio_pci_dev_t *pci_dev);
extern void iodrive_pci_shutdown(kfio_pci_dev_t *pci_dev);

/* All-in-one pipeline creation helper for simple ports. */
extern int  iodrive_pci_create_pipeline(kfio_pci_dev_t *pci_dev, int pipeline, void *context, void **port_cdev);
extern void iodrive_pci_destroy_pipeline(kfio_pci_dev_t *pci_dev, int pipeline, int shutdown);
extern void *iodrive_pci_get_pipeline_device(kfio_pci_dev_t *pci_dev);

#if PORT_SUPPORTS_DEVFS_HANDLE
extern void *kfio_get_pipeline_devfs_handle(kfio_pci_dev_t *pci_dev, int pipeline);
#else
static inline void *kfio_get_pipeline_devfs_handle(kfio_pci_dev_t *pci_dev, int pipeline)
{
    return pci_dev;
}
#endif

#ifdef PORT_SUPPORTS_BUS_EXPLORE
/* Functions to explore the PCI bus hierarchy. */
extern kfio_pci_bus_t *kfio_bus_from_pci_dev(kfio_pci_dev_t *pdev);
extern kfio_pci_bus_t *kfio_pci_bus_parent(kfio_pci_bus_t *bus);
extern int kfio_pci_bus_istop(kfio_pci_bus_t *bus);
extern kfio_pci_dev_t *kfio_pci_bus_self(kfio_pci_bus_t *bus);
extern uint8_t kfio_pci_bus_number(kfio_pci_bus_t *bus);
#endif

#if  PORT_SUPPORTS_PCI_NUMA_INFO
extern kfio_numa_node_t kfio_pci_get_node(kfio_pci_dev_t *pdev);
extern void kfio_pci_set_node(kfio_pci_dev_t *pdev, kfio_numa_node_t node);
#else
static inline kfio_numa_node_t kfio_pci_get_node(kfio_pci_dev_t *pdev)
{
    return 0;
}
static inline void kfio_pci_set_node(kfio_pci_dev_t *pdev, kfio_numa_node_t node)
{
    return;
}
#endif

#if PORT_SUPPORTS_NUMA_NODE_OVERRIDE
extern int kfio_get_numa_node_override(kfio_pci_dev_t *pdev, const char *name,
                                       kfio_numa_node_t *nodep);
#else
static inline int kfio_get_numa_node_override(kfio_pci_dev_t *pdev, const char *name,
                                              kfio_numa_node_t *nodep)
{
    return -ENOENT;
}
#endif
extern uint64_t kfio_pci_get_bar_size(void);

#endif /* __FIO_PORT_KPCI_H__ */
