//-----------------------------------------------------------------------------
// Copyright (c) 2006-2014, Fusion-io, Inc.(acquired by SanDisk Corp. 2014)
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

#if !defined (__linux__)
#error This file supports linux only
#endif

#include <fio/port/port_config.h>
#include <fio/port/kfio_config.h>
#include <fio/port/ktypes.h>
#include <fio/port/dbgset.h>
#include <fio/port/message_ids.h>
#include <linux/mm.h>
#include <linux/vmalloc.h>
#include <linux/version.h>
#include <asm/io.h>

/**
 * @ingroup PORT_LINUX
 * @{
 */

/**
 * @brief allocates physically contiguous memory.
 * No i/o is allowed to fulfill request, but the allocation may block.
 *
 * Some OSes want the size requested to be passed into the fuction to free
 * the memory.  A quick solution is to allocate (wordsize) more bytes than
 * requested, place the size in the first four bytes, and return the pointer
 * to (ptr_from_alloc + wordsize).
 */
static void *__kfio_malloc(fio_size_t size, gfp_t gfp, kfio_numa_node_t node)
{
    // This has been determined to be a place to put this injector agent, but there are compile problems.
    // Apparently fio-port.ko cannot find the error_injection_agents.o file.
#if 0   //ENABLE_ERROR_INJECTION
    if(inject_hard_alloc_error())
        return NULL;
#endif

    FUSION_ALLOCATION_TRIPWIRE_TEST();
    kassert(size <= KFIO_MALLOC_SIZE_MAX);
    return kmalloc(size, gfp);
}

/**
 *
 */
void *kfio_malloc(fio_size_t size)
{
    return __kfio_malloc(size, GFP_NOIO, -1);
}

/**
 *
 */
void *kfio_malloc_node(fio_size_t size, kfio_numa_node_t node)
{
    return __kfio_malloc(size, GFP_NOIO, node);
}

/**
 *
 */
void *kfio_malloc_atomic(fio_size_t size)
{
    return __kfio_malloc(size, GFP_NOWAIT, -1);
}

/**
 *
 */
void *kfio_malloc_atomic_node(fio_size_t size, kfio_numa_node_t node)
{
    return __kfio_malloc(size, GFP_NOWAIT, node);
}

/**
 *
 */
void noinline kfio_free(void *ptr, fio_size_t size)
{
    (void) size;

    kfree(ptr);
}

/**
 * @brief allocates virtual memory mapped into kernel space that is
 * not assumed to be contiguous.
 */
void *noinline kfio_vmalloc(fio_size_t size)
{
    FUSION_ALLOCATION_TRIPWIRE_TEST();
    return vmalloc(size);
}

/**
 *
 */
void noinline kfio_vfree(void *ptr, fio_size_t sz)
{
    (void) sz;
    vfree(ptr);
}

/*---------------------------------------------------------------------------*/

void noinline kfio_free_page(fusion_page_t pg)
{
    __free_page((struct page *) pg);
}

/** @brief returns the page for a virtual address
 */
fusion_page_t noinline kfio_page_from_virt(void *vaddr)
{
    kassert(!is_vmalloc_addr(vaddr));
    return (fusion_page_t)virt_to_page(vaddr);
}

/** @brief returns a virtual address given a page
 */
void * noinline kfio_page_address(fusion_page_t pg)
{
    return page_address((struct page *) pg);
}

/**
 * The flags passed in are @e not compatible with the linux flags.
 */
fusion_page_t noinline kfio_alloc_0_page(kfio_maa_t flags)
{
    gfp_t lflags = 0;
    fusion_page_t page = NULL;

    if (flags & KFIO_MAA_NORMAL)
        lflags |= GFP_KERNEL;
    if (flags & KFIO_MAA_NOIO)
        lflags |= GFP_NOIO;
    if (flags & KFIO_MAA_NOWAIT)
        lflags |= (GFP_NOWAIT | __GFP_NOWARN);
/** linux-2.6.11 introduced __GFP_ZERO
 * We don't have a KFIOC_ for this because defines are trivially detectable.
 */

#if defined(__GFP_ZERO)
    page = (fusion_page_t) alloc_page(lflags | __GFP_ZERO);
#else
    page = (fusion_page_t) alloc_page(lflags);
#endif

#if !defined(__GFP_ZERO)
    if (page)
        memset(kfio_page_address(page), 0, FUSION_PAGE_SIZE);
#endif

    return page;
}

void *kfio_phys_to_virt(uint64_t paddr)
{
    return phys_to_virt(paddr);
}

/**
 * @}
 */
