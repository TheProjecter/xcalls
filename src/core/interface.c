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
 * \file interface.c
 *
 * \brief Public interface implementation.
 *
 * Wrapper functions implementing the public interface of the library.
 *
 */

#include <sys/time.h>
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <setjmp.h>
#include <assert.h>
#include <sched.h>
#include <misc/debug.h>
#include <misc/generic_types.h>
#include <core/stats.h>
#include <core/sentinel.h>
#include <core/koa.h>
#include <core/fm.h>
#include <core/buffer.h>
#include <core/config.h>
#include <core/tx.h>


extern txc_sentinelmgr_t *txc_g_sentinelmgr;
extern txc_buffermgr_t *txc_g_buffermgr;
extern txc_koamgr_t *txc_g_koamgr;
extern txc_txmgr_t *txc_g_txmgr;
extern txc_statsmgr_t *txc_g_statsmgr;

#ifndef _TXC_COMMIT_FUNCTION_T
#define _TXC_COMMIT_FUNCTION_T
typedef void (*_TXC_commit_function_t)(void *, int *);
#endif
#ifndef _TXC_UNDO_FUNCTION_T
#define _TXC_UNDO_FUNCTION_T
typedef void (*_TXC_undo_function_t)(void *, int *);
#endif

struct timeval txc_initialization_time;

/**
 * \brief Performs global initialization of the library.
 *
 * Creates all necessary managers.
 *
 * \return Returns 0 on success.
 */
int
_TXC_global_init()
{
	txc_config_init();
	txc_sentinelmgr_create(&txc_g_sentinelmgr);
	txc_buffermgr_create(&txc_g_buffermgr);
#ifdef _TXC_STATS_BUILD	
	txc_statsmgr_create(&txc_g_statsmgr);
#endif	
	txc_txmgr_create(&txc_g_txmgr, txc_g_buffermgr, txc_g_sentinelmgr, 
	                 txc_g_statsmgr);
	txc_koamgr_create(&txc_g_koamgr, txc_g_sentinelmgr, txc_g_buffermgr);

	if (txc_runtime_settings.debug_all == TXC_BOOL_TRUE) {
		txc_runtime_settings.debug_koa      = TXC_BOOL_TRUE;
		txc_runtime_settings.debug_pool     = TXC_BOOL_TRUE;
		txc_runtime_settings.debug_sentinel = TXC_BOOL_TRUE;
		txc_runtime_settings.debug_tx       = TXC_BOOL_TRUE;
		txc_runtime_settings.debug_xcall    = TXC_BOOL_TRUE;
	}

	return (int) TXC_R_SUCCESS;
}


/**
 * \brief Shutdowns the library.
 *
 * \todo Destroy all library objects.
 * \return Returns 0 on success.
 */

int
_TXC_global_shutdown()
{
	txc_stats_print(txc_g_statsmgr);

	return (int) TXC_R_SUCCESS;
}


/**
 * \brief Initializes and assigns an xCalls descriptor to a thread.
 *
 * \return Returns 0 on success.
 */
int
_TXC_thread_init()
{
	txc_tx_create(txc_g_txmgr, &txc_l_txd);
	return (int) TXC_R_SUCCESS;
}


/**
 * \brief Aborts a transaction.
 * 
 * Requests the xCall library to abort a transaction.
 *
 * \param[in] abort_reason Reason for abort.
 * \return Never returns.
 */
void                                                                          
_TXC_transaction_abort(txc_tx_abortreason_t abort_reason)
{
	txc_tx_abort_transaction(txc_l_txd, abort_reason);
}


/**
 * \brief Returns the transactional state of the current thread.
 *
 * \return 
 *	Running outside a transaction (TXC_XACTSTATE_NONTRANSACTIONAL = 0), 
 *  Running inside a retryable tranasction (TXC_XACTSTATE_TRANSACTIONAL_RETRYABLE = 1), 
 *	Running inside an irrevocable (non-retryable) transaction  
 *  TXC_XACTSTATE_TRANSACTIONAL_IRREVOCABLE = 2
 */
txc_tx_xactstate_t
_TXC_get_xactstate() 
{
	txc_tx_t *txd = txc_l_txd;

	return txc_tx_get_xactstate(txd);
}


/**
 * \brief Informs the xCall library that a transaction is going to begin.
 *
 * \param[in] srcloc_str String describing the transaction's source location.
 * \param[in] src_file The file where the transaction is located.
 * \param[in] src_fun The function where the transaction is located.
 * \param[in] src_line The line number where the transaction is located.
 * \return None
 */
void
_TXC_transaction_pre_begin(const char *srcloc_str, const char *src_file, const char *src_fun, int src_line)
{
	txc_tx_t        *txd   = txc_l_txd;
	txc_tx_srcloc_t srcloc = {src_file, src_fun, src_line};

	txc_tx_pre_begin(txd, srcloc_str, &srcloc);
}


/**
 * \brief Informs the xCall library that a transaction has begun.
 */
void
_TXC_transaction_post_begin()
{
	txc_tx_t        *txd   = txc_l_txd;

	txc_tx_post_begin(txd);
}


/**
 * \brief Specifies what action to take when an asynchronous failure occurs.
 */
void
_TXC_fm_register(int flags, int *err)
{
	txc_tx_t        *txd   = txc_l_txd;

	txc_fm_register(txd, flags, err);
}


int
_TXC_register_commit_action(_TXC_commit_function_t function, void *args, int *error_result) 
{
	txc_tx_t        *txd   = txc_l_txd;

	return (int) txc_tx_register_commit_action(txd, 
	                                           (txc_tx_commit_function_t) function, 
	                                           args, 
	                                           error_result, 
	                                           TXC_TX_REGULAR_COMMIT_ACTION_ORDER);
}


int
_TXC_register_undo_action(_TXC_undo_function_t function, void *args, int *error_result) 
{
	txc_tx_t        *txd   = txc_l_txd;

	return (int) txc_tx_register_undo_action(txd, 
	                                         (txc_tx_undo_function_t) function, 
	                                         args, 
	                                         error_result, 
	                                         TXC_TX_REGULAR_UNDO_ACTION_ORDER);
}
