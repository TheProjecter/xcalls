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
 * \file x_recvmsg.c
 *
 * \brief x_recvmsg implementation.
 */

#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <misc/debug.h>
#include <misc/errno.h>
#include <misc/generic_types.h>
#include <core/tx.h>
#include <core/config.h>
#include <core/koa.h>
#include <core/buffer.h>
#include <libc/syscalls.h>
#include <core/txdesc.h>
#include <core/stats.h>
#include <xcalls/xcalls.h>
#include <netinet/in.h>

#define TXC_DGRAM_MAX_SIZE      1024*64
#define TXC_CONTROLMSG_MAX_SIZE 1024*2

typedef struct txc_socket_msghdr_s txc_socket_msghdr_t;

struct txc_socket_msghdr_s {
	struct sockaddr_in msg_name;
	socklen_t          msg_namelen;
	char               *msg_iov_base;
	size_t             msg_iov_len;		/* length in bytes */
	void               *msg_control;
	socklen_t          msg_controllen;
	int                msg_flags;
};



typedef struct x_recvmsg_commit_undo_args_s x_recvmsg_commit_undo_args_t;

struct x_recvmsg_commit_undo_args_s {
	txc_koa_t             *koa;
	txc_buffer_circular_t *buffer;
};


static
void 
x_recvmsg_undo(void *args, int *result) 
{
	x_recvmsg_commit_undo_args_t *args_undo = (x_recvmsg_commit_undo_args_t *) args;
	int                          local_result = 0;

	args_undo->buffer->state = TXC_BUFFER_STATE_NON_SPECULATIVE;
	if (result) {
		*result = local_result;
	}
}


static 
void
x_recvmsg_commit(void *args, int *result)
{
	x_recvmsg_commit_undo_args_t *args_commit = (x_recvmsg_commit_undo_args_t *) args;
	int                          local_result = 0;
	txc_buffer_circular_t        *buffer;

	buffer = args_commit->buffer;

	if (buffer->speculative_primary_tail != buffer->speculative_primary_head) {
		buffer->primary_head = buffer->speculative_primary_head; 
		buffer->primary_tail = buffer->speculative_primary_tail; 
	} else if (buffer->speculative_secondary_tail != buffer->speculative_secondary_head) {
		buffer->primary_head = buffer->speculative_secondary_head; 
		buffer->primary_tail = buffer->speculative_secondary_tail; 
		buffer->secondary_head = buffer->secondary_tail = 0;
	} else {
		buffer->primary_head = buffer->primary_tail = 0;
		buffer->secondary_head = buffer->secondary_tail = 0;
	}
	buffer->state = TXC_BUFFER_STATE_NON_SPECULATIVE;
	if (result) {
		*result = local_result;
	}
}


static 
ssize_t 
__txc_recvmsg(txc_tx_t *txd, txc_bool_t speculative_read,
              int fd, struct msghdr *msg, int flags, int *result)
{
	txc_koamgr_t                   *koamgr = txc_g_koamgr;
	txc_koa_t                      *koa;
	txc_sentinel_t                 *sentinel;
	txc_result_t                   xret;
	int                            ret;
	x_recvmsg_commit_undo_args_t  *args_commit_undo;
	int                            local_result = 0;
	int                            store_in_primary_buffer;
	int                            store_in_secondary_buffer;
	int                            hdr_base_index;
	int                            needed_bytes;
	txc_buffer_circular_t          *buffer;
	struct iovec                   temp_iovec;
	struct msghdr                  temp_msghdr;
	txc_socket_msghdr_t            *temp_buffer_msghdr;
	ssize_t                        len;
	int                            s;
	int                            i;

	
	/* 
	 * FIXME: We don't handle the case where we need to consume more data 
	 * than available in the buffer. In such a case you need to request
	 * the additional data.
	 */

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
	buffer = (txc_buffer_circular_t *) txc_koa_get_buffer(koa);

	if (!speculative_read) {
		txc_koa_unlock_fd(koamgr, fd);
		if ((buffer->primary_tail != buffer->primary_head) ||
		    (buffer->secondary_tail != buffer->secondary_head)) {
			/* There are buffered data available */
			if (buffer->primary_tail != buffer->primary_head) {
				/* Consume data from the primary buffer */
				hdr_base_index = buffer->primary_head;
				temp_buffer_msghdr = (txc_socket_msghdr_t *) &(buffer->buf[hdr_base_index]);
				len = sizeof(txc_socket_msghdr_t) +
				      temp_buffer_msghdr->msg_iov_len + 
				      temp_buffer_msghdr->msg_controllen;
				buffer->primary_head += len;
			} else {
				/* Consume data from the secondary buffer and make the secondary 
				 * buffer primary 
				 */
				hdr_base_index = buffer->secondary_head;
				temp_buffer_msghdr = (txc_socket_msghdr_t *) &(buffer->buf[hdr_base_index]);
				len = sizeof(txc_socket_msghdr_t) +
				      temp_buffer_msghdr->msg_iov_len + 
				      temp_buffer_msghdr->msg_controllen;
				buffer->secondary_head += len;
				buffer->primary_tail = buffer->secondary_tail; 
				buffer->primary_head = buffer->secondary_head; 
				buffer->secondary_head = buffer->secondary_tail = 0;
			}
			/* Now fall through to do the copy */
		} else { 
			ret = txc_libc_recvmsg(fd, msg, flags);
			local_result = errno;
			goto done;
		}
	} else {
		/* READ is speculative */		
		sentinel = txc_koa_get_sentinel(koa);
		xret = txc_sentinel_tryacquire(txd, sentinel, 
									   TXC_SENTINEL_ACQUIREONRETRY);
		txc_koa_unlock_fd(koamgr, fd);
		if (xret == TXC_R_BUSYSENTINEL) {
			txc_tx_abort_transaction(txd, TXC_ABORTREASON_BUSYSENTINEL);
			TXC_INTERNALERROR("Never gets here. Transaction abort failed.\n");
		}

		/* Got sentinel. Continue with the rest of the stuff. */
		if (buffer->state == TXC_BUFFER_STATE_NON_SPECULATIVE) {
			if ((args_commit_undo = (x_recvmsg_commit_undo_args_t *)
									 txc_buffer_linear_malloc(txd->buffer_linear, 
															  sizeof(x_recvmsg_commit_undo_args_t)))
				 == NULL)
			{
				local_result = ENOMEM;
				ret = -1;
				goto done;
			}
			buffer->speculative_primary_head = buffer->primary_head;
			buffer->speculative_primary_tail = buffer->primary_tail;
			buffer->speculative_secondary_head = buffer->secondary_head;
			buffer->speculative_secondary_tail = buffer->secondary_tail;
			buffer->state = TXC_BUFFER_STATE_SPECULATIVE;

			args_commit_undo->koa = koa;
			args_commit_undo->buffer = buffer;

			txc_tx_register_commit_action(txd, x_recvmsg_commit, 
										  (void *) args_commit_undo, result,
										  TXC_TX_REGULAR_COMMIT_ACTION_ORDER);
			txc_tx_register_undo_action(txd, x_recvmsg_undo, 
										(void *) args_commit_undo, result,
										TXC_TX_REGULAR_UNDO_ACTION_ORDER);
		}

		if (buffer->speculative_primary_tail != buffer->speculative_primary_head ||
				buffer->speculative_secondary_tail != buffer->speculative_secondary_head) {
			/* There are buffered data available to consume*/
			if (buffer->speculative_primary_tail != buffer->speculative_primary_head) {
				/* Consume data from the primary buffer */
				hdr_base_index = buffer->speculative_primary_head;
				temp_buffer_msghdr = (txc_socket_msghdr_t *) &(buffer->buf[hdr_base_index]);
				len = sizeof(txc_socket_msghdr_t) + 
				      temp_buffer_msghdr->msg_iov_len + 
				      temp_buffer_msghdr->msg_controllen;
				buffer->speculative_primary_head += len;
			} else {
				/* Consume data from the secondary buffer */
				hdr_base_index = buffer->speculative_secondary_head;
				temp_buffer_msghdr = (txc_socket_msghdr_t *) &(buffer->buf[hdr_base_index]);
				buffer->speculative_secondary_head += len;
			}
		} else { 
			/* No buffered data available 
			 * 1) Find space in the buffer
			 * 3) Issue a read call to the kernel to bring the data in the buffer
			 * 4) Copy the buffered data back to the application buffer
			 */
			needed_bytes = sizeof(txc_socket_msghdr_t) + 
			               TXC_DGRAM_MAX_SIZE + TXC_CONTROLMSG_MAX_SIZE;
			store_in_primary_buffer = store_in_secondary_buffer = 0;
			if (buffer->primary_tail < (buffer->size_max - needed_bytes)) {
				/* There is space in the primary part of the buffer */
				hdr_base_index = buffer->primary_tail;
				store_in_primary_buffer = 1;
			} else if (buffer->secondary_tail < 
			           (buffer->primary_head + 1 - needed_bytes)) 
			{
				/* There is space in the secondary part of the buffer */
				hdr_base_index = buffer->secondary_tail;
				store_in_secondary_buffer = 1;
			} else {
				/* There is not space in the buffer */
				local_result = ENOMEM;
				ret = -1;
				goto done;
			}
			temp_buffer_msghdr = (txc_socket_msghdr_t *) &(buffer->buf[hdr_base_index]);
			temp_buffer_msghdr->msg_iov_base = 	(void *) (temp_buffer_msghdr + 
			                                              sizeof(txc_socket_msghdr_t));
			temp_buffer_msghdr->msg_control = (void *) (temp_buffer_msghdr + 
			                                            sizeof(txc_socket_msghdr_t) + 
			                                            TXC_DGRAM_MAX_SIZE);
			temp_iovec.iov_base = temp_buffer_msghdr->msg_iov_base; 
			temp_iovec.iov_len = TXC_DGRAM_MAX_SIZE;
			temp_msghdr.msg_name = (void *) &(temp_buffer_msghdr->msg_name);
			temp_msghdr.msg_namelen = sizeof(struct sockaddr_in);
			temp_msghdr.msg_iov = &temp_iovec;
			temp_msghdr.msg_iovlen = 1;
			temp_msghdr.msg_control = temp_buffer_msghdr->msg_control;
			temp_msghdr.msg_controllen = TXC_CONTROLMSG_MAX_SIZE;
			ret = txc_libc_recvmsg(fd, &temp_msghdr, flags);
			if (ret > 0) {
				temp_buffer_msghdr->msg_iov_len = ret;
				temp_buffer_msghdr->msg_namelen = temp_msghdr.msg_namelen;
				temp_buffer_msghdr->msg_controllen = temp_msghdr.msg_controllen;
				temp_buffer_msghdr->msg_flags = temp_msghdr.msg_flags;
				if (store_in_primary_buffer) {
					buffer->primary_tail += sizeof(txc_socket_msghdr_t) +
					                        temp_buffer_msghdr->msg_iov_len +
					                        temp_buffer_msghdr->msg_controllen;	
				} else if (store_in_secondary_buffer) {
					buffer->secondary_tail += sizeof(txc_socket_msghdr_t) +
					                          temp_buffer_msghdr->msg_iov_len +
					                          temp_buffer_msghdr->msg_controllen;	
				} else {
					TXC_INTERNALERROR("Sanity check -- shouldn't come here.\n");
				}
				/* shift control message to the end of the msg_data */
				if (temp_buffer_msghdr->msg_controllen > 0) {
					memcpy((char *) temp_buffer_msghdr->msg_iov_base + temp_buffer_msghdr->msg_iov_len, 
					       temp_buffer_msghdr->msg_control,
					       temp_buffer_msghdr->msg_controllen);
				}
			}
		}
		txc_stats_txstat_increment(txd, XCALL, x_recvmsg, 1);
	}

	/* Copy data from the buffer back to the application buffer */
	memcpy(msg->msg_name, 
	       (void *)&(temp_buffer_msghdr->msg_name), 
	       sizeof(struct sockaddr_in));
	msg->msg_namelen = temp_buffer_msghdr->msg_namelen;
	for (s=0, i=0; i<msg->msg_iovlen; i++) {
		memcpy(msg->msg_iov[i].iov_base, 
		       &temp_buffer_msghdr->msg_iov_base[s],
		       msg->msg_iov[i].iov_len);
		s += msg->msg_iov[i].iov_len;
	}
	msg->msg_flags = temp_buffer_msghdr->msg_flags;
	msg->msg_controllen = temp_buffer_msghdr->msg_controllen;
	memcpy(msg->msg_control, temp_buffer_msghdr->msg_control, 
	       temp_buffer_msghdr->msg_controllen);
	ret = s;
	local_result = 0;
done:
	if (result) {
		*result = local_result;
	}
	return ret;
}



/**
 * \brief Receives a message from a socket.
 * 
 * This xCall buffers any received data so that in case of transaction 
 * abort received data are not consumed but stay present for the next 
 * socket receive.
 *
 * <b> Execution </b>: in-place
 *
 * <b> Asynchronous failures </b>: commit, abort
 *
 * \param[in] fd The file descriptor of the socket.
 * \param[in] msg Where to store the received message.
 * \param[in] flags See man recvmsg.
 * \param[out] result Where to store any asynchronous failures.
 * failure occurred (in which case, errno is set appropriately).
 * 
 * \todo Find which flags of recvmsg can be used with x_recvmsg? 
 */
ssize_t 
XCALL_DEF(x_recvmsg)(int s, struct msghdr *msg, int flags, int *result)
{
	txc_tx_t *txd;
	int      ret;

	txd = txc_tx_get_txd();

	switch(txc_tx_get_xactstate(txd)) {
		case TXC_XACTSTATE_TRANSACTIONAL_IRREVOCABLE:
		case TXC_XACTSTATE_NONTRANSACTIONAL:
			ret = __txc_recvmsg(txd, TXC_BOOL_FALSE, s, msg, flags, result);
			break;
		case TXC_XACTSTATE_TRANSACTIONAL_RETRYABLE:
			ret = __txc_recvmsg(txd, TXC_BOOL_TRUE, s, msg, flags, result);
			break;
		default:
			TXC_INTERNALERROR("Unknown transaction state\n");
	}
	return ret;
}
