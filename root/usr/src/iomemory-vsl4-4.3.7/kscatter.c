//-----------------------------------------------------------------------------
// Copyright (c) 2006-2014, Fusion-io, Inc.(acquired by SanDisk Corp. 2014)
// Copyright (c) 2014-2017 SanDisk Corp. and/or all its affiliates. (acquired by Western Digital Corp. 2016)
// Copyright (c) 2016-2018 Western Digital Technologies, Inc. All rights reserved.
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
#define KSCATTER_IMPL
#include "port-internal.h"
#include <fio/port/dbgset.h>
#include <fio/port/message_ids.h>
#include <fio/port/kscatter.h>
#include <fio/port/ktime.h>
#include <linux/version.h>

/**
 * @ingroup PORT_LINUX
 * @{
 */

#ifndef MIN
#define MIN(a, b)   (((a) < (b)) ? (a) : (b))
#endif

struct linux_sgentry
{
    uint32_t            flags;
    void               *this_current;
};

#define SGE_BUFFERED    0x1
#define SGE_USER        0x2     /* SGE is from a page mapped from user process */
#define SGE_READ_MAPPED 0x4     /* SGE has been mapped at least once for reading */

struct kfio_dma_map
{
    uint32_t              map_offset;
    uint32_t              map_length;
    uint32_t              sge_count;
    uint32_t              sge_skip;
    uint32_t              sge_trunc;
    struct linux_sgentry *sge_first;
    struct linux_sgentry *sge_last;
    struct linux_sgl     *lsg;
};

struct linux_sgl
{
    uint32_t            max_entries;
    uint32_t            num_entries;
    uint32_t            num_mapped;
    uint32_t            sgl_size;
    struct linux_sgentry *sge;
    struct scatterlist *sl;
};
#define sge_to_sl(lsg, sge) ((lsg)->sl + ((sge) - (lsg)->sge))

struct linux_dma_cookie
{
    struct pci_dev *pci_dev;
    int             pci_dir;
};

C_ASSERT(sizeof(struct linux_dma_cookie) < sizeof(kfio_dma_cookie_t));

#define GET_USER_PAGES_TASK

/* Newer kernels combine the write and force flags into a single parameter */
#if KFIOC_GET_USER_PAGES_HAS_GUP_FLAGS
    #define GET_USER_PAGES_FLAGS(write, force) (write? FOLL_WRITE : 0 | force? FOLL_FORCE : 0)
#else
    #define GET_USER_PAGES_FLAGS(write, force) write, force
#endif


int kfio_dma_cookie_create(kfio_dma_cookie_t *_cookie, kfio_pci_dev_t *pcidev, int nvecs)
{
    struct linux_dma_cookie *cookie = (struct linux_dma_cookie *)_cookie;

    cookie->pci_dev = (struct pci_dev *)pcidev;
    cookie->pci_dir = -1;

    return 0;
}

int kfio_dma_cookie_destroy(kfio_dma_cookie_t *_cookie)
{
    return 0;
}

uint32_t kfio_sgl_size_bytes(uint32_t nvecs)
{
    return sizeof(struct linux_sgl) +
        nvecs * (sizeof(struct linux_sgentry) + sizeof(struct scatterlist));
}

int kfio_sgl_alloc_nvec(kfio_numa_node_t node, kfio_sg_list_t **sgl, int nvecs)
{
    uint32_t sgl_bytes = kfio_sgl_size_bytes(nvecs);
    struct linux_sgl *lsg;

    lsg = kfio_malloc_node(sgl_bytes, node);

    if (NULL == lsg)
    {
        *sgl = NULL;
        return -ENOMEM;
    }

    lsg->max_entries = nvecs;
    lsg->num_entries = 0;
    lsg->num_mapped  = 0;
    lsg->sgl_size    = 0;
    lsg->sge = (struct linux_sgentry *)(lsg + 1);
    lsg->sl = (struct scatterlist *)(lsg->sge + nvecs);
    sg_init_table(lsg->sl, nvecs);

    *sgl = lsg;
    return 0;
}

void kfio_sgl_destroy(kfio_sg_list_t *sgl)
{
    struct linux_sgl *lsg = sgl;

    kassert(lsg->num_mapped == 0);


    kfio_free(lsg, sizeof(*lsg) +
                    lsg->max_entries * (sizeof(struct linux_sgentry) + sizeof(struct scatterlist)));
}

static void kfio_sgl_unmap(kfio_sg_list_t *sgl)
{
    struct linux_sgl *lsg = sgl;
    int i;

    // DMA must be complete.
    kassert(lsg->num_mapped == 0);

    for (i = 0; i < lsg->num_entries; i++)
    {
        struct linux_sgentry *sge;
        struct scatterlist *sl;

        sge = &lsg->sge[i];
        sl = &lsg->sl[i];

        /*
         * User pages are "locked" into memory when added to the sg list,
         * so we need to clean that up now.  We have to call this from
         * the same context where get_user_pages were called. If we
         * changed the page (by reading data into it), we have to
         * dirty the page. We don't hold a page_lock on it, so we
         * have to called the _locked() version.
         */
        if (sge->flags & SGE_USER)
        {
            kassert(current == sge->this_current);
            if (sge->flags & SGE_READ_MAPPED)
            {
                set_page_dirty_lock(sg_page(sl));
            }
            put_page(sg_page(sl));
        }
    }
}

void kfio_sgl_reset(kfio_sg_list_t *sgl)
{
    struct linux_sgl *lsg = sgl;

    kassert(lsg->num_mapped == 0);

    kfio_sgl_unmap(sgl);

    lsg->sgl_size    = 0;
    lsg->num_mapped  = 0;
    lsg->num_entries = 0;
}

uint32_t kfio_sgl_size(kfio_sg_list_t *sgl)
{
    struct linux_sgl *lsg = sgl;

    return lsg->sgl_size;
}

/**
 * returns virtual address
 */
static void *kfio_sgl_get_vaddr(struct scatterlist *sl)
{
    return page_address(sg_page(sl)) + sl->offset;
}

int kfio_sgl_map_bytes_gen(kfio_sg_list_t *sgl, const void *buffer, uint32_t size, kfio_mem_seg_t seg)
{
    const uint8_t *bp = buffer;
    struct linux_sgl *lsg = sgl;
    int old_num;
    bool vmalloc_buffer;

    vmalloc_buffer = (((uintptr_t)buffer) >= VMALLOC_START &&
                      ((uintptr_t)buffer) < VMALLOC_END);
    old_num = lsg->num_entries;

    while (size)
    {
        fusion_page_t page;
        uint32_t     mapped_bytes;
        uint32_t     page_offset, page_remainder;
        struct linux_sgentry *sge;
        struct scatterlist *sl;

        sge = &lsg->sge[lsg->num_entries];
        sl = &lsg->sl[lsg->num_entries];

        sge->flags     = 0;
        if (lsg->num_entries >= lsg->max_entries)
        {
            engprint("%s: too few sg entries (cnt: %d nvec: %d size: %d)\n",
                     __func__, lsg->num_entries, lsg->max_entries, size);

            return -ENOMEM;
        }

        page_offset    = (uint32_t)((uintptr_t)bp % FUSION_PAGE_SIZE);
        page_remainder = FUSION_PAGE_SIZE - page_offset;
        mapped_bytes   = MIN(size, page_remainder);

        if  (seg == kfio_mem_seg_system)
        {
            if (vmalloc_buffer)
            {
                /*
                 * Do extra casting to get rid of const not expected by
                 * vmalloc_to_page on older Linux kernels.
                 */
                page = (fusion_page_t) vmalloc_to_page((void *)(uintptr_t)bp);
            }
            else
            {
                page = (fusion_page_t) virt_to_page((void *)(uintptr_t)bp);
            }
        }
        else
        {
            int retval;

            // XXX Do we need to widen the interface to allow non-writable here?
            down_read(&current->mm->mmap_sem);
            retval = get_user_pages(GET_USER_PAGES_TASK (uintptr_t)bp, 1, GET_USER_PAGES_FLAGS(1, 0), (struct page **)&page, NULL);
            up_read(&current->mm->mmap_sem);
            if (retval <= 0)
            {
                // Release the pages that worked up until now and return the error.
                int i;

                engprint("%s: can't map user page for offset %p retval %d\n",
                         __func__, bp, retval);
                for (i = old_num; i < lsg->num_entries; i++)
                {
                    put_page(sg_page(&lsg->sl[i]));
                }
                return (retval);
            }
            sge->flags |= SGE_USER;
            sge->this_current = current;
        }

        kassert_release(page != NULL);

        sg_set_page(sl, (struct page *)page, mapped_bytes, page_offset);

        size -= mapped_bytes;
        bp   += mapped_bytes;

        lsg->num_entries++;
        lsg->sgl_size += mapped_bytes;
    }
    return 0;
}

/**
 * @brief Inserts page into the provided kfio_sg_list_t, and updates
 *   sgl->num_entries. Does not actually perform DMA mapping.
 * Called by kfio_sgl_map_bio(..) q.v., and by
 * fio_request_map_metadata_block(..)
 *
 * @return 0 on success
 */
int kfio_sgl_map_page(kfio_sg_list_t *sgl, fusion_page_t page,
                      uint32_t offset, uint32_t size)
{
    struct linux_sgl *lsg = sgl;
    struct linux_sgentry *sge;
    struct scatterlist *sl;

    if (unlikely(lsg->num_entries >= lsg->max_entries))
    {
        engprint("No room to map page num_entries: %d max_entries: %d\n",
                 lsg->num_entries, lsg->max_entries);
        return -EINVAL;
    }

    if (unlikely(offset + size > FUSION_PAGE_SIZE))
    {
#if !defined(DMA_X_PAGE_BOUNDARY)
        engprint("Attempt to map too great a span\n");
        return -EINVAL;
#endif
    }

    sge = &lsg->sge[lsg->num_entries];
    sl = &lsg->sl[lsg->num_entries];
    sge->flags     = 0;

    sg_set_page(sl, (struct page *)page, size, offset);

    lsg->num_entries++;
    lsg->sgl_size += size;

    return 0;
}

int kfio_sgl_map_bio(kfio_sg_list_t *sgl, struct bio *pbio)
{
    struct linux_sgl *lsg = sgl;
    struct scatterlist *sl, *old_sl;

#if KFIOC_HAS_BIOVEC_ITERATORS
    struct bio_vec vec;
    struct bvec_iter bv_i;
#define BIOV_MEMBER(vec, member) (vec.member)
#else
    struct bio_vec *vec;
    int bv_i;
#define BIOV_MEMBER(vec, member) (vec->member)
#endif

    uint32_t old_len, old_sgl_size, old_sgl_len;
    int rval = 0;

    // Make sure combining this pbio into the current sgl won't result in too many sg vectors.
    // The bio_for_each_segment() loop below will catch this, but it seems more efficient to catch it here.
    if (lsg->num_entries + bio_segments(pbio) > lsg->max_entries)
    {
        return -EAGAIN;                 // Too many segments. Try the pbio again.
    }

    /* Remember SGL vitals in case we need to back out. */
    if (lsg->num_entries > 0)
    {
        old_sl = &lsg->sl[lsg->num_entries - 1];
        old_len = old_sl->length;
    }
    else
    {
        old_sl = NULL;
        old_len = 0;
    }
    old_sgl_len  = lsg->num_entries;
    old_sgl_size = lsg->sgl_size;

    bio_for_each_segment(vec, pbio, bv_i)
    {
        if (lsg->num_entries > 0)
        {
            /*
             * This iovec shares page with previous one. Merge them together. The biggest
             * entry we can get this way is limited by page size.
             */
            sl = &lsg->sl[lsg->num_entries - 1];

            if (sg_page(sl) == BIOV_MEMBER(vec, bv_page) &&
                sl->offset + sl->length == BIOV_MEMBER(vec, bv_offset))
            {
                sl->length   += BIOV_MEMBER(vec, bv_len);
                lsg->sgl_size += BIOV_MEMBER(vec, bv_len);
                continue;
            }

            /* The list will grow: check for overflow here. */
            if (unlikely(lsg->num_entries >= lsg->max_entries))
            {
                // Returning a non-zero value should cause callers to split the bio and re-submit it to us.
                rval = -EAGAIN;
                break;
            }
        }

        rval = kfio_sgl_map_page(sgl,
                                 (fusion_page_t) BIOV_MEMBER(vec, bv_page),
                                 BIOV_MEMBER(vec, bv_offset),
                                 BIOV_MEMBER(vec, bv_len));
        if (rval)
        {
            engprint("kfio_sgl_map_page failed with error %d in map_bio\n", rval);
            break;
        }
    }

    if (rval)
    {
        /*
         * Roll back all of the updates made so far. Unfortunately our
         * callers expect bio mapping to be either all successful or
         * not, with no partial result left in.
         */
        lsg->num_entries = old_sgl_len;
        lsg->sgl_size = old_sgl_size;

        if (old_sl != NULL)
        {
            old_sl->length = old_len;
        }
    }
    return rval;
}

int kfio_sgl_dma_map(kfio_sg_list_t *sgl, kfio_dma_cookie_t *_cookie, kfio_dma_map_t *dmap, int dir)
{
    struct linux_sgl *lsg = sgl;
    struct linux_dma_cookie *cookie = (struct linux_dma_cookie *)_cookie;
    int i;

    cookie->pci_dir = dir;

    for (i = 0; i < lsg->num_entries; i++)
    {
        struct linux_sgentry *sge;
        struct scatterlist *sl;

        sge = &lsg->sge[i];
        sl = &lsg->sl[i];

        if (dir == IODRIVE_DMA_DIR_READ)
        {
            sge->flags |= SGE_READ_MAPPED;
        }
    }

    i = pci_map_sg(cookie->pci_dev, lsg->sl, lsg->num_entries,
                    dir == IODRIVE_DMA_DIR_READ ? PCI_DMA_FROMDEVICE : PCI_DMA_TODEVICE);
    lsg->num_mapped = i;
    if (i < lsg->num_entries)
    {
        goto bail;
    }

    /*
     * Fill in a map covering the whole scatter-gather list if caller is
     * interested in the information.
     */
    if (dmap != NULL)
    {
        dmap->map_offset = 0;
        dmap->map_length = lsg->sgl_size;
        dmap->sge_count  = lsg->num_mapped;
        dmap->sge_skip   = 0;
        dmap->sge_trunc  = 0;
        dmap->sge_first  = &lsg->sge[0];
        dmap->sge_last   = &lsg->sge[lsg->num_mapped - 1];
        dmap->lsg        = lsg;
    }
    return lsg->num_mapped;

bail:
    kfio_sgl_dma_unmap(sgl, _cookie);
    return -EINVAL;
}

int kfio_sgl_dma_unmap(kfio_sg_list_t *sgl, kfio_dma_cookie_t *_cookie)
{
    struct linux_sgl *lsg = sgl;
    struct linux_dma_cookie *cookie = (struct linux_dma_cookie *)_cookie;
    int i;

    for (i = 0; i < lsg->num_mapped; i++)
    {
        struct linux_sgentry *sge;
        struct scatterlist *sl;

        sge = &lsg->sge[i];
        sl = &lsg->sl[i];
    }
    pci_unmap_sg(cookie->pci_dev, lsg->sl, lsg->num_entries,
                 cookie->pci_dir == IODRIVE_DMA_DIR_READ ? PCI_DMA_FROMDEVICE : PCI_DMA_TODEVICE);
    lsg->num_mapped = 0;

    cookie->pci_dir = -1;

    return 0;
}

#if defined(DMA64_OS32)
#define PRIdma "llx"
#define DMA_PR_T long long
#else
#define PRIdma "p"
#define DMA_PR_T void *
#endif

void kfio_sgl_dump(kfio_sg_list_t *sgl, kfio_dma_map_t *dmap, const char *prefix, unsigned dump_contents)
{
    struct linux_sgl *lsg = sgl;
    uint32_t i;

    infprint("%s sglist %p num %u max %u size %u\n",
             prefix, lsg, lsg->num_entries, lsg->max_entries, lsg->sgl_size);

    if (dmap != NULL)
    {
        infprint("%s map %p offset %u size %u count %u\n",
                 prefix, dmap, dmap->map_offset, dmap->map_length, dmap->sge_count);
    }

    for (i = 0; i < lsg->num_entries; i++)
    {
        struct linux_sgentry *sge = &lsg->sge[i];
        struct scatterlist *sl = &lsg->sl[i];
        const uint8_t *bp = kfio_sgl_get_vaddr(sl);

        infprint("%s        mvec %d: vaddr: %p offset: %u size: %d paddr: %" PRIdma"\n",
                 prefix, i, bp, sl->offset, sl->length, (DMA_PR_T)sg_dma_address(sl));

        if (dmap != NULL)
        {
            if (sge == dmap->sge_first)
            {
                infprint("%s        map first skip %u\n",
                         prefix, dmap->sge_skip);
            }

            if (sge == dmap->sge_last)
            {
                infprint("%s        map last trunc %u\n",
                         prefix, dmap->sge_trunc);
            }
        }

        if (dump_contents)
        {
            infprintbuf8(bp, sl->length, 16, " %02x", "%04d:", NULL);

            // Give the linux print FIFO time to catch up
            // Yes, I know this ain't pretty, but neither is
            // an unreliable print buffer
            kfio_msleep(10);
        }
    }
}

/// @brief return a kernel virtual address for the byte 'offset' bytes into the given SGL.
///
/// Note that only one byte is guaranteed valid!
///
/// @param sgl     the scatter-gather list.
/// @param offset  byte offset within the SGL.
///
/// @return pointer, null on error.
void *kfio_sgl_get_byte_pointer(kfio_sg_list_t *sgl, uint32_t offset)
{
    struct linux_sgl *lsg = sgl;
    uint32_t i;
    uint32_t total_offset = 0;

    for (i = 0; i < lsg->num_entries; i++)
    {
        struct scatterlist *sl = &lsg->sl[i];

        if (offset < total_offset + sl->length)
        {
            uint8_t *bp = kfio_sgl_get_vaddr(sl);

            bp += (offset - total_offset);
            return bp;
        }

        total_offset += sl->length;
   }

    return 0;
}

kfio_dma_map_t *kfio_dma_map_alloc(int may_sleep, kfio_numa_node_t node)
{
    kfio_dma_map_t *dmap;

    if (may_sleep)
    {
        dmap = kfio_malloc_node(sizeof(*dmap), node);
    }
    else
    {
        dmap = kfio_malloc_atomic_node(sizeof(*dmap), node);
    }
    if (dmap != NULL)
    {
        kfio_memset(dmap, 0, sizeof(*dmap));
    }
    return dmap;
}

void kfio_dma_map_free(kfio_dma_map_t *dmap)
{
    if (dmap != NULL)
    {
        kfio_free(dmap, sizeof(*dmap));
    }
}

int kfio_sgl_dma_slice(kfio_sg_list_t *sgl, kfio_dma_cookie_t *cookie, kfio_dma_map_t *dmap,
                       uint32_t offset, uint32_t length)
{
    struct linux_sgl *lsg = sgl;
    uint32_t          i;

    dmap->map_offset = offset;
    dmap->lsg = lsg;

    for (i = 0; i < lsg->num_mapped; i++)
    {
        struct linux_sgentry *sge = &lsg->sge[i];
        struct scatterlist *sl = &lsg->sl[i];

        if (offset < sl->length)
        {
            dmap->sge_first = sge;
            dmap->sge_last  = sge;

            dmap->map_length = sl->length - offset;
            dmap->sge_skip   = offset;
            dmap->sge_count  = 1;
            break;
        }

        offset -= sl->length;
    }

    kassert(i < lsg->num_entries);
    kassert(dmap->sge_first != NULL);

    for (i = i + 1; i <= lsg->num_mapped; i++)
    {
        struct linux_sgentry *sge = &lsg->sge[i];
        struct scatterlist *sl = &lsg->sl[i];

        if (dmap->map_length >= length)
        {
            dmap->sge_trunc   = dmap->map_length - length;
            dmap->map_length -= dmap->sge_trunc;
            break;
        }
        kassert(i < lsg->num_entries);
        dmap->map_length += sl->length;
        dmap->sge_last    = sge;
        dmap->sge_count++;
    }

    return 0;
}

int kfio_dma_map_nvecs(kfio_dma_map_t *dmap)
{
    return dmap->sge_count;
}

uint32_t kfio_dma_map_size(kfio_dma_map_t *dmap)
{
    return dmap->map_length;
}

/* Physical segment enumeration interface. */
kfio_dmap_iter_t kfio_dma_map_first(kfio_dma_map_t *dmap, kfio_sgl_phys_t *segp)
{
    struct linux_sgentry *sge = dmap->sge_first;
    struct scatterlist *sl = sge_to_sl(dmap->lsg, sge);

    if (sge != NULL)
    {
        if (segp != NULL)
        {
            segp->addr = sg_dma_address(sl) + dmap->sge_skip;
            segp->len  = sl->length - dmap->sge_skip;

            if (sge == dmap->sge_last)
                segp->len -= dmap->sge_trunc;
        }
        return sge;
    }
    return NULL;
}

kfio_dmap_iter_t kfio_dma_map_next(kfio_dma_map_t *dmap, kfio_dmap_iter_t iter, kfio_sgl_phys_t *segp)
{
    struct linux_sgentry *sge = iter;
    struct scatterlist *sl;

    if (sge >= dmap->sge_first && sge < dmap->sge_last)
    {
        sge++;

        sl = sge_to_sl(dmap->lsg, sge);
        if (segp != NULL)
        {
            segp->addr = sg_dma_address(sl);
            segp->len  = sl->length;

            if (sge == dmap->sge_last)
                segp->len -= dmap->sge_trunc;
        }
        return sge;
    }
    return NULL;
}

#if PORT_SUPPORTS_SGLIST_COPY
/// @brief Copy data between scatter gather lists.
///
/// @param dst     the destination scatter-gather list.
/// @param src     the source scatter-gather list.
/// @param length  number of bytes to copy from source to destination
///                scatter-gather list.
/// Note: the entries in linux_sgl for src and dst should be updated by the
///       caller (such as num_entries, sgl->size, etc.)
/// @return 0, -EFAULT on error.
int kfio_sgl_copy_data(kfio_sg_list_t *dst, kfio_sg_list_t *src, uint32_t length)
{
    struct linux_sgl *ldst = dst;
    struct linux_sgl *lsrc = src;

    struct scatterlist   *ldst_sl = &ldst->sl[0];
    struct scatterlist   *lsrc_sl = &lsrc->sl[0];

    uint32_t ldst_sge_off = ldst_sl->offset;
    uint32_t lsrc_sge_off = lsrc_sl->offset;

    if (length > ldst->sgl_size || length > lsrc->sgl_size)
    {
        return -EFAULT;
    }

    while (length)
    {
        int sub_length = length;
        void *pdst, *psrc;

        sub_length = MIN(sub_length, ldst_sl->length - ldst_sge_off);
        sub_length = MIN(sub_length, lsrc_sl->length - lsrc_sge_off);

        preempt_disable();

        pdst = kmap_atomic(sg_page(ldst_sl));
        psrc = kmap_atomic(sg_page(lsrc_sl));

        kfio_memcpy(pdst + ldst_sge_off, psrc + lsrc_sge_off, sub_length);

        kunmap_atomic(psrc);
        kunmap_atomic(pdst);

        preempt_enable();

        ldst_sge_off += sub_length;
        lsrc_sge_off += sub_length;

        if (ldst_sge_off == ldst_sl->length)
        {
            ldst_sl++;
            ldst_sge_off = ldst_sl->offset;
        }

        if (lsrc_sge_off == lsrc_sl->length)
        {
            lsrc_sl++;
            lsrc_sge_off = lsrc_sl->offset;
        }

        length -= sub_length;
    }

    return 0;
}
#endif

/**
 * @}
 */
