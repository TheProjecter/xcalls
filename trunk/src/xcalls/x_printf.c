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
 * \file x_printf.c
 *
 * \brief x_printf implemenation
 */
#include <stdarg.h>
#include <stdio.h>
#include <core/stats.h>
#include <xcalls/xcalls.h>


/**
 * \brief Formatted output conversion
 * 
 * <b> Execution </b>: deferred
 *
 * <b> Asynchronous failures </b>: abort
 *
 * \param[out] result Where to store any asynchronous failures.
 * \return The number of characters printed on success, or -1 if a synchronous failure occurred 
 * (in which case, errno is set appropriately).
 */
int
XCALL_DEF(x_printf)(const char *format, ...)
{
	char     buf[512];
	va_list  ap;
	int      len;
	ssize_t  ret;
	txc_tx_t *txd;

	txd = txc_tx_get_txd();

	va_start (ap, format);
	len = vsprintf(buf, format, ap);
	va_end (ap);
	ret = _XCALL(x_write_pipe)(1, buf, len, NULL);
	txc_stats_txstat_increment(txd, XCALL, x_printf, 1);
	return ret;
}
