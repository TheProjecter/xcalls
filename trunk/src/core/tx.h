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
 * \file tx.h
 * 
 * \brief Generic transaction manager interface.
 */

#ifndef _TX_H
#define _TX_H

#include <core/buffer.h>
#include <core/sentinel.h>

/* 
 * This opaque type is defined here. 
 * To resolve any circular dependencies between tx.h and other header files,
 * allow other header files to define this opaque type and define it if others
 * have not done so.  
 */
# ifndef TYPEDEF_TXC_TX_T
# define TYPEDEF_TXC_TX_T
typedef struct txc_tx_s txc_tx_t;
# endif /* TYPEDEF_TXC_TX_T */

typedef struct txc_txmgr_s txc_txmgr_t;

typedef void (*txc_tx_function_t)(void *, int *);
typedef void (*txc_tx_undo_function_t)(void *, int *);
typedef void (*txc_tx_commit_function_t)(void *, int *);


/* This type is also exported to users of the library via txc.h 
 * Must keep it in sync.
 */
# ifndef TYPEDEF_TXC_TX_ABORTREASON_T
# define TYPEDEF_TXC_TX_ABORTREASON_T
typedef enum {
	TXC_ABORTREASON_TMCONFLICT = 0, 
    TXC_ABORTREASON_USERABORT = 1, 
	TXC_ABORTREASON_USERRETRY = 2, 
	TXC_ABORTREASON_BUSYSENTINEL = 4, 
	TXC_ABORTREASON_INCONSISTENCY = 8, 
	TXC_ABORTREASON_BUSYTXLOCK = 16, 
} txc_tx_abortreason_t;
# endif /* TYPEDEF_TXC_TX_ABORT_REASON_T */


/* This type is also exported to users of the library via txc.h 
 * Must keep it in sync.
 */
# ifndef TYPEDEF_TXC_TX_XACTSTATE_T
# define TYPEDEF_TXC_TX_XACTSTATE_T
typedef enum {
	TXC_XACTSTATE_NONTRANSACTIONAL = 0, 
    TXC_XACTSTATE_TRANSACTIONAL_RETRYABLE = 1, 
	TXC_XACTSTATE_TRANSACTIONAL_IRREVOCABLE = 2
} txc_tx_xactstate_t;
# endif /* TYPEDEF_TXC_TX_XACTSTATE_T */


typedef struct txc_tx_srcloc_s txc_tx_srcloc_t;

struct txc_tx_srcloc_s {
	const char *file;
	const char *fun;
	int  line;
};


extern txc_txmgr_t *txc_g_txmgr;
extern __thread txc_tx_t *txc_l_txd;

int txc_tx_xact_state_get();

#include <core/stats.h>

txc_result_t txc_txmgr_create(txc_txmgr_t **, txc_buffermgr_t *, txc_sentinelmgr_t *, txc_statsmgr_t *statsmgr);
txc_result_t txc_txmgr_destroy(txc_txmgr_t **);
txc_result_t txc_tx_create(txc_txmgr_t *, txc_tx_t **);
txc_result_t txc_tx_destroy(txc_tx_t **);
void txc_tx_abort_transaction(txc_tx_t *, txc_tx_abortreason_t);
txc_result_t txc_tx_register_commit_action(txc_tx_t *, txc_tx_commit_function_t, void *, int *, int); 
txc_result_t txc_tx_register_undo_action(txc_tx_t *, txc_tx_undo_function_t, void *, int *, int);
txc_tx_t *txc_tx_get_txd();   
unsigned int txc_tx_get_tid(txc_tx_t *txd);
unsigned int txc_tx_get_tid_pthread(txc_tx_t *txd);
txc_tx_xactstate_t txc_tx_get_xactstate(txc_tx_t *txd); 
unsigned int txc_tx_get_forced_retries(txc_tx_t *txd);
void txc_tx_pre_begin(txc_tx_t *txd, const char *srcloc_str, txc_tx_srcloc_t *srcloc);
void txc_tx_post_begin(txc_tx_t *txd);

void txc_txmgr_print(txc_txmgr_t *);
txc_result_t txc_tx_exists(txc_txmgr_t *, txc_tx_t *);

#endif    /* _TX_H */
