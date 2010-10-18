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
 * \file x_pipe.c
 *
 * \brief x_pipe implementation.
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


typedef struct x_pipe_undo_args_s x_pipe_undo_args_t;

struct x_pipe_undo_args_s {
	int       fd[2];
	txc_koa_t *koa[2];
};


static
void
x_pipe_undo(void *args, int *result)
{
	int                local_errno = 0; 
	x_pipe_undo_args_t *myargs = (x_pipe_undo_args_t *) args;
	txc_koamgr_t       *koamgr;

	koamgr = txc_koa_get_koamgr(myargs->koa[0]);
	if (myargs->fd[0] < myargs->fd[1]) {
		txc_koa_lock_fd(koamgr, myargs->fd[0]);
		txc_koa_lock_fd(koamgr, myargs->fd[1]);
	} else {
		txc_koa_lock_fd(koamgr, myargs->fd[1]);
		txc_koa_lock_fd(koamgr, myargs->fd[0]);
	}
	txc_koa_detach_fd(myargs->koa[0], myargs->fd[0], 0);
	txc_koa_detach_fd(myargs->koa[1], myargs->fd[1], 0);
	if (txc_libc_close(myargs->fd[0]) < 0) {
		local_errno = errno;
	}	
	if (txc_libc_close(myargs->fd[1]) < 0) {
		local_errno = errno;
	}	
	txc_koa_unlock_fd(koamgr, myargs->fd[0]);
	txc_koa_unlock_fd(koamgr, myargs->fd[1]);
	if (result) {
		*result = local_errno;
	}
}


/**
 * \brief Creates a pipe
 * 
 * Creates a pair of file descriptors, pointing to a pipe inode.
 *
 * <b> Execution </b>: in-place
 *
 * <b> Asynchronous failures </b>: abort
 *
 * \param[in,out] fildes Array where to place the new file descriptors. 
 * \param[out] result Where to store any asynchronous failures.
 * \return 0 on success, or -1 if a synchronous failure occurred 
 * (in which case, errno is set appropriately).
 */
int
XCALL_DEF(x_pipe)(int fildes[2], int *result) 
{
	txc_tx_t           *txd;
	txc_koamgr_t       *koamgr = txc_g_koamgr;
	txc_koa_t          *koa0;
	txc_koa_t          *koa1;
	txc_sentinel_t     *sentinel0;
	txc_sentinel_t     *sentinel1;
	txc_result_t       xret;
	int                ret;
	x_pipe_undo_args_t *args_undo; 
	int                local_result = 0;

	txd = txc_tx_get_txd();

	switch(txc_tx_get_xactstate(txd)) {
		case TXC_XACTSTATE_TRANSACTIONAL_RETRYABLE:
			if ((ret = txc_libc_pipe(fildes)) < 0) { 
				local_result = errno;
				goto done;
			}
			txc_koa_create(koamgr, &koa0, TXC_KOA_IS_PIPE_READ_END, NULL); 
			txc_koa_create(koamgr, &koa1, TXC_KOA_IS_PIPE_WRITE_END, NULL);
			/* Always acquire locks in increasing order to avoid any deadlock */
			if (fildes[0] < fildes[1]) {
				txc_koa_lock_fd(koamgr, fildes[0]);
				txc_koa_lock_fd(koamgr, fildes[1]);
			} else {
				txc_koa_lock_fd(koamgr, fildes[1]);
				txc_koa_lock_fd(koamgr, fildes[0]);
			}
			txc_koa_attach_fd(koa0, fildes[0], 0);
			txc_koa_attach_fd(koa1, fildes[1], 0);
			sentinel0 = txc_koa_get_sentinel(koa0);
			xret = txc_sentinel_tryacquire(txd, sentinel0, 0);
			if (xret != TXC_R_SUCCESS) {
				TXC_INTERNALERROR("Cannot acquire the sentinel of the KOA I've just created!\n");
			}
			sentinel1 = txc_koa_get_sentinel(koa1);
			xret = txc_sentinel_tryacquire(txd, sentinel1, 0);
			if (xret != TXC_R_SUCCESS) {
				TXC_INTERNALERROR("Cannot acquire the sentinel of the KOA I've just created!\n");
			}
	
			args_undo = (x_pipe_undo_args_t *)
			            txc_buffer_linear_malloc(txd->buffer_linear, 
			                                     sizeof(x_pipe_undo_args_t));
			if (args_undo == NULL) {
				TXC_INTERNALERROR("Allocation failed. Linear buffer out of space.\n");
			}
			args_undo->koa[0] = koa0;
			args_undo->koa[1] = koa1;
			args_undo->fd[0] = fildes[0];
			args_undo->fd[1] = fildes[1];

			txc_tx_register_undo_action  (txd, x_pipe_undo, 
			                              (void *) args_undo, result,
			                              TXC_KOA_CREATE_UNDO_ACTION_ORDER);

			txc_koa_unlock_fd(koamgr, fildes[0]);
			txc_koa_unlock_fd(koamgr, fildes[1]);
			ret = 0;
			txc_stats_txstat_increment(txd, XCALL, x_pipe, 1);
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
			if ((ret = txc_libc_pipe(fildes)) < 0) { 
				local_result = errno;
				goto done;
			}
			txc_koa_create(koamgr, &koa0, TXC_KOA_IS_PIPE_READ_END, NULL); 
			txc_koa_create(koamgr, &koa1, TXC_KOA_IS_PIPE_WRITE_END, NULL);
			/* Always acquire locks in increasing order to avoid any deadlock */
			if (fildes[0] < fildes[1]) {
				txc_koa_lock_fd(koamgr, fildes[0]);
				txc_koa_lock_fd(koamgr, fildes[1]);
			} else {
				txc_koa_lock_fd(koamgr, fildes[1]);
				txc_koa_lock_fd(koamgr, fildes[0]);
			}
			txc_koa_attach_fd(koa0, fildes[0], 0);
			txc_koa_attach_fd(koa1, fildes[1], 0);
			txc_koa_unlock_fd(koamgr, fildes[0]);
			txc_koa_unlock_fd(koamgr, fildes[1]);
			ret = 0;
	}
done:
	if (result) {
		*result = local_result;
	}
	return ret;
}
