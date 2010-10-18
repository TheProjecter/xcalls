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

#include <math.h>
#include <txc/txc.h>
#include <misc/result.h>
#include <core/tx.h>
#include <core/sentinel.h>
#include <core/koa.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include "util/ut.h"
#include "util/ut_file.h"

char *test_file = "/tmp/libtxc.tmp.test";
char *test_file_initial_contents;


UT_BARRIER_T test1_barrier1;

UT_START_TEST_THREAD(test1_thread)
{
	txc_tx_t     *txd;
	int          i;
	int          j;
	int          fd;
	int          result;
	int          ret;
	char         *final_contents;
	volatile int test_retries;
	int          tid = (int) __arg;
	char         buf[512];


	UT_ASSERT_EQUAL(TXC_R_SUCCESS, _TXC_thread_init());
	UT_BARRIER_WAIT(&test1_barrier1);

	txd = txc_tx_get_txd();

	UT_BARRIER_WAIT(&test1_barrier1);

	buf[0] = (char) ((int) '0' + tid);
	for (i=0; i<32; i++) {
		test_retries = 0;
		XACT_BEGIN(xact_1)
			fd = _XCALL(x_open)(test_file, O_RDWR, S_IRUSR|S_IWUSR, NULL);
			for (j=0; j<1024; j++) {
				ret = _XCALL(x_write_ovr_save)(fd, buf, 1, &result);
			}
			_XCALL(x_close)(fd, NULL);
		XACT_END(xact_1)
	}
}
UT_END_TEST_THREAD


UT_START_TEST(test1_4T)
{
	int i;
	txc_tx_t *txd;
	UT_THREAD_T thread[4];

	UT_ASSERT_EQUAL(TXC_R_SUCCESS, _TXC_global_init());

	UT_BARRIER_INIT(&test1_barrier1, 4);
	UT_ASSERT_EQUAL(0, create_file(test_file, ""));

    UT_THREAD_CREATE (&thread[0], NULL, test1_thread, (void *) 0);
    UT_THREAD_CREATE (&thread[1], NULL, test1_thread, (void *) 1);
    UT_THREAD_CREATE (&thread[2], NULL, test1_thread, (void *) 2);
    UT_THREAD_CREATE (&thread[3], NULL, test1_thread, (void *) 3);

	UT_THREAD_JOIN(thread[0]);
	UT_THREAD_JOIN(thread[1]);
	UT_THREAD_JOIN(thread[2]);
	UT_THREAD_JOIN(thread[3]);
}
UT_END_TEST


int          test2_fd;
UT_BARRIER_T test2_barrier1;

UT_START_TEST_THREAD(test2_thread)
{
	txc_tx_t     *txd;
	int          i;
	int          j;
	int          fd;
	int          result;
	int          ret;
	char         *final_contents;
	volatile int test_retries;
	int          tid = (int) __arg;
	char         buf[512];


	UT_ASSERT_EQUAL(TXC_R_SUCCESS, _TXC_thread_init());
	UT_BARRIER_WAIT(&test2_barrier1);

	txd = txc_tx_get_txd();

	UT_BARRIER_WAIT(&test2_barrier1);

	buf[0] = (char) ((int) '0' + tid);
	for (i=0; i<32; i++) {
		test_retries = 0;
		XACT_BEGIN(xact_1)
			for (j=0; j<1024; j++) {
				ret = _XCALL(x_write_ovr_save)(test2_fd, buf, 1, &result);
			}
		XACT_END(xact_1)
	}
}
UT_END_TEST_THREAD


UT_START_TEST(test2_4T)
{
	int         i;
	int         n;
	int         c;
	int         prev_c;
	FILE        *fstream;
	txc_tx_t    *txd;
	UT_THREAD_T thread[4];

	UT_ASSERT_EQUAL(TXC_R_SUCCESS, _TXC_global_init());

	UT_BARRIER_INIT(&test2_barrier1, 4);
	UT_ASSERT_EQUAL(0, create_file(test_file, ""));

	test2_fd = _XCALL(x_open)(test_file, O_RDWR, S_IRUSR|S_IWUSR, NULL);

    UT_THREAD_CREATE (&thread[0], NULL, test2_thread, (void *) 0);
    UT_THREAD_CREATE (&thread[1], NULL, test2_thread, (void *) 1);
    UT_THREAD_CREATE (&thread[2], NULL, test2_thread, (void *) 2);
    UT_THREAD_CREATE (&thread[3], NULL, test2_thread, (void *) 3);

	UT_THREAD_JOIN(thread[0]);
	UT_THREAD_JOIN(thread[1]);
	UT_THREAD_JOIN(thread[2]);
	UT_THREAD_JOIN(thread[3]);

	_XCALL(x_close)(test2_fd, NULL);

	/* Check whether there were any racy interleavings */
	if ((fstream = fopen(test_file, "r")) != NULL) {
		prev_c = 0;
		n = -1;
		while(1) {
			c = fgetc(fstream);
			if (c == EOF) {
				fclose(fstream);
				break;
			}
			if (prev_c == 0) {
				prev_c = c;
			}	
			n++;
			if (c != prev_c) {
				if (n % 1024 != 0) {
					/* race found */ 
					UT_ASSERT(0);
					break;
				}
				n = 0;
			}	
			prev_c = c;
		}
	}	

}
UT_END_TEST


UT_BARRIER_T test3_barrier1;

UT_START_TEST_THREAD(test3_thread)
{
	txc_tx_t     *txd;
	int          i;
	int          j;
	int          fd;
	int          result;
	int          ret;
	char         *final_contents;
	volatile int test_retries;
	int          tid = (int) __arg;
	char         buf[512];


	UT_ASSERT_EQUAL(TXC_R_SUCCESS, _TXC_thread_init());
	UT_BARRIER_WAIT(&test3_barrier1);

	txd = txc_tx_get_txd();

	UT_BARRIER_WAIT(&test3_barrier1);

	buf[0] = (char) ((int) '0' + tid);
	for (i=0; i<32; i++) {
		test_retries = 0;
		XACT_BEGIN(xact_1)
			fd = _XCALL(x_open)(test_file, O_RDWR, S_IRUSR|S_IWUSR, NULL);
			for (j=0; j<1024; j++) {
				ret = _XCALL(x_write_ovr_save)(fd, buf, 1, &result);
			}
			XACT_WAIVER {
				if (test_retries++ < 2) {
					XACT_ABORT(TXC_ABORTREASON_USERRETRY);	
				}
			}
			_XCALL(x_close)(fd, NULL);
		XACT_END(xact_1)
	}
}
UT_END_TEST_THREAD


UT_START_TEST(test3_4T)
{
	int i;
	txc_tx_t *txd;
	UT_THREAD_T thread[4];

	UT_ASSERT_EQUAL(TXC_R_SUCCESS, _TXC_global_init());

	UT_BARRIER_INIT(&test3_barrier1, 4);
	UT_ASSERT_EQUAL(0, create_file(test_file, ""));

    UT_THREAD_CREATE (&thread[0], NULL, test3_thread, (void *) 0);
    UT_THREAD_CREATE (&thread[1], NULL, test3_thread, (void *) 1);
    UT_THREAD_CREATE (&thread[2], NULL, test3_thread, (void *) 2);
    UT_THREAD_CREATE (&thread[3], NULL, test3_thread, (void *) 3);

	UT_THREAD_JOIN(thread[0]);
	UT_THREAD_JOIN(thread[1]);
	UT_THREAD_JOIN(thread[2]);
	UT_THREAD_JOIN(thread[3]);
}
UT_END_TEST




int
main(int argc, char *argv[])
{
	ut_suite_t *suite;
	ut_suite_create(&suite, "test_x_open_write_multithread");
	ut_suite_add_test(suite, "test1_4T", test1_4T);
	ut_suite_add_test(suite, "test2_4T", test2_4T);
	ut_suite_add_test(suite, "test3_4T", test3_4T);

	ut_suite_run_all(suite);
}
