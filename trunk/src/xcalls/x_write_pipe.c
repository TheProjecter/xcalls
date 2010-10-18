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
 * \file x_write_pipe.c
 *
 * \brief x_write_pipe implementation.
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


typedef struct x_write_commit_args_s x_write_commit_args_t;

struct x_write_commit_args_s {
	int  fd;
	int  nbyte;
	void *buf;
};

static
void
x_write_pipe_commit(void *args, int *result)
{
	x_write_commit_args_t *args_commit = (x_write_commit_args_t *) args;
	int                   local_result = 0;

	if (txc_libc_write(args_commit->fd, args_commit->buf, 
	                   args_commit->nbyte)  < 0)
	{
		local_result = errno;
	}
	if (result) {
		*result = local_result;
	}	
}


/**
 * \brief Writes to a pipe.
 * 
 * <b> Execution </b>: deferred
 *
 * <b> Asynchronous failures </b>: commit
 *
 * \param[in] fd The file descriptor of the pipe's write end.
 * \param[in] buf The buffer that has the data to write.
 * \param[in] nbyte The number of bytes to write.
 * \param[out] result Where to store any asynchronous failures.
 * \return The number of written bytes on success, or -1 if a synchronous failure occurred 
 * (in which case, errno is set appropriately).
 */
ssize_t 
XCALL_DEF(x_write_pipe)(int fd, const void *buf, size_t nbyte, int *result)
{
	txc_tx_t              *txd;
	txc_koamgr_t          *koamgr = txc_g_koamgr;
	txc_koa_t             *koa;
	txc_sentinel_t        *sentinel;
	txc_result_t          xret;
	int                   ret;
	void                  *deferred_data;
	x_write_commit_args_t *args_write_commit;
	int                   local_result;


	txd = txc_tx_get_txd();

	switch(txc_tx_get_xactstate(txd)) {
		case TXC_XACTSTATE_TRANSACTIONAL_IRREVOCABLE:
		case TXC_XACTSTATE_NONTRANSACTIONAL:
			if ((ret = txc_libc_write(fd, buf, nbyte)) < 0) {
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
			if ((args_write_commit = (x_write_commit_args_t *)
   	                                 txc_buffer_linear_malloc(txd->buffer_linear, 
			                                                  sizeof(x_write_commit_args_t)))
			    == NULL)
			{	
				local_result = ENOMEM;
				ret = -1;
				goto done;
			}
			if ((deferred_data = (void *) txc_buffer_linear_malloc(txd->buffer_linear, 
			                                                       sizeof(char) * nbyte))
			    == NULL) 
			{
				local_result = ENOMEM;
				ret = -1;
				goto error_handler;
			}
			memcpy (deferred_data, buf, nbyte);
			args_write_commit->nbyte = nbyte;
			args_write_commit->buf = deferred_data;
			args_write_commit->fd = fd;
			txc_tx_register_commit_action(txd, x_write_pipe_commit, 
			                              (void *) args_write_commit, result,
			                              TXC_TX_REGULAR_COMMIT_ACTION_ORDER);
			local_result = 0;							
			ret = args_write_commit->nbyte;
			txc_stats_txstat_increment(txd, XCALL, x_write_pipe, 1);
			goto done;
		default:
			TXC_INTERNALERROR("Unknown transaction state\n");
	}

error_handler:
	txc_buffer_linear_free(txd->buffer_linear,
		                   sizeof(x_write_commit_args_t));
done:
	if (result) {
		*result = local_result;
	}
	return ret;
}
