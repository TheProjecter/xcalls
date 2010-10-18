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
 * \file tls-simple.h
 *
 * \brief Simple thread local storage used by transactional condition variables.
 *
 * \todo Integrate it into the xCalls' library generic thread descriptor.
 */

/* Definition for thread-local data handling.  nptl/i386 version.
   Copyright (C) 2002, 2003, 2004, 2005 Free Software Foundation, Inc.
   This file is part of the GNU C Library.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
   02111-1307 USA.  */

#ifndef _TXC_TLS_SIMPLE_H
#define _TXC_TLS_SIMPLE_H 1

//#include <dl-sysdep.h>
#ifndef __ASSEMBLER__
# include <stdbool.h>
# include <stddef.h>
# include <stdint.h>
# include <stdlib.h>


/* Type for the dtv.  */
typedef union dtv
{
	size_t counter;
	struct
	{
		void *val;
		bool is_static;
	} pointer;
} dtv_t;


typedef struct
{
	void      *tcb;	            /* Pointer to the TCB.  Not necessarily the
	                               thread descriptor used by libpthread.  */
	dtv_t     *dtv;
	void      *self;		    /* Pointer to the thread descriptor.  */
	int       multiple_threads;
	uintptr_t sysinfo;
	uintptr_t stack_guard;
	uintptr_t pointer_guard;
	int       gscope_flag;
} tcbhead_t;

# define TLS_MULTIPLE_THREADS_IN_TCB 1

#else /* __ASSEMBLER__ */
# include <tcb-offsets.h>
#endif

#endif	/* _TXC_TLS_SIMPLE_H */
