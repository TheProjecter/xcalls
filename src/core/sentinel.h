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
 * \file sentinel.h
 *
 * \brief Sentinel manager interface.
 */

#ifndef _TXC_SENTINEL_H
#define _TXC_SENTINEL_H

#include <misc/result.h>


#define TXC_SENTINEL_ACQUIRED               0x1 /**< Sentinel has been acquired. Drop it when transaction completes (commit/abort) or restarts execution. */
#define TXC_SENTINEL_ACQUIREONRETRY         0x2 /**< Acquire the sentinel after transaction restarts. */
/* 
 * This opaque type is normally defined in tx.h. 
 * To resolve the circular dependency between tx.h and sentinel.h,
 * define this type here and ensure that this type is not redefined. 
 */
# ifndef TYPEDEF_TXC_TX_T
# define TYPEDEF_TXC_TX_T
typedef struct txc_tx_s txc_tx_t;
# endif /* TYPEDEF_TXC_TX_T */

typedef struct txc_sentinel_s txc_sentinel_t;
typedef struct pool_element_sentinel_s pool_element_sentinel_t;
typedef struct txc_sentinelmgr_s txc_sentinelmgr_t;

typedef struct txc_sentinel_list_s txc_sentinel_list_t;
typedef struct txc_sentinel_list_entry_s txc_sentinel_list_entry_t;

extern txc_sentinelmgr_t *txc_g_sentinelmgr;

txc_result_t txc_sentinelmgr_create(txc_sentinelmgr_t **);
void txc_sentinelmgr_destroy(txc_sentinelmgr_t **sentinelmgrp);
txc_result_t txc_sentinel_create(txc_sentinelmgr_t *, txc_sentinel_t **);
void txc_sentinel_destroy(txc_sentinel_t *);
txc_result_t txc_sentinel_list_create(txc_sentinelmgr_t *sentinelmgr, txc_sentinel_list_t **sentinel_list);
txc_result_t txc_sentinel_detach(txc_sentinel_t *sentinel);
txc_result_t txc_sentinel_list_destroy(txc_sentinel_list_t **sentinel_list);
txc_result_t txc_sentinel_list_init(txc_sentinel_list_t *sentinel_list);
txc_result_t txc_sentinel_tryacquire(txc_tx_t *, txc_sentinel_t *, int);
void txc_sentinel_transaction_postbegin(txc_tx_t *txd);
void txc_sentinelmgr_print_pools(txc_sentinelmgr_t *sentinelmgr);
txc_tx_t *txc_sentinel_owner(txc_sentinel_t *sentinel);
txc_result_t txc_sentinel_is_enlisted(txc_tx_t *txd, txc_sentinel_t *sentinel);
txc_result_t txc_sentinel_enlist(txc_tx_t *txd, txc_sentinel_t *sentinel, int flags);
void txc_sentinel_register_sentinelmgr_commit_action(txc_tx_t *txd);
void txc_sentinel_register_sentinelmgr_undo_action(txc_tx_t *txd);
#endif
