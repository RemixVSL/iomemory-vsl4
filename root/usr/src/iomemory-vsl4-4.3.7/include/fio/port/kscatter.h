//-----------------------------------------------------------------------------
// Copyright (c) 2006-2014, Fusion-io, Inc.(acquired by SanDisk Corp. 2014)
// Copyright (c) 2014 SanDisk Corp. and/or all its affiliates. All rights reserved.
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

/** @file
 *     NO OS-SPECIFIC REFERENCES ARE TO BE IN THIS FILE
 *
 */

#ifndef __FIO_PORT_KTYPES_H__
#error Please include <fio/port/ktypes.h> before this file
#endif
#ifndef __FIO_PORT_KSCATTER_H__
#define __FIO_PORT_KSCATTER_H__

#include <fio/port/kmem.h>
#include <fio/port/kpci.h>

/// @defgroup kfio_sglist Scatter Gather List
/// @{
///
/// ::kfio_sg_list_t contains a port-specific set of elements needed to track
/// the mappings within the sg list.  The sg list items are kept around in a
/// pool.  This pool is managed by the device independent code. During the life
/// cycle, the sg list is mapped, possibly many times, to create the data needed
/// for the low level DMA to happen.  After the I/O completes, and before the
/// sg list is reused, it will be reset.  The port implementation may assume that
/// this happens relatively quickly, but may not assume any specific timing.  User
/// pages that are mapped must be "locked" into memory somehow when the kfio_sgl_map_bytes()
/// function is called since it is called from a sleepable context within the driver
/// where it is called.  Once the I/O is dispatched to the device, it may not be called
/// from a context that can accomplish this locking.  When the sgl list is reset, any
/// such "locking" must be undone.

/// The max number of sectors the OS is allowed to ask for in a single request.
///
/// This value was previously hardcoded in kfio_setup_gendisk(); its derivation is
/// unknown. It is likely it was made up at random.
///
/// Note that in linux, this value is allowed to differ from the maximum scatter-gather
/// list we will accept from the OS. That value is determined by the
/// ioctx_nvec setting, derived from FUSION_IOCTX_MIN_VECS.
/// It is likely other OSes may have to cap FUSION_MAX_SECTORS_PER_OS_RW_REQUEST to
/// prevent overrunning the scatter-gather list.
/// 2048 allows 1MB IO at 512B sectors
#define FUSION_MAX_SECTORS_PER_OS_RW_REQUEST (2048)  ///< Trims can be larger than this!

#define FUSION_DMA_ALIGNMENT_REQUIRED (8)     ///< DMA engine requires addresses be 8-byte aligned

/* Transparent sglist handle. */
#ifndef KSCATTER_IMPL
typedef struct  __kfio_port_sg_list kfio_sg_list_t;
#else
typedef void    kfio_sg_list_t;
#endif
typedef struct  kfio_dma_map kfio_dma_map_t;

/// Arbitrary DMA cookie, port specific in meaning used to map/unmap DMA operations with
typedef struct  kfio_dma_cookie
{
  uint64_t buf[16];
} kfio_dma_cookie_t;

/// Type of address that is being used, either system/kernel address, or an address from the
/// user space of the current executing thread.
typedef enum
{
    kfio_mem_seg_system,
    kfio_mem_seg_user
} kfio_mem_seg_t;

/// @brief Return size of an SGL in bytes (used for accounting)
/// @param nvecs Number of vectors for the sg list.
/// @return Bytes used by an sgl with given nvec count
///
extern uint32_t kfio_sgl_size_bytes(uint32_t nvecs);

/// @brief allocate a scatter/gather vector
///
/// @param node  Numa node to allocate resources on
/// @param sgl   Returns a potiner to the scatter/gather list
/// @param nvecs Number of vectors for the sg list.
///
/// @note The number of vectors (@a nvecs) must be at least 2 larger than the number
/// of vm pages in the I/O to be performed. A port may use fewer vectors, if it can,
/// but may not use more than are passed in.
///
extern int      kfio_sgl_alloc_nvec(kfio_numa_node_t node, kfio_sg_list_t **sgl, int nvecs);

/// @brief Destory a scatter gather list's resources
extern void     kfio_sgl_destroy(kfio_sg_list_t *sgl);

/// @brief Reset the current scatter gather list.
///
/// Releases the resources used by the sgl. No buffers may be mapped for DMA when this is
/// called.  However, some ports may tag pages from user processes that are directly mapped
/// into the kernel in kfio_sgl_map_bytes(), and any such tagging must be undone here.
extern void     kfio_sgl_reset(kfio_sg_list_t *sgl);

/// @brief returns the total number of bytes mapped by this SGL.
extern uint32_t kfio_sgl_size(kfio_sg_list_t *sgl);

/// @brief map kernel pages into the SG list.
///
/// @param sgl    Scatter gather list
/// @param page   Page to map in...
/// @param offset Offset within the page to start at
/// @param size   number of bytes to map
extern int      kfio_sgl_map_page(kfio_sg_list_t *sgl, fusion_page_t page,
                                  uint32_t offset, uint32_t size);

/// @brief Map bytes into a SG list
///
/// @param sgl    Scatter gather list to map into
/// @param buffer Starting address to map in.
/// @param size   number of bytes to map in
/// @param seg    Segment to map
///
/// Map pages into the sgl for future I/O.  User segment pages are also tagged in the OS as busy
/// in some manner so their VA to PA mapping doesn't change, and the PA isn't reused for other purposes.
/// Ports are not required to support mixing user and kernel segments, but if they don't they must
/// return an error.  The port uses this mapping information later when dma maps are loaded later
/// in the driver operation.  Note, a SGL may be mapped or unmapped for DMA multiple times, so any
/// tagging of pages in the OS must be undone only in kfio_sgl_reset(), and not in the dma map routines.
extern int kfio_sgl_map_bytes_gen(kfio_sg_list_t *sgl, const void *buffer, uint32_t size, kfio_mem_seg_t seg);

#if defined(USERSPACE_KERNEL)
/// When mapping from a userspace pointer, we need to know which NBD client is connected such that we copy data
/// from the appropriate client's process space.  This results in the need for two pointers: one for the
/// start address of the data buffer, and one to identify the client.
extern int kfio_sgl_map_bytes_gen_client(kfio_sg_list_t *sgl, const void *buffer, const void* client,
                                         uint32_t size, kfio_mem_seg_t seg);
#endif

static inline int kfio_sgl_map_bytes(kfio_sg_list_t *sgl, const void *buffer, uint32_t size)
{
    return kfio_sgl_map_bytes_gen(sgl, buffer, size, kfio_mem_seg_system);
}

/// @brief dumps the sgl for debugging purposes
extern void     kfio_sgl_dump(kfio_sg_list_t *sgl, kfio_dma_map_t *dmap, const char *prefix, unsigned dump_contents);

/// @brief Number of segments mapped
extern int      kfio_sgl_num_mapped(kfio_sg_list_t *sgl);

struct kfio_sgl_phys
{
    uint64_t addr;
    uint32_t len;
};

typedef struct kfio_sgl_phys kfio_sgl_phys_t;
typedef void  *kfio_dmap_iter_t;

extern kfio_dma_map_t *kfio_dma_map_alloc(int may_sleep, kfio_numa_node_t node);
extern void kfio_dma_map_free(kfio_dma_map_t *dmap);

extern kfio_dmap_iter_t kfio_dma_map_first(kfio_dma_map_t *dmap, kfio_sgl_phys_t *segp);
extern kfio_dmap_iter_t kfio_dma_map_next(kfio_dma_map_t *dmap, kfio_dmap_iter_t iter, kfio_sgl_phys_t *segp);

/// @brief Return number of vectors active in a mapped dma map
extern int kfio_dma_map_nvecs(kfio_dma_map_t *dmap);

/// @brief Return the number of bytes mapped in a mapped dma map
extern uint32_t kfio_dma_map_size(kfio_dma_map_t *dmap);

/// @brief Create a DMA cookie associated with a given device
extern int kfio_dma_cookie_create(kfio_dma_cookie_t *cookie, kfio_pci_dev_t *pcidev, int nvecs);

/// @brief Destroy a DMA cookie
extern int kfio_dma_cookie_destroy(kfio_dma_cookie_t *cookie);

/*@brief DMA mapping direction */
#define IODRIVE_DMA_DIR_WRITE      0
#define IODRIVE_DMA_DIR_READ       1
#define IODRIVE_DMA_DIR_INVALID   -1

/// @brief Map the physical pages for I/O
///
/// @param sgl       Scatter gather list
/// @param _cookie   DMA Cookie
/// @param dmap      dma map for device
/// @param dir       DMA direction: one of IODRIVE_DMA_DIR_READ or IODRIVE_DMA_DIR_WRITE
///
/// Individual sgl may be mapped multiple times. They will be unmapped the same number of times with
/// kfio_sgl_dma_unmap().  On architectures with incoherent caches, the cache must also be
/// flushed/invalidated as is appropriate for the operation.  iommu operations may also be needed.
/// More generally, the CPU's PAs are translated/mapped to the bus' PA so the hardware can bus
/// master the data directly to the desired memory location, if possible, or to an appropriate bounce
/// buffer if not.
extern int kfio_sgl_dma_map(kfio_sg_list_t *sgl, kfio_dma_cookie_t *_cookie, kfio_dma_map_t *dmap, int dir);

/// @brief Unmap the physical pages after I/O
///
/// @param sgl       Scatter gather list
/// @param _cookie    DMA Cookie
///
/// Undoes whatever was done to map the pages for DMA.
extern int kfio_sgl_dma_unmap(kfio_sg_list_t *sgl, kfio_dma_cookie_t *_cookie);
extern int kfio_sgl_dma_slice(kfio_sg_list_t *sgl, kfio_dma_cookie_t * cookie, kfio_dma_map_t *dmap,
                              uint32_t offset, uint32_t length);

#if PORT_SUPPORTS_SGLIST_COPY
extern int kfio_sgl_copy_data(kfio_sg_list_t *dst, kfio_sg_list_t *src, uint32_t length);
#endif

/* Debug interface used only in error injection. */
#if ENABLE_ERROR_INJECTION
# if defined(__linux__) || (defined(__SVR4) && defined(__sun)) || defined (__OSX__) || defined(USERSPACE_KERNEL)
extern void *kfio_sgl_get_byte_pointer(kfio_sg_list_t *sgl, uint32_t offset);
# else
#  define kfio_sgl_get_byte_pointer(sgl, offset) 0
# endif
#endif

/// @}

#endif  /* __FIO_PORT_KSCATTER_H__ */
