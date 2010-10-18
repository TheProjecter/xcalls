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
 * \file fm.h
 *
 * \brief Failure manager interface.
 */

#ifndef _TXC_FM_H
#define _TXC_FM_H

#include <core/tx.h>
#include <core/txdesc.h>


/* Failure manager flags */
#ifndef TXC_FM_FLAGS
# define TXC_FM_FLAGS
# define TXC_FM_NO_RETRY 0x1  /**< Indicates that the transaction should not be
                               *   retried after a failure of a compensating 
                               *   (undo) action.
                               */ 							   
#endif


void txc_fm_init(txc_tx_t *txd);
void txc_fm_register(txc_tx_t *txd, int flags, int *err);
void txc_fm_transaction_postbegin(txc_tx_t *txd);
void txc_fm_handle_commit_failure(txc_tx_t *txd, int error_result);
void txc_fm_handle_undo_failure(txc_tx_t *txd, int error_result);

#endif
