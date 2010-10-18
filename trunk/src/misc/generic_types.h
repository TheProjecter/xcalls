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
 * \file generic_types.h
 *
 * \brief Generic type definitions.
 */

#ifndef _TXC_GENERIC_TYPES_H
#define _TXC_GENERIC_TYPES_H

typedef enum {
	TXC_BOOL_FALSE = 0, 
	TXC_BOOL_TRUE = 1,
} txc_bool_t;

/**
 * \todo Use the bool standard type definition instead (stdbool.h) of txc_bool_t
 */

#endif    /* _TXC_GENERIC_TYPES_H */
