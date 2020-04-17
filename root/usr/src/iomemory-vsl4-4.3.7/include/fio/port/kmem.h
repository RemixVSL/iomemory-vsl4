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

/*******************************************************************************
 * Supported platforms / OS:
 * x86_64 Linux, Solaris, FreeBSD
 * PPC_64 Linux
 * SPARC_64 Solaris
 * Apple OSX v 10.6+
 ******************************************************************************/
#ifndef __FIO_PORT_KTYPES_H__
#error Please include <fio/port/ktypes.h> rather than this file directly.
#endif
#include <fio/port/errno.h>
#ifndef __FIO_PORT_KMEM_H__
#define __FIO_PORT_KMEM_H__

/** @brief these are arbitrary and do @e not correspond to the linux flags
 */
/* general memory allocation with no special context */
#define KFIO_MAA_NORMAL         1
/* IO isn't allowed during allocation */
#define KFIO_MAA_NOIO           2
/* Atomic and high priority/emergency pool access */
#define KFIO_MAA_NOWAIT         4
/* Use the md page memory pool - ESX specific */
#define KFIO_MAA_MD_POOL        8

typedef struct __fusion_page *fusion_page_t;

// Come to a screeching stop on the first allocation after the tripwire is armed.
#if FUSION_DEBUG && defined(FUSION_ALLOCATION_TRIPWIRE) && FUSION_ALLOCATION_TRIPWIRE
# define FUSION_ALLOCATION_TRIPWIRE_TEST() \
    kassert(g_fusion_allocation_tripwire == 0)
#else
# define FUSION_ALLOCATION_TRIPWIRE_TEST() ;
#endif

// Limit to the size that can be allocated via kfio_malloc()
// This limitation is imposed by Linux
#define KFIO_MALLOC_SIZE_MAX (128 * 1024)

#if TRACE_MEM_ALLOCS && ! defined(USERSPACE_KERNEL) // Userspace is defined in test jigs too
#if ! defined(__FreeBSD__)
#error "TRACE_MEM_ALLOCS only implelemted in FreeBSD for now."
#else
extern void *kfio__malloc(fio_size_t size, char *file, int line);
extern void *kfio__vmalloc(fio_size_t size, char *file, int line);
extern void *kfio__malloc_node(fio_size_t size, kfio_numa_node_t node, char *file, int line);
extern void *kfio__malloc_atomic(fio_size_t size, char *file, int line);
extern void *kfio__malloc_atomic_node(fio_size_t size, kfio_numa_node_t node, char *file, int line);
extern void  kfio__free(void *ptr, fio_size_t size, char *file, int line);
extern void  kfio__vfree(void *ptr, fio_size_t size, char *file, int line);

#define kfio_malloc(size)                                                \
          kfio__malloc((size), __FILE__, __LINE__)
#define kfio_vmalloc(size)                                                \
          kfio__vmalloc((size), __FILE__, __LINE__)
#define kfio_malloc_node(size, node)                                     \
          kfio__malloc_node((size), (node), __FILE__, __LINE__)
#define kfio_malloc_atomic(size)                                         \
          kfio__malloc_atomic((size), __FILE__, __LINE__)
#define kfio_malloc_atomic_node(size, node)                              \
          kfio__malloc_atomic_node((size), (node), __FILE__, __LINE__)
#define kfio_free(ptr, size)                                             \
          kfio__free((ptr), (size), __FILE__, __LINE__)
#define kfio_vfree(ptr, size)                                            \
          kfio__vfree((ptr), (size), __FILE__, __LINE__)
#endif
#else

extern void *kfio_malloc(fio_size_t size);
extern void *kfio_malloc_node(fio_size_t size, kfio_numa_node_t node);
extern void *kfio_malloc_atomic(fio_size_t size);
extern void *kfio_malloc_atomic_node(fio_size_t size, kfio_numa_node_t node);
extern void  kfio_free(void *ptr, fio_size_t size);

/********************************************************************************
 * All OSes are different, when applying this rule, fine-tuning may be desireable
 *
 * When determining kfio_malloc vs kfio_vmalloc, the general recommendation is:
 * 1. cut-off line is set to 4K at this time.
 * 2. if memory size can be determined during compile time, such as one or more structs,
 *    total size is less than the cut-off line, use kfio_malloc.
 *    Otherwise, use kfio_vmalloc
 * 3. if memory size is determined during run-time, but unlikely being greater than the cut-off line,
 *    such as a path name in kinfo.c, use kfio_malloc.
 * 4. if memory size is determined during run-time (such as user input, num of blocks, etc)
 *    and if it is hard to predict the size, use kfio_vmalloc
 ********************************************************************************/
extern void *kfio_vmalloc(fio_size_t size);
extern void  kfio_vfree(void *ptr, fio_size_t size);
#endif

extern void  kfio_free_page(fusion_page_t pg );
extern fusion_page_t kfio_page_from_virt(void *vaddr);
extern void *kfio_page_address(fusion_page_t pg);
extern fusion_page_t kfio_alloc_0_page(kfio_maa_t flags);

/// @brief Return the maximum number of pages which might be used by an address range
static inline uint32_t kfio_get_memory_range_num_pages(fio_uintptr_t start, size_t len)
{
    return (uint32_t)(((start + len + FUSION_PAGE_SIZE - 1) >> FUSION_PAGE_SHIFT) -
                      (start >> FUSION_PAGE_SHIFT));
}

extern void *kfio_phys_to_virt(uint64_t paddr);


static inline int kfio_prealloc_reserve(uint32_t prealloc_mb)
{
    // no-op in non-ESX ports
    (void)prealloc_mb;
    return 0;
}

static inline void kfio_free_md_page(fusion_page_t pg)
{
    kfio_free_page(pg);
}


#endif //__FIO_PORT_KMEM_H__
