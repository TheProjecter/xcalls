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
 * \file wordcopy.h
 *
 * \brief Memory copy operations interface.
 */

#ifndef _TXC_WORDCOPY_H
#define _TXC_WORDCOPY_H

#include <sys/cdefs.h>
#include "tm_macros.h"


extern TM_CALLABLE void txc_libc_wordcopy_fwd_aligned (long int, long int, size_t) __THROW;
extern TM_CALLABLE void txc_libc_wordcopy_fwd_dest_aligned (long int, long int, size_t) __THROW;
extern TM_CALLABLE void txc_libc_wordcopy_bwd_aligned (long int, long int, size_t) __THROW;
extern TM_CALLABLE void txc_libc_wordcopy_bwd_dest_aligned (long int, long int, size_t) __THROW;
#endif
