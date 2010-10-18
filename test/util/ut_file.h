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

#ifndef _UT_FILE_H
#define _UT_FILE_H

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include "ut_types.h"   1.

void 
ut_mkdir_r(const char *dir, mode_t mode) 
{
	char   tmp[256];
	char   *p = NULL;
	size_t len;

	snprintf(tmp, sizeof(tmp), "%s", dir);
	len = strlen(tmp);
	if(tmp[len - 1] == '/') {
		tmp[len - 1] = 0;
		for(p = tmp + 1; *p; p++)
			if(*p == '/') {
				*p = 0;
				mkdir(tmp, mode);
				*p = '/';
		}
	}	
	mkdir(tmp, mode);
}

int
create_file(const char *pathname, char *str)
{
	int  fd;
	int  i;

	fd = creat(pathname, S_IRUSR|S_IWUSR);
	if (fd < 0) {
		return fd;
	}	

	write(fd, str, strlen(str));
	close(fd);
	return 0;
}

ut_bool_t
exists_file(const char *pathname)
{
	struct stat stat_buf;
	int         ret;

	ret = stat(pathname, &stat_buf);
	if (ret == 0) {
		return UT_TRUE;
	} else {
		return UT_FALSE;
	}	
}

int
remove_file(const char *pathname)
{
	return unlink(pathname);
}


ut_bool_t 
file_equal_str(const char *pathname, char *str)
{
	FILE *fstream;
	int  n = 0;
	int  i = 0;
	int  c;

	if ((fstream = fopen(pathname, "r")) == NULL) {
		return UT_FALSE;
	}	

	while(1) {
		c = fgetc(fstream);
		if (c == EOF) {
			fclose(fstream);
			if (str[i] != '\0') {
				return UT_FALSE;
			} else {
				return UT_TRUE;
			}	
		}
		if (str[i] != '\0') {
			if (str[i++] != c) {
				return UT_FALSE;
			}
		}	
	}
}

int
compare_file(const char *pathname, char *str)
{
	return (file_equal_str(pathname, str) == UT_TRUE ? 0: -1);
}

void 
print_file(const char *pathname)
{
	FILE *fstream;
	int  n = 0;
	int  i = 0;
	int  c;

	if ((fstream = fopen(pathname, "r")) == NULL) {
		return;
	}	

	while(1) {
		c = fgetc(fstream);
		if (c == EOF) {
			break;
		}
		printf("%c", c);
	}
	fclose(fstream);
}


int
create_file2(const char *pathname, char *str_table[])
{
	int  fd;
	int  i;
	char *str;

	fd = creat(pathname, S_IRUSR|S_IWUSR);
	if (fd < 0) {
		return fd;
	}	

	for (i=0; str_table[i] != NULL; i++) {
		str = str_table[i]; 
		write(fd, str, strlen(str));
	}
	close(fd);
	return 0;
}


int 
compare_file2(const char *pathname, char *str_table[])
{
	FILE *fstream;
	int  n = 0;
	int  i = 0;
	int  c;
	char *str;
	char str_table_char;

	if ((fstream = fopen(pathname, "r")) == NULL) {
		return -1;
	}	

	while(1) {
		c = fgetc(fstream);
		if (c == EOF) {
			break;
		}
		str_table_char = str_table[n][i++];
		if (str_table[n][i] == '\0') {
			n++;
			i = 0;
		}
		if (str_table_char != c) {
			return -1;
		}
	}
	fclose(fstream);
	return 0;
}

char ** 
str_table_create(char *src[])
{
	int  i;
	int  n;
	int  len;
	char **str_table_new;

	for (n=0; src[n] != NULL; n++);
	str_table_new = (char **) malloc(sizeof(char *) * (n+1));
	for (i=0; src[i] != NULL; i++) {
		len = strlen(src[i]);
		str_table_new[i] = (char *) malloc(sizeof(char) * (len+1));
		if (src[i] != NULL) {
			strcpy(str_table_new[i], src[i]);
		} else {
			str_table_new[i] = NULL;
		}
	}

	return str_table_new;
}



void
str_table_destroy(char *src[])
{
	int  i;

	for (i=0; src[i] != NULL; i++) {
		free(src[i]);
	}
	free(src);
}


void
str_table_print(char *src[])
{
	int  i;

	for (i=0; src[i] != NULL; i++) {
		printf("%s", src[i]);
	}
}


char * 
str_create(char *src)
{
	char *newstr;

	newstr = (char *) malloc(strlen(src)+1);
	strcpy(newstr, src);
	return newstr;
}


char * 
str_create2(char *src[])
{
	int  l;
	int  n;
	int  i;
	int  len;
	char *newstr;

	for (l=0, n=0; src[n] != NULL; n++) {
		l+=strlen(src[n]);
	}
	newstr = (char *) malloc(sizeof(char) * l);

	for (l=0, n=0; src[n] != NULL; n++) {
		for (i=0; src[n][i] != '\0'; i++, l++) {
			newstr[l] = src[n][i];
		}	
	}
	newstr[l] = '\0';

	return newstr;
}

void
str_destroy(char *src)
{
	free(src);
}


void
str_modify(char **strp, int n, char *mdfstr)
{
	char *oldstr = *strp; 
	char *newstr;
	int  oldlen;
	int  mdflen;
	int  i;

	oldlen = strlen(oldstr);
	mdflen = strlen(mdfstr);
	if (n + mdflen > oldlen) {
		newstr = (char *) malloc(sizeof(char) * (n+mdflen+1));
		strcpy(newstr, oldstr);
		for (i=oldlen; i<n; i++) {
			newstr[i] = ' ';
		}
		newstr[n+mdflen] ='\0';
		free(oldstr);
	} else {
		newstr = oldstr;
	}
	for (i=0; i<mdflen; i++) {
		newstr[i+n] = mdfstr[i];
	}
	*strp = newstr;
}

#endif
