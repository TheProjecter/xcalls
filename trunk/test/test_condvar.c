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

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>

#include <txc/txc.h>
#include "util/buf-m-2.h"


txc_cond_event_t done_cond;

#define NUM_READ 1
#define NUM_WRITE 1

int num_read = NUM_READ;
int num_write = NUM_WRITE;
int iter = 100;
int sleep_frac = 2;
int read_sleep = 1000;
int write_sleep = 1000;


void *reader(void *ev)
{
	int     tid = (int) ev;
	__int64 tsc;

	while (1) { // !done) {
		__int64 value;
		value = GET();
		tsc = _rdtsc();
		printf("%d: Got delta %f, size=%d\n",tid, ((double) (tsc - value)) / (double) 3000000000.0,GETSIZE());
		usleep(read_sleep);
	}
	printf("Thread exiting\n");
	return(NULL);
}


void *writer(void *ev)
{
	int tid = (int) ev;
	int i;
	int j;

	sleep (1);
	for (i = 0; i < iter; i++) {
		__int64 val;
		if ((rand() % sleep_frac) == 0) {
			usleep(write_sleep);
		}

		val = _rdtsc(); // rand();
		printf("%d:%d Put size=%d\n",tid, i, GETSIZE());

		PUT(val);
	}

	COND_XACT_BEGIN;
		num_write --;
		if (num_write  == 0) {
			txc_cond_event_tm_signal(&done_cond);
		}
	COND_XACT_END;

	return(NULL);
}


int main(int argc, char * argv[])
{
	pthread_t thd1 = 0;
	int tid1;
	pthread_t thd2 = 0;
	int tid2;
	int i;

	txc_cond_event_init(&done_cond);
	if (argc >= 3) {
		num_read = strtol(argv[1], NULL, 10);
		num_write = strtol(argv[2], NULL, 10);
	}
	if (argc >= 4) {
		iter = strtol(argv[3], NULL, 10);
	}
	if (argc >= 5) {
		sleep_frac = strtol(argv[4], NULL, 10);
	}
	if (argc >= 6) {
		read_sleep = strtol(argv[5], NULL, 10);
	}
	if (argc >= 7) {
		write_sleep = strtol(argv[6], NULL, 10);
	}

	printf("Readers = %d, writers = %d, iterations = %d, sleep_frac = %d, read_sleep = %d, write_sleep = %d\n",
	       num_read,num_write, iter, sleep_frac, read_sleep, write_sleep);
	for (i = 0; i < num_read; i++) {
		tid1 = pthread_create(&thd1, NULL, reader, (void *) i);
	}
	for (i = 0; i < num_write; i++) {
		tid2 = pthread_create(&thd2, NULL, writer, (void *) i);
	}

	sleep(1);

	COND_XACT_BEGIN;
	{
		while (num_write != 0) {
			txc_cond_wait(&done_cond);
		}
	} 
	COND_XACT_END;

	sleep(7);
	return(0);
}
