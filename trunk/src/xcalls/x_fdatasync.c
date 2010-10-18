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
 * \file x_fdatasync.c
 *
 * \brief x_fdatasync implementation.
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


typedef struct x_fdatasync_commit_args_s x_fdatasync_commit_args_t;

struct x_fdatasync_commit_args_s {
	int       fd;
};


static
void
x_fdatasync_commit(void *args, int *result)
{
	x_fdatasync_commit_args_t *args_commit = (x_fdatasync_commit_args_t *) args;
	int                   local_result = 0;

	if (txc_libc_fdatasync(args_commit->fd) < 0)
	{
		local_result = errno;
	}
	if (result) {
		*result = local_result;
	}	
}


/**
 * \brief Synchronizes a file in-core state with storage device.
 * 
 * <b> Execution </b>: deferred
 *
 * <b> Asynchronous failures </b>: commit
 *
 * \param[in] fildes The file's file descriptor.
 * \param[out] result Where to store any asynchronous failures.
 * \return 0 on success, or -1 if a synchronous failure occurred 
 * (in which case, errno is set appropriately).
 */
int
XCALL_DEF(x_fdatasync)(int fildes, int *result) 
{
	txc_tx_t              *txd;
	txc_koamgr_t          *koamgr = txc_g_koamgr;
	txc_koa_t             *koa;
	txc_sentinel_t        *sentinel;
	txc_result_t          xret;
	int                   ret;
	x_fdatasync_commit_args_t *args_commit; 
	int                   local_result = 0;

	txd = txc_tx_get_txd();

	switch(txc_tx_get_xactstate(txd)) {
		case TXC_XACTSTATE_TRANSACTIONAL_RETRYABLE:
			txc_koa_lock_fd(koamgr, fildes);
			xret = txc_koa_lookup_fd2koa(koamgr, fildes, &koa);
			if (xret == TXC_R_FAILURE) {
				/* 
				 * The KOA mapped to the file descriptor has gone. Report
				 * this error as invalid file descriptor.
				 */
				txc_koa_unlock_fd(koamgr, fildes);
				local_result = EBADF;
				ret = -1;
				goto done;
			}

			sentinel = txc_koa_get_sentinel(koa);
			xret = txc_sentinel_tryacquire(txd, sentinel, TXC_SENTINEL_ACQUIREONRETRY);
			if (xret == TXC_R_BUSYSENTINEL) {
				txc_koa_unlock_fd(koamgr, fildes);
				txc_tx_abort_transaction(txd, TXC_ABORTREASON_BUSYSENTINEL);
				TXC_INTERNALERROR("Never gets here. Transaction abort failed.\n");
			} else {
				txc_koa_unlock_fd(koamgr, fildes);
			}

			args_commit = (x_fdatasync_commit_args_t *)
			              txc_buffer_linear_malloc(txd->buffer_linear, 
			                                       sizeof(x_fdatasync_commit_args_t));
			if (args_commit == NULL) {
				TXC_INTERNALERROR("Allocation failed. Linear buffer out of space.\n");
			}
			args_commit->fd = fildes;

			txc_tx_register_commit_action(txd, x_fdatasync_commit, 
			                              (void *) args_commit, result,
			                              TXC_TX_REGULAR_COMMIT_ACTION_ORDER);

			txc_koa_unlock_fd(koamgr, fildes);
			ret = 0;
			txc_stats_txstat_increment(txd, XCALL, x_fdatasync, 1);
			break;
		case TXC_XACTSTATE_TRANSACTIONAL_IRREVOCABLE:
		case TXC_XACTSTATE_NONTRANSACTIONAL:
			if ((ret = txc_libc_fdatasync(fildes)) < 0) { 
				local_result = errno;
				goto done;
			}
	}
done:
	if (result) {
		*result = local_result;
	}
	return ret;
}
