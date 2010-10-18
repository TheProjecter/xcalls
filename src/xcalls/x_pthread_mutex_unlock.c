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
 * \file x_pthread_mutex_unlock.c
 *
 * \brief x_pthread_mutex_unlock implementation.
 */

#include <pthread.h>
#include <core/tx.h>
#include <core/config.h>
#include <core/txdesc.h>
#include <misc/debug.h>
#include <misc/errno.h>
#include <xcalls/xcalls.h>

typedef struct x_commit_args_s x_commit_args_t;

struct x_commit_args_s {
	pthread_mutex_t *mutex;
};

static
void
x_pthread_mutex_unlock_commit(void *args, int *result)
{
	x_commit_args_t *args_commit = (x_commit_args_t *) args;
	int             local_result = 0;
	int             ret;

	if ((ret = pthread_mutex_unlock(args_commit->mutex)) != 0) {
		local_result = ret;
	}
	if (result) {
		*result = local_result;
	}	
}


/**
 * \brief Unlocks a mutex. 
 * 
 * It safely unlocks a mutex by releasing the lock when the transaction commits.
 * 
 * <b> Execution </b>: deferred
 *
 * <b> Asynchronous failures </b>: commit
 *
 * \param[in] mutex The mutex to unlock.
 * \param[out] result Where to store any asynchronous failures.
 * \return The new file descriptor on success, or -1 if a synchronous failure occurred 
 * (in which case, errno is set appropriately).
 */
int
XCALL_DEF(x_pthread_mutex_unlock)(pthread_mutex_t *mutex, int *result) 
{
	txc_tx_t        *txd;
	int             ret;
	x_commit_args_t *args_commit; 
	int             local_result = 0;

	txd = txc_tx_get_txd();

	switch(txc_tx_get_xactstate(txd)) {
		case TXC_XACTSTATE_TRANSACTIONAL_RETRYABLE:
			args_commit = (x_commit_args_t *)
			            txc_buffer_linear_malloc(txd->buffer_linear, 
			                                     sizeof(x_commit_args_t));
			if (args_commit == NULL) {
				TXC_INTERNALERROR("Allocation failed. Linear buffer out of space.\n");
			}
			args_commit->mutex = mutex;
			txc_tx_register_commit_action(txd, x_pthread_mutex_unlock_commit, 
			                              (void *) args_commit, result,
			                              TXC_TX_REGULAR_COMMIT_ACTION_ORDER);
			ret = 0;
			break;
		case TXC_XACTSTATE_TRANSACTIONAL_IRREVOCABLE:
		case TXC_XACTSTATE_NONTRANSACTIONAL:
			if ((ret = pthread_mutex_unlock(mutex)) != 0) { 
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
