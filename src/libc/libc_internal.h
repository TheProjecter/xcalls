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
 * \file libc_internal.h
 *
 * \brief Generic macro and type definitions used by the implemented 
 * C library functions.
 */

#ifndef _TXC_LIBC_INTERNAL_H
#define _TXC_LIBC_INTERNAL_H

#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <nl_types.h>
#include <stdarg.h>
#include <iconv.h>


#define HUGE    1.701411733192644270e38
#define LOGHUGE 39

#define ic_from(x)  (((x))&0xffff)
#define ic_to(x)  (((x)>>16)&0xffff)

# define __ptr_t    char * /* Not C++ or ANSI C.  */
//# define __ptr_t    void *  /* C++ or ANSI C.  */
typedef unsigned short int uint16_t;
typedef unsigned int uint32_t;

/* Flags for file I/O */
#define	_IOREAD	01
#define	_IOWRT	02
#define	_IONBF	04
#define	_IOMYBUF	010
#define	_IOEOF	020
#define	_IOERR	040
#define	_IOSTRG	0100
#define	_IORW	0200
#define	EOF	(-1)
#define	_NFILE	20

struct catalog_obj
{
	u_int32_t magic;
	u_int32_t plane_size;
	u_int32_t plane_depth;
	/* This is in fact two arrays in one: always a pair of name and
	 *pointer into the data area.  */
	u_int32_t name_ptr[0];
};


/* This structure will be filled after loading the catalog.  */
typedef struct catalog_info
{
	enum { mmapped, malloced } status;
    size_t plane_size;
    size_t plane_depth;
    u_int32_t *name_ptr;
    const char *strings;
    struct catalog_obj *file_ptr;
    size_t file_size;
} *__nl_catd;

enum charset {
	INVALID=0,
	ISO_8859_1,
	UTF_8,
	UCS_2,
	UCS_4,
	UTF_16_BE,
	UTF_16_LE,
	UTF_16
};


#include <inc/txc/libc.h>

#endif
