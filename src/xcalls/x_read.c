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
 * \file x_read.c
 *
 * \brief x_read implementation.
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


typedef struct x_read_undo_args_s x_read_undo_args_t;

struct x_read_undo_args_s {
	int  fd;
	int  nbyte;
};


static
void
x_read_undo(void *args, int *result)
{
	x_read_undo_args_t *args_undo = (x_read_undo_args_t *) args;
	int                local_result = 0;

	if (txc_libc_lseek(args_undo->fd, 
	                   -args_undo->nbyte, SEEK_CUR) < 0) 
	{
		local_result = errno;
	}
	if (result) {
		*result = local_result;
	}
}


/**
 * \brief Reads from a file.
 * 
 * <b> Execution </b>: in-place
 *
 * <b> Asynchronous failures </b>: abort
 *
 * \param[in] fd The file descriptor of the file to read from.
 * \param[in] buf The buffer where to store read data.
 * \param[in] nbyte The number of bytes to read.
 * \param[out] result Where to store any asynchronous failures.
 * \return The number of read bytes on success, or -1 if a synchronous failure occurred 
 * (in which case, errno is set appropriately).
 */
ssize_t 
XCALL_DEF(x_read)(int fd, void *buf, size_t nbyte, int *result)
{
	txc_tx_t           *txd;
	txc_koamgr_t       *koamgr = txc_g_koamgr;
	txc_koa_t          *koa;
	txc_sentinel_t     *sentinel;
	txc_result_t       xret;
	int                ret;
	x_read_undo_args_t *args_undo;
	int                local_result;


	txd = txc_tx_get_txd();

	switch(txc_tx_get_xactstate(txd)) {
		case TXC_XACTSTATE_TRANSACTIONAL_IRREVOCABLE:
		case TXC_XACTSTATE_NONTRANSACTIONAL:
			if ((ret = txc_libc_read(fd, buf, nbyte)) < 0) {
				local_result = errno;
			} else {
				local_result = 0;
			}
			goto done;

		case TXC_XACTSTATE_TRANSACTIONAL_RETRYABLE:
			txc_koa_lock_fd(koamgr, fd);
			xret = txc_koa_lookup_fd2koa(koamgr, fd, &koa);
			if (xret == TXC_R_FAILURE) {
				/* 
				 * The KOA mapped to the file descriptor has gone. Report
				 * this error as invalid file descriptor.
				 */
				txc_koa_unlock_fd(koamgr, fd);
				local_result = EBADF;
				ret = -1;
				goto done;
			}
			sentinel = txc_koa_get_sentinel(koa);
			xret = txc_sentinel_tryacquire(txd, sentinel, 
			                               TXC_SENTINEL_ACQUIREONRETRY);
			txc_koa_unlock_fd(koamgr, fd);
			if (xret == TXC_R_BUSYSENTINEL) {
				txc_tx_abort_transaction(txd, TXC_ABORTREASON_BUSYSENTINEL);
				TXC_INTERNALERROR("Never gets here. Transaction abort failed.\n");
			}

			/* Got sentinel. Continue with the rest of the stuff. */

			if ((args_undo = (x_read_undo_args_t *)
			                 txc_buffer_linear_malloc(txd->buffer_linear, 
			                                          sizeof(x_read_undo_args_t)))
			     == NULL)
			{	
				local_result = ENOMEM;
				ret = -1;
				goto error_handler_0;
			}
			args_undo->fd = fd;
			if ((ret = txc_libc_read(fd, buf, nbyte)) < 0) {
				local_result = errno;
				goto error_handler_1;
			}
			args_undo->nbyte = ret;
			txc_tx_register_undo_action(txd, x_read_undo, 
			                            (void *) args_undo, result,
			                            TXC_TX_REGULAR_UNDO_ACTION_ORDER);
			local_result = 0;
			txc_stats_txstat_increment(txd, XCALL, x_read, 1);
			goto done;

		default:
			TXC_INTERNALERROR("Unknown transaction state\n");
	}

error_handler_1:
	txc_buffer_linear_free(txd->buffer_linear,
		                   sizeof(x_read_undo_args_t));
error_handler_0:
done:
	if (result) {
		*result = local_result;
	}
	return ret;
}
