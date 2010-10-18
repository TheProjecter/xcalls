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
 * \file itm.h
 *
 * \brief Intel STM wrapper functions for use by the 
 * generic transactional memory manager.
 */

#if (_TM_SYSTEM_ITM)

#ifndef _TXC_ITM_H
#define _TXC_ITM_H

#include <itm.h>

static inline
void
tmsystem_tx_init(txc_tx_t *txd)
{
	txd->itm_td = _ITM_getTransaction();
	txd->tid = (unsigned int) _ITM_getThreadnum();
}


_ITM_CALL_CONVENTION void
tmsystem_generic_undo_action(void *arg) 
{
	tx_generic_undo_action((txc_tx_t *) arg);
}


_ITM_CALL_CONVENTION void
tmsystem_generic_commit_action(void *arg) 
{
	tx_generic_commit_action((txc_tx_t *) arg);
}


static inline 
void
tmsystem_register_generic_undo_action(txc_tx_t *txd)
{
	if (txd->generic_undo_action_registered == 0) {
		_ITM_addUserUndoAction(txd->itm_td, 
		                       tmsystem_generic_undo_action, (void *) txd);
		txd->generic_undo_action_registered = 1;					   
	}
}


static inline 
void
tmsystem_register_generic_commit_action(txc_tx_t *txd)
{
	if (txd->generic_commit_action_registered == 0) {
		/* 
		 * Using ticket number 2 to register commit action to the outer 
		 * transaction 
		 */
		_ITM_addUserCommitAction(txd->itm_td, 
		                         tmsystem_generic_commit_action, 2, (void *) txd);
		txd->generic_commit_action_registered = 1;					   
	}
}


static inline
void
tmsystem_abort_transaction(txc_tx_t *txd, txc_tx_abortreason_t abort_reason)
{
	tmsystem_register_generic_undo_action(txd);
    txd->abort_reason = abort_reason;
	txd->forced_retries++;
	switch (abort_reason) {
		case TXC_ABORTREASON_USERABORT:
			_ITM_abortTransaction(txd->itm_td, userAbort, NULL);
			break;
		case TXC_ABORTREASON_TMCONFLICT:
			_ITM_abortTransaction(txd->itm_td, TMConflict, NULL);
			break;
		case TXC_ABORTREASON_USERRETRY:
		case TXC_ABORTREASON_BUSYSENTINEL:
		case TXC_ABORTREASON_INCONSISTENCY:
		case TXC_ABORTREASON_BUSYTXLOCK:
			_ITM_abortTransaction(txd->itm_td, userRetry, NULL);
			break;
		default:
			TXC_INTERNALERROR("Unknown abort reason\n");
			/* never returns here */
	}
}


static inline 
txc_tx_xactstate_t
tmsystem_get_xactstate(txc_tx_t *txd) 
{
    _ITM_transaction *td;
    _ITM_howExecuting mode;

    td = txd->itm_td;
    mode = _ITM_inTransaction(td);
	switch (mode) {
		case outsideTransaction:
			return TXC_XACTSTATE_NONTRANSACTIONAL;
		case inRetryableTransaction:
			return TXC_XACTSTATE_TRANSACTIONAL_RETRYABLE;
		case inIrrevocableTransaction:
			return TXC_XACTSTATE_TRANSACTIONAL_IRREVOCABLE;
		default:
			TXC_INTERNALERROR("Unknown transaction state");	
	}   
}


#endif /* _TXC_ITM_H */

#endif /* _TM_SYSTEM_ITM */
