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
 * \file futex.c
 *
 * \brief High level futex implementation.
 */

#include <limits.h>
#include "lowlevellock.h"
#include "futex.h"

TM_PURE int futex_wait(void *futex, int val)
{
  int result;

  result = lll_futex_wait(futex, val);

  return(result);
}

TM_PURE 
int 
futex_wake(void * futex, int nwake)
{
	lll_futex_wake(futex, nwake);

	return(0);
}


TM_PURE 
int 
txc_cond_event_init(txc_cond_event_t * ev)
{
	ev->event = 0;
	ev->loops = 0;
	return(0);
}


TM_PURE 
int 
txc_cond_event_signal(txc_cond_event_t * ev) 
{
	ev->event++;
	futex_wake(&ev->event, 1);
	return(0);
}


TM_PURE 
int 
txc_cond_event_tm_signal_body(txc_cond_event_t * ev) 
{
	futex_wake(&ev->event, 1);
	return(0);
}


TM_CALLABLE 
int 
txc_cond_event_tm_signal(txc_cond_event_t * ev) 
{
	ev->event++;
	txc_cond_event_tm_signal_body(ev);
	return(0);
}


TM_PURE 
int 
txc_cond_event_broadcast(txc_cond_event_t * ev)
{
	ev->event++;
	futex_wake(&ev->event, INT_MAX);
	return(0);
}


TM_PURE 
int 
txc_cond_event_wait(txc_cond_event_t * ev)
{
	futex_wait(&ev->event, ev->event);
	return(0);
}


TM_CALLABLE 
int 
txc_cond_event_get(txc_cond_event_t * ev)
{
	return(ev->event);
}


TM_PURE 
int 
txc_cond_event_deferred_wait(txc_cond_event_t * ev, int val)
{
  if (val == ev->event) {
    futex_wait(&ev->event, val);
  }
  return(0);
}


TM_PURE 
void 
txc_cond_event_inc(txc_cond_event_t * ev) 
{
  ev->loops++;
}
