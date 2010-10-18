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
 * \file debug.c
 *
 * \brief Debugging helper functions.
 *
 * You should use the debugging MACROs defined in debug.h instead of using 
 * these functions directly.
 */

#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <misc/debug.h>
#include <core/config.h>
#include <core/tx.h>

void
txc_debug_printmsg(char *file, int line, int fatal, const char *prefix,
                   const char *strformat, ...) 
{
	char    buf[512];
	va_list ap;
	int     len;
	if (prefix) {
		len = sprintf(buf, "%s ", prefix); 
	}
	if (file) {
		len += sprintf(&buf[len], "%s(%d)", file, line); 
	}
	if (file || prefix) {
		len += sprintf(&buf[len], ": "); 
	}
	va_start (ap, strformat);
	vsnprintf(&buf[len], sizeof (buf) - 1 - len, strformat, ap);
	va_end (ap);
	fprintf(TXC_DEBUG_OUT, "%s", buf);
	if (fatal) {
		exit(1);
	}
}


void 
txc_debug_printmsg_L(int debug_flag, const char *strformat, ...) 
{
	char           msg[512];
	int            len; 
	va_list        ap;
	struct timeval curtime;
	int            xact_state;
	unsigned int   tid;
	unsigned int   tid_pthread;
	
	if (!debug_flag) {
		return;
	} 

	xact_state = txc_tx_get_xactstate(txc_l_txd);
	tid = txc_tx_get_tid(txc_l_txd); 
	tid_pthread = txc_tx_get_tid_pthread(txc_l_txd); 
	gettimeofday(&curtime, NULL); 
	len = sprintf(msg, "[TXC_DEBUG: T-%02u (%u) TS=%04u%06u TX=%d PC=%p] ", 
	              tid, 
	              tid_pthread, 
	              (unsigned int) curtime.tv_sec, (unsigned int) curtime.tv_usec,
	              xact_state,
	              __builtin_return_address(0)); 
	va_start (ap, strformat);
	vsnprintf(&msg[len], sizeof (msg) - 1 - len, strformat, ap);
	va_end (ap);

	len = strlen(msg);
	msg[len] = '\0';

	fprintf(TXC_DEBUG_OUT, "%s", msg);
}
