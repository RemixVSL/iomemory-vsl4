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

/*******************************************************************************
 * Supported platforms / OS:
 * x86_64 Linux, Solaris, FreeBSD
 * PPC_64 Linux
 * SPARC_64 Solaris
 * Apple OS X v 10.6.2+
 ******************************************************************************/
#ifndef __FIO_PORT_KCACHE_H__
#define __FIO_PORT_KCACHE_H__

#include <fio/port/errno.h>
#include <fio/port/list.h>

struct fio_mem_stat;

/*---------------------------------------------------------------------------*/

/**
 * @brief OS-independent structure for cache.
 *
 * @note Linux - if KFIOC_HAS_KMEM_CACHE p -> struct kmem_cache
 *               else   p-> struct kmem_cache_s
 *
 * @note MS Windows p -> NPAGED_LOOKASIDE_LIST
 *
 * @note FreeBSD p = uma_zone_t (struct uma_zone *)
 *
 * @note Solaris kmem_cache
 *
 * @note OS X homegrown double-linked list of free and used elements.
 */
typedef struct
{
/* Rename "struct kmem_cache_s" -> "struct kmem_cache" linux-2.6.15
 * commit: 2109a2d1b175dfcffbfdac693bdbe4c4ab62f11f
 */
/* Remove typedef kmem_cache_t linux-2.6.23
 * commit: 698827fa9f45019df1609bb686bc51c94e127fbc
 */
    void *p;
    char name[40];
#if FUSION_DEBUG_CACHE
    fusion_atomic32_t count;
#endif
    uint32_t          size;
    uint32_t          align;
    fusion_spinlock_t lock;
    uint32_t          reserved_count;
    struct fio_mem_stat *mem_stat;
    fusion_list_t     reserved_list;
} fusion_mem_cache_t;

#define FUSION_CACHE_VALID(c)    ((c).p != NULL)

extern int __kfio_create_cache(fusion_mem_cache_t *pcache, char *name, uint32_t size, uint32_t align);

/// @brief create a memory cache suitable for allocation pool of fixed size objects.
///
/// @param pcache pointer to cache structure to initialize (need not be initialized before call).
/// @param name     name of cache (for debugging purposes).
/// @param size     size of cache elements in bytes, Must be non-zero.
/// @param align    alignment of cache elements, Must be >= sizeof void * and a power of 2.
///
/// @returns zero on success,
static inline int kfio_create_cache(fusion_mem_cache_t *pcache, char *name,
                                    uint32_t size, uint32_t align)
{
    kassert(size != 0);
    kassert(align != 0);
    kassert((align % sizeof(void *)) == 0);
    kassert((align & (align - 1)) == 0);

    fusion_init_spin(&pcache->lock, "mem-cache-lock");
    pcache->reserved_count = 0;
    pcache->size = size;
    pcache->align = align;
    pcache->mem_stat = NULL;
    fusion_init_list(&pcache->reserved_list);

    return __kfio_create_cache(pcache, name, size, align);
}

/// @brief initialize the cache structure 'c' to allocate elements of type 't'
#define fusion_create_cache(c, name, t)                                 \
    kfio_create_cache(c, name, sizeof(t), __alignof__(t))

/// @brief initialize the cache structure 'c' to allocate elements of type struct 't'
#define fusion_create_struct_cache(c, name, t)                          \
    kfio_create_cache(c, name, sizeof(struct t), __alignof__(struct t))

extern void *kfio_cache_alloc(fusion_mem_cache_t *cache, int can_wait);
extern void *kfio_cache_alloc_node(fusion_mem_cache_t *cache, int can_wait,
                                    kfio_numa_node_t node);
extern void kfio_cache_free(fusion_mem_cache_t *cache, void *p);

#if defined(PORT_HAS_KFIO_CACHE_FREE_NODE) && PORT_HAS_KFIO_CACHE_FREE_NODE
extern void kfio_cache_free_node(fusion_mem_cache_t *cache, void *p);
#else
#define kfio_cache_free_node kfio_cache_free
#endif
extern void kfio_cache_destroy(fusion_mem_cache_t *cache);

/*---------------------------------------------------------------------------*/

/// @brief Destroy a fusion_cache
/// @note  All preallocated blocks must be removed from the cache before this call
static inline void fusion_cache_destroy(fusion_mem_cache_t *cache)
{
    kassert(fusion_list_empty(&cache->reserved_list));
    fusion_destroy_spin(&cache->lock);
    kfio_cache_destroy(cache);
}

/*---------------------------------------------------------------------------*/

#endif //__FIO_PORT_KCACHE_H__

