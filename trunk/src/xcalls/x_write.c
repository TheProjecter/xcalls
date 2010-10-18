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
 * \file x_write.c
 *
 * \brief x_write_seq, x_write_ignore, x_write_ovr implementation.
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


#define TXC_WRITE_SEQ         0x1
#define TXC_WRITE_OVR_SAVE    0x2
#define TXC_WRITE_OVR_IGNORE  0x3

typedef struct x_write_undo_args_s x_write_undo_args_t;

struct x_write_undo_args_s {
	int  fd;
	void *buf;
	int  nbyte_new;
	int  nbyte_old;
};


static
void
x_write_seq_undo(void *args, int *result)
{
	x_write_undo_args_t *args_undo = (x_write_undo_args_t *) args;
	off_t               offset;
	int                 local_result = 0;

	if ((offset = txc_libc_lseek(args_undo->fd, 
	                             -(args_undo->nbyte_new), SEEK_CUR)) < 0) 
	{
		local_result = errno;
		goto done;
	}
	if (txc_libc_ftruncate(args_undo->fd, offset) < 0) {
		local_result = errno;
	}	
done:
	if (result) {
		*result = local_result;
	}	
}


static
void
x_write_ovr_undo(void *args, int *result)
{
	x_write_undo_args_t *args_undo = (x_write_undo_args_t *) args;
	off_t               offset;
	int                 local_result = 0;

	if ((offset = txc_libc_lseek(args_undo->fd, 
	                             -(args_undo->nbyte_new), SEEK_CUR)) < 0) 
	{
		local_result = errno;
		goto done;
	}
	if (args_undo->buf) {	
		if (args_undo->nbyte_old < args_undo->nbyte_new) {
			/*
			 * (nbyte_old < nbyte_new) means we wrote more data than we were 
			 * able to read. Treat this as the write was near the end-of-file 
			 * and truncate the file to its original size.
			 * (nbyte_old < nbyte_new) could also hold if we were interrupted 
			 * by a signal but currently we treat this as a failure and report
			 * it to user.
			 */
			if (txc_libc_ftruncate(args_undo->fd, offset) < 0) {
				local_result = errno;
				goto done;
			}	
		}
		if (txc_libc_pwrite(args_undo->fd, args_undo->buf, 
		                    args_undo->nbyte_old, offset)  < 0)
		{
			local_result = errno;
		}
	}
done:
	if (result) {
		*result = local_result;
	}	
}


static
ssize_t 
x_write(int fd, const void *buf, size_t nbyte, int *result, int flags)
{
	txc_tx_t            *txd;
	txc_koamgr_t        *koamgr = txc_g_koamgr;
	txc_koa_t           *koa;
	txc_sentinel_t      *sentinel;
	txc_result_t        xret;
	int                 ret;
	void                *old_data;
	x_write_undo_args_t *args_write_undo;
	int                 local_result;


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
			switch (flags) {
				case TXC_WRITE_SEQ:
					if ((args_write_undo = (x_write_undo_args_t *)
   			                               txc_buffer_linear_malloc(txd->buffer_linear, 
					                                                sizeof(x_write_undo_args_t)))
						== NULL)
					{
						local_result = ENOMEM;
						ret = -1;
						goto error_handler_write_seq_0;
					}
					args_write_undo->fd = fd;
					if ((ret = txc_libc_write(fd, buf, nbyte)) < 0) {
						local_result = errno;
						goto error_handler_write_seq_1;
					}
					args_write_undo->nbyte_new = ret;
					txc_tx_register_undo_action(txd, x_write_seq_undo, 
					                            (void *) args_write_undo, result,
					                            TXC_TX_REGULAR_UNDO_ACTION_ORDER);
					local_result = 0;							
					ret = args_write_undo->nbyte_new;
					txc_stats_txstat_increment(txd, XCALL, x_write_seq, 1);
					goto done;

			case TXC_WRITE_OVR_IGNORE:
			case TXC_WRITE_OVR_SAVE:
					if ((args_write_undo = (x_write_undo_args_t *)
   			                               txc_buffer_linear_malloc(txd->buffer_linear, 
					                                                sizeof(x_write_undo_args_t)))
					    == NULL)
					{	
						local_result = ENOMEM;
						ret = -1;
						goto error_handler_write_ovr_0;
					}
					if (flags == TXC_WRITE_OVR_SAVE) {
						if ((old_data = (void *) txc_buffer_linear_malloc(txd->buffer_linear, 
						                                                  sizeof(char) * nbyte))
						    == NULL) 
						{
							local_result = ENOMEM;
							ret = -1;
							goto error_handler_write_ovr_1;
						}
						if ((ret = txc_libc_read(fd, old_data, nbyte)) < 0) {
							local_result = errno;
							goto error_handler_write_ovr_2;					   
						}
						args_write_undo->nbyte_old = ret;
					} else { /* TXC_WRITE_OVR_IGNORE */
						args_write_undo->nbyte_old = 0;
						old_data = NULL;
					}
					args_write_undo->buf = old_data;
					args_write_undo->fd = fd;
					if (args_write_undo->nbyte_old > 0) {
						txc_libc_lseek(fd, -args_write_undo->nbyte_old, SEEK_CUR);
					}	
					if ((ret = txc_libc_write(fd, buf, nbyte)) < 0) {
						local_result = errno;
						goto error_handler_write_ovr_2;
					}
					args_write_undo->nbyte_new = ret;
					txc_tx_register_undo_action(txd, x_write_ovr_undo, 
					                            (void *) args_write_undo, result,
					                            TXC_TX_REGULAR_UNDO_ACTION_ORDER);
					local_result = 0;							
					ret = args_write_undo->nbyte_new;
					if (flags == TXC_WRITE_OVR_SAVE) {
						txc_stats_txstat_increment(txd, XCALL, x_write_ovr, 1);
					} else {
						txc_stats_txstat_increment(txd, XCALL, x_write_ovr_ignore, 1);
					}
					goto done;

				default:
					TXC_INTERNALERROR("Unknown x_write type\n");
			}
			
		default:
			TXC_INTERNALERROR("Unknown transaction state\n");
	}

error_handler_write_ovr_2:
	if (old_data) {
		txc_buffer_linear_free(txd->buffer_linear, 
		                       sizeof(char) *nbyte);
	}

error_handler_write_seq_1:
error_handler_write_ovr_1:
	txc_buffer_linear_free(txd->buffer_linear,
		                   sizeof(x_write_undo_args_t));

error_handler_write_seq_0:
error_handler_write_ovr_0:
done:
	if (result) {
		*result = local_result;
	}
	return ret;
}


/**
 * \brief Write appends to a file.
 * 
 * <b> Execution </b>: in-place
 *
 * <b> Asynchronous failures </b>: abort
 *
 * \param[in] fd The file descriptor of the file.
 * \param[in] buf The buffer that has the data to be written.
 * \param[in] nbyte The number of bytes to write.
 * \param[out] result Where to store any asynchronous failures.
 * \return The number of written bytes on success, or -1 if a synchronous failure occurred 
 * (in which case, errno is set appropriately).
 */
ssize_t 
XCALL_DEF(x_write_seq)(int fd, const void *buf, size_t nbyte, int *result)
{
	return x_write(fd, buf, nbyte, result, TXC_WRITE_SEQ);
}


/**
 * \brief Writes to a file without saving any overwritten data.
 * 
 * <b> Execution </b>: in-place
 *
 * <b> Asynchronous failures </b>: abort
 *
 * \param[in] fd The file descriptor of the file.
 * \param[in] buf The buffer that has the data to be written.
 * \param[in] nbyte The number of bytes to write.
 * \param[out] result Where to store any asynchronous failures.
 * \return The number of written bytes on success, or -1 if a synchronous failure occurred 
 * (in which case, errno is set appropriately).
 */
ssize_t 
XCALL_DEF(x_write_ovr_ignore)(int fd, const void *buf, size_t nbyte, int *result)
{
	return x_write(fd, buf, nbyte, result, TXC_WRITE_OVR_IGNORE);
}


/**
 * \brief Alias of x_write_ovr.
 */
ssize_t 
XCALL_DEF(x_write_ovr_save)(int fd, const void *buf, size_t nbyte, int *result)
{
	return x_write(fd, buf, nbyte, result, TXC_WRITE_OVR_SAVE);
}


/**
 * \brief Writes to a file, while it saves any overwritten data.
 * 
 * <b> Execution </b>: in-place
 *
 * <b> Asynchronous failures </b>: abort
 *
 * \param[in] fd The file descriptor of the file.
 * \param[in] buf The buffer that has the data to be written.
 * \param[in] nbyte The number of bytes to write.
 * \param[out] result Where to store any asynchronous failures.
 * \return The number of written bytes on success, or -1 if a synchronous failure occurred 
 * (in which case, errno is set appropriately).
 */
ssize_t 
XCALL_DEF(x_write_ovr)(int fd, const void *buf, size_t nbyte, int *result)
{
	return x_write(fd, buf, nbyte, result, TXC_WRITE_OVR_SAVE);
}
