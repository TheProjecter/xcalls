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
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include "util/ut.h"
#include "util/ut_file.h"

char *test_file = "/tmp/libtxc.tmp.test";
char *test_file_initial_contents[] = {"LINE_1\n", 
                                      "LINE_2\n", 
                                      NULL};


UT_START_TEST(test1)
{
	txc_tx_t     *txd;
	volatile int test_retries;
	int          fd;
	int          ret;
	char         *final_contents;
	int          result;

	UT_ASSERT_EQUAL(TXC_R_SUCCESS, _TXC_global_init());
	UT_ASSERT_EQUAL(TXC_R_SUCCESS, _TXC_thread_init());

	txd = txc_tx_get_txd();

	final_contents = str_create("");
	str_modify(&final_contents, 0, "DEADBEEF");

	test_retries = 0;
	fd = _XCALL(x_create)(test_file, S_IRUSR|S_IWUSR, NULL);

	XACT_BEGIN(xact_1)
		ret = _XCALL(x_write_ovr_save)(fd, "DEADBEEF", 8, &result);
		_XCALL(x_close)(fd, NULL);
		XACT_WAIVER {
			if (test_retries++ <= 1024) {
				XACT_ABORT(TXC_ABORTREASON_USERRETRY);	
			}
		}
	XACT_END(xact_1)
	ret = _XCALL(x_write_ovr_save)(fd, "MADCOW", 6, &result);

	UT_ASSERT_EQUAL(UT_TRUE, file_equal_str(test_file, final_contents));
}
UT_END_TEST


UT_START_TEST(test2)
{
	txc_tx_t     *txd;
	volatile int test_retries;
	int          fd;
	int          ret;
	char         *final_contents;
	int          result;

	UT_ASSERT_EQUAL(TXC_R_SUCCESS, _TXC_global_init());
	UT_ASSERT_EQUAL(TXC_R_SUCCESS, _TXC_thread_init());

	txd = txc_tx_get_txd();

	final_contents = str_create("");
	str_modify(&final_contents, 0, "MADCOW");

	test_retries = 0;
	fd = _XCALL(x_create)(test_file, S_IRUSR|S_IWUSR, NULL);

	XACT_BEGIN(xact_1)
		ret = _XCALL(x_write_ovr_save)(fd, "DEADBEEF", 8, &result);
		_XCALL(x_close)(fd, NULL);
		XACT_WAIVER {
			if (test_retries++ <= 1024) {
				XACT_ABORT(TXC_ABORTREASON_USERRETRY);	
			} else {
				XACT_ABORT(TXC_ABORTREASON_USERABORT);	
			}
		}
	XACT_END(xact_1)
	ret = _XCALL(x_write_ovr_save)(fd, "MADCOW", 6, &result);
	_XCALL(x_close)(fd, NULL);

	UT_ASSERT_EQUAL(UT_TRUE, file_equal_str(test_file, final_contents));
}
UT_END_TEST


UT_START_TEST(test3)
{
	txc_tx_t     *txd;
	volatile int test_retries;
	int          fd;
	int          ret;
	char         *final_contents;
	int          result;
	int          i;

	UT_ASSERT_EQUAL(TXC_R_SUCCESS, _TXC_global_init());
	UT_ASSERT_EQUAL(TXC_R_SUCCESS, _TXC_thread_init());

	txd = txc_tx_get_txd();

	final_contents = str_create("");
	str_modify(&final_contents, 0, "DEADBEEF");

	test_retries = 0;

	for (i=0; i<128; i++) {
	XACT_BEGIN(xact_1)
		fd = _XCALL(x_create)(test_file, S_IRUSR|S_IWUSR, NULL);
		ret = _XCALL(x_write_ovr_save)(fd, "DEADBEEF", 8, &result);
		_XCALL(x_close)(fd, NULL);
		_XCALL(x_close)(fd, NULL);
	XACT_END(xact_1)
	}

	UT_ASSERT_EQUAL(UT_TRUE, file_equal_str(test_file, final_contents));
}
UT_END_TEST


UT_START_TEST(test4)
{
	txc_tx_t     *txd;
	volatile int test_retries;
	int          fd;
	int          ret;
	char         *final_contents;
	int          result;

	UT_ASSERT_EQUAL(TXC_R_SUCCESS, _TXC_global_init());
	UT_ASSERT_EQUAL(TXC_R_SUCCESS, _TXC_thread_init());

	txd = txc_tx_get_txd();

	final_contents = str_create("");
	str_modify(&final_contents, 0, "DEADBEEF");

	test_retries = 0;

	XACT_BEGIN(xact_1)
		fd = _XCALL(x_create)(test_file, S_IRUSR|S_IWUSR, NULL);
		ret = _XCALL(x_write_ovr_save)(fd, "DEADBEEF", 8, &result);
		XACT_WAIVER {
			if (test_retries++ <= 1024) {
				XACT_ABORT(TXC_ABORTREASON_USERRETRY);	
			} else {
				XACT_ABORT(TXC_ABORTREASON_USERABORT);	
			}
		}
	XACT_END(xact_1)
	ret = _XCALL(x_write_ovr_save)(fd, "MADCOW", 6, &result);
	_XCALL(x_close)(fd, NULL);
	_XCALL(x_close)(fd, NULL);

	UT_ASSERT_EQUAL(UT_TRUE, file_equal_str(test_file, final_contents));
}
UT_END_TEST



int
main(int argc, char *argv[])
{
	ut_suite_t *suite;
	ut_suite_create(&suite, "test_x_close");
	ut_suite_add_test(suite, "test1", test1);
	ut_suite_add_test(suite, "test2", test2);
	ut_suite_add_test(suite, "test3", test3);
	ut_suite_add_test(suite, "test4", test4);
	ut_suite_run_all(suite);
}
