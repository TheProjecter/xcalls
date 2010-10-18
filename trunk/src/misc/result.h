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
 * \file result.h
 *
 * \brief Error/result codes.
 *
 */

#ifndef _TXC_RESULT_H
#define _TXC_RESULT_H

#define TXC_R_SUCCESS           0   /*%< success */
#define TXC_R_NOMEMORY          1   /*%< out of memory */
#define TXC_R_TIMEDOUT          2   /*%< timed out */
#define TXC_R_NOTHREADS         3   /*%< no available threads */
#define TXC_R_ADDRNOTAVAIL      4   /*%< address not available */
#define TXC_R_ADDRINUSE         5   /*%< address in use */
#define TXC_R_NOPERM            6   /*%< permission denied */
#define TXC_R_NOCONN            7   /*%< no pending connections */
#define TXC_R_NETUNREACH        8   /*%< network unreachable */
#define TXC_R_HOSTUNREACH       9   /*%< host unreachable */
#define TXC_R_NETDOWN           10  /*%< network down */
#define TXC_R_HOSTDOWN          11  /*%< host down */
#define TXC_R_CONNREFUSED       12  /*%< connection refused */
#define TXC_R_NORESOURCES       13  /*%< not enough free resources */
#define TXC_R_EOF               14  /*%< end of file */
#define TXC_R_BOUND             15  /*%< socket already bound */
#define TXC_R_RELOAD            16  /*%< reload */
#define TXC_R_LOCKBUSY          17  /*%< lock busy */
#define TXC_R_EXISTS            18  /*%< already exists */
#define TXC_R_NOSPACE           19  /*%< ran out of space */
#define TXC_R_CANCELED          20  /*%< operation canceled */
#define TXC_R_NOTBOUND          21  /*%< socket is not bound */
#define TXC_R_SHUTTINGDOWN      22  /*%< shutting down */
#define TXC_R_NOTFOUND          23  /*%< not found */
#define TXC_R_UNEXPECTEDEND     24  /*%< unexpected end of input */
#define TXC_R_FAILURE           25  /*%< generic failure */
#define TXC_R_IOERROR           26  /*%< I/O error */
#define TXC_R_NOTIMPLEMENTED    27  /*%< not implemented */
#define TXC_R_INVALIDFILE       28  /*%< invalid file */
#define TXC_R_UNEXPECTEDTOKEN   29	/*%< unexpected token */
#define TXC_R_UNEXPECTED        30  /*%< unexpected error */
#define TXC_R_ALREADYRUNNING    31  /*%< already running */
#define TXC_R_IGNORE            32  /*%< ignore */
#define TXC_R_FILENOTFOUND      33  /*%< file not found */
#define TXC_R_FILEEXISTS        34  /*%< file already exists */
#define TXC_R_NOTCONNECTED      35  /*%< socket is not connected */
#define TXC_R_RANGE             36  /*%< out of range */
#define TXC_R_NOTFILE           37  /*%< not a file */
#define TXC_R_NOTDIRECTORY      38  /*%< not a directory */
#define TXC_R_QUEUEFULL         39  /*%< queue is full */
#define ISC_R_TOOMANYOPENFILES  40  /*%< too many open files */
#define ISC_R_NOTBLOCKING       41  /*%< not blocking */
#define ISC_R_INPROGRESS        42  /*%< operation in progress */
#define ISC_R_CONNECTIONRESET   43  /*%< connection reset */
#define ISC_R_BADNUMBER         44  /*%< not a valid number */
#define ISC_R_DISABLED          45  /*%< disabled */
#define ISC_R_MAXSIZE           46  /*%< max size */
#define ISC_R_BADADDRESSFORM    47  /*%< invalid address format */
#define TXC_R_NOTINITLOCK       48  /*%< could not initialize lock */
#define TXC_R_BUSYSENTINEL      49  /*%< busy sentinel  */
#define TXC_R_NOTEXISTS         50  /*%< does not exist */	
#define TXC_R_NULLITER          51  /*%< NULL iterator */	
 	
/*% Not a result code: the number of results. */
#define TXC_R_NRESULTS          52

typedef unsigned int            txc_result_t;  /*%< Result */

#endif
