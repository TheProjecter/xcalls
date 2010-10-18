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
 * \file x_unlink.c
 *
 * \brief x_unlink implementation.
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


typedef struct x_unlink_commit_args_s x_unlink_commit_args_t;

struct x_unlink_commit_args_s {
	int       fd;
	txc_koa_t *koa;
	char      pathname[TXC_MAX_LEN_PATHNAME];
};


static
void
x_unlink_commit(void *args, int *result)
{
	int                    local_errno = 0; 
	x_unlink_commit_args_t *myargs = (x_unlink_commit_args_t *) args;
	txc_koamgr_t           *koamgr;
	txc_result_t           xret;

	koamgr = txc_koa_get_koamgr(myargs->koa);
	txc_koa_lock_alias_cache(koamgr);
	txc_koa_lock_fds_refby_koa(myargs->koa);
	xret = txc_koa_detach(myargs->koa);
	if (xret == TXC_R_SUCCESS) {
		if (txc_libc_unlink(myargs->pathname) < 0) {
			local_errno = errno;
		}	
	} else {
		local_errno = EBADF;

	}
	txc_koa_unlock_fds_refby_koa(myargs->koa);
	txc_koa_unlock_alias_cache(koamgr);
	if (result) {
		*result = local_errno;
	}
}


/**
 * \brief Deletes a name and possibly the file it refers to.
 * 
 * <b> Execution </b>: deferred
 *
 * <b> Asynchronous failures </b>: commit
 *
 * \param[in] pathname The location of the file.
 * \param[out] result Where to store any asynchronous failures.
 * \return 0 on success, or -1 if a synchronous failure occurred 
 * (in which case, errno is set appropriately).
 */

int
XCALL_DEF(x_unlink)(const char *pathname, int *result) 
{
	txc_tx_t               *txd;
	txc_koamgr_t           *koamgr = txc_g_koamgr;
	txc_koa_t              *koa;
	txc_sentinel_t         *sentinel;
	txc_result_t           xret;
	int                    ret;
	ino_t                  inode;
	x_unlink_commit_args_t *args_commit; 
	int                    local_result = 0;

	txd = txc_tx_get_txd();

	switch(txc_tx_get_xactstate(txd)) {
		case TXC_XACTSTATE_TRANSACTIONAL_RETRYABLE:
			txc_koa_lock_alias_cache(koamgr);
			txc_koa_path2inode(pathname, &inode);
			if (inode == 0) {
				/* File does not exist. */
				txc_koa_unlock_alias_cache(koamgr);
				local_result = EACCES;
				ret = -1;
				goto done;
			} else {
				if (txc_koa_alias_cache_lookup_inode(koamgr, inode, &koa) 
				    == TXC_R_SUCCESS) 
				{
					/* 
					 * CASE 1:
					 *  - KOA exists in the alias cache so other in-flight 
					 *    transaction may operate on the file.
				 	 */
					txc_koa_lock_fds_refby_koa(koa);
					txc_koa_attach(koa);
					sentinel = txc_koa_get_sentinel(koa);
					xret = txc_sentinel_tryacquire(txd, sentinel, 
					                               TXC_SENTINEL_ACQUIREONRETRY);
					if (xret == TXC_R_BUSYSENTINEL) {
						txc_koa_detach(koa);
						txc_koa_unlock_fds_refby_koa(koa);
						txc_koa_unlock_alias_cache(koamgr);
						txc_tx_abort_transaction(txd, TXC_ABORTREASON_BUSYSENTINEL);
						TXC_INTERNALERROR("Never gets here. Transaction abort failed.\n");
					}
					txc_koa_unlock_fds_refby_koa(koa);
				} else {
					/* 
					 * CASE 2:
					 * - KOA does not exist in the alias cache so there is no
					 *   way for some other in-flight transaction to have a reference 
					 *   to the file to operate on it.
				 	 */
					txc_koa_create(koamgr, &koa, TXC_KOA_IS_FILE, (void *) inode);
					txc_koa_attach(koa);
					sentinel = txc_koa_get_sentinel(koa);
					xret = txc_sentinel_tryacquire(txd, sentinel, 0);
					if (xret != TXC_R_SUCCESS) {
						TXC_INTERNALERROR("Cannot acquire the sentinel of the KOA I've just created!\n");
					}
				}
				args_commit = (x_unlink_commit_args_t *)
				              txc_buffer_linear_malloc(txd->buffer_linear, 
				                                       sizeof(x_unlink_commit_args_t));
				if (args_commit == NULL) {
					TXC_INTERNALERROR("Allocation failed. Linear buffer out of space.\n");
				}

				args_commit->koa = koa;
				strcpy(args_commit->pathname, pathname);

				txc_tx_register_commit_action(txd, x_unlink_commit, 
				                              (void *) args_commit, result,
				                              TXC_KOA_DESTROY_COMMIT_ACTION_ORDER);
				
				txc_koa_unlock_alias_cache(koamgr);
				ret = 0;
			}
			txc_stats_txstat_increment(txd, XCALL, x_unlink, 1);
			break;

		case TXC_XACTSTATE_TRANSACTIONAL_IRREVOCABLE:
		case TXC_XACTSTATE_NONTRANSACTIONAL:
			if ((ret = txc_libc_unlink(pathname)) < 0) { 
				local_result = errno;
			}
			ret = 0;
	}	
done:
	if (result) {
		*result = local_result;
	}
	return ret;
}
