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


UT_START_TEST(test1)
{
	txc_tx_t     *txd;
	int          result;
	int          ret;
	char         *final_contents;
	volatile int test_retries;
	int          pipefd[2];
	char         buf[512];


	UT_ASSERT_EQUAL(TXC_R_SUCCESS, _TXC_global_init());
	UT_ASSERT_EQUAL(TXC_R_SUCCESS, _TXC_thread_init());

	txd = txc_tx_get_txd();

	_XCALL(x_pipe)(pipefd, NULL);
	XACT_BEGIN(xact_1)
		ret = _XCALL(x_write_pipe)(pipefd[1], "DEADBEEF", 9, &result);
	XACT_END(xact_1)
	read(pipefd[0], buf, 9);
	UT_ASSERT_EQUAL(0, strcmp(buf, "DEADBEEF"));
}
UT_END_TEST


UT_START_TEST(test2)
{
	txc_tx_t     *txd;
	int          result;
	int          ret;
	char         *final_contents;
	volatile int test_retries;
	int          pipefd[2];
	char         buf[512];
	int          f;

	UT_ASSERT_EQUAL(TXC_R_SUCCESS, _TXC_global_init());
	UT_ASSERT_EQUAL(TXC_R_SUCCESS, _TXC_thread_init());

	txd = txc_tx_get_txd();

	_XCALL(x_pipe)(pipefd, NULL);
	XACT_BEGIN(xact_1)
		ret = _XCALL(x_write_pipe)(pipefd[1], "DEADBEEF", 9, &result);
		XACT_ABORT(TXC_ABORTREASON_USERABORT);	
	XACT_END(xact_1)

	/* Make pipe non-blocking */
	f = fcntl(pipefd[0], F_GETFL, 0);
	f |= O_NONBLOCK;
	fcntl(pipefd[0], F_SETFL, f);
	read(pipefd[0], buf, 9);
	UT_ASSERT_NOTEQUAL(0, strcmp(buf, "DEADBEEF"));
}
UT_END_TEST


UT_START_TEST(test3)
{
	txc_tx_t     *txd;
	int          result;
	int          ret;
	char         *final_contents;
	volatile int test_retries;
	int          pipefd[2];
	char         buf[128];
	int          f;

	UT_ASSERT_EQUAL(TXC_R_SUCCESS, _TXC_global_init());
	UT_ASSERT_EQUAL(TXC_R_SUCCESS, _TXC_thread_init());

	txd = txc_tx_get_txd();

	_XCALL(x_pipe)(pipefd, NULL);
	test_retries = 0;
	XACT_BEGIN(xact_1)
		ret = _XCALL(x_write_pipe)(pipefd[1], "DEADBEEF", 9, &result);
		XACT_WAIVER {
			if (test_retries++ <= 8) {
				XACT_ABORT(TXC_ABORTREASON_USERRETRY);	
			}
		}
	XACT_END(xact_1)

	/* Make pipe non-blocking */
	f = fcntl(pipefd[0], F_GETFL, 0);
	f |= O_NONBLOCK;
	fcntl(pipefd[0], F_SETFL, f);
	read(pipefd[0], buf, 9);
	UT_ASSERT_EQUAL(0, strcmp(buf, "DEADBEEF"));
	memset(buf, 0, 128);
	read(pipefd[0], buf, 9);
	UT_ASSERT_NOTEQUAL(0, strcmp(buf, "DEADBEEF"));
}
UT_END_TEST


UT_START_TEST(test4)
{
	txc_tx_t     *txd;
	int          result;
	int          ret;
	char         *final_contents;
	volatile int test_retries;
	int          pipefd[2];
	char         buf[128];
	int          f;

	UT_ASSERT_EQUAL(TXC_R_SUCCESS, _TXC_global_init());
	UT_ASSERT_EQUAL(TXC_R_SUCCESS, _TXC_thread_init());

	txd = txc_tx_get_txd();

	_XCALL(x_pipe)(pipefd, NULL);
	_XCALL(x_write_pipe)(pipefd[1], "DEADBEEF", 9, NULL);
	test_retries = 0;
	XACT_BEGIN(xact_1)
		XACT_WAIVER {
			memset(buf, 128, 0);
		}
		ret = _XCALL(x_read_pipe)(pipefd[0], buf, 9, &result);
	XACT_END(xact_1)
	UT_ASSERT_EQUAL(0, strcmp(buf, "DEADBEEF"));
}
UT_END_TEST


UT_START_TEST(test5)
{
	txc_tx_t     *txd;
	int          result;
	int          ret;
	char         *final_contents;
	volatile int test_retries;
	int          pipefd[2];
	char         buf[128];
	int          f;

	UT_ASSERT_EQUAL(TXC_R_SUCCESS, _TXC_global_init());
	UT_ASSERT_EQUAL(TXC_R_SUCCESS, _TXC_thread_init());

	txd = txc_tx_get_txd();

	_XCALL(x_pipe)(pipefd, NULL);
	_XCALL(x_write_pipe)(pipefd[1], "DEADBEEF", 9, NULL);
	test_retries = 0;
	XACT_BEGIN(xact_1)
		XACT_WAIVER {
			memset(buf, 128, 0);
		}
		ret = _XCALL(x_read_pipe)(pipefd[0], buf, 9, &result);
		XACT_WAIVER {
			UT_ASSERT_EQUAL(0, strcmp(buf, "DEADBEEF"));
			if (test_retries++ <= 8) {
				XACT_ABORT(TXC_ABORTREASON_USERRETRY);	
			}
		}
	XACT_END(xact_1)
	UT_ASSERT_EQUAL(0, strcmp(buf, "DEADBEEF"));
}
UT_END_TEST


int
main(int argc, char *argv[])
{
	ut_suite_t *suite;
	ut_suite_create(&suite, "test_x_write");
	ut_suite_add_test(suite, "test1", test1);
	ut_suite_add_test(suite, "test2", test2);
	ut_suite_add_test(suite, "test3", test3);
	ut_suite_add_test(suite, "test4", test4);
	ut_suite_add_test(suite, "test5", test5);

	ut_suite_run_all(suite);
}
