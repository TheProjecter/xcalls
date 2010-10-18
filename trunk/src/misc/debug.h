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
 * \file debug.h
 *
 * \brief Debugging helper MACROs.
 */

#ifndef _TXC_DEBUG_H
#define _TXC_DEBUG_H

#include <stdio.h>
#include <assert.h>

/* Debugging levels per module */
#define TXC_DEBUG_POOL     txc_runtime_settings.debug_pool
#define TXC_DEBUG_KOA      txc_runtime_settings.debug_koa
#define TXC_DEBUG_SENTINEL txc_runtime_settings.debug_sentinel
#define TXC_DEBUG_TX       txc_runtime_settings.debug_tx
#define TXC_DEBUG_XCALL    txc_runtime_settings.debug_xcall

#define TXC_DEBUG_OUT             stderr


/* Operations on timevals */
#define timevalclear(tvp)      ((tvp)->tv_sec = (tvp)->tv_usec = 0)
#define timevaladd(vvp, uvp)                                                \
    do {                                                                    \
        (vvp)->tv_sec += (uvp)->tv_sec;                                     \
        (vvp)->tv_usec += (uvp)->tv_usec;                                   \
        if ((vvp)->tv_usec >= 1000000) {                                    \
            (vvp)->tv_sec++;                                                \
            (vvp)->tv_usec -= 1000000;                                      \
        }                                                                   \
    } while (0)

#define timevalsub(vvp, uvp)                                                \
    do {                                                                    \
        (vvp)->tv_sec -= (uvp)->tv_sec;                                     \
        (vvp)->tv_usec -= (uvp)->tv_usec;                                   \
        if ((vvp)->tv_usec < 0) {                                           \
            (vvp)->tv_sec--;                                                \
            (vvp)->tv_usec += 1000000;                                      \
        }                                                                   \
    } while (0)


#define TXC_INTERNALERROR(msg, ...)                                         \
    txc_debug_printmsg(__FILE__, __LINE__, 1, "Internal Error: ",           \
                       msg, ##__VA_ARGS__ )

#define TXC_ERROR(msg, ...)                                                 \
    txc_debug_printmsg(NULL, 0, 1, "Error",	msg, ##__VA_ARGS__ )


#ifdef _TXC_DEBUG_BUILD

# define TXC_WARNING(msg, ...)                                              \
    txc_debug_printmsg(NULL, 0, 0, "Warning",	msg, ##__VA_ARGS__ )


# define TXC_DEBUG_PRINT(debug_level, msg, ...)                             \
    txc_debug_printmsg_L(debug_level,	msg, ##__VA_ARGS__ )

# define TXC_ASSERT(condition) assert(condition) 

#else /* !_TXC_DEBUG_BUILD */

# define TXC_WARNING(msg, ...)                   ((void) 0)
# define TXC_DEBUG_PRINT(debug_level, msg, ...)  ((void) 0)
# define TXC_ASSERT(condition)                   ((void) 0)

#endif /* !_TXC_DEBUG_BUILD */


/* Interface functions */

void txc_debug_printmsg(char *file, int line, int fatal, const char *prefix, const char *strformat, ...); 
void txc_debug_printmsg_L(int debug_level, const char *strformat, ...); 

#endif /* _TXC_DEBUG_H */
