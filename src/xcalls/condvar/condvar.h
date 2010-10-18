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
 * \file condvar.h
 *
 * \brief Transactional condition variable MACROs.
 *
 * Notes on condition variables:
 *
 * - currently requires a system call on all signals. 
 * - Problem: how do you atomically test if anyone is waiting? Just read # of waiters, if zero can ignore?
*/

#ifndef _TXC_CONDVAR_H
#define _TXC_CONDVAR_H

#include "futex.h"
#pragma warning(disable:177) 

#define _BEGIN_TX __tm_atomic
#define _END_TX

#define txc_cond_event_inc_loops(_ev_) txc_cond_event_inc(_ev_)
#define LABEL_CAT(a, b) LABEL_CAT_I(a, b)
#define LABEL_CAT_I(a, b) a ## b
#define NEXT_LABEL(T) LABEL_HELPER(T,LABEL_CAT(label,__LINE__))
#define LABEL_HELPER(T,L)  do { T = &&L; L: __dummy = 0;} while(0)

#define COND_XACT_BEGIN                                                      \
 {                                                                           \
   int __do_wait = 0, __cond_val, __do_exit = 0;                             \
   int __dummy = 0;                                                          \
   txc_cond_event_t * __cond_var;                                            \
   void * __wait_label;                                                      \
   void * __begin_label;                                                     \
   do {                                                                      \
     if (__do_wait) {                                                        \
       txc_cond_event_deferred_wait(__cond_var,__cond_val);                  \
     }                                                                       \
     _BEGIN_TX {                                                             \
       NEXT_LABEL(__begin_label);                                            \
       if (!__do_exit) {                                                     \
         if (__do_wait) {                                                    \
           __do_wait = 0;                                                    \
           goto *__wait_label;                                               \
         } 

#define COND_XACT_END                                                        \
       } else {__do_exit = 0;} }                                             \
     _END_TX;                                                                \
   } while (__do_wait); }


#define txc_cond_wait_label( _cvar_, _label_)                                \
 {                                                                           \
   __cond_var = (_cvar_);		                                             \
   __cond_val = txc_cond_event_get(_cvar_);	                                 \
   __do_wait = 1;                                                            \
   __do_exit = 1;                                                            \
   __wait_label = && _label_;                                                \
   goto *__begin_label;                                                      \
   _label_:                                                                  \
   __dummy = 0;	                                                             \
 }


#define txc_cond_wait( _cvar_)                                               \
 do {                                                                        \
   txc_cond_event_inc_loops( _cvar_ );                                       \
   txc_cond_wait_label( _cvar_, LABEL_CAT(__txc_cond_wait_, __LINE__));      \
 } while (0)

#endif /* TXC_CONDVAR_H */



