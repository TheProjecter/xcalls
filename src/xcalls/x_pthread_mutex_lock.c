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
 * \file x_pthread_mutex_lock.c
 *
 * \brief x_pthread_mutex_lock implementation.
 */

#include <pthread.h>
#include <core/tx.h>
#include <core/config.h>
#include <core/txdesc.h>
#include <misc/debug.h>
#include <misc/errno.h>
#include <xcalls/xcalls.h>

typedef struct x_undo_args_s x_undo_args_t;

struct x_undo_args_s {
	pthread_mutex_t *mutex;
};

static
void
x_pthread_mutex_lock_undo(void *args, int *result)
{
	x_undo_args_t *args_undo = (x_undo_args_t *) args;
	int           local_result = 0;
	int           ret;

	if ((ret = pthread_mutex_unlock(args_undo->mutex)) != 0) {
		local_result = ret;
	}
	if (result) {
		*result = local_result;
	}	
}

/**
 * \brief Locks a mutex. 
 * 
 * It safely locks a mutex by releasing the lock if the transaction aborts.
 * 
 * <b> Execution </b>: in-place
 *
 * <b> Asynchronous failures </b>: abort
 *
 * \param[in] mutex The mutex to lock.
 * \param[out] result Where to store any asynchronous failures.
 * \return The new file descriptor on success, or -1 if a synchronous failure occurred 
 * (in which case, errno is set appropriately).
 */
int
XCALL_DEF(x_pthread_mutex_lock)(pthread_mutex_t *mutex, int *result) 
{
	txc_tx_t      *txd;
	int           ret;
	x_undo_args_t *args_undo; 
	int           local_result = 0;

	txd = txc_tx_get_txd();

	switch(txc_tx_get_xactstate(txd)) {
		case TXC_XACTSTATE_TRANSACTIONAL_RETRYABLE:
			args_undo = (x_undo_args_t *)
			            txc_buffer_linear_malloc(txd->buffer_linear, 
			                                     sizeof(x_undo_args_t));
			if (args_undo == NULL) {
				TXC_INTERNALERROR("Allocation failed. Linear buffer out of space.\n");
			}
			args_undo->mutex = mutex;
			if ((ret = pthread_mutex_trylock(mutex)) != 0) {
				txc_tx_abort_transaction(txd, TXC_ABORTREASON_BUSYTXLOCK);
				TXC_INTERNALERROR("Never gets here. Transaction abort failed.\n");
			}	

			txc_tx_register_undo_action(txd, x_pthread_mutex_lock_undo, 
			                            (void *) args_undo, result,
			                            TXC_TX_REGULAR_UNDO_ACTION_ORDER);
			
			break;
		case TXC_XACTSTATE_TRANSACTIONAL_IRREVOCABLE:
		case TXC_XACTSTATE_NONTRANSACTIONAL:
			if ((ret = pthread_mutex_lock(mutex)) != 0) { 
				local_result = ret;
				goto done;
			}
	}
done:
	if (result) {
		*result = local_result;
	}
	return ret;
}
