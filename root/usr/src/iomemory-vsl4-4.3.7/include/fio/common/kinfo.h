//-----------------------------------------------------------------------------
// Copyright (c) 2014 Fusion-io, Inc.(acquired by SanDisk Corp. 2014)
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
//-----------------------------------------------------------------------------

#ifndef _COMMON_KINFO_H
#define _COMMON_KINFO_H

kfio_info_node_t *kfio_info_get_root_node(void);
void kfio_info_lock(void);
void kfio_info_unlock(void);
kfio_info_node_t *kfio_info_find_node(kfio_info_node_t *topnode, char *fullname);
int kfio_info_node_pathname(kfio_info_node_t *topnode, kfio_info_node_t *nodep, char *pathname, int len);
int kfio_info_node_walk_tree(kfio_info_node_t *topnode, kfio_info_node_t *startnode, kfio_info_data_t *dbh);
char *kfio_info_get_data_buffer(kfio_info_data_t *dbh);
fio_size_t kfio_info_get_node_size(kfio_info_node_t *nodep);
int kfio_info_get_node_type(kfio_info_node_t *nodep);


#endif // _COMMON_KINFO_H
