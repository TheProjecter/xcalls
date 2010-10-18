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
 * \file libc.h
 *
 * \brief Function declarations of the transactional safe versions of some
 * C library functions.
 */

#ifndef _TXC_LIBC_H
#define _TXC_LIBC_H

#ifndef TM_WRAP
#define TM_WRAP(foo) __attribute__((tm_wrapping(foo))) 
#endif

/* STRING */
char *strcpy (char *dest, const char *src);
char *strncpy (char *s1, const char *s2, size_t n);
char *strcat (char *dest, const char *src);
char *strncat (char *s1, const char *s2, size_t n);
int strcmp (const char * a, const char * b);
int strncmp (const char *s1, const char *s2, size_t n);
int strcasecmp(const char *s1, const char *s2);
size_t strlen(char *str);
char *strchr (const char *s, int c_in);
char *strrchr (const char *s, int c);
int strncasecmp (const char *s1, const char *s2,  size_t n);

TM_WRAP(strcpy) char *txc_libc_strcpy (char *dest, const char *src);
TM_WRAP(strncpy) char *txc_libc_strncpy (char *s1, const char *s2, size_t n);
TM_WRAP(strcat) char *txc_libc_strcat (char *dest, const char *src);
TM_WRAP(strncat) char *txc_libc_strncat (char *s1, const char *s2, size_t n);
TM_WRAP(strcmp) int txc_libc_strcmp (const char * a, const char * b);
TM_WRAP(strncmp) int txc_libc_strncmp (const char *s1, const char *s2, size_t n);
TM_WRAP(strcasecmp) int txc_libc_strcasecmp(const char *s1, const char *s2);
TM_WRAP(strlen) size_t txc_libc_strlen(char *str);
TM_WRAP(strchr) char *txc_libc_strchr (const char *s, int c_in);
TM_WRAP(strrchr) char *txc_libc_strrchr (const char *s, int c);
TM_WRAP(strncasecmp) int txc_libc_strncasecmp (const char *s1, const char *s2,  size_t n);


/* STDLIB */
double atof(char *p);
int atoi(char *p);

TM_WRAP(atoi) int txc_libc_atoi(char *p);
TM_WRAP(atof) double txc_libc_atof(char *p);


/* NETWORK */
uint16_t htons (uint16_t x);
int inet_aton(const char *cp, struct in_addr *addr);
const char *inet_ntop(int af, const void *src, char *dst, socklen_t size);

TM_WRAP(htons) uint16_t txc_libc_htons (uint16_t x);
TM_WRAP(inet_aton) int txc_libc_inet_aton(const char *cp, struct in_addr *addr);
TM_WRAP(inet_ntop) const char *txc_libc_inet_ntop(int af, const void *src, char *dst, socklen_t size);
#endif
