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
 * \file stdlib.c
 *
 * \brief Implementation of transactional safe versions of some C library (libc) 
 *        standard functions.
 * 
 */

#include "tm_macros.h"
#include "libc_internal.h"
#include "tm_macros.h"
#include <ctype.h>


int
txc_libc_atoi(char *p)
{
	int n, f;

    TM_BEGIN
	n = 0;
	f = 0;
	for(;;p++) {
		switch(*p) {
			case ' ':
			case '\t':
				continue;
			case '-':
				f++;
			case '+':
				p++;
		}
		break;
	}
	while(*p >= '0' && *p <= '9')
		n = n*10 + *p++ - '0';
	return(f? -n: n);
	TM_END
}

TM_WAIVER
int txc_libc_isdigit(int c)
{
	return isdigit(c);
}

double
txc_libc_atof(char *p)
{
	register c;
	double fl, flexp, exp5;
	double big = 72057594037927936.;  /*2^56*/
	double ldexp();
	int nd;
	register eexp, exp, neg, negexp, bexp;

    TM_BEGIN
	neg = 1;
	while((c = *p++) == ' ')
		;
	if (c == '-')
		neg = -1;
	else if (c=='+')
		;
	else
		--p;

	exp = 0;
	fl = 0;
	nd = 0;
	while ((c = *p++), txc_libc_isdigit(c)) {
		if (fl<big)
			fl = 10*fl + (c-'0');
		else
			exp++;
		nd++;
	}

	if (c == '.') {
		while ((c = *p++), txc_libc_isdigit(c)) {
			if (fl<big) {
				fl = 10*fl + (c-'0');
				exp--;
			}
		nd++;
		}
	}

	negexp = 1;
	eexp = 0;
	if ((c == 'E') || (c == 'e')) {
		if ((c= *p++) == '+')
			;
		else if (c=='-')
			negexp = -1;
		else
			--p;

		while ((c = *p++), txc_libc_isdigit(c)) {
			eexp = 10*eexp+(c-'0');
		}
		if (negexp<0)
			eexp = -eexp;
		exp = exp + eexp;
	}

	negexp = 1;
	if (exp<0) {
		negexp = -1;
		exp = -exp;
	}


	if((nd+exp*negexp) < -LOGHUGE){
		fl = 0;
		exp = 0;
	}
	flexp = 1;
	exp5 = 5;
	bexp = exp;
	for (;;) {
		if (exp&01)
			flexp *= exp5;
		exp >>= 1;
		if (exp==0)
			break;
		exp5 *= exp5;
	}
	if (negexp<0)
		fl /= flexp;
	else
		fl *= flexp;
	fl = ldexp(fl, negexp*bexp);
	if (neg<0)
		fl = -fl;
	return(fl);
	TM_END
}

