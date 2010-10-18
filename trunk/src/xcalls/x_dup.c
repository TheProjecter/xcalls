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
 * \file x_dup.c
 *
 * \brief x_dup implementation.
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

typedef struct x_dup_undo_args_s x_dup_undo_args_t;

struct x_dup_undo_args_s {
	int       fd;
	txc_koa_t *koa;
};


static
void
x_dup_undo(void *args, int *result)
{
	int               local_errno = 0; 
	x_dup_undo_args_t *myargs = (x_dup_undo_args_t *) args;
	txc_koamgr_t      *koamgr;

	koamgr = txc_koa_get_koamgr(myargs->koa);
	txc_koa_lock_fd(koamgr, myargs->fd);
	txc_koa_detach_fd(myargs->koa, myargs->fd, 0);
	if (txc_libc_close(myargs->fd) < 0) {
		local_errno = errno;
	}	
	txc_koa_unlock_fd(koamgr, myargs->fd);
	if (result) {
		*result = local_errno;
	}
}


/**
 * \brief Duplicates a file descriptor.
 * 
 * The new file descriptor shares the sentinels of the original file descriptor.
 *
 * <b> Execution </b>: in-place
 *
 * <b> Asynchronous failures</b>: abort
 *
 * \param[in] oldfd The original file descriptor to be duplicated.
 * \param[out] result Where to store any asynchronous failures.
 * \return The new file descriptor on success, or -1 if a synchronous failure occurred 
 * (in which case, errno is set appropriately).
 */
int 
XCALL_DEF(x_dup)(int oldfd, int *result)
{
	txc_tx_t           *txd;
	txc_koamgr_t       *koamgr = txc_g_koamgr;
	txc_koa_t          *koa;
	txc_koa_t          *koa2;
	txc_sentinel_t     *sentinel;
	txc_result_t       xret;
	int                ret;
	int                fildes;
	x_dup_undo_args_t *args_undo;
	int                local_result;


	txd = txc_tx_get_txd();

	switch(txc_tx_get_xactstate(txd)) {
		case TXC_XACTSTATE_TRANSACTIONAL_RETRYABLE:
			txc_koa_lock_fd(koamgr, oldfd);
			xret = txc_koa_lookup_fd2koa(koamgr, oldfd, &koa);
			if (xret == TXC_R_FAILURE) {
				/* 
				 * The KOA mapped to the file descriptor has gone. Report
				 * this error as invalid file descriptor.
				 */
				local_result = EBADF;
				ret = -1;
				goto done;
			}
			/* 
			 * Before locking all file descriptors pointing to koa,
			 * unlock oldfd to avoid any DEADLOCK either with yourself or
			 * someone else (e.g. someone doing an x_open:case 1). This 
			 * though opens a small window where koa could have been unmapped, 
			 * so check that koa is still mapped to oldfd.
			 */
			txc_koa_unlock_fd(koamgr, oldfd);
			txc_koa_lock_fds_refby_koa(koa);
			xret = txc_koa_lookup_fd2koa(koamgr, oldfd, &koa2);
			if (xret == TXC_R_FAILURE || koa != koa2) {
				local_result = EBADF;
				ret = -1;
				goto done;
			}
			sentinel = txc_koa_get_sentinel(koa);
			xret = txc_sentinel_tryacquire(txd, sentinel, 
			                               TXC_SENTINEL_ACQUIREONRETRY);
			if (xret == TXC_R_BUSYSENTINEL) {
				txc_koa_unlock_fds_refby_koa(koa);
				txc_tx_abort_transaction(txd, TXC_ABORTREASON_BUSYSENTINEL);
				TXC_INTERNALERROR("Never gets here. Transaction abort failed.\n");
			}

			if ((ret = fildes = txc_libc_dup(oldfd)) < 0) { 
				txc_koa_unlock_fds_refby_koa(koa);
				local_result = errno;
				goto done;
			}

			if ((args_undo = (x_dup_undo_args_t *)
			                 txc_buffer_linear_malloc(txd->buffer_linear, 
			                                          sizeof(x_dup_undo_args_t)))
			     == NULL)
			{
				txc_libc_close(fildes);
				txc_koa_unlock_fds_refby_koa(koa);
				local_result = ENOMEM;
				ret = -1;
				goto done;
			}
			txc_koa_attach_fd(koa, fildes, 0);
			txc_koa_unlock_fds_refby_koa(koa);

			args_undo->fd = fildes;
			args_undo->koa = koa;

			txc_tx_register_undo_action(txd, x_dup_undo, 
			                            (void *) args_undo, result,
			                            TXC_TX_REGULAR_UNDO_ACTION_ORDER);
			local_result = 0;
			ret = fildes;
			txc_stats_txstat_increment(txd, XCALL, x_dup, 1);
			goto done;
		case TXC_XACTSTATE_TRANSACTIONAL_IRREVOCABLE:
		case TXC_XACTSTATE_NONTRANSACTIONAL:
			xret = txc_koa_lookup_fd2koa(koamgr, oldfd, &koa);
			if (xret == TXC_R_FAILURE) {
				/* 
				 * The KOA mapped to the file descriptor has gone. Report
				 * this error as invalid file descriptor.
				 */
				local_result = EBADF;
				ret = -1;
				goto done;
			}
			if ((ret = fildes = txc_libc_dup(oldfd)) < 0) { 
				return ret;
			}
			txc_koa_lock_fds_refby_koa(koa);
			txc_koa_attach_fd(koa, fildes, 0);
			txc_koa_unlock_fds_refby_koa(koa);
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
