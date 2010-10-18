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
 * \file fm.c
 *
 * \brief Failure manager implementation.
 *
 * This subsystem informs programs when a compensating or deferred 
 * action fails. It exposes an interface to applications to specify an 
 * action to take or a variable to set when an asynchronous failure 
 * (one during commit or abort) occurs.
 * For example, the program may specify that the transaction
 * should not be retried if a compensating action fails by calling
 * the manager with the X NO RETRY flag.
 */

#include <misc/result.h>
#include <misc/malloc.h>
#include <misc/debug.h>
#include <misc/mutex.h>
#include <misc/generic_types.h>
#include <core/config.h>
#include <core/fm.h>
#include <core/tx.h>
#include <core/txdesc.h>

void
txc_fm_init(txc_tx_t *txd)
{
	txd->fm_abort = TXC_BOOL_FALSE;
	txd->fm_error_result = 0;
	txd->fm_flags = 0;
}

void
txc_fm_register(txc_tx_t *txd, int flags, int *error_result)
{
	txd->fm_flags = flags;
	txd->fm_error_result = error_result;
}


void 
txc_fm_transaction_postbegin(txc_tx_t *txd)
{
	if (txd->fm_abort == TXC_BOOL_TRUE) {
		txc_tx_abort_transaction(txd, TXC_ABORTREASON_USERABORT);
	}
}


void
txc_fm_handle_commit_failure(txc_tx_t *txd, int error_result)
{
	if (txd->fm_error_result) {
		*txd->fm_error_result = error_result;
	}	
}


void
txc_fm_handle_undo_failure(txc_tx_t *txd, int error_result)
{
	if (error_result) {
		if (txd->fm_error_result) {
			*txd->fm_error_result = error_result;
		}	
		if (txd->fm_flags && TXC_FM_NO_RETRY) {
			/* 
			 * The transaction is already at the rollback state and will
			 * restart after finished with the undo actions. Set the
			 * fm_abort flag so that we abort the transaction right
			 * after restart.
			 */
			txd->fm_abort = TXC_BOOL_TRUE;
		}	
	}
	/* 
	 * Reset the failure flags for the next dynamic instance of the 
	 * current transaction.
	 */
	txd->fm_flags = 0;
}
