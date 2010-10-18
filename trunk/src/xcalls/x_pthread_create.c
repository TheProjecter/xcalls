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
 * \file x_pthread_create.c
 *
 * \brief x_pthread_create implementation.
 */

#include <pthread.h>
#include <core/tx.h>
#include <core/config.h>
#include <core/txdesc.h>
#include <misc/debug.h>
#include <misc/errno.h>
#include <misc/malloc.h>
#include <xcalls/xcalls.h>

typedef struct x_pthread_create_control_block_s x_pthread_create_control_block_t;

struct  x_pthread_create_control_block_s {
	pthread_mutex_t start_lock;
	volatile void   *(*start)(void *);
	void            *arg;
};


static
void
x_pthread_create_commit(void *args, int *result)
{
	x_pthread_create_control_block_t *control_block = (x_pthread_create_control_block_t *) args;
	int                              local_result = 0;

	pthread_mutex_unlock(&control_block->start_lock);

	if (result) {
		*result = local_result;
	}	
}


static
void
x_pthread_create_undo(void *args, int *result)
{
	x_pthread_create_control_block_t *control_block = (x_pthread_create_control_block_t *) args;
	int                              local_result = 0;

	/* 
	 * Don't let child thread execute start function by nullifying the 
	 * start_routine pointer.
	 */
	control_block->start = NULL;
	pthread_mutex_unlock(&control_block->start_lock);

	if (result) {
		*result = local_result;
	}	
}



static
void *
x_pthread_create_start_indirection(void *args)
{
	void                             *(*start_routine)(void *);
	void                             *myarg;
	x_pthread_create_control_block_t *control_block = (x_pthread_create_control_block_t *) args;

	myarg = control_block->arg;
	pthread_mutex_lock(&control_block->start_lock);
	start_routine = control_block->start;
	FREE(control_block);
	/* If start_routine is NULL then parent aborted so don't run child */
	if (start_routine) {
		return start_routine(myarg);
	} else {
		return (void *) 0x0;
	}
}


/**
 * \brief Creates a thread
 * 
 * <b> Execution </b>: in-place creation of the thread but deferred execution of 
 *                     the start_routine
 *
 * <b> Asynchronous failures </b>: commit, abort
 *
 * \param[out] thread The new thread's identifier.
 * \param[in] attr Creation attributes.
 * \param[in] start_routine The routine that the new thread executes.
 * \param[in] arg Parameter passed to start_routine
 * \param[out] result Where to store any asynchronous failures.
 * \return The new file descriptor on success, or -1 if a synchronous failure occurred 
 * (in which case, errno is set appropriately).
 */
int
XCALL_DEF(x_pthread_create)(pthread_t *thread,
   	                        const pthread_attr_t *attr,
					        void *(*start_routine)(void*),
                            void *arg,
                            int *result)
{
	txc_tx_t                         *txd;
	int                              ret;
	x_pthread_create_control_block_t *control_block;
	int                              local_result = 0;

	txd = txc_tx_get_txd();

	switch(txc_tx_get_xactstate(txd)) {
		case TXC_XACTSTATE_TRANSACTIONAL_RETRYABLE:
			control_block = (x_pthread_create_control_block_t *) MALLOC(sizeof(x_pthread_create_control_block_t));

			control_block->arg = arg;
			control_block->start = start_routine;
			pthread_mutex_init(&control_block->start_lock, NULL);
			pthread_mutex_lock(&control_block->start_lock);
			pthread_create(thread, attr, x_pthread_create_start_indirection, (void *) control_block);

			txc_tx_register_commit_action(txd, x_pthread_create_commit, 
			                              (void *) control_block, result,
			                              TXC_TX_REGULAR_COMMIT_ACTION_ORDER);
			txc_tx_register_undo_action(txd, x_pthread_create_undo, 
			                              (void *) control_block, result,
			                              TXC_TX_REGULAR_UNDO_ACTION_ORDER);

			ret = 0;
			break;
		case TXC_XACTSTATE_TRANSACTIONAL_IRREVOCABLE:
		case TXC_XACTSTATE_NONTRANSACTIONAL:
			if ((ret = pthread_create(thread, attr, start_routine, arg)) != 0) { 
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
