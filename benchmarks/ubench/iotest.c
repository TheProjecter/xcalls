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
#include <stdint.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <getopt.h>
#include <fcntl.h>
#include <util/ut_barrier.h>
#include <util/ut_file.h>
#include <txc/txc.h>

//#define __DEBUG_BUILD

static const char __whitespaces[] = "                                                              ";
#define WHITESPACE(len) &__whitespaces[sizeof(__whitespaces) - (len) -1]

#define MAX_NUM_THREADS 16
#define OPS_PER_CHUNK   4
#define BLOCK_SIZE      16*1024

typedef enum {
	SYSTEM_UNKNOWN = -1,
	SYSTEM_NATIVE = 0,
	SYSTEM_STM,
	SYSTEM_XCALLS,
	num_of_systems
} system_t;	

typedef enum {
	UBENCH_UNKNOWN = -1,
	UBENCH_OPEN = 0,
	UBENCH_READ,
	UBENCH_WRITE,
	num_of_benchs
} ubench_t;

char                  *progname = "iotest";
char                  *base_pathname = "/tmp/libtxc.tmp.iotest/";
int                   num_threads;
ubench_t              ubench_to_run;
system_t              system_to_use;
unsigned long long    duration;
struct timeval        global_begin_time;
ut_barrier_t          start_timer_barrier;
ut_barrier_t          start_ubench_barrier;
volatile unsigned int short_circuit_terminate;
unsigned long long    thread_total_ops[MAX_NUM_THREADS];
unsigned long long    thread_actual_duration[MAX_NUM_THREADS];

typedef struct {
	unsigned int tid;
	unsigned int chunks;
} ubench_args_t;

struct {
	char     *str;
	system_t val;
} systems[] = { 
	{ "native", SYSTEM_NATIVE},
	{ "stm", SYSTEM_STM},
	{ "xcalls", SYSTEM_XCALLS}
};

struct {
	char     *str;
	ubench_t val;
} ubenchs[] = { 
	{ "open", UBENCH_OPEN},
	{ "read", UBENCH_READ},
	{ "write", UBENCH_WRITE},
};

static void run(void* arg);
void ubench_native_open(void *);
void ubench_stm_open(void *);
void ubench_xcalls_open(void *);
void ubench_native_write(void *);
void ubench_stm_write(void *);
void ubench_xcalls_write(void *);
void ubench_native_read(void *);
void ubench_stm_read(void *);
void ubench_xcalls_read(void *);
void prepare_ubench_open(void *arg);
void prepare_ubench_write(void *arg);
void prepare_ubench_read(void *arg);

void (*ubenchf_array[3][3])(void *) = {
	{ ubench_native_open, ubench_stm_open, ubench_xcalls_open},
	{ ubench_native_read, ubench_stm_read, ubench_xcalls_read},
	{ ubench_native_write, ubench_stm_write, ubench_xcalls_write},
};	

void (*ubenchf_array_prepare[3][3])(void *) = {
	{ prepare_ubench_open, prepare_ubench_open, prepare_ubench_open},
	{ prepare_ubench_read, prepare_ubench_read, prepare_ubench_read},
	{ prepare_ubench_write, prepare_ubench_write, prepare_ubench_write},
};	




static
void usage(char *name) 
{
	printf("Usage: %s   %s\n", name                    , "--ubench=MICROBENCHMARK_TO_RUN");
	printf("       %s   %s\n", WHITESPACE(strlen(name)), "--system=SYSTEM_TO_USE");
	printf("       %s   %s\n", WHITESPACE(strlen(name)), "--duration=DURATION_OF_EXPERIMENT_IN_SECONDS");
	printf("       %s   %s\n", WHITESPACE(strlen(name)), "--numthreads=NUMBER_OF_THREADS");
	printf("\nValid arguments:\n");
	printf("  --ubench     [open|read|write]\n");
	printf("  --system     [native|stm|xcalls]\n");
	printf("  --numthreads [1-%d]\n", MAX_NUM_THREADS);
	exit(1);
}


int
main(int argc, char *argv[])
{
	extern char        *optarg;
	pthread_t          threads[MAX_NUM_THREADS];
	int                c;
	int                i;
	int                j;
	char               pathname[512];
	unsigned long long total_ops;
	unsigned long long avg_duration;
	double             throughput;

	/* Default values */
	system_to_use = SYSTEM_NATIVE;
	ubench_to_run = UBENCH_OPEN;
	duration = 30 * 1000 * 1000;

	while (1) {
		static struct option long_options[] = {
			{"ubench",  required_argument, 0, 'b'},
			{"duration",  required_argument, 0, 'd'},
			{"system",  required_argument, 0, 's'},
			{"numthreads", required_argument, 0, 'n'},
			{0, 0, 0, 0}
		};
		int option_index = 0;
     
		c = getopt_long (argc, argv, "b:d:s:t:",
		                 long_options, &option_index);
     
		/* Detect the end of the options. */
		if (c == -1)
			break;
     
		switch (c) {
			case 's':
				system_to_use = SYSTEM_UNKNOWN;
				for (i=0; i<num_of_systems; i++) {
					if (strcmp(systems[i].str, optarg) == 0) {
						system_to_use = (system_t) i;
						break;
					}
				}
				if (system_to_use == SYSTEM_UNKNOWN) {
					usage(progname);
				}
				break;

			case 'b':
				ubench_to_run = UBENCH_UNKNOWN;
				for (i=0; i<num_of_benchs; i++) {
					if (strcmp(ubenchs[i].str, optarg) == 0) {
						ubench_to_run = (ubench_t) i;
						break;
					}
				}
				if (ubench_to_run == UBENCH_UNKNOWN) {
					usage(progname);
				}
				break;

			case 'n':
				num_threads = atoi(optarg);
				break;

			case 'd':
				duration = atoi(optarg) * 1000 * 1000; 
				break;

			case '?':
				/* getopt_long already printed an error message. */
				usage(progname);
				break;
     
			default:
				abort ();
		}
	}

	ut_mkdir_r(base_pathname, 0777);
	for (i=0; i<num_threads; i++) {
		sprintf(pathname, "%s/dir-%d", base_pathname, i);
		ut_mkdir_r(pathname, 0777);
		for (j=0; j<OPS_PER_CHUNK; j++) {
			sprintf(pathname, "%s/dir-%d/file-%d", base_pathname, i, j);
			create_file(pathname, "");
		}	
	}	

	ut_barrier_init(&start_timer_barrier, num_threads+1);
	ut_barrier_init(&start_ubench_barrier, num_threads+1);
	short_circuit_terminate = 0;

	if (system_to_use == SYSTEM_XCALLS) {
		_TXC_global_init();
	}

	for (i=0; i<num_threads; i++) {
		pthread_create(&threads[i], NULL, (void *(*)(void *)) run, (void *) i);
	}

	ut_barrier_wait(&start_timer_barrier);
	gettimeofday(&global_begin_time, NULL);
	ut_barrier_wait(&start_ubench_barrier);

	total_ops = 0;
	avg_duration = 0;
	for (i=0; i<num_threads; i++) {
		pthread_join(threads[i], NULL);
		total_ops += thread_total_ops[i];
		avg_duration += thread_actual_duration[i];
	}
	avg_duration = avg_duration/num_threads;
	throughput = ((double) total_ops) / ((double) avg_duration);

	printf("total operations: %llu\n", total_ops);
	printf("avg duration    : %llu ms\n", avg_duration/1000);
	printf("throughput      : %f (ops/s) \n", throughput * 1000 * 1000);

	return 0;
}


static
void run(void* arg)
{
 	unsigned int       tid = (unsigned int) arg;
	ubench_args_t      args;
	void               (*ubenchf)(void *);
	void               (*ubenchf_prepare)(void *);
	struct timeval     current_time;
	unsigned long long experiment_time_duration;
	unsigned long long n;

	args.tid = tid;
	args.chunks = 1024;

	if (system_to_use == SYSTEM_XCALLS) {
		_TXC_thread_init();
	}	

	ubenchf = ubenchf_array[ubench_to_run][system_to_use];

	ubenchf_prepare = ubenchf_array_prepare[ubench_to_run][system_to_use];

	if (ubenchf_prepare) {
		ubenchf_prepare(&args);
	}
	ut_barrier_wait(&start_timer_barrier);
	ut_barrier_wait(&start_ubench_barrier);

	n = 0;
	do {
		ubenchf(&args);
		n++;
		gettimeofday(&current_time, NULL);
		experiment_time_duration = 1000000 * (current_time.tv_sec - global_begin_time.tv_sec) +
		                           current_time.tv_usec - global_begin_time.tv_usec;
	} while (experiment_time_duration < duration && short_circuit_terminate == 0);
	
	short_circuit_terminate = 1;
	thread_actual_duration[tid] = experiment_time_duration;
	thread_total_ops[tid] = n * args.chunks;
}


/* 
 * NATIVE OPEN 
 */

struct {
	char files[OPS_PER_CHUNK][128];
} prepared_state_ubench_open[MAX_NUM_THREADS];

void prepare_ubench_open(void *arg)
{
	int          i;
	int          j;


	for (i=0; i<num_threads; i++) {
		for (j=0; j<OPS_PER_CHUNK; j++) {
			sprintf(prepared_state_ubench_open[i].files[j], "%s/dir-%d/file-%d", base_pathname, i, j);
		}	
	}
}

void ubench_native_open(void *arg)
{
 	unsigned int   tid = ((ubench_args_t *) arg)->tid;
 	unsigned int   chunks = ((ubench_args_t *) arg)->chunks;
	int            i;
	int            fds[OPS_PER_CHUNK];
	
	for (i=0; i<chunks; i++) {
		fds[0] = open(prepared_state_ubench_open[tid].files[0], O_RDWR);
		fds[1] = open(prepared_state_ubench_open[tid].files[1], O_RDWR);
		fds[2] = open(prepared_state_ubench_open[tid].files[2], O_RDWR);
		fds[3] = open(prepared_state_ubench_open[tid].files[3], O_RDWR);
		close(fds[0]);
		close(fds[1]);
		close(fds[2]);
		close(fds[3]);
	}	
}

/* 
 * STM OPEN 
 */

void ubench_stm_open(void *arg)
{
 	unsigned int   tid = ((ubench_args_t *) arg)->tid;
 	unsigned int   chunks = ((ubench_args_t *) arg)->chunks;
	int            i;
	int            fds[OPS_PER_CHUNK];

	for (i=0; i<chunks; i++) {
		__tm_atomic {
			fds[0] = open(prepared_state_ubench_open[tid].files[0], O_RDWR);
			fds[1] = open(prepared_state_ubench_open[tid].files[1], O_RDWR);
			fds[2] = open(prepared_state_ubench_open[tid].files[2], O_RDWR);
			fds[3] = open(prepared_state_ubench_open[tid].files[3], O_RDWR);
#ifdef __DEBUG_BUILD			
			__tm_waiver {
				int j;
				for (j=0; j<4; j++) {
					assert(fds[j] != -1);
				}	
			}
#endif			
			close(fds[0]);
			close(fds[1]);
			close(fds[2]);
			close(fds[3]);
		}	
	}	
}

/* 
 * XCALLS OPEN 
 */

void ubench_xcalls_open(void *arg)
{
 	unsigned int   tid = ((ubench_args_t *) arg)->tid;
 	unsigned int   chunks = ((ubench_args_t *) arg)->chunks;
	int            i;
	int            fds[OPS_PER_CHUNK];

	for (i=0; i<chunks; i++) {
		XACT_BEGIN(xact)
			fds[0] = _XCALL(x_open)(prepared_state_ubench_open[tid].files[0], O_RDWR, S_IRUSR|S_IWUSR, NULL);
			fds[1] = _XCALL(x_open)(prepared_state_ubench_open[tid].files[1], O_RDWR, S_IRUSR|S_IWUSR, NULL);
			fds[2] = _XCALL(x_open)(prepared_state_ubench_open[tid].files[2], O_RDWR, S_IRUSR|S_IWUSR, NULL);
			fds[3] = _XCALL(x_open)(prepared_state_ubench_open[tid].files[3], O_RDWR, S_IRUSR|S_IWUSR, NULL);
#ifdef __DEBUG_BUILD			
			__tm_waiver {
				int j;
				for (j=0; j<4; j++) {
					assert(fds[j] != -1);
				}	
			}
#endif			
			_XCALL(x_close)(fds[0], NULL);
			_XCALL(x_close)(fds[1], NULL);
			_XCALL(x_close)(fds[2], NULL);
			_XCALL(x_close)(fds[3], NULL);
		XACT_END(xact)	
	}	
}


/* 
 * NATIVE WRITE 
 */

struct {
	int fds[OPS_PER_CHUNK];
} prepared_state_ubench_write[MAX_NUM_THREADS];

void prepare_ubench_write(void *arg)
{
 	unsigned int tid = ((ubench_args_t *) arg)->tid;
	int          i;
	int          j;
	char         pathname[128];

	for (j=0; j<OPS_PER_CHUNK; j++) {
		sprintf(pathname, "%s/dir-%d/file-%d", base_pathname, tid, j);
		if (system_to_use == SYSTEM_XCALLS) {
			prepared_state_ubench_write[tid].fds[j] = _XCALL(x_open)(pathname, O_RDWR, S_IRUSR|S_IWUSR, NULL);
		} else {
			prepared_state_ubench_write[tid].fds[j] = open(pathname, O_RDWR);
		}	
	}
}

void ubench_native_write(void *arg)
{
 	unsigned int   tid = ((ubench_args_t *) arg)->tid;
 	unsigned int   chunks = ((ubench_args_t *) arg)->chunks;
	int            i;
	int            fds[OPS_PER_CHUNK];
	char           buf[BLOCK_SIZE];

	for (i=0; i<chunks; i++) {
		write(prepared_state_ubench_write[tid].fds[0], buf, BLOCK_SIZE);
		write(prepared_state_ubench_write[tid].fds[1], buf, BLOCK_SIZE);
		write(prepared_state_ubench_write[tid].fds[2], buf, BLOCK_SIZE);
		write(prepared_state_ubench_write[tid].fds[3], buf, BLOCK_SIZE);
	}

	for (i=0; i<OPS_PER_CHUNK; i++) {
		lseek(prepared_state_ubench_write[tid].fds[i], 0, SEEK_SET);
	}	
}

/* 
 * STM WRITE 
 */

void ubench_stm_write(void *arg)
{
 	unsigned int   tid = ((ubench_args_t *) arg)->tid;
 	unsigned int   chunks = ((ubench_args_t *) arg)->chunks;
	int            i;
	int            fds[OPS_PER_CHUNK];
	char           buf[BLOCK_SIZE];

	for (i=0; i<chunks; i++) {
		__tm_atomic {
			write(prepared_state_ubench_write[tid].fds[0], buf, BLOCK_SIZE);
			write(prepared_state_ubench_write[tid].fds[1], buf, BLOCK_SIZE);
			write(prepared_state_ubench_write[tid].fds[2], buf, BLOCK_SIZE);
			write(prepared_state_ubench_write[tid].fds[3], buf, BLOCK_SIZE);
		}
	}

	for (i=0; i<OPS_PER_CHUNK; i++) {
		lseek(prepared_state_ubench_write[tid].fds[i], 0, SEEK_SET);
	}	
}


/* 
 * XCALLS WRITE 
 */

void ubench_xcalls_write(void *arg)
{
 	unsigned int   tid = ((ubench_args_t *) arg)->tid;
 	unsigned int   chunks = ((ubench_args_t *) arg)->chunks;
	int            i;
	int            fds[OPS_PER_CHUNK];
	char           buf[BLOCK_SIZE];

	for (i=0; i<chunks; i++) {
		__tm_atomic {
			_XCALL(x_write_ovr)(prepared_state_ubench_write[tid].fds[0], buf, BLOCK_SIZE, NULL);
			_XCALL(x_write_ovr)(prepared_state_ubench_write[tid].fds[1], buf, BLOCK_SIZE, NULL);
			_XCALL(x_write_ovr)(prepared_state_ubench_write[tid].fds[2], buf, BLOCK_SIZE, NULL);
			_XCALL(x_write_ovr)(prepared_state_ubench_write[tid].fds[3], buf, BLOCK_SIZE, NULL);
		}	
	}

	for (i=0; i<OPS_PER_CHUNK; i++) {
		lseek(prepared_state_ubench_write[tid].fds[i], 0, SEEK_SET);
	}	
}


/* 
 * NATIVE READ
 */

struct {
	int fds[OPS_PER_CHUNK];
} prepared_state_ubench_read[MAX_NUM_THREADS];

void prepare_ubench_read(void *arg)
{
 	unsigned int tid = ((ubench_args_t *) arg)->tid;
 	unsigned int chunks = ((ubench_args_t *) arg)->chunks;
	int          i;
	int          j;
	char         pathname[128];
	char         buf[BLOCK_SIZE];

	for (j=0; j<OPS_PER_CHUNK; j++) {
		sprintf(pathname, "%s/dir-%d/file-%d", base_pathname, tid, j);
		if (system_to_use == SYSTEM_XCALLS) {
			prepared_state_ubench_read[tid].fds[j] = _XCALL(x_open)(pathname, O_RDWR, S_IRUSR|S_IWUSR, NULL);
		} else {
			prepared_state_ubench_read[tid].fds[j] = open(pathname, O_RDWR);
		}	
		for (i=0; i<chunks+1; i++) {
			write(prepared_state_ubench_read[tid].fds[j], buf, BLOCK_SIZE);
		}	
		lseek(prepared_state_ubench_write[tid].fds[j], 0, SEEK_SET);
	}
}

void ubench_native_read(void *arg)
{
 	unsigned int   tid = ((ubench_args_t *) arg)->tid;
 	unsigned int   chunks = ((ubench_args_t *) arg)->chunks;
	int            i;
	int            fds[OPS_PER_CHUNK];
	char           buf[BLOCK_SIZE];

	for (i=0; i<chunks; i++) {
		read(prepared_state_ubench_read[tid].fds[0], buf, BLOCK_SIZE);
		read(prepared_state_ubench_read[tid].fds[1], buf, BLOCK_SIZE);
		read(prepared_state_ubench_read[tid].fds[2], buf, BLOCK_SIZE);
		read(prepared_state_ubench_read[tid].fds[3], buf, BLOCK_SIZE);
	}

	for (i=0; i<OPS_PER_CHUNK; i++) {
		lseek(prepared_state_ubench_write[tid].fds[i], 0, SEEK_SET);
	}	
}

/* 
 * STM READ
 */

void ubench_stm_read(void *arg)
{
 	unsigned int   tid = ((ubench_args_t *) arg)->tid;
 	unsigned int   chunks = ((ubench_args_t *) arg)->chunks;
	int            i;
	int            fds[OPS_PER_CHUNK];
	char           buf[BLOCK_SIZE];

	__tm_atomic {
		for (i=0; i<chunks; i++) {
			read(prepared_state_ubench_read[tid].fds[0], buf, BLOCK_SIZE);
			read(prepared_state_ubench_read[tid].fds[1], buf, BLOCK_SIZE);
			read(prepared_state_ubench_read[tid].fds[2], buf, BLOCK_SIZE);
			read(prepared_state_ubench_read[tid].fds[3], buf, BLOCK_SIZE);
		}
	}	

	for (i=0; i<OPS_PER_CHUNK; i++) {
		lseek(prepared_state_ubench_write[tid].fds[i], 0, SEEK_SET);
	}	
}

/* 
 * XCALLS READ
 */

void ubench_xcalls_read(void *arg)
{
 	unsigned int   tid = ((ubench_args_t *) arg)->tid;
 	unsigned int   chunks = ((ubench_args_t *) arg)->chunks;
	int            i;
	int            fds[OPS_PER_CHUNK];
	char           buf[BLOCK_SIZE];

	__tm_atomic {
		for (i=0; i<chunks; i++) {
			_XCALL(x_read)(prepared_state_ubench_read[tid].fds[0], buf, BLOCK_SIZE, NULL);
			_XCALL(x_read)(prepared_state_ubench_read[tid].fds[1], buf, BLOCK_SIZE, NULL);
			_XCALL(x_read)(prepared_state_ubench_read[tid].fds[2], buf, BLOCK_SIZE, NULL);
			_XCALL(x_read)(prepared_state_ubench_read[tid].fds[3], buf, BLOCK_SIZE, NULL);
		}
	}	

	for (i=0; i<OPS_PER_CHUNK; i++) {
		lseek(prepared_state_ubench_write[tid].fds[i], 0, SEEK_SET);
	}	
}



