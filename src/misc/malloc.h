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
 * \file malloc.h
 *
 * \brief Memory allocation MACROs
 *
 * The library should use these macros whenever it needs to call the 
 * memory allocator. This allows us to use other more scalable allocators
 * in the future if we find it useful.
 */

#ifndef _TXC_MALLOC_H
#define _TXC_MALLOC_H

#include <stdlib.h>

#define MALLOC  malloc
#define CALLOC  calloc
#define REALLOC realloc 
#define FREE    free

#endif /* _TXC_MALLOC_H */
