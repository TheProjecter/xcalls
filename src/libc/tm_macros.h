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
 * \file tm_macros.h
 *
 * \brief Macros for transactional annotations.
 */

#ifndef _TXC_TM_MACROS_H
#define _TXC_TM_MACROS_H

#define TM_WAIVER   __attribute__ ((tm_pure)) 
#define TM_PURE     __attribute__ ((tm_pure)) 
#define TM_CALLABLE __attribute__ ((tm_callable)) 
#define TM_WRAP(foo) __attribute__((tm_wrapping(foo))) 
#define TM_BEGIN __tm_atomic {
#define TM_END }

#endif
