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
 * \file net.c
 *
 * \brief Implementation of transactional safe versions of some C library (libc) 
 *        network functions.
 */

#include <netinet/in.h>
#include <assert.h>
#include <errno.h>
#include "tm_macros.h"
#include "libc_internal.h"

#undef  htons
#undef  ntohs

#define NS_INADDRSZ   4


TM_WAIVER unsigned long int strtoul(const char *nptr, char **endptr,  int base);
TM_WAIVER int isspace(int c);

uint16_t
txc_libc_htons (uint16_t x)
{
#if BYTE_ORDER == BIG_ENDIAN
  return x;
#elif BYTE_ORDER == LITTLE_ENDIAN
  return __bswap_16 (x);
#else
# error "What kind of system is this?"
#endif
}

/* 
 * Check whether "cp" is a valid ASCII representation
 * of an Internet address and convert to a binary address.
 * Returns 1 if the address is valid, 0 if not.
 * This replaces inet_addr, the return value from which
 * cannot distinguish between failure and a local broadcast address.
 */
int
txc_libc_inet_aton(const char *cp, struct in_addr *addr)
{
	u_long parts[4];
	in_addr_t val;
	char *c;
	char *endptr;
	int gotend, n;

	c = (char *)cp;
	n = 0;
	/*
	 * Run through the string, grabbing numbers until
	 * the end of the string, or some error
	 */
	TM_BEGIN 
	gotend = 0;
	while (!gotend) {
		errno = 0;
		val = strtoul(c, &endptr, 0);

		if (errno == ERANGE)	/* Fail completely if it overflowed. */
			return (0);
		
		/* 
		 * If the whole string is invalid, endptr will equal
		 * c.. this way we can make sure someone hasn't
		 * gone '.12' or something which would get past
		 * the next check.
		 */
		if (endptr == c)
			return (0);
		parts[n] = val;
		c = endptr;

		/* Check the next character past the previous number's end */
		switch (*c) {
		case '.' :
			/* Make sure we only do 3 dots .. */
			if (n == 3)	/* Whoops. Quit. */
				return (0);
			n++;
			c++;
			break;

		case '\0':
			gotend = 1;
			break;

		default:
			if (isspace((unsigned char)*c)) {
				gotend = 1;
				break;
			} else
				return (0);	/* Invalid character, so fail */
		}

	}

	/*
	 * Concoct the address according to
	 * the number of parts specified.
	 */

	switch (n) {
	case 0:				/* a -- 32 bits */
		/*
		 * Nothing is necessary here.  Overflow checking was
		 * already done in strtoul().
		 */
		break;
	case 1:				/* a.b -- 8.24 bits */
		if (val > 0xffffff || parts[0] > 0xff)
			return (0);
		val |= parts[0] << 24;
		break;

	case 2:				/* a.b.c -- 8.8.16 bits */
		if (val > 0xffff || parts[0] > 0xff || parts[1] > 0xff)
			return (0);
		val |= (parts[0] << 24) | (parts[1] << 16);
		break;

	case 3:				/* a.b.c.d -- 8.8.8.8 bits */
		if (val > 0xff || parts[0] > 0xff || parts[1] > 0xff ||
		    parts[2] > 0xff)
			return (0);
		val |= (parts[0] << 24) | (parts[1] << 16) | (parts[2] << 8);
		break;
	}

	if (addr != NULL)
		addr->s_addr = htonl(val);
	return (1);

	TM_END
}


TM_WAIVER
static int inet_ntop4_sprintf(char *tmp, const char *fmt, const u_char arg1, const u_char arg2, const u_char arg3, const u_char arg4) {
	return sprintf(tmp, fmt, arg1, arg2, arg3, arg4);
}


TM_CALLABLE
static const char *
inet_ntop4(const u_char *src, char *dst, socklen_t size) 
{
//  static const char fmt[] = "%u.%u.%u.%u";
  char tmp[sizeof "255.255.255.255"];

  TM_BEGIN
  //if (inet_ntop4_sprintf(tmp, fmt, src[0], src[1], src[2], src[3]) >= size) {
  if (inet_ntop4_sprintf(tmp, "%u.%u.%u.%u", src[0], src[1], src[2], src[3]) >= size) {
		errno = ENOSPC;
		//__set_errno (ENOSPC);
    return (NULL);
  }
  return strcpy(dst, tmp);
  TM_END
}

const char *
txc_libc_inet_ntop(int af, const void *src, char *dst, socklen_t size) 
{
  TM_BEGIN
  switch (af) {
  case AF_INET:
    return (inet_ntop4(src, dst, size));
  case AF_INET6:
		assert(0);
  default:
		errno = EAFNOSUPPORT;
    //__set_errno (EAFNOSUPPORT);
    return (NULL);
  }
  TM_END
}

