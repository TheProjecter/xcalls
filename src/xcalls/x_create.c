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
 * \file x_create.c
 *
 * \brief x_create implementation.
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
#include <core/stats.h>
#include <libc/syscalls.h>
#include <core/txdesc.h>
#include <xcalls/xcalls.h>


/**
 * INVARIANT: Files never disappear until commit. This simplifies conflict 
 * detection since we can acquire sentinels on inode numbers.
 *
 */

typedef struct x_create_case1_commit_undo_args_s   x_create_case1_commit_undo_args_t;

struct x_create_case1_commit_undo_args_s {
	int          fd;
	txc_koa_t    *koa;
	char         pathname[TXC_MAX_LEN_PATHNAME];
};


static
void
x_create_case1_undo(void *args, int *result)
{
	int                                ret1;
	int                                local_result = 0; 
	x_create_case1_commit_undo_args_t *myargs = (x_create_case1_commit_undo_args_t *) args;
	txc_koamgr_t                       *koamgr;

	koamgr = txc_koa_get_koamgr(myargs->koa);
	txc_koa_lock_alias_cache(koamgr);
	if ((ret1 = txc_libc_unlink(myargs->pathname)) < 0) {
		local_result = errno;
	}
	txc_koa_detach_fd(myargs->koa, myargs->fd, 1);
	if (txc_libc_close(myargs->fd) < 0) {
		if (ret1 == 0) {
			local_result = errno;
		}	
	}	
	txc_koa_unlock_alias_cache(koamgr);
	if (result) {
		*result = local_result;
	}
}


/*
 * Case 2a: commit and undo actions
 */

typedef struct x_create_case2_commit_undo_args_s   x_create_case2_commit_undo_args_t;

struct x_create_case2_commit_undo_args_s {
	int          fd;
	txc_koa_t    *koa;
	char         temp_pathname[TXC_MAX_LEN_PATHNAME];
	char         original_pathname[TXC_MAX_LEN_PATHNAME];

};


static
void
x_create_case2_commit(void *args, int *result)
{
	int                                local_result = 0;
	x_create_case2_commit_undo_args_t *myargs = (x_create_case2_commit_undo_args_t *) args;

	if ((txc_libc_unlink(myargs->temp_pathname) < 0)) {
		local_result = errno;
	}	
	if (result) {
		*result = local_result;
	}
}


static
void
x_create_case2_undo(void *args, int *result)
{
	int                                ret1;
	int                                ret2;
	int                                local_result = 0; 
	x_create_case2_commit_undo_args_t *myargs = (x_create_case2_commit_undo_args_t *) args;
	txc_koamgr_t                       *koamgr;

	koamgr = txc_koa_get_koamgr(myargs->koa);
	txc_koa_lock_alias_cache(koamgr);
	if ((ret1 = txc_libc_unlink(myargs->original_pathname)) < 0) {
		local_result = errno;
	}
	if ((ret2 = txc_libc_rename(myargs->temp_pathname, 
	                            myargs->original_pathname)) < 0) 
	{
		if (ret1 == 0) {
			local_result = errno;
		}	
	}	
	txc_koa_detach_fd(myargs->koa, myargs->fd, 1);
	if (txc_libc_close(myargs->fd) < 0) {
		if (ret1 == 0 && ret2 == 0) {
			local_result = errno;
		}	
	}	
	txc_koa_unlock_alias_cache(koamgr);
	if (result) {
		*result = local_result;
	}
}


/**
 * \brief Creates a file.
 * 
 * Creates a file using the creation flags: O_CREAT| O_NOCTTY| O_TRUNC| O_RDWR.
 * If a file already exists then it is renamed and removed if transaction finally 
 * commits.
 *
 * <b> Execution </b>: in-place
 *
 * <b> Asynchronous failures </b>: commit, abort
 *
 * \param[in] pathname Path to the file to be created.
 * \param[in] mode Permissions.
 * \param[out] result Where to store any asynchronous failures.
 * \return The new file descriptor on success, or -1 if a synchronous failure occurred 
 * (in which case, errno is set appropriately).
 */
int
XCALL_DEF(x_create)(const char *pathname, mode_t mode, int *result) 
{
	txc_tx_t       *txd;
	txc_koamgr_t   *koamgr = txc_g_koamgr;
	txc_koa_t      *koa_new;
	txc_koa_t      *koa_old;
	txc_sentinel_t *sentinel;
	txc_result_t   xret;
	int            fildes;
	int            ret;
	int            local_result = 0;
	ino_t          inode;
	char           temp_pathname[128]; 
	int            creation_flags = O_CREAT| O_NOCTTY| O_TRUNC| O_RDWR;

	txd = txc_tx_get_txd();

	switch(txc_tx_get_xactstate(txd)) {
		case TXC_XACTSTATE_TRANSACTIONAL_RETRYABLE:
			/* Serialize open/create/close file operations to detect aliasing */
			txc_koa_lock_alias_cache(koamgr);
			txc_koa_path2inode(pathname, &inode);
			TXC_DEBUG_PRINT(TXC_DEBUG_XCALL, "X_CREATE: path = %s, inode= %d\n", 
			                pathname, inode);
			if (inode == 0) {
				x_create_case1_commit_undo_args_t *args_commit_undo; 
				/* 
				 * CASE 1: File does not exist.  
				 */

				/* 
				 * The logic is similar to CASE 2. The difference is that we 
				 * don't have an existing file to rename.
				 */

				TXC_ASSERT(txc_koa_alias_cache_lookup_inode(koamgr, inode, &koa_old) 
				           == TXC_R_NOTEXISTS);

				if ((ret = fildes = txc_libc_open(pathname, 
				                                  creation_flags, 
					                              mode)) < 0) 
				{
					txc_koa_unlock_alias_cache(koamgr);
					local_result = errno;
					goto done;
				}
				txc_koa_path2inode(pathname, &inode);
				txc_koa_create(koamgr, &koa_new, TXC_KOA_IS_FILE, (void *) inode);
				txc_koa_lock_fd(koamgr, fildes);
				txc_koa_attach_fd(koa_new, fildes, 0);
				sentinel = txc_koa_get_sentinel(koa_new);
				 /* 
				  * We don't ask the sentinel to be acquired after restart.
				  * This is because the KOA is created by this transaction, 
				  * thus it won't be live after transaction rollbacks. So 
				  * the sentinel mapped to the KOA does not need to be 
				  * reacquired.
				  *
				  * Note though that someone may see the KOA, try to get
				  * the sentinel and fail, and retry when the transaction 
				  * restarts. Thus, this guy continues holding a reference 
				  * to the sentinel even if the KOA has been be destroyed. 
				  * This is fine since a sentinel is detached from a KOA 
				  * when the latter is destroyed, but is not destroyed 
				  * until its refcnt reaches zero.
				  */
				xret = txc_sentinel_tryacquire(txd, sentinel, 0);
				if (xret != TXC_R_SUCCESS) {
					TXC_INTERNALERROR("Cannot acquire the sentinel of the KOA I've just created!\n");
				}
				args_commit_undo = (x_create_case1_commit_undo_args_t *)
				                   txc_buffer_linear_malloc(txd->buffer_linear, 
				                                            sizeof(x_create_case1_commit_undo_args_t));
				if (args_commit_undo == NULL) {
					TXC_INTERNALERROR("Allocation failed. Linear buffer out of space.\n");
				}
				strcpy(args_commit_undo->pathname, pathname);
				args_commit_undo->koa = koa_new;
				args_commit_undo->fd = fildes;

				txc_tx_register_undo_action  (txd, x_create_case1_undo, 
				                              (void *) args_commit_undo, result,
				                              TXC_KOA_CREATE_UNDO_ACTION_ORDER);

				txc_koa_unlock_fd(koamgr, fildes);
				txc_koa_unlock_alias_cache(koamgr);
				ret = fildes;
			} else {
				/* 
				 * CASE 2: Overwriting an existing file.  
				 */
				 
				if (txc_koa_alias_cache_lookup_inode(koamgr, inode, &koa_old) 
				    == TXC_R_SUCCESS) 
				{
					TXC_DEBUG_PRINT(TXC_DEBUG_XCALL, "X_CREATE: Case 2A\n");
					/* 
					 * CASE 2a:
				 	 *  - Overwriting an existing file, and 
					 *  - KOA exists in the alias cache so other in-flight 
					 *    transaction may operate on the file.
				 	 */

					/*
					 * Acquire the sentinel attached to the existing KOA to
					 * properly isolated ourself from other transaction 
					 * operating on the file.
					 * e.g other transaction could be:
					 *   atomic {
					 *     fd = create(pathname, ...);
					 *     write(fd, ...);
					 *     close(fd);
					 *     unlink(pathname);
					 *   }
					 *
					 * While this transaction might not make any sense, we 
					 * still need to ensure it is executed correctly. Thus,
					 * this transaction must delete the file it created.
					 * If here we don't acquire the sentinel of that KOA
					 * then the unlink operation will see the new file (because
					 * we rename in-place) and therefore will try to
					 * acquire the sentinel of the new KOA and fail. While
					 * this will abort the transaction and result in correct
					 * execution we still feel that is not the right thing
					 * to do and could make the implementation more complex.
					 */

					txc_koa_lock_fds_refby_koa(koa_old);
					sentinel = txc_koa_get_sentinel(koa_old);
					 /* We don't ask the sentinel to be acquired after restart. */
					xret = txc_sentinel_tryacquire(txd, sentinel, 0);
					if (xret == TXC_R_BUSYSENTINEL) {
						txc_koa_unlock_fds_refby_koa(koa_old);
						txc_koa_unlock_alias_cache(koamgr);
						txc_tx_abort_transaction(txd, TXC_ABORTREASON_BUSYSENTINEL);
						TXC_INTERNALERROR("Never gets here. Transaction abort failed.\n");
					} else {
						txc_koa_unlock_fds_refby_koa(koa_old);
					}
				
				} else {
					TXC_DEBUG_PRINT(TXC_DEBUG_XCALL, "X_CREATE: Case 2B\n");
					/* 
					 * CASE 2b:
				 	 * - Overwriting an existing file, and
					 * - KOA does not exist in the alias cache so there is no
					 *   way for some other in-flight transaction to have a 
					 *   reference to the file to operate on it.
				 	 */

					 /* 
					  * No special actions for this case. Fall through the 
					  * common code.
					  */
				}	
				x_create_case2_commit_undo_args_t *args_commit_undo; 
				strcpy(temp_pathname, "/tmp/libtxc.tmp.XXXXXX");
				mktemp(temp_pathname); 
				txc_libc_rename(pathname, temp_pathname);
				if ((ret = fildes = txc_libc_open(pathname, 
				                                  creation_flags, 
					                              mode)) < 0) 
				{
					txc_libc_rename(temp_pathname, pathname);
					txc_koa_unlock_alias_cache(koamgr);
					local_result = errno;
					goto done;
				}
				txc_koa_path2inode(pathname, &inode);
				txc_koa_create(koamgr, &koa_new, TXC_KOA_IS_FILE, (void *) inode);

				TXC_DEBUG_PRINT(TXC_DEBUG_XCALL, 
				                "X_CREATE: Case 2B: KOA = %x\n", 
				                koa_old);
				txc_koa_lock_fd(koamgr, fildes);
				txc_koa_attach_fd(koa_new, fildes, 0);
				TXC_ASSERT(TXC_R_SUCCESS == txc_koa_alias_cache_lookup_inode(koamgr, inode, &koa_old)); 
				sentinel = txc_koa_get_sentinel(koa_new);
				 /* 
				  * We don't ask the sentinel to be acquired after restart.
				  * This is because the KOA is created by this transaction, 
				  * thus it won't be live after transaction rollbacks. So 
				  * the sentinel mapped to the KOA does not need to be 
				  * reacquired.
				  *
				  * Note though that someone may see the KOA, try to get
				  * the sentinel and fail, and retry when the transaction 
				  * restarts. Thus, this guy continues holding a reference 
				  * to the sentinel even if the KOA has been be destroyed. 
				  * This is fine since a sentinel is detached from a KOA 
				  * when the latter is destroyed, but is not destroyed 
				  * until its refcnt reaches zero.
				  */
				xret = txc_sentinel_tryacquire(txd, sentinel, 0);
				if (xret != TXC_R_SUCCESS) {
					TXC_INTERNALERROR("Cannot acquire the sentinel of the KOA I've just created!\n");
				}
				args_commit_undo = (x_create_case2_commit_undo_args_t *)
				                   txc_buffer_linear_malloc(txd->buffer_linear, 
				                                            sizeof(x_create_case2_commit_undo_args_t));
				if (args_commit_undo == NULL) {
					TXC_INTERNALERROR("Allocation failed. Linear buffer out of space.\n");
				}
				strcpy(args_commit_undo->temp_pathname, temp_pathname);
				strcpy(args_commit_undo->original_pathname, pathname);
				args_commit_undo->koa = koa_new;
				args_commit_undo->fd = fildes;

				txc_tx_register_commit_action(txd, x_create_case2_commit, 
				                              (void *) args_commit_undo, result,
				                              TXC_KOA_CREATE_COMMIT_ACTION_ORDER);
				txc_tx_register_undo_action  (txd, x_create_case2_undo, 
				                              (void *) args_commit_undo, result,
				                              TXC_KOA_CREATE_UNDO_ACTION_ORDER);

				txc_koa_unlock_fd(koamgr, fildes);
				txc_koa_unlock_alias_cache(koamgr);
				ret = fildes;
			}
			txc_stats_txstat_increment(txd, XCALL, x_create, 1);
			break;
		case TXC_XACTSTATE_TRANSACTIONAL_IRREVOCABLE:
		case TXC_XACTSTATE_NONTRANSACTIONAL:
			/* 
			 * Even if xCalls don't provide transactional properties when called
			 * outside of a transaction, we still need to create a KOA and assign
			 * a sentinel to be used later by any transactional xCalls. Thus, we
			 * still need to properly synchronize accesses to internal data
			 * structures.
			 */
			txc_koa_lock_alias_cache(koamgr);
			if ((ret = fildes = txc_libc_open(pathname, creation_flags, mode)) < 0) { 
				local_result = errno;
				goto done;
			}
			txc_koa_path2inode(pathname, &inode);
			txc_koa_create(koamgr, &koa_new, TXC_KOA_IS_FILE, (void *) inode);
			txc_koa_lock_fd(koamgr, fildes);
			txc_koa_attach_fd(koa_new, fildes, 0);
			sentinel = txc_koa_get_sentinel(koa_new);
			txc_koa_unlock_fd(koamgr, fildes);
			txc_koa_unlock_alias_cache(koamgr);
			ret = fildes;
			break;
		default:
			TXC_INTERNALERROR("Unknown transaction state\n");
	}
done:
	if (result) {
		*result = local_result; 
	}
	return ret;
}
