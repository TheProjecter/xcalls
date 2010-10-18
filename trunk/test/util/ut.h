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

#ifndef _TEST_UTIL_H
#define _TEST_UTIL_H

#define UT_PRINT_OUT stderr
#define UT_TEST_SUCCESS 0
#define UT_TEST_FAILURE 1

#include <core/sentinel.h>
#include <core/koa.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <pthread.h>
#include <unistd.h>
#include <assert.h>
#include <stdio.h>
#include <fcntl.h>
#include <math.h>
#include <stdarg.h>
#include "ut_barrier.h"
#include "ut_types.h"

typedef struct ut_suite_s ut_suite_t;
typedef struct ut_test_s ut_test_t;
typedef struct ut_msg_s ut_msg_t;
typedef struct ut_msg_header_s ut_msg_header_t;
typedef enum {MSG_FAILURE} msg_type_t;

struct ut_msg_header_s {
	msg_type_t type;
	ssize_t body_len;
};

struct ut_msg_s {
	ut_msg_t* next; 
	ut_msg_header_t header;
	char* body;
};

struct ut_test_s {
	ut_test_t* next;
	char* name;
	int (*f)();
	int status;
	ut_msg_t* msg_list_head;
	ut_msg_t* msg_list_tail;
};

struct ut_suite_s {
	ut_test_t* head;
	ut_test_t* tail;
	char* name;
	int num_tests;
	int num_tests_failed;
};


static const char padder_dots[] = "........................................................................................................";
static const char padder_spaces[] = "                                     ";

int verbose = 1;
int pfd[2];

#define SPACES(len)                                                           \
    padder_spaces + sizeof(padder_spaces) - len - 1

#define UT_FAILURE(msg, ...)                                                  \
do {                                                                          \
    char __buf[1024];                                                         \
    ssize_t __len;                                                            \
    ut_msg_header_t *__msg_header = (ut_msg_header_t *) __buf;                \
    __len = ut_sprintf(__buf + sizeof(ut_msg_header_t),                       \
                       __FILE__,                                              \
                       __LINE__,                                              \
                       "",                                                    \
                       msg,                                                   \
                       ##__VA_ARGS__);                                        \
	__msg_header->type = MSG_FAILURE;                                         \
	__msg_header->body_len = __len;                                           \
	write(pfd[1], __buf, sizeof(ut_msg_header_t)+__len);  /* atomic write */  \
} while (0);	

#define UT_ASSERT(expr)                                                       \
do {                                                                          \
	if (!expr) {                                                              \
		__ret__ = __ret__ || UT_TEST_FAILURE;                                 \
		UT_FAILURE("UT_ASSERT(%s)", #expr);                                   \
	}                                                                         \
} while (0);


#define UT_ASSERT_EQUAL(val1, val2)                                           \
do {                                                                          \
    if ((val1) != (val2))                                                     \
	{                                                                         \
	    __ret__ = __ret__ || UT_TEST_FAILURE;                                 \
		UT_FAILURE("UT_ASSERT_EQUAL(%s, %s)",                                 \
		           #val1,                                                     \
                   #val2);                                                    \
	}                                                                         \
} while (0);


#define UT_ASSERT_NOTEQUAL(val1, val2)                                        \
do {                                                                          \
    if ((val1) == (val2))                                                     \
	{                                                                         \
	    __ret__ = __ret__ || UT_TEST_FAILURE;                                 \
		UT_FAILURE("UT_ASSERT_NOTEQUAL(%s, %s)", #val1, #val2);               \
	}                                                                         \
} while (0);


#define UT_ASSERT_STR_EQUAL(val1, val2)                                       \
do {                                                                          \
    if (0 != strcmp(val1, val2))                                              \
	{                                                                         \
	    __ret__ = __ret__ || UT_TEST_FAILURE;                                 \
		UT_FAILURE("UT_ASSERT_STR_EQUAL(%s, %s)",                             \
		           #val1,                                                     \
                   #val2);                                                    \
	}                                                                         \
} while (0);


#define UT_ASSERT_STR_NOTEQUAL(val1, val2)                                    \
do {                                                                          \
    if (0 == strcmp(val1, val2))                                              \
	{                                                                         \
	    __ret__ = __ret__ || UT_TEST_FAILURE;                                 \
		UT_FAILURE("UT_ASSERT_STR_NOTEQUAL(%s, %s)",                          \
		           #val1,                                                     \
                   #val2);                                                    \
	}                                                                         \
} while (0);


#define UT_START_TEST_THREAD(fname)                                           \
    void *fname(void *__arg)                                                  \
    {                                                                         \
        volatile int __ret__ = UT_TEST_SUCCESS;


#define UT_END_TEST_THREAD                                                    \
        return (void *) __ret__;                                              \
    }


#define UT_START_TEST(fname)                                                  \
    int fname()                                                               \
    {                                                                         \
        volatile int __ret__ = UT_TEST_SUCCESS;                               \
        void         *__thread_join_val__;


#define UT_END_TEST                                                           \
         return __ret__;                                                      \
    }

#define UT_RETURN return __ret__;

#define UT_STATUS2STR(status)                                                 \
    (status == UT_TEST_SUCCESS) ? "passed" : "FAILED"                         \ 

#define UT_THREAD_T pthread_t
#define UT_THREAD_CREATE pthread_create

#define UT_THREAD_JOIN(__thread__)                                            \
    pthread_join(__thread__, &__thread_join_val__);                           \
    __ret__ = __ret__ || ((int) __thread_join_val__);


/* Sentinel Helper functions */

txc_sentinel_t *
fd2sentinel(int fd)
{
	txc_koa_t *koa;
	
    if (TXC_R_SUCCESS == txc_koa_lookup_fd2koa(txc_g_koamgr, fd, &koa)) {
    	return txc_koa_get_sentinel(koa);
	}
	return NULL;
}

#define SENTINEL_OWNER(__sentinel__)                                          \
    txc_sentinel_owner(__sentinel__)

#define FD2SENTINEL(fd)                                                       \
    fd2sentinel(fd)

#define SENTINEL_ENLISTED(__txd__, __sentinel__)                              \
    txc_sentinel_is_enlisted(__txd__, __sentinel__)



static ssize_t
ut_sprintf(char *str, 
           char *file, 
           int line, 
           const char *prefix,
           const char *strformat, ...) 
{
	va_list ap;
	int len;
	ssize_t wlen;
	if (prefix) {
		len = sprintf(str, "%s", prefix); 
	}
	if (file) {
		len += sprintf(&str[len], "%s(%d)", file, line); 
	}
	if (file || prefix) {
		len += sprintf(&str[len], ": "); 
	}
	va_start (ap, strformat);
	wlen = len + vsnprintf(&str[len], sizeof (str) - 1 - len, strformat, ap);
	va_end (ap);
	return wlen;
}


static void
ut_suite_create(ut_suite_t **suitep, char *name)
{
	ut_suite_t *suite;

	suite = (ut_suite_t *) malloc(sizeof(ut_suite_t));
	*suitep = suite;
	suite->head = suite->tail = NULL;
	suite->num_tests = suite->num_tests_failed = 0;
	suite->name = name;
}


static void 
ut_test_run(ut_test_t* test)
{
	pid_t pid;
	int status;
	ut_msg_header_t msg_header;
	ut_msg_t *msg;
	char c;
	int i;

    if (pipe(pfd) == -1) { perror("pipe"); exit(1); }

	pid = fork();
	if (pid == 0) {
		close(pfd[0]);
		status = test->f();
		exit(status);
	} else {
		close(pfd[1]);
		while (read(pfd[0], &msg_header, sizeof(ut_msg_header_t)) > 0) {
			msg = (ut_msg_t*) malloc(sizeof(ut_msg_t));
			if (test->msg_list_head == NULL) 
			{
				test->msg_list_head = test->msg_list_tail = msg;
			} else {
				test->msg_list_tail->next = msg;
				test->msg_list_tail = msg;
			}
			msg->header.type = msg_header.type;
			msg->header.body_len = msg_header.body_len;
			msg->body = (char*) malloc(msg_header.body_len+1);
			msg->next = NULL;
			switch(msg_header.type)
			{
				case MSG_FAILURE:
					for (i=0; i<msg_header.body_len; i++) 
					{
						read(pfd[0], &msg->body[i], 1);
					}
					msg->body[msg_header.body_len] = '\0';
					break;
			}
		}

		wait(&status);
		test->status = status;
	}
}


static void 
ut_suite_add_test(ut_suite_t *suite, char *name, int (*testf)())
{
	ut_test_t *test;

	test = (ut_test_t *) malloc(sizeof(ut_test_t));
	test->next = NULL;
	test->name = name;
	test->f = testf;
	test->msg_list_head = test->msg_list_tail = NULL;
	if (suite->head == NULL) {
		assert(suite->tail == NULL);
		suite->head = suite->tail = test;
	} else {
		suite->tail->next = test;
		suite->tail = test;
	}
}

# if 0

static void
ut_suite_run_all(ut_suite_t *suite)
{
	ut_test_t* test;
	ut_msg_t* msg;
	ut_msg_t* msg_next;
	int i;

	fprintf(UT_PRINT_OUT, 
	        "Suite: %s %s", 
	        suite->name,
	        padder_dots + strlen("Suite: ") + strlen(suite->name) + 1
	        );

	for (test = suite->head; 
	     test != NULL; 
	     test=test->next)
	{
		ut_test_run(test);	
		if (test->status != UT_TEST_SUCCESS) 
		{
			suite->num_tests_failed++;
		}
	}
	/*
	fprintf(UT_PRINT_OUT, 
	        "Suite: %s %s %s\n", 
	        suite->name,
	        padder_dots + strlen("Suite: ") + strlen(suite->name) + 1,
	        UT_STATUS2STR(suite->num_tests_failed != 0));
	*/
	fprintf(UT_PRINT_OUT, 
	        " %s\n", 
	        UT_STATUS2STR(suite->num_tests_failed != 0));

	if (verbose) 
	{
		for (test = suite->head; 
		     test != NULL; 
		     test=test->next)
		{
			fprintf(UT_PRINT_OUT, 
			        "%sTest: %s %s %s", 
			        SPACES(4),
			        test->name, 
					padder_dots + 4 + strlen("\tTest: ") + strlen(test->name),
			        UT_STATUS2STR(test->status));
			if (test->status < 256 && test->status > 0) 
			{
				fprintf(UT_PRINT_OUT, " (%d)", test->status);
			}
			fprintf(UT_PRINT_OUT, "\n");
			if (test->status != UT_TEST_SUCCESS) 
			{
				suite->num_tests_failed++;
			}

			if (test->msg_list_head) 
			{
				fprintf(UT_PRINT_OUT, "%sFailures:\n", SPACES(8)); 
			}	
			for (msg = test->msg_list_head, i=1;
			     msg != NULL;
			     msg = msg_next, i++)
			{
				fprintf(UT_PRINT_OUT, 
				        "%s%2d. %s\n", 
				        SPACES(10), 
				        i,
				        msg->body);
				msg_next = msg->next;
				free(msg->body);
				free(msg);
			}	
		}
	}	


	if (suite->num_tests_failed > 0) {
		exit(UT_TEST_FAILURE);
	} else {
		exit(UT_TEST_SUCCESS);
	}	
}

# else

static void
ut_suite_run_all(ut_suite_t *suite)
{
	ut_test_t* test;
	ut_msg_t* msg;
	ut_msg_t* msg_next;
	int i;

	fprintf(UT_PRINT_OUT, 
	        "Suite: %s\n", suite->name
	        );

	for (test = suite->head; 
	     test != NULL; 
	     test=test->next)
	{
		if (verbose) {
			fprintf(UT_PRINT_OUT, 
			        "%sTest : %s %s", 
			        SPACES(3),
			        test->name, 
					padder_dots + 4 + strlen("\tTest: ") + strlen(test->name));
		}

		ut_test_run(test);	
		if (test->status != UT_TEST_SUCCESS) 
		{
			suite->num_tests_failed++;
		}

		if (verbose) {
			fprintf(UT_PRINT_OUT, 
			        " %s", 
			        UT_STATUS2STR(test->status));
			if (test->status < 256 && test->status > 0) 
			{
				fprintf(UT_PRINT_OUT, " (%d)", test->status);
			}
			fprintf(UT_PRINT_OUT, "\n");
			if (test->status != UT_TEST_SUCCESS) 
			{
				suite->num_tests_failed++;
			}

			if (test->msg_list_head) 
			{
				fprintf(UT_PRINT_OUT, "%sFailures:\n", SPACES(8)); 
			}	
			for (msg = test->msg_list_head, i=1;
			     msg != NULL;
			     msg = msg_next, i++)
			{
				fprintf(UT_PRINT_OUT, 
				        "%s%2d. %s\n", 
				        SPACES(10), 
				        i,
				        msg->body);
				msg_next = msg->next;
				free(msg->body);
				free(msg);
			}	
		}
	}
	
	fprintf(UT_PRINT_OUT, 
	        "%sSuite: %s %s\n", 
	        SPACES(3),
	        padder_dots + strlen("Suite: ") + 2 + 1,
	        UT_STATUS2STR(suite->num_tests_failed != 0));

	if (suite->num_tests_failed > 0) {
		exit(UT_TEST_FAILURE);
	} else {
		exit(UT_TEST_SUCCESS);
	}	
}

# endif 

#endif
