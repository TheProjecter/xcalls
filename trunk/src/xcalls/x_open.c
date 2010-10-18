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
 * \file x_open.c
 *
 * \brief x_open implementation.
 */

#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <misc/debug.h>
#include <misc/errno.h>
#include <core/tx.h>
#include <core/config.h>
#include <core/koa.h>
#include <core/buffer.h>
#include <core/txdesc.h>
#include <core/stats.h>
#include <libc/syscalls.h>
#include <xcalls/xcalls.h>



typedef struct x_open_undo_args_s x_open_undo_args_t;

struct x_open_undo_args_s {
	int       fd;
	txc_koa_t *koa;
};


static
void
x_open_undo(void *args, int *result)
{
	int                local_errno = 0; 
	x_open_undo_args_t *myargs = (x_open_undo_args_t *) args;
	txc_koamgr_t       *koamgr;

	koamgr = txc_koa_get_koamgr(myargs->koa);
	txc_koa_lock_alias_cache(koamgr);
	txc_koa_lock_fd(koamgr, myargs->fd);
	txc_koa_detach_fd(myargs->koa, myargs->fd, 0);
	if (txc_libc_close(myargs->fd) < 0) {
		local_errno = errno;
	}	
	txc_koa_unlock_fd(koamgr, myargs->fd);
	txc_koa_unlock_alias_cache(koamgr);
	if (result) {
		*result = local_errno;
	}
}


/**
 * \brief Opens a file.
 * 
 * <b> Execution </b>: in-place
 *
 * <b> Asynchronous failures </b>: abort
 *
 * \param[in] pathname  
 * \param[out] result Where to store any asynchronous failures.
 * \return The new file descriptor, or -1 if a synchronous failure occurred 
 * (in which case, errno is set appropriately).
 */
int
XCALL_DEF(x_open)(const char *pathname, int flags, mode_t mode, int *result) 
{
	txc_tx_t           *txd;
	txc_koamgr_t       *koamgr = txc_g_koamgr;
	txc_koa_t          *koa;
	txc_sentinel_t     *sentinel;
	txc_result_t       xret;
	int                fildes;
	int                ret;
	ino_t              inode;
	x_open_undo_args_t *args_undo; 
	int                open_flags = O_RDWR | flags;
	int                local_result = 0;

	txd = txc_tx_get_txd();

	switch(txc_tx_get_xactstate(txd)) {
		case TXC_XACTSTATE_TRANSACTIONAL_RETRYABLE:
			/* Serialize open/create/close file operations to detect aliasing */
			txc_koa_lock_alias_cache(koamgr);
			txc_koa_path2inode(pathname, &inode);
			if (inode == 0) {
				/* File does not exist. */
				txc_koa_unlock_alias_cache(koamgr);
				local_result = EACCES;
				ret = -1;
				goto done;
			} else {
				if ((ret = fildes = txc_libc_open(pathname, open_flags, mode)) < 0) { 
					txc_koa_unlock_alias_cache(koamgr);
					local_result = errno;
					goto done;
				}
				if (txc_koa_alias_cache_lookup_inode(koamgr, inode, &koa) 
				    == TXC_R_SUCCESS) 
				{
					/* 
					 * CASE 1:
					 *  - KOA exists in the alias cache so other in-flight 
					 *    transaction may operate on the file.
				 	 */
					txc_koa_lock_fds_refby_koa(koa);
					txc_koa_lock_fd(koamgr, fildes);
					txc_koa_attach_fd(koa, fildes, 0);
					sentinel = txc_koa_get_sentinel(koa);
					xret = txc_sentinel_tryacquire(txd, sentinel, 0);
					if (xret == TXC_R_BUSYSENTINEL) {
						txc_libc_close(fildes);
						txc_koa_detach_fd(koa, fildes, 0);
						/* 
						 * Explicitly release the lock on fildes because we 
						 * have detached it from the KOA.
						 */
						txc_koa_unlock_fd(koamgr, fildes);
						txc_koa_unlock_fds_refby_koa(koa);
						txc_koa_unlock_alias_cache(koamgr);
						txc_tx_abort_transaction(txd, TXC_ABORTREASON_BUSYSENTINEL);
						TXC_INTERNALERROR("Never gets here. Transaction abort failed.\n");
					}

					args_undo = (x_open_undo_args_t *)
					            txc_buffer_linear_malloc(txd->buffer_linear, 
					                                     sizeof(x_open_undo_args_t));
					if (args_undo == NULL) {
						TXC_INTERNALERROR("Allocation failed. Linear buffer out of space.\n");
					}
					args_undo->koa = koa;
					args_undo->fd = fildes;

					txc_tx_register_undo_action  (txd, x_open_undo, 
					                              (void *) args_undo, result,
					                              TXC_KOA_CREATE_UNDO_ACTION_ORDER);

					/* 
					 * Don't need to explicitly release the lock on fildes 
					 * because it is attached to KOA koa, so it will be  
					 * released by the following operation:
					 *   txc_koa_unlock_fds_refby_koa(koa);
					 */
					txc_koa_unlock_fds_refby_koa(koa);
					txc_koa_unlock_alias_cache(koamgr);
					ret = fildes;
				} else {
					/* 
					 * CASE 2:
					 * - KOA does not exist in the alias cache so there is no
					 *   way for some other in-flight transaction to have a reference 
					 *   to the file to operate on it.
				 	 */
					txc_koa_create(koamgr, &koa, TXC_KOA_IS_FILE, (void *) inode);
					txc_koa_lock_fd(koamgr, fildes);
					txc_koa_attach_fd(koa, fildes, 0);
					sentinel = txc_koa_get_sentinel(koa);
					xret = txc_sentinel_tryacquire(txd, sentinel, 0);
					if (xret != TXC_R_SUCCESS) {
						TXC_INTERNALERROR("Cannot acquire the sentinel of the KOA I've just created!\n");
					}
					args_undo = (x_open_undo_args_t *)
					            txc_buffer_linear_malloc(txd->buffer_linear, 
					                                     sizeof(x_open_undo_args_t));
					if (args_undo == NULL) {
						TXC_INTERNALERROR("Allocation failed. Linear buffer out of space.\n");
					}
					args_undo->koa = koa;
					args_undo->fd = fildes;

					txc_tx_register_undo_action  (txd, x_open_undo, 
					                              (void *) args_undo, result,
					                              TXC_KOA_CREATE_UNDO_ACTION_ORDER);

					txc_koa_unlock_fd(koamgr, fildes);
					txc_koa_unlock_alias_cache(koamgr);
					ret = fildes;
				}
			}
			txc_stats_txstat_increment(txd, XCALL, x_open, 1);
			break;
		case TXC_XACTSTATE_TRANSACTIONAL_IRREVOCABLE:
		case TXC_XACTSTATE_NONTRANSACTIONAL:
			/* 
			 * Even if xCalls don't provide transactional properties when called
			 * outside of a transaction, we still need to create a KOA and assign
			 * a sentinel to be used later by any transactional xCalls. Thus we
			 * still need to properly synchronize accesses to internal data
			 * structures.
			 */
			txc_koa_lock_alias_cache(koamgr);
			if ((ret = fildes = txc_libc_open(pathname, open_flags, mode)) < 0) { 
				txc_koa_unlock_alias_cache(koamgr);
				local_result = errno;
				goto done;
			}
			txc_koa_path2inode(pathname, &inode);
			if (txc_koa_alias_cache_lookup_inode(koamgr, inode, &koa) 
			    != TXC_R_SUCCESS) 
			{
				txc_koa_create(koamgr, &koa, TXC_KOA_IS_FILE, (void *) inode);
			}	
			txc_koa_lock_fd(koamgr, fildes);
			txc_koa_attach_fd(koa, fildes, 0);
			sentinel = txc_koa_get_sentinel(koa);
			txc_koa_unlock_fd(koamgr, fildes);
			txc_koa_unlock_alias_cache(koamgr);
			ret = fildes;
	}
done:
	if (result) {
		*result = local_result;
	}
	return ret;
}
