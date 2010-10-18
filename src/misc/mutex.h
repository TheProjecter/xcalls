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
 * \file mutex.h
 *
 * \brief Mutex lock MACROs
 *
 * The library should use these macros whenever it needs to use mutexes.
 * This allows us to use other mutex implementations in the future if we 
 * find it useful.
 */

#ifndef _TXC_MUTEX_H
#define _TXC_MUTEX_H

#include <pthread.h>

typedef pthread_mutex_t txc_mutex_t;

#define TXC_MUTEX_INIT     pthread_mutex_init
#define TXC_MUTEX_LOCK     pthread_mutex_lock
#define TXC_MUTEX_UNLOCK   pthread_mutex_unlock
#define TXC_MUTEX_TRYLOCK  pthread_mutex_trylock

#endif
