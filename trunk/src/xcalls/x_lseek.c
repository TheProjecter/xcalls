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
 * \file x_lseek.c
 *
 * \brief x_lseek implementation.
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


typedef struct x_lseek_undo_args_s x_lseek_undo_args_t;

struct x_lseek_undo_args_s {
	int   fd;
	off_t old_position;
};


static
void
x_lseek_undo(void *args, int *result)
{
	x_lseek_undo_args_t *args_undo = (x_lseek_undo_args_t *) args;
	int                 local_result = 0;

	if (txc_libc_lseek(args_undo->fd, 
	                   args_undo->old_position, SEEK_SET) < 0)
	{
		local_result = errno;
		goto done;
	}
done:
	if (result) {
		*result = local_result;
	}
}


/**
 * \brief Repositions read/write file offset.
 * 
 * <b> Execution </b>: in-place
 *
 * <b> Asynchronous failures </b>: abort
 *
 * \param[in] fd File descriptor.
 * \param[in] offset The new offset relative to whence.
 * \param[in] whence The absolute offset that the file offset is relatively 
 *            repositioned to. 
 *            \li SEEK_SET: The offset is set to offset bytes.
 *            \li SEEK_CUR: The offset is set to its current location plus offset bytes.
 *            \li SEEK_END: The offset is set to the size of the file plus offset bytes.
 * \param[out] result Where to store any asynchronous failures.
 * \return The resulting offset location as measured in bytes
 * from the beginning of the file on success, or (off_t) -1 if a synchronous failure occurred 
 * (in which case, errno is set appropriately).
 */
off_t 
XCALL_DEF(x_lseek)(int fd, off_t offset, int whence, int *result)
{
	txc_tx_t            *txd;
	txc_koamgr_t        *koamgr = txc_g_koamgr;
	txc_koa_t           *koa;
	txc_sentinel_t      *sentinel;
	txc_result_t        xret;
	int                 ret;
	x_lseek_undo_args_t *args_undo;
	int                 local_result;


	txd = txc_tx_get_txd();

	switch(txc_tx_get_xactstate(txd)) {
		case TXC_XACTSTATE_TRANSACTIONAL_IRREVOCABLE:
		case TXC_XACTSTATE_NONTRANSACTIONAL:
			if ((ret = txc_libc_lseek(fd, offset, whence)) < 0) {
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

			if ((args_undo = (x_lseek_undo_args_t *)
			                 txc_buffer_linear_malloc(txd->buffer_linear, 
			                                          sizeof(x_lseek_undo_args_t)))
			     == NULL)
			{	
				local_result = ENOMEM;
				ret = -1;
				goto error_handler_0;
			}
			args_undo->fd = fd;

			/* 
			 * If seek is relative to current file position then we can find
			 * out our current position using simple arithmetic. Otherwise we
			 * need a second lseek to get the current position.
			 */
			if (whence == SEEK_CUR) {
				if ((ret = txc_libc_lseek(fd, offset, whence)) < 0) {
					local_result = errno;
					goto error_handler_1;
				}
				args_undo->old_position = ret - offset;
			} else {
				args_undo->old_position = txc_libc_lseek(fd, 0, SEEK_CUR);
				if ((ret = txc_libc_lseek(fd, offset, whence)) < 0) {
					local_result = errno;
					goto error_handler_1;
				}
			}
			txc_tx_register_undo_action(txd, x_lseek_undo, 
			                            (void *) args_undo, result,
			                            TXC_TX_REGULAR_UNDO_ACTION_ORDER);
			local_result = 0;
			/* 
			 * ret was assigned offset location as measured in bytes from the 
			 * beginning of the file 
			 */
			txc_stats_txstat_increment(txd, XCALL, x_lseek, 1);
			goto done;

		default:
			TXC_INTERNALERROR("Unknown transaction state\n");
	}

error_handler_1:
	txc_buffer_linear_free(txd->buffer_linear,
		                   sizeof(x_lseek_undo_args_t));
error_handler_0:
done:
	if (result) {
		*result = local_result;
	}
	return ret;
}
