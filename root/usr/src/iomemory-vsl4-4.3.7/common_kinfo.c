//-----------------------------------------------------------------------------
// Copyright (c) 2006-2014, Fusion-io, Inc.(acquired by SanDisk Corp. 2014)
// Copyright (c) 2014-2015 SanDisk Corp. and/or all its affiliates. All rights reserved.
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

#include <fio/port/dbgset.h>
#include <fio/port/kinfo.h>
#include <fio/port/list.h>
#include <fio/port/ufio.h>  // for UFIO_KINFO_ROOT
#include <fio/common/kinfo.h>

struct kfio_info_node
{
    struct kfio_info_node *parent;
    void                  *data;
    int                    type;
    fio_size_t             size;
    fio_mode_t             mode;
    void                  *os_priv;
    fusion_list_entry_t    node_entry;
    union
    {
        kfio_info_type_handler_t *type_handler;
        kfio_info_text_handler_t *text_handler;
        kfio_info_seq_ops_t      *seqf_ops;
        fusion_list_t             subnode_list;
    } uu;
    char                   name[1];
};

#if KFIO_INFO_USE_OS_BACKEND
/* global init state for OS backend. */
static int            kinfo_os_init;
#endif

/* lock to protect access */
static fusion_mutex_t kinfo_lock;

/* Top of our hierarchy. */
static kfio_info_node_t *kinfo_root_node;

static kfio_info_node_t * kfio_info_alloc_node(int type, const char *name)
{
    kfio_info_node_t *nodep;
    fio_size_t size, namelen;

    namelen = kfio_strlen(name);
    if (namelen > KFIO_INFO_MAX_NAMELEN)
    {
        return NULL;
    }

    size  = sizeof (struct kfio_info_node) + namelen;
    nodep = kfio_malloc(size);
    if (nodep == NULL)
    {
       return NULL;
    }

    kfio_memset(nodep, 0, size);

    kfio_strncpy(nodep->name, name, namelen);
    nodep->type = type;

    fusion_init_list_entry(&nodep->node_entry);

    return nodep;
}

static void kfio_info_free_node(kfio_info_node_t *nodep)
{
    fio_size_t size;
    size  = sizeof (struct kfio_info_node) + kfio_strlen(nodep->name);

    kfio_free(nodep, size);
}

/**
 * kinfo_lock must be held for reading by caller
 * Note: callers to make sure parent node has type KFIO_INFO_DIR. Otherwise, this function will assert
 */
static kfio_info_node_t * kfio_info_lookup_node(kfio_info_node_t *parent,
                                                const char *name)
{
    kfio_info_node_t *nodep;

    kassert(parent->type == KFIO_INFO_DIR);
    fusion_list_for_each_entry(nodep, &parent->uu.subnode_list, node_entry, kfio_info_node_t)
    {
        if (kfio_strcmp(nodep->name, name) == 0)
        {
            return nodep;
        }
    }
    return NULL;
}

/**
 * kinfo_lock must be held for writing by caller
 */
static int kfio_info_insert_node(kfio_info_node_t *parent, kfio_info_node_t *nodep)
{
    int rc = 0;

    if (parent != NULL)
    {
        if (NULL != kfio_info_lookup_node(parent, nodep->name))
        {
            return -EEXIST;
        }
    }

#if KFIO_INFO_USE_OS_BACKEND
    rc = kfio_info_os_create_node(parent, nodep);
    if (rc != 0)
    {
        return rc;
    }
#endif

    if (parent == NULL)
    {
        parent = kinfo_root_node;
    }

    kassert(parent != NULL);
    nodep->parent = parent;
    fusion_list_add_tail(&nodep->node_entry, &parent->uu.subnode_list);

    return rc;
}

/**
 *   kinfo_lock must be held for writing by caller
 */
static void kfio_info_unlink_node(kfio_info_node_t *nodep)
{
#if KFIO_INFO_USE_OS_BACKEND
    /* OS backend knows nothing about our fake node. */
    kfio_info_os_remove_node(nodep->parent == kinfo_root_node ? NULL : nodep->parent, nodep);
#endif

    if (nodep->parent != NULL)
    {
        fusion_list_del(&nodep->node_entry);
        nodep->parent = NULL;
    }
}

int kfio_info_node_get_type(kfio_info_node_t *nodep)
{
    return nodep->type;
}

fio_ssize_t kfio_info_node_get_size(kfio_info_node_t *nodep)
{
    return nodep->size;
}

fio_mode_t kfio_info_node_get_mode(kfio_info_node_t *nodep)
{
    return nodep->mode;
}

const char *kfio_info_node_get_name(kfio_info_node_t *nodep)
{
    return nodep->name;
}

void *kfio_info_node_get_data(kfio_info_node_t *nodep)
{
    return nodep->data;
}

void *kfio_info_node_get_os_private(kfio_info_node_t *nodep)
{
    return nodep->os_priv;
}

void  kfio_info_node_set_os_private(kfio_info_node_t *nodep, void *priv)
{
    nodep->os_priv = priv;
}

/**
 *  kinfo_lock must not be held by caller
 */
int kfio_info_create_dir(kfio_info_node_t *parent, const char *name, kfio_info_node_t **newdir)
{
    kfio_info_node_t *dirp;
    int              rc;

    dirp = kfio_info_alloc_node(KFIO_INFO_DIR, name);
    if (dirp == NULL)
    {
        return -ENOMEM;
    }
    dirp->mode = S_IRUGO;
    fusion_init_list(&dirp->uu.subnode_list);

    fusion_mutex_lock(&kinfo_lock);
    rc = kfio_info_insert_node(parent, dirp);
    fusion_mutex_unlock(&kinfo_lock);

    if (rc != 0)
    {
        kfio_info_free_node(dirp);
        return rc;
    }

    *newdir = dirp;
    return 0;
}

/**
 * @brief removes from tree the children of *nodepp, and then the node itself.
 *  Sets the *nodepp to NULL
 *  kfio_lock must not be held by caller
 */
void kfio_info_remove_node(kfio_info_node_t **nodepp)
{
    kfio_info_node_t *nodep;

    fusion_mutex_lock(&kinfo_lock);

    if (nodepp == NULL || *nodepp == NULL)
    {
        fusion_mutex_unlock(&kinfo_lock);
        return;
    }
    nodep = *nodepp;

    if (nodep->type == KFIO_INFO_DIR)
    {
        kfio_info_node_t *parent, *child;

        /* Simulate recursion and destroy all children if any. */
        parent = nodep;
        nodep  = nodep->parent;

        while (parent != nodep)
        {
            while (!fusion_list_empty(&parent->uu.subnode_list))
            {
                child = fusion_list_get_head(&parent->uu.subnode_list,
                                             kfio_info_node_t,
                                             node_entry);
                kassert(child != NULL);
                if (child->type == KFIO_INFO_DIR)
                {
                    /* Move down one level. */
                    parent = child;
                }
                else
                {
                    kfio_info_unlink_node(child);
                    kfio_info_free_node(child);
                }
            }
            /* Move up one level. */
            child  = parent;
            parent = parent->parent;

            kassert(parent != NULL);

            kfio_info_unlink_node(child);
            kfio_info_free_node(child);
        }
    }
    else
    {
        kfio_info_unlink_node(nodep);
        kfio_info_free_node(nodep);
    }

    *nodepp = NULL;

    fusion_mutex_unlock(&kinfo_lock);
}

/**
 *  kfio_lock must not be held by caller
 */
int kfio_info_create_type(kfio_info_node_t *parent, const char *name, int type, fio_mode_t mode,
                          void *data, fio_size_t size)
{
    return kfio_info_create_proc(parent, name, type, mode, NULL, data, size);
}

/**
 *  kfio_lock must not be held by caller
 */
int kfio_info_create_proc(kfio_info_node_t *parent, const char *name, int type, fio_mode_t mode,
                          kfio_info_type_handler_t *handler, void *param, fio_size_t size)
{
    kfio_info_node_t *nodep;
    int               rc;

    if (NULL == parent)
    {
        return -EINVAL;
    }
    nodep = kfio_info_alloc_node(type, name);

    if (nodep == NULL)
    {
        return -ENOMEM;
    }

    nodep->data = param;
    nodep->mode = mode;
    nodep->uu.type_handler = handler;

    /* Only fixed size types are supported here. */
    switch (type)
    {
    case KFIO_INFO_INT32:
        nodep->size = sizeof(int32_t);
        break;
    case KFIO_INFO_UINT32:
        nodep->size = sizeof(uint32_t);
        break;
    case KFIO_INFO_UINT64:
        nodep->size = sizeof(uint64_t);
        break;
    case KFIO_INFO_STRING:
        nodep->size = size;
        break;
    default:
        kfio_info_free_node(nodep);
        return -EINVAL;
    }
    kassert(size == nodep->size || size == 0);

    fusion_mutex_lock(&kinfo_lock);
    rc = kfio_info_insert_node(parent, nodep);
    fusion_mutex_unlock(&kinfo_lock);
    if (rc != 0)
    {
        kfio_info_free_node(nodep);
    }
    return rc;
}

/**
 *  kfio_lock must not be held by caller
 */
int kfio_info_create_seqf(kfio_info_node_t *parent, const char *name, fio_mode_t mode,
                          kfio_info_seq_ops_t *ops, void *param)
{
    kfio_info_node_t *nodep;
    int               rc;

    if (NULL == parent)
    {
        return -EINVAL;
    }
    nodep = kfio_info_alloc_node(KFIO_INFO_SEQFILE, name);

    if (nodep == NULL)
    {
        return -ENOMEM;
    }

    nodep->data = param;
    nodep->mode = mode;
    nodep->uu.seqf_ops = ops;

    fusion_mutex_lock(&kinfo_lock);
    rc = kfio_info_insert_node(parent, nodep);
    fusion_mutex_unlock(&kinfo_lock);
    if (rc != 0)
    {
        kfio_info_free_node(nodep);
    }
    return rc;
}

/**
 *  kfio_lock must not be held by caller
 */
int kfio_info_create_text(kfio_info_node_t *parent, const char *name, fio_mode_t mode,
                          kfio_info_text_handler_t *handler, void *param)
{
    kfio_info_node_t *nodep;
    int               rc;

    if (NULL == parent)
    {
        return -EINVAL;
    }
    nodep = kfio_info_alloc_node(KFIO_INFO_TEXT, name);

    if (nodep == NULL)
    {
        return -ENOMEM;
    }


    nodep->data = param;
    nodep->mode = mode;
    nodep->uu.text_handler = handler;

    fusion_mutex_lock(&kinfo_lock);
    rc = kfio_info_insert_node(parent, nodep);
    fusion_mutex_unlock(&kinfo_lock);
    if (rc != 0)
    {
        kfio_info_free_node(nodep);
    }
    return rc;
}

fio_size_t kfio_info_get_node_size(kfio_info_node_t *nodep)
{
    return nodep->size;
}

int kfio_info_get_node_type(kfio_info_node_t *nodep)
{
    return nodep->type;
}

/*
 * Helpers to manipulate transparent data handle
 */
struct kfio_info_data
{
    kfio_info_node_t *node;
    char             *buffer;
    fio_size_t        max_size;
    fio_size_t        cur_size;
    fio_size_t        tot_size;
    int               eof;
};

int kfio_info_alloc_data_handle(kfio_info_node_t *node, void *data, fio_size_t size,
                                kfio_info_data_t **dbhp)
{
    kfio_info_data_t *dbh;

    dbh = kfio_malloc(sizeof *dbh);
    if (dbh == NULL)
    {
        *dbhp = NULL;
        return -ENOMEM;
    }
    kfio_info_reset_data_handle(node, data, size, dbh);
    *dbhp = dbh;
    return 0;
}

void kfio_info_reset_data_handle(kfio_info_node_t *node, void *data, fio_size_t size,
                                kfio_info_data_t *dbh)
{
    dbh->node     = node;
    dbh->buffer   = data;
    dbh->max_size = size;
    dbh->cur_size = 0;
    dbh->tot_size = 0;
    dbh->eof      = 1;
}

void kfio_info_free_data_handle(kfio_info_data_t *dbh)
{
    if (dbh != NULL)
    {
        kfio_free(dbh, sizeof(*dbh));
    }
}

char *kfio_info_get_data_buffer(kfio_info_data_t *dbh)
{
    return dbh->buffer;
}

fio_ssize_t kfio_info_printf(kfio_info_data_t *dbh, const char *fmt, ...)
{
    fio_ssize_t len, avail;
    va_list     ap;

    avail = dbh->max_size - dbh->cur_size;

    va_start(ap, fmt);
    len = kfio_vsnprintf(dbh->buffer + dbh->cur_size, avail,
                         fmt, ap);
    va_end(ap);

    if (len < 0)
    {
        /* Treat error cases as if printf did not happen at all. */
        return 0;
    }

    if (len > avail)
    {
        dbh->cur_size = dbh->max_size;
    }
    else
    {
        dbh->cur_size += len;
    }
    dbh->tot_size += len;

    return len;
}

fio_ssize_t kfio_info_write(kfio_info_data_t *dbh, const void *data, fio_size_t size)
{
    fio_ssize_t avail;

    avail = dbh->max_size - dbh->cur_size;

    if (size < avail)
    {
        avail = size;
    }

    kfio_memcpy(dbh->buffer + dbh->cur_size, data, avail);
    dbh->tot_size += size;
    dbh->cur_size += avail;

    return avail;
}

kfio_info_node_t *kfio_info_data_node(kfio_info_data_t *dbh)
{
    return dbh->node;
}

int kfio_info_data_overflow(kfio_info_data_t *dbh)
{
    return dbh->tot_size > dbh->max_size;
}

fio_size_t kfio_info_data_size_limit(kfio_info_data_t *dbh)
{
    return dbh->max_size;
}

void kfio_info_need_buffer_size(kfio_info_data_t *dbh, fio_size_t size)
{
    kassert(dbh->max_size == 0 && dbh->tot_size == 0);
    dbh->tot_size = size;
}

fio_size_t kfio_info_data_size_valid(kfio_info_data_t *dbh)
{
    return dbh->cur_size;
}

fio_size_t kfio_info_data_size_written(kfio_info_data_t *dbh)
{
    return dbh->tot_size;
}

fio_size_t kfio_info_data_size_free(kfio_info_data_t *dbh)
{
    return dbh->max_size - dbh->cur_size;
}

int kfio_info_data_get_eof(kfio_info_data_t *handle)
{
    return handle->eof != 0;
}

void kfio_info_data_set_eof(kfio_info_data_t *handle, int flag)
{
    handle->eof = (flag != 0);
}

/*
 * Helpers to set/extract values from opaque kfio_info_val_t structures.
 */
void *kfio_info_seq_init(kfio_info_node_t *nodep, fio_loff_t *pos, kfio_info_data_t *dbh)
{
    kassert(nodep != NULL);
    kassert(nodep->type == KFIO_INFO_SEQFILE);
    kassert(nodep->uu.seqf_ops != NULL);

    return nodep->uu.seqf_ops->seq_init(nodep->data, pos, dbh);
}

void *kfio_info_seq_next(kfio_info_node_t *nodep, void *cookie, fio_loff_t *pos)
{
    kassert(nodep != NULL);
    kassert(nodep->type == KFIO_INFO_SEQFILE);
    kassert(nodep->uu.seqf_ops != NULL);

    return nodep->uu.seqf_ops->seq_next(nodep->data, cookie, pos);
}

void kfio_info_seq_stop(kfio_info_node_t *nodep, void *cookie)
{
    kassert(nodep != NULL);
    kassert(nodep->type == KFIO_INFO_SEQFILE);
    kassert(nodep->uu.seqf_ops != NULL);

    nodep->uu.seqf_ops->seq_stop(nodep->data, cookie);
}

int kfio_info_seq_show(kfio_info_node_t *nodep, void *cookie, kfio_info_data_t *dbh)
{
    kassert(nodep != NULL);
    kassert(nodep->type == KFIO_INFO_SEQFILE);
    kassert(nodep->uu.seqf_ops != NULL);

    return nodep->uu.seqf_ops->seq_show(nodep->data, cookie, dbh);
}

/*
 * Helpers to set/extract values from opaque kfio_info_val_t structures.
 */

int kfio_info_handle_cmd(int cmd, kfio_info_val_t *oldval, kfio_info_val_t *newval, void *data)
{
    kassert (oldval == NULL || newval == NULL || newval->size == oldval->size);
    kassert (oldval == NULL || newval == NULL || newval->type == oldval->type);

    if (oldval != NULL)
    {
        kfio_memcpy(oldval->data, data, oldval->size);
    }
    if (newval != NULL)
    {
        kfio_memcpy(data, newval->data, newval->size);
    }
    return 0;
}

int kfio_info_generic_type_handler(kfio_info_node_t *nodep, int cmd,
                                   kfio_info_val_t *oldval, kfio_info_val_t *newval)
{
    if (nodep->type == KFIO_INFO_UINT32 || nodep->type == KFIO_INFO_UINT64 ||
        nodep->type == KFIO_INFO_INT32  || nodep->type == KFIO_INFO_STRING)
    {
        if (nodep->uu.type_handler != NULL)
        {
            return nodep->uu.type_handler(nodep->data, cmd, oldval, newval);
        }
        else
        {
            return kfio_info_handle_cmd(cmd, oldval, newval, nodep->data);
        }
    }
    return -EIO;
}

int kfio_info_generic_text_handler(kfio_info_node_t *nodep, kfio_info_data_t *dbh)
{
    if (nodep->type == KFIO_INFO_TEXT)
    {
        if (nodep->uu.text_handler != NULL)
        {
            return nodep->uu.text_handler(nodep->data, KFIO_INFO_READ, dbh);
        }
    }
    return -EIO;
}

kfio_info_node_t *kfio_info_get_root_node(void)
{
    return kinfo_root_node;
}

FIO_SUPPRESS_MISSING_ANNOTATION_IN_HEADER
FIO_ACQUIRES_MUTEX_OPAQUE(kinfo_lock)
void kfio_info_lock(void)
{
    fusion_mutex_lock(&kinfo_lock);
}

FIO_SUPPRESS_MISSING_ANNOTATION_IN_HEADER
FIO_RELEASES_MUTEX_OPAQUE(kinfo_lock)
void kfio_info_unlock(void)
{
    fusion_mutex_unlock(&kinfo_lock);
}

/**
 * deep find of full node path
 *  kinfo_lock must be held for reading by caller
 */
kfio_info_node_t *kfio_info_find_node(kfio_info_node_t *topnode, char *fullname)
{
    kfio_info_node_t *nodep;
    char *p, *q;
    char orig;

    nodep = topnode;
    p = fullname;

    //first round, nodep and p are at the same level (root)
    //after that, p is the subnode which we will search under nodep, i.e. nodep is one level up than p
    while (nodep != NULL)
    {
        for (; *p == '.'; p++)
        {
            continue;
        }
        for (q = p; (*q) && (*q != '.'); q++)
        {
            continue;
        }
        if (p == q)
        {
            return nodep;
        }
        orig = *q;
        *q = '\x0';

        if (nodep->type == KFIO_INFO_DIR)   //not leaf node, go ahead search for the matching subnode
        {
            nodep = kfio_info_lookup_node(nodep, p);
        }
        else        //this is a leaf node, can't search into subnodes
        {
            nodep = NULL;
        }

        *q = orig;
        p = q;
    }

    return nodep;
}

/**
* @brief given node, construct full pathname into pathname
* @return number of bytes used
*  kinfo_lock must be held by caller for reading
*/
int kfio_info_node_pathname(kfio_info_node_t *topnode, kfio_info_node_t *nodep, char *pathname, int len)
{
    char *cur = pathname + len;
    int   sln = 0;

    while (nodep != topnode)
    {
        sln = (int)kfio_strlen(nodep->name);

        /* Adjust the cur pointer to accommodate this path component and terminating character. */
        cur -= sln + 1;

        /* If there is no space left to acomodate full name component, return error. */
        if (cur < pathname)
        {
            return -EINVAL;
        }

        /* Copy the name at the end of the current buffer, adjust pointers accordingly. */
        kfio_memcpy(cur, nodep->name, sln);
        cur[sln] = '.';

        nodep = nodep->parent;
    }

    /* Calculate the path length. */
    sln = len - (int)(cur - pathname);

    /*
     * If there is anything on the buffer, it needs to be trimmed to kill the
     * terminating path separator.
     */
    if (sln > 0)
    {
        sln--;
    }

    /*
     * Move data backward in memory so that the string is left aligned in
     * the buffer.
     */
    if (cur > pathname)
    {
        kfio_memmove(pathname, cur, sln);
    }

    /* Replace last path component terminator with NUL character. */
    pathname[sln] = '\0';

    return sln;
}

/**
 * Iterate through node list building full node pathnames
 * @return number of bytes used , negative => error
 *  kinfo_lock must be held by caller for reading
 */
int kfio_info_node_walk_tree(kfio_info_node_t *topnode, kfio_info_node_t *startnode, kfio_info_data_t *dbh)
{
    char parent_path[KFIO_INFO_MAX_FULLPATHLEN];
    int  retval, parent_pathlen;
    kfio_info_node_t    *nodep, *parent;
    fusion_list_entry_t *entry;

    if (startnode == NULL || startnode->type != KFIO_INFO_DIR)
    {
        return -EINVAL;
    }

    /* Calculate the parent path, leaving one character for the patch terminator at the end. */
    retval = kfio_info_node_pathname(topnode, startnode, parent_path, sizeof(parent_path) - 1);
    if (retval < 0)
    {
        return retval;
    }

    parent_pathlen = retval;
    kassert(parent_pathlen <= sizeof(parent_path) - 2);

    /* Append the path separator if there is anything in the buffer. */
    if (parent_pathlen > 0)
    {
        parent_path[parent_pathlen++] = '.';
        parent_path[parent_pathlen]   = '\0';
    }

    /* Start by enumerating the top node content. */
    parent = startnode;
    entry = &parent->uu.subnode_list;

    for (/* none */; /* none */; /* none */)
    {
        entry = entry->next;

        kassert(parent != NULL); /* Makes Windows code analysis happy */

        /* Reached the end of the current parent subnode list. Try to back one step up. */
        if (entry == &parent->uu.subnode_list)
        {
            /* Reached end of the top level parent. Walk is complete. */
            if (parent == startnode)
            {
                break;
            }

            /* Trim parent_path. */
            parent_pathlen -= (int)kfio_strlen(parent->name) + 1;
            if (parent_pathlen < 0)
            {
                parent_pathlen = 0;
            }
            parent_path[parent_pathlen] = '\0';

            /* Resume iteration of the previos parent node. */
            entry  = &parent->node_entry;
            parent = parent->parent;

            continue;
        }

        nodep = fusion_list_entry(entry, kfio_info_node_t, node_entry);

        /*
         * We found the new sub-dir entry. Arrange for the scan to continue starting with its
         * first child and restart the loop.
         */
        if (nodep->type == KFIO_INFO_DIR)
        {
            int new_pathlen;

            /* Append component to the current parent path. */
            new_pathlen = (int)kfio_strlen(nodep->name) + 1;

            if (parent_pathlen + new_pathlen >= sizeof(parent_path))
            {
                infprint("Skipping entry with long path '%s%s' (len %d)\n",
                         parent_path, nodep->name, parent_pathlen + new_pathlen);
                continue;
            }

            kfio_memcpy(&parent_path[parent_pathlen], nodep->name, new_pathlen);
            parent_pathlen += new_pathlen;
            parent_path[parent_pathlen - 1] = '.';
            parent_path[parent_pathlen] = '\0';

            /* Arrange for the loop to continue with this entry as a parent. */
            parent = nodep;
            entry  = &nodep->uu.subnode_list;

            continue;
        }

        /*
         * Copy the full parent path into the buffer and append the current node name to it.
         */
        kfio_info_write(dbh, parent_path, parent_pathlen);
        kfio_info_write(dbh, nodep->name, kfio_strlen(nodep->name) + 1);
    }

    return 0;
}

/**
 * @brief Initialize global kinfo state.
 */
int kfio_info_driver_init(void)
{
    int rc = 0;

    kassert(kinfo_root_node == NULL);

    /*
     * Initialize fake root node for compatibility reasons.
     */
    kinfo_root_node = kfio_info_alloc_node(KFIO_INFO_DIR, UFIO_KINFO_ROOT);
    if (kinfo_root_node == NULL)
    {
        return -ENOMEM;
    }
    kinfo_root_node->mode = S_IRUGO;
    fusion_init_list(&kinfo_root_node->uu.subnode_list);

    fusion_mutex_init(&kinfo_lock, "fio_kinfo");

#if KFIO_INFO_USE_OS_BACKEND
    kassert(kinfo_os_init == 0);

    rc = kfio_info_os_driver_init();
    /* Undo the initialization made so far. */
    if (rc != 0)
    {
        kfio_info_driver_fini();
        return rc;
    }
    kinfo_os_init = 1;
#endif
    return rc;
}

/**
 * @brief Destroy global kinfo state.
 */
void kfio_info_driver_fini(void)
{
#if KFIO_INFO_USE_OS_BACKEND
    if (kinfo_os_init)
    {
        kfio_info_os_driver_fini();
        kinfo_os_init = 0;
    }
#endif
    if (kinfo_root_node != NULL)
    {
        kfio_info_free_node(kinfo_root_node);
        kinfo_root_node = NULL;

        fusion_mutex_destroy(&kinfo_lock);
    }
}
