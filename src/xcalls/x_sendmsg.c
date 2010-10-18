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
 * \file x_sendmsg.c
 *
 * \brief x_sendmsg implementation.
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
#include <sys/types.h>
#include <sys/socket.h>

typedef struct x_sendmsg_commit_args_s x_sendmsg_commit_args_t;

struct x_sendmsg_commit_args_s {
	int  fd;
	int  flags;
	struct msghdr msg;
};

static
void
x_sendmsg_commit(void *args, int *result)
{
	x_sendmsg_commit_args_t *args_commit = (x_sendmsg_commit_args_t *) args;
	int                     local_result = 0;

	if (txc_libc_sendmsg(args_commit->fd, &args_commit->msg, 
	                     args_commit->flags)  < 0)
	{
		local_result = errno;
	}
	if (result) {
		*result = local_result;
	}	
}


/**
 * \brief Sends a message to a socket.
 * 
 * <b> Execution </b>: deferred
 *
 * <b> Asynchronous failures </b>: commit
 *
 * \param[in] fd The file descriptor of the socket.
 * \param[in] msg The message to send.
 * \param[in] flags See man sendmsg.
 * \param[out] result Where to store any asynchronous failures.
 * \return The number of sent characters on success, or -1 if a synchronous failure occurred 
 * (in which case, errno is set appropriately).
 * 
 * \todo Find which flags of sendmsg can be used with x_sendmsg? 
 */
ssize_t 
XCALL_DEF(x_sendmsg)(int fd, const struct msghdr *msg, int flags, int *result)
{
	txc_tx_t                *txd;
	txc_koamgr_t            *koamgr = txc_g_koamgr;
	txc_koa_t               *koa;
	txc_sentinel_t          *sentinel;
	txc_result_t            xret;
	int                     ret;
	x_sendmsg_commit_args_t *args_commit;
	int                     local_result;


	txd = txc_tx_get_txd();

	switch(txc_tx_get_xactstate(txd)) {
		case TXC_XACTSTATE_TRANSACTIONAL_IRREVOCABLE:
		case TXC_XACTSTATE_NONTRANSACTIONAL:
			if ((ret = txc_libc_sendmsg(fd, msg, flags)) < 0) {
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
			if ((args_commit = (x_sendmsg_commit_args_t *)
   	                           txc_buffer_linear_malloc(txd->buffer_linear, 
			                                            sizeof(x_sendmsg_commit_args_t)))
			    == NULL)
			{	
				local_result = ENOMEM;
				ret = -1;
				goto done;
			}
			memcpy(&args_commit->msg, msg, sizeof(struct msghdr));
			args_commit->fd = fd;
			args_commit->flags = flags;
			if ((args_commit->msg.msg_iov = (struct iovec *) txc_buffer_linear_malloc(txd->buffer_linear, 
			                                                                  msg->msg_iovlen))
			    == NULL) 
			{
				local_result = ENOMEM;
				ret = -1;
				goto error_handler;
			}
			if ((args_commit->msg.msg_control = (void *) txc_buffer_linear_malloc(txd->buffer_linear, 
			                                                                      msg->msg_controllen))
			    == NULL) 
			{
				local_result = ENOMEM;
				ret = -1;
				goto error_handler2;
			}

			memcpy (args_commit->msg.msg_iov, msg->msg_iov, msg->msg_iovlen);
			memcpy (args_commit->msg.msg_control, msg->msg_control, msg->msg_controllen);
			txc_tx_register_commit_action(txd, x_sendmsg_commit, 
			                              (void *) args_commit, result,
			                              TXC_TX_REGULAR_COMMIT_ACTION_ORDER);
			local_result = 0;							
			txc_stats_txstat_increment(txd, XCALL, x_sendmsg, 1);
			goto done;
		default:
			TXC_INTERNALERROR("Unknown transaction state\n");
	}

error_handler2:
	txc_buffer_linear_free(txd->buffer_linear,
		                   msg->msg_iovlen);
error_handler:
	txc_buffer_linear_free(txd->buffer_linear,
		                   sizeof(x_sendmsg_commit_args_t));
done:
	if (result) {
		*result = local_result;
	}
	return ret;
}
