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
 * \file x_pthread_mutex_init.c
 *
 * \brief x_pthread_mutex_init implementation.
 */

#include <pthread.h>
#include <core/tx.h>
#include <core/config.h>
#include <core/txdesc.h>
#include <misc/debug.h>
#include <misc/errno.h>
#include <xcalls/xcalls.h>

int 
XCALL_DEF(x_pthread_mutex_init)(pthread_mutex_t *mutex,
                                const pthread_mutexattr_t *attr, 
                                int *result)
{
	txc_tx_t      *txd;
	int           ret;
	int           local_result = 0;

	txd = txc_tx_get_txd();

	switch(txc_tx_get_xactstate(txd)) {
		case TXC_XACTSTATE_TRANSACTIONAL_RETRYABLE:
		case TXC_XACTSTATE_TRANSACTIONAL_IRREVOCABLE:
		case TXC_XACTSTATE_NONTRANSACTIONAL:
			if ((ret = pthread_mutex_init(mutex, attr)) != 0) { 
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
