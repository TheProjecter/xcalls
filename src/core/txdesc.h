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
 * \file txdesc.h
 *
 * \brief Transaction descriptor.
 */

#ifndef _TXDESC_H
#define _TXDESC_H

#if (_TM_SYSTEM_ITM)
#  include <itm.h>
#endif

typedef struct txc_tx_action_list_s txc_tx_commit_action_list_t;
typedef struct txc_tx_action_list_s txc_tx_undo_action_list_t;
typedef struct txc_tx_action_list_s txc_tx_action_list_t;
typedef struct txc_tx_action_list_entry_s txc_tx_commit_action_list_entry_t;
typedef struct txc_tx_action_list_entry_s txc_tx_undo_action_list_entry_t;
typedef struct txc_tx_action_list_entry_s txc_tx_action_list_entry_t;


/** Transaction descriptor. */
struct txc_tx_s {
	unsigned int                 tid;                                    /**< Thread ID as assigned by the TM library. */
	unsigned int                 tid_pthread;                            /**< Thread ID as assigned by the Pthreads library. */
	int                          sentinelmgr_undo_action_registered;     /**< If set, then sentinel manager has registered its undo action with this transaction instance. */
	int                          sentinelmgr_commit_action_registered;   /**< If set, then sentinel manager has registered its commit action with this transaction instance. */
	int                          statsmgr_undo_action_registered;        /**< If set, then statistics manager has registered its undo action with this transaction instance. */
	int                          statsmgr_commit_action_registered;      /**< If set, then statistics manager has registered its commit action with this transaction instance. */
	int                          generic_undo_action_registered;         /**< If set, then the generic transaction manager has registered its undo action with this transaction instance. */
	int                          generic_commit_action_registered;       /**< If set, then the generic transaction manager has registered its commit action with this transaction instance. */
	txc_tx_abortreason_t         abort_reason;                           /**< Indicates the reason the last transaction has been aborted. */
	unsigned int                 forced_retries;                         /**< Number of times the transaction was forced to retry. */
	txc_tx_commit_action_list_t  *commit_action_list;                    /**< List of undo actions to be executed after the transaction rollbacks. */
	txc_tx_undo_action_list_t    *undo_action_list;                      /**< List of commit actions to be executed after the transaction commits. */
	txc_sentinel_list_t          *sentinel_list;                         /**< List of sentinels the transaction has tried to acquired together with an indication of the acquisition's success/failure. */
	txc_sentinel_list_t          *sentinel_list_preacquire;              /**< List of sentinels to preacquire before transaction restarts. */
	txc_buffer_linear_t          *buffer_linear;                         /**< Private linear buffer. */
	txc_txmgr_t                  *manager;                               /**< Generic transaction manager responsible for this transaction descriptor. */
	struct txc_tx_s              *next;                                  /**< Next transaction descriptor in the list of descriptors. */
	struct txc_tx_s              *prev;                                  /**< Previous transaction descriptor in the list of descriptors. */
	txc_stats_txstat_t           *txstat;                                /**< Transactional status (non-transactional, retryable, irrevocable) */
	txc_stats_threadstat_t       *threadstat;                            /**< Statistics collection. */
	int                          fm_flags;                               /**< Failure manager flags. */
	int                          *fm_error_result;                       /**< Indicates asynchronous failure (undo or commit action failure). */ 
	txc_bool_t                   fm_abort;                               /**< Indicates whether failure manager requested this transaction to abort. */
#if (_TM_SYSTEM_ITM)
	_ITM_transaction *itm_td;                                            /**< Pointer to the TM library's transaction descriptor */ 
#endif	
};

#endif /* _TXDESC_H */
