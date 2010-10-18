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
 * \file x_rename.c
 *
 * \brief x_rename implementation
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


typedef struct x_rename_commit_undo_args_s x_rename_commit_undo_args_t;

struct x_rename_commit_undo_args_s {
	txc_koa_t *oldpath_koa;
	txc_koa_t *newpath_koa;
	int       newpath_koa_is_valid;
	char      oldpath[TXC_MAX_LEN_PATHNAME];
	char      newpath[TXC_MAX_LEN_PATHNAME];
	char      temppath[TXC_MAX_LEN_PATHNAME];
};


static
void
x_rename_commit(void *args, int *result)
{
	int                         local_errno = 0; 
	x_rename_commit_undo_args_t *myargs = (x_rename_commit_undo_args_t *) args;
	txc_koamgr_t                *koamgr;

	koamgr = txc_koa_get_koamgr(myargs->oldpath_koa);
	txc_koa_lock_alias_cache(koamgr);
	txc_koa_lock_fds_refby_koa(myargs->oldpath_koa);
	if (myargs->newpath_koa_is_valid) {
		txc_koa_lock_fds_refby_koa(myargs->newpath_koa);
	}	
	txc_koa_detach(myargs->oldpath_koa);
	if (myargs->newpath_koa_is_valid) {
		txc_koa_detach(myargs->newpath_koa);
	}
	if (txc_libc_unlink(myargs->oldpath) < 0) {
		local_errno = errno;
	}	
	if (myargs->newpath_koa_is_valid) {
		txc_koa_unlock_fds_refby_koa(myargs->newpath_koa);
	}	
	txc_koa_unlock_fds_refby_koa(myargs->oldpath_koa);
	txc_koa_unlock_alias_cache(koamgr);
	if (result) {
		*result = local_errno;
	}
}


static
void
x_rename_undo(void *args, int *result)
{
	int                         local_errno = 0; 
	x_rename_commit_undo_args_t *myargs = (x_rename_commit_undo_args_t *) args;
	txc_koamgr_t                *koamgr;

	koamgr = txc_koa_get_koamgr(myargs->oldpath_koa);
	txc_koa_lock_alias_cache(koamgr);
	txc_koa_lock_fds_refby_koa(myargs->oldpath_koa);
	if (myargs->newpath_koa_is_valid) {
		txc_koa_lock_fds_refby_koa(myargs->newpath_koa);
	}	
	txc_koa_detach(myargs->oldpath_koa);
	if (myargs->newpath_koa_is_valid) {
		txc_koa_detach(myargs->newpath_koa);
	}
	if (txc_libc_unlink(myargs->newpath) < 0) {
		local_errno = errno;
	}	
	if (myargs->newpath_koa_is_valid) {
		if (txc_libc_rename(myargs->temppath, myargs->newpath) < 0) {
			local_errno = errno;
		}	
	}
	if (myargs->newpath_koa_is_valid) {
		txc_koa_unlock_fds_refby_koa(myargs->newpath_koa);
	}	
	txc_koa_unlock_fds_refby_koa(myargs->oldpath_koa);
	txc_koa_unlock_alias_cache(koamgr);
	if (result) {
		*result = local_errno;
	}
}



/**
 * \brief Changes the name or location of a file.
 * 
 * If a file at the new location exists, then it is renamed to a temporary file
 * and removed only when the transaction commits. This allows restoring the file
 * in case the transaction aborts.
 *
 * <b> Execution </b>: in-place
 *
 * <b> Asynchronous failures </b>: commit, abort
 *
 * \param[in] oldpath The old location of the file.
 * \param[in] oldpath The new location of the file.
 * \param[out] result Where to store any asynchronous failures.
 * \return 0 on success, or -1 if a synchronous failure occurred 
 * (in which case, errno is set appropriately).
 */
int
XCALL_DEF(x_rename)(const char *oldpath, const char *newpath, int *result) 
{
	txc_tx_t                    *txd;
	txc_koamgr_t                *koamgr = txc_g_koamgr;
	txc_koa_t                   *oldpath_koa;
	txc_koa_t                   *newpath_koa;
	txc_sentinel_t              *sentinel;
	txc_result_t                xret;
	int                         ret;
	int                         newpath_koa_is_valid = 0;
	ino_t                       oldpath_inode;
	ino_t                       newpath_inode;
	char                        temppath[128]; 
	x_rename_commit_undo_args_t *args_commit_undo; 
	int                         local_result = 0;

	txd = txc_tx_get_txd();

	switch(txc_tx_get_xactstate(txd)) {
		case TXC_XACTSTATE_TRANSACTIONAL_RETRYABLE:
			txc_koa_lock_alias_cache(koamgr);

			txc_koa_path2inode(oldpath, &oldpath_inode);
			if (oldpath_inode == 0) {
				txc_koa_unlock_alias_cache(koamgr);
				local_result = ENOENT;
				ret = -1;
				goto done;
			}

			/* Get the sentinel on the oldpath */
			if (txc_koa_alias_cache_lookup_inode(koamgr, oldpath_inode, &oldpath_koa) 
			    == TXC_R_SUCCESS) 
			{
				txc_koa_lock_fds_refby_koa(oldpath_koa);
				txc_koa_attach(oldpath_koa);
				sentinel = txc_koa_get_sentinel(oldpath_koa);
				xret = txc_sentinel_tryacquire(txd, sentinel, 
				                               TXC_SENTINEL_ACQUIREONRETRY);
				if (xret == TXC_R_BUSYSENTINEL) {
					txc_koa_detach(oldpath_koa);
					txc_koa_unlock_fds_refby_koa(oldpath_koa);
					txc_koa_unlock_alias_cache(koamgr);
					txc_tx_abort_transaction(txd, TXC_ABORTREASON_BUSYSENTINEL);
					TXC_INTERNALERROR("Never gets here. Transaction abort failed.\n");
				}
				txc_koa_unlock_fds_refby_koa(oldpath_koa);
			} else {
				/* No KOA for oldpath; create it */
				txc_koa_create(koamgr, &oldpath_koa, TXC_KOA_IS_FILE, (void *) oldpath_inode);
				txc_koa_attach(oldpath_koa);
				sentinel = txc_koa_get_sentinel(oldpath_koa);
				xret = txc_sentinel_tryacquire(txd, sentinel, 0);
				if (xret != TXC_R_SUCCESS) {
					TXC_INTERNALERROR("Cannot acquire the sentinel of the KOA I've just created!\n");
				}
			}

			/* Get the sentinel on the newpath */
			txc_koa_path2inode(newpath, &newpath_inode);
			if (newpath_inode == 0) {
				/* 
				 * Newpath does not exist so we don't need to acquire a sentinel
				 * on it. When we do the rename of the oldpath to the newpath 
				 * the sentinel we already got on oldpath will also protect the 
				 * newpath because we acquire sentinels on inodes and inodes don't
				 * change on rename operations.
				 */
			} else {
				if (txc_koa_alias_cache_lookup_inode(koamgr, newpath_inode, &newpath_koa) 
				    == TXC_R_SUCCESS) 
				{
					txc_koa_lock_fds_refby_koa(newpath_koa);
					txc_koa_attach(newpath_koa);
					sentinel = txc_koa_get_sentinel(newpath_koa);
					xret = txc_sentinel_tryacquire(txd, sentinel, 0);
					if (xret == TXC_R_BUSYSENTINEL) {
						txc_koa_detach(newpath_koa);
						txc_koa_unlock_fds_refby_koa(newpath_koa);
						txc_koa_unlock_alias_cache(koamgr);
						txc_tx_abort_transaction(txd, TXC_ABORTREASON_BUSYSENTINEL);
						TXC_INTERNALERROR("Never gets here. Transaction abort failed.\n");
					} else {
						txc_koa_unlock_fds_refby_koa(newpath_koa);
					}
				} else {
					/* No KOA for newpath; create it */
					txc_koa_create(koamgr, &newpath_koa, TXC_KOA_IS_FILE, (void *) newpath_inode);
					txc_koa_attach(newpath_koa);
					sentinel = txc_koa_get_sentinel(newpath_koa);
					xret = txc_sentinel_tryacquire(txd, sentinel, 0);
					if (xret != TXC_R_SUCCESS) {
						TXC_INTERNALERROR("Cannot acquire the sentinel of the KOA I've just created!\n");
					}
				}
				/* 
				 * Rename the newpath to a temp name to be able to restore it
				 * in case of abort
				 */
				strcpy(temppath, "/tmp/libtxc.tmp.XXXXXX");
				mktemp(temppath); 
				txc_libc_rename(newpath, temppath);
				newpath_koa_is_valid = 1;
			}
			/* OK. We have isolation. Now, proceed with the renaming */

			/* 
			 * We need to keep oldpath pointing to the file's inode so that 
			 * sentinel conflict detection can work properly. So we don't use 
			 * the rename syscall, but rather we create a new hard link named
			 * newpath. Later on commit we drop the oldpath link to complete
			 * the rename.
			 */
			txc_libc_link(oldpath, newpath);

			args_commit_undo = (x_rename_commit_undo_args_t *)
			                   txc_buffer_linear_malloc(txd->buffer_linear, 
			                                            sizeof(x_rename_commit_undo_args_t));
			if (args_commit_undo == NULL) {
				TXC_INTERNALERROR("Allocation failed. Linear buffer out of space.\n");
			}

			strcpy(args_commit_undo->temppath, temppath);
			strcpy(args_commit_undo->newpath, newpath);
			strcpy(args_commit_undo->oldpath, oldpath);
			args_commit_undo->newpath_koa = newpath_koa;
			args_commit_undo->oldpath_koa = oldpath_koa;
			args_commit_undo->newpath_koa_is_valid = newpath_koa_is_valid;

			txc_tx_register_commit_action(txd, x_rename_commit, 
			                              (void *) args_commit_undo, result,
			                              TXC_KOA_CREATE_COMMIT_ACTION_ORDER);
			txc_tx_register_undo_action  (txd, x_rename_undo, 
			                              (void *) args_commit_undo, result,
			                              TXC_KOA_CREATE_UNDO_ACTION_ORDER);

			txc_koa_unlock_alias_cache(koamgr);
			ret = 0;
			txc_stats_txstat_increment(txd, XCALL, x_rename, 1);
			break;
		case TXC_XACTSTATE_TRANSACTIONAL_IRREVOCABLE:
		case TXC_XACTSTATE_NONTRANSACTIONAL:
			if ((ret = txc_libc_rename(oldpath, newpath)) < 0) { 
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
