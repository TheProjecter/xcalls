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
 * \file x_socket.c
 *
 * \brief x_socket implementation.
 */

#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
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


typedef struct x_socket_undo_args_s x_socket_undo_args_t;

struct x_socket_undo_args_s {
	int       fd;
	txc_koa_t *koa;
};


static
void
x_socket_undo(void *args, int *result)
{
	int                local_errno = 0; 
	x_socket_undo_args_t *myargs = (x_socket_undo_args_t *) args;
	txc_koamgr_t       *koamgr;

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
 * \brief Creates an endpoint for communication.
 * 
 * <b> Execution </b>: in-place
 *
 * <b> Asynchronous failures </b>: abort
 *
 * \param[in] domain Communication domain; this selects the protocol 
 *            family which will be used for communication. These families 
 *            are defined in <sys/socket.h>. 
 * \param[in] type Specifies communication semantics. Currently we support:
 *            \li SOCK_DGRAM: Supports datagrams (connectionless, unreliable 
 *                messages of a fixed maximum length).
 * \param[in] protocol Specifies a particular protocol to be used with the 
 *            socket.
 * \param[out] result Where to store any asynchronous failures.
 * \return A new file descriptor for the new socket, or -1 if a synchronous 
 *         failure occurred (in which case, errno is set appropriately).
 * 
 * \todo Implement SOCK_STREAM.
 */
int 
XCALL_DEF(x_socket)(int domain, int type, int protocol, int *result) 
{
	int                fildes;
	txc_tx_t           *txd;
	txc_koamgr_t       *koamgr = txc_g_koamgr;
	txc_koa_t          *koa;
	txc_sentinel_t     *sentinel;
	txc_result_t       xret;
	int                ret;
	x_socket_undo_args_t *args_undo; 
	int                local_result = 0;
	int                koa_type;

	txd = txc_tx_get_txd();

	switch(type) {
		case SOCK_DGRAM:
			koa_type = TXC_KOA_IS_SOCK_DGRAM;
			break;
		case SOCK_STREAM:
			local_result = EINVAL;
			ret = -1;
			goto done;
	}		

	switch(txc_tx_get_xactstate(txd)) {
		case TXC_XACTSTATE_TRANSACTIONAL_RETRYABLE:
			if ((ret = fildes = txc_libc_socket(domain, type, protocol)) < 0) { 
				local_result = errno;
				goto done;
			}
			txc_koa_create(koamgr, &koa, koa_type, NULL); 
			txc_koa_lock_fd(koamgr, fildes);
			txc_koa_attach_fd(koa, fildes, 0);
			sentinel = txc_koa_get_sentinel(koa);
			xret = txc_sentinel_tryacquire(txd, sentinel, 0);
			if (xret != TXC_R_SUCCESS) {
				TXC_INTERNALERROR("Cannot acquire the sentinel of the KOA I've just created!\n");
			}
	
			args_undo = (x_socket_undo_args_t *)
			            txc_buffer_linear_malloc(txd->buffer_linear, 
			                                     sizeof(x_socket_undo_args_t));
			if (args_undo == NULL) {
				TXC_INTERNALERROR("Allocation failed. Linear buffer out of space.\n");
			}
			args_undo->koa = koa;
			args_undo->fd = fildes;

			txc_tx_register_undo_action  (txd, x_socket_undo, 
			                              (void *) args_undo, result,
			                              TXC_KOA_CREATE_UNDO_ACTION_ORDER);

			txc_koa_unlock_fd(koamgr, fildes);
			ret = fildes;
			txc_stats_txstat_increment(txd, XCALL, x_socket, 1);
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
			if ((ret = fildes = txc_libc_socket(domain, type, protocol)) < 0) { 
				local_result = errno;
				goto done;
			}
			txc_koa_create(koamgr, &koa, koa_type, NULL); 
			txc_koa_lock_fd(koamgr, fildes);
			txc_koa_attach_fd(koa, fildes, 0);
			txc_koa_unlock_fd(koamgr, fildes);
			ret = fildes;
	}
done:
	if (result) {
		*result = local_result;
	}
	return ret;
}
