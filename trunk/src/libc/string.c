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
 * \file string.c
 *
 * \brief Implementation of transactional safe versions of some C library (libc) 
 *        string functions.
 * 
 */

/* 
 * Some of the code below has been extracted from the uClibc. 
 * Library's license follows.
 */

/* uClibc
 *
 * Copyright (C) 2002 by Erik Andersen <andersen@uclibc.org>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Library General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU Library General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 */

#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <ctype.h>
#include "memcopy.h"
#include "pagecopy.h"
#include "libc_internal.h"
#include "tm_macros.h"


TM_WAIVER int tolower(int c);
TM_WAIVER extern __const __int32_t **__ctype_tolower_loc (void)
     __attribute__ ((__const));


char *
txc_libc_strcpy (char *dest, const char *src)
{
   const char *p;
   char *q; 

   TM_BEGIN
   for(p = src, q = dest; *p != '\0'; p++, q++)
      *q = *p;
			  
   *q = '\0';
   TM_END

   return dest;
}


char *
txc_libc_strncpy (char *s1, const char *s2, size_t n)
{
  reg_char c;
  char *s = s1;

  TM_BEGIN
  --s1;

  if (n >= 4)
    {
      size_t n4 = n >> 2;

      for (;;)
	{
	  c = *s2++;
	  *++s1 = c;
	  if (c == '\0')
	    break;
	  c = *s2++;
	  *++s1 = c;
	  if (c == '\0')
	    break;
	  c = *s2++;
	  *++s1 = c;
	  if (c == '\0')
	    break;
	  c = *s2++;
	  *++s1 = c;
	  if (c == '\0')
	    break;
	  if (--n4 == 0)
	    goto last_chars;
	}
      n = n - (s1 - s) - 1;
      if (n == 0)
	return s;
      goto zero_fill;
    }

 last_chars:
  n &= 3;
  if (n == 0)
    return s;

  do
    {
      c = *s2++;
      *++s1 = c;
      if (--n == 0)
	return s;
    }
  while (c != '\0');

 zero_fill:
  do
    *++s1 = '\0';
  while (--n > 0);

  return s;
  TM_END
}


char *
txc_libc_strcat (char *dest, const char *src)
{
  char *s1 = dest;
  const char *s2 = src;
  reg_char c;

  /* Find the end of the string.  */
  do
    c = *s1++;
  while (c != '\0');

  /* Make S1 point before the next character, so we can increment
     it while memory is read (wins on pipelined cpus).  */
  s1 -= 2;

  do
    {
      c = *s2++;
      *++s1 = c;
    }
  while (c != '\0');

  return dest;
}


char *
txc_libc_strncat (char *s1, const char *s2, size_t n)
{
  reg_char c;
  char *s = s1;

  TM_BEGIN 
  /* Find the end of S1.  */
  do
    c = *s1++;
  while (c != '\0');

  /* Make S1 point before next character, so we can increment
     it while memory is read (wins on pipelined cpus).  */
  s1 -= 2;

  if (n >= 4)
    {
      size_t n4 = n >> 2;
      do
	{
	  c = *s2++;
	  *++s1 = c;
	  if (c == '\0')
	    return s;
	  c = *s2++;
	  *++s1 = c;
	  if (c == '\0')
	    return s;
	  c = *s2++;
	  *++s1 = c;
	  if (c == '\0')
	    return s;
	  c = *s2++;
	  *++s1 = c;
	  if (c == '\0')
	    return s;
	} while (--n4 > 0);
      n &= 3;
    }

  while (n > 0)
    {
      c = *s2++;
      *++s1 = c;
      if (c == '\0')
	return s;
      n--;
    }

  if (c != '\0')
    *++s1 = '\0';

  return s;
  TM_END
}


int 
txc_libc_strcmp (const char * a, const char * b)
{
	char c1;
	char c2;

	TM_BEGIN
	do 
	{
		int delta = (c1=*a++)-(c2=*b++);
		if (delta)
			return delta;
	} while (c1 && c2);

	return 0;
	TM_END
}

/* Compare no more than N characters of S1 and S2,
   returning less than, equal to or greater than zero
   if S1 is lexicographically less than, equal to or
   greater than S2.  */
int
txc_libc_strncmp (const char *s1, const char *s2, size_t n)
{
  unsigned reg_char c1 = '\0';
  unsigned reg_char c2 = '\0';

  TM_BEGIN
  if (n >= 4)
    {
      size_t n4 = n >> 2;
      do
	{
    c1 = (unsigned char) *s1++;
    c2 = (unsigned char) *s2++;
    if (c1 == '\0' || c1 != c2)
      return c1 - c2;
    c1 = (unsigned char) *s1++;
    c2 = (unsigned char) *s2++;
    if (c1 == '\0' || c1 != c2)
      return c1 - c2;
    c1 = (unsigned char) *s1++;
    c2 = (unsigned char) *s2++;
    if (c1 == '\0' || c1 != c2)
      return c1 - c2;
    c1 = (unsigned char) *s1++;
    c2 = (unsigned char) *s2++;
    if (c1 == '\0' || c1 != c2)
      return c1 - c2;
  } while (--n4 > 0);
      n &= 3;
    }

  while (n > 0)
    {
      c1 = (unsigned char) *s1++;
      c2 = (unsigned char) *s2++;
      if (c1 == '\0' || c1 != c2)
  return c1 - c2;
      n--;
    }

  return c1 - c2;
  TM_END
}


int 
txc_libc_strcasecmp(const char *s1, const char *s2) 
{
  TM_BEGIN
  while (*s1 != '\0' && tolower(*s1) == tolower(*s2))
    {
      s1++;
      s2++;
    }

  return tolower(*(unsigned char *) s1) - tolower(*(unsigned char *) s2);
  TM_END
}


size_t 
txc_libc_strlen(char *str)
{
	char *s;

	TM_BEGIN
   	if (str == NULL) {
		return -1; /* invalid string */
	}
				 
	for (s=str; *s; s++);
	return s-str;
	TM_END
}



/* Find the first occurrence of C in S.  */
char *
txc_libc_strchr (const char *s, int c_in)
{
  const unsigned char *char_ptr;
  const unsigned long int *longword_ptr;
  unsigned long int longword, magic_bits, charmask;
  unsigned reg_char c;

  c = (unsigned char) c_in;

  TM_BEGIN

  /* Handle the first few characters by reading one character at a time.
     Do this until CHAR_PTR is aligned on a longword boundary.  */
  for (char_ptr = (const unsigned char *) s;
       ((unsigned long int) char_ptr & (sizeof (longword) - 1)) != 0;
       ++char_ptr)
    if (*char_ptr == c)
      return (void *) char_ptr;
    else if (*char_ptr == '\0')
      return NULL;

  /* All these elucidatory comments refer to 4-byte longwords,
     but the theory applies equally well to 8-byte longwords.  */

  longword_ptr = (unsigned long int *) char_ptr;

  /* Bits 31, 24, 16, and 8 of this number are zero.  Call these bits
     the "holes."  Note that there is a hole just to the left of
     each byte, with an extra at the end:

     bits:  01111110 11111110 11111110 11111111
     bytes: AAAAAAAA BBBBBBBB CCCCCCCC DDDDDDDD

     The 1-bits make sure that carries propagate to the next 0-bit.
     The 0-bits provide holes for carries to fall into.  */
  switch (sizeof (longword))
    {
    case 4: magic_bits = 0x7efefeffL; break;
    case 8: magic_bits = ((0x7efefefeL << 16) << 16) | 0xfefefeffL; break;
    default:
      abort ();
    }

  /* Set up a longword, each of whose bytes is C.  */
  charmask = c | (c << 8);
  charmask |= charmask << 16;
  if (sizeof (longword) > 4)
    /* Do the shift in two steps to avoid a warning if long has 32 bits.  */
    charmask |= (charmask << 16) << 16;
  if (sizeof (longword) > 8)
    abort ();

  /* Instead of the traditional loop which tests each character,
     we will test a longword at a time.  The tricky part is testing
     if *any of the four* bytes in the longword in question are zero.  */
  for (;;)
    {
      /* We tentatively exit the loop if adding MAGIC_BITS to
	 LONGWORD fails to change any of the hole bits of LONGWORD.

	 1) Is this safe?  Will it catch all the zero bytes?
	 Suppose there is a byte with all zeros.  Any carry bits
	 propagating from its left will fall into the hole at its
	 least significant bit and stop.  Since there will be no
	 carry from its most significant bit, the LSB of the
	 byte to the left will be unchanged, and the zero will be
	 detected.

	 2) Is this worthwhile?  Will it ignore everything except
	 zero bytes?  Suppose every byte of LONGWORD has a bit set
	 somewhere.  There will be a carry into bit 8.  If bit 8
	 is set, this will carry into bit 16.  If bit 8 is clear,
	 one of bits 9-15 must be set, so there will be a carry
	 into bit 16.  Similarly, there will be a carry into bit
	 24.  If one of bits 24-30 is set, there will be a carry
	 into bit 31, so all of the hole bits will be changed.

	 The one misfire occurs when bits 24-30 are clear and bit
	 31 is set; in this case, the hole at bit 31 is not
	 changed.  If we had access to the processor carry flag,
	 we could close this loophole by putting the fourth hole
	 at bit 32!

	 So it ignores everything except 128's, when they're aligned
	 properly.

	 3) But wait!  Aren't we looking for C as well as zero?
	 Good point.  So what we do is XOR LONGWORD with a longword,
	 each of whose bytes is C.  This turns each byte that is C
	 into a zero.  */

      longword = *longword_ptr++;

      /* Add MAGIC_BITS to LONGWORD.  */
      if ((((longword + magic_bits)

	    /* Set those bits that were unchanged by the addition.  */
	    ^ ~longword)

	   /* Look at only the hole bits.  If any of the hole bits
	      are unchanged, most likely one of the bytes was a
	      zero.  */
	   & ~magic_bits) != 0 ||

	  /* That caught zeroes.  Now test for C.  */
	  ((((longword ^ charmask) + magic_bits) ^ ~(longword ^ charmask))
	   & ~magic_bits) != 0)
	{
	  /* Which of the bytes was C or zero?
	     If none of them were, it was a misfire; continue the search.  */

	  const unsigned char *cp = (const unsigned char *) (longword_ptr - 1);

	  if (*cp == c)
	    return (char *) cp;
	  else if (*cp == '\0')
	    return NULL;
	  if (*++cp == c)
	    return (char *) cp;
	  else if (*cp == '\0')
	    return NULL;
	  if (*++cp == c)
	    return (char *) cp;
	  else if (*cp == '\0')
	    return NULL;
	  if (*++cp == c)
	    return (char *) cp;
	  else if (*cp == '\0')
	    return NULL;
	  if (sizeof (longword) > 4)
	    {
	      if (*++cp == c)
		return (char *) cp;
	      else if (*cp == '\0')
		return NULL;
	      if (*++cp == c)
		return (char *) cp;
	      else if (*cp == '\0')
		return NULL;
	      if (*++cp == c)
		return (char *) cp;
	      else if (*cp == '\0')
		return NULL;
	      if (*++cp == c)
		return (char *) cp;
	      else if (*cp == '\0')
		return NULL;
	    }
	}
    }

  return NULL;
  TM_END
}


char *
txc_libc_strrchr (const char *s, int c)
{
  register const char *found, *p;

  c = (unsigned char) c;

  /* Since strchr is fast, we use it rather than the obvious loop.  */

  TM_BEGIN
  if (c == '\0')
    return strchr (s, '\0');

  found = NULL;
  while ((p = strchr (s, c)) != NULL)
    {
      found = p;
      s = p + 1;
    }

  return (char *) found;
  TM_END
}


int 
txc_libc_strncasecmp (const char *s1, const char *s2,  size_t n)
{
  if (n == 0)
    return 0;

  TM_BEGIN
  while (n-- != 0 && tolower(*s1) == tolower(*s2))
    {
      if (n == 0 || *s1 == '\0' || *s2 == '\0')
  break;
      s1++;
      s2++;
    }

  return tolower(*(unsigned char *) s1) - tolower(*(unsigned char *) s2);
  TM_END
}
