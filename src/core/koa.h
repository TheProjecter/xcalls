/*
    Copyright (C) 2008-2009 Computer Sciences Department, 
    University of Wisconsin -- Madison

    ----------------------------------------------------------------------

    This file is part of the xCalls transactional API, originally 
    developed at the University of Wisconsin -- Madison.

    xCalls was originally developed primarily by Haris Volos and 
    Neelam Goyal with contributions from Andres Jaan Tack.

    ----------------------------------------------------------------------

    xCalls is free software: you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as 
    published by the Free Software Foundation, either version 3 of 
    the License, or (at your option) any later version.

    xCalls is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public 
    License along with xCalls.  If not, see <http://www.gnu.org/licenses/>.

### END HEADER ###
*/

/**
 * \file koa.h
 *
 * \brief KOA manager interface.
 *
 */

#ifndef _TXC_KOA_H
#define _TXC_KOA_H

#include <misc/result.h>
#include <core/sentinel.h>
#include <core/buffer.h>
#include <sys/stat.h>


#define TXC_KOA_INVALID                     -1 /**< Invalid KOA */
#define TXC_KOA_IS_SOCK_DGRAM               1  /**< KOA for a datagram socket */
#define TXC_KOA_IS_SOCK_STREAM              2  /**< KOA for a stream socket */
#define TXC_KOA_IS_PIPE_READ_END            3  /**< KOA for a pipe's read end */
#define TXC_KOA_IS_PIPE_WRITE_END           4  /**< KOA for a pipe's write end */
#define TXC_KOA_IS_FILE                     5  /**< KOA for a file */

typedef struct txc_koa_s txc_koa_t;
typedef struct txc_koamgr_s txc_koamgr_t;

extern txc_koamgr_t *txc_g_koamgr;

txc_result_t txc_koamgr_create(txc_koamgr_t **, txc_sentinelmgr_t *, txc_buffermgr_t *); 
void txc_koamgr_destroy(txc_koamgr_t **);
txc_result_t txc_koa_create(txc_koamgr_t *, txc_koa_t **, int, void *);
void txc_koa_destroy(txc_koa_t **);
txc_result_t txc_koa_path2inode(const char *, ino_t *); 
txc_result_t txc_koa_attach(txc_koa_t *);
txc_result_t txc_koa_detach(txc_koa_t *);
txc_result_t txc_koa_attach_fd(txc_koa_t *koa, int fd, int lock);
txc_result_t txc_koa_detach_fd(txc_koa_t *koa, int fd, int lock);
txc_result_t txc_koa_lock_alias_cache(txc_koamgr_t *koamgr);
txc_result_t txc_koa_unlock_alias_cache(txc_koamgr_t *koamgr);
txc_result_t txc_koa_lookup_fd2koa(txc_koamgr_t *, int, txc_koa_t **);
txc_result_t txc_koa_alias_cache_lookup_inode(txc_koamgr_t *koamgr, ino_t inode_number, txc_koa_t **koap);
txc_result_t txc_koa_lock_fd(txc_koamgr_t *koamgr, int fd);
txc_result_t txc_koa_unlock_fd(txc_koamgr_t *koamgr, int fd);
txc_result_t txc_koa_lock_fds_refby_koa(txc_koa_t *koa);
txc_result_t txc_koa_unlock_fds_refby_koa(txc_koa_t *koa);
txc_sentinel_t *txc_koa_get_sentinel(txc_koa_t *koa);
txc_koamgr_t *txc_koa_get_koamgr(txc_koa_t *koa);
void *txc_koa_get_buffer(txc_koa_t *koa);

#endif /* _TXC_KOA_H */
