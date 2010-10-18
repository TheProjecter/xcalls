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


UT_START_TEST(test1)
{
	txc_tx_t     *txd;
	volatile int test_retries;
	int          fd;

	UT_ASSERT_EQUAL(TXC_R_SUCCESS, _TXC_global_init());
	UT_ASSERT_EQUAL(TXC_R_SUCCESS, _TXC_thread_init());

	txd = txc_tx_get_txd();

	remove_file(test_file);
	UT_ASSERT_EQUAL(UT_FALSE, exists_file(test_file));

	test_retries = 0;
	XACT_BEGIN(xact_undo_action1)
		XACT_WAIVER {
			UT_ASSERT_EQUAL(UT_FALSE, exists_file(test_file));
		}	
		fd = _XCALL(x_create)(test_file, S_IRUSR|S_IWUSR, NULL);
		XACT_WAIVER {
			UT_ASSERT_EQUAL(txd, txc_sentinel_owner(FD2SENTINEL(fd)));
			UT_ASSERT_EQUAL(TXC_R_SUCCESS, SENTINEL_ENLISTED(txd, FD2SENTINEL(fd)));
			if (test_retries++ <= 1024) {
				XACT_ABORT(TXC_ABORTREASON_USERRETRY);	
			}
		}
	XACT_END(xact_undo_action1)
	UT_ASSERT_NOTEQUAL(txd, txc_sentinel_owner(FD2SENTINEL(fd)));
	UT_ASSERT_EQUAL(TXC_R_FAILURE, SENTINEL_ENLISTED(txd, FD2SENTINEL(fd)));
	UT_ASSERT_EQUAL(UT_TRUE, exists_file(test_file));
}
UT_END_TEST


UT_START_TEST(test2)
{
	txc_tx_t     *txd;
	volatile int test_retries;
	int          fd;
	int          saved_fd;

	UT_ASSERT_EQUAL(TXC_R_SUCCESS, _TXC_global_init());
	UT_ASSERT_EQUAL(TXC_R_SUCCESS, _TXC_thread_init());

	txd = txc_tx_get_txd();

	remove_file(test_file);
	UT_ASSERT_EQUAL(UT_FALSE, exists_file(test_file));

	test_retries = 0;
	XACT_BEGIN(xact_undo_action1)
		XACT_WAIVER {
			UT_ASSERT_EQUAL(UT_FALSE, exists_file(test_file));
		}	
		fd = _XCALL(x_create)(test_file, S_IRUSR|S_IWUSR, NULL);
		XACT_WAIVER {
			saved_fd = fd;
			UT_ASSERT_EQUAL(txd, txc_sentinel_owner(FD2SENTINEL(fd)));
			UT_ASSERT_EQUAL(TXC_R_SUCCESS, SENTINEL_ENLISTED(txd, FD2SENTINEL(fd)));
			if (test_retries++ <= 1024) {
				XACT_ABORT(TXC_ABORTREASON_USERRETRY);	
			} else {
				XACT_ABORT(TXC_ABORTREASON_USERABORT);	
			}
		}
	XACT_END(xact_undo_action1)
	UT_ASSERT_EQUAL(NULL, FD2SENTINEL(saved_fd));
	UT_ASSERT_EQUAL(UT_FALSE, exists_file(test_file));
}
UT_END_TEST


int
main(int argc, char *argv[])
{
	ut_suite_t *suite;
	ut_suite_create(&suite, "test_x_create_case1");
	ut_suite_add_test(suite, "test1", test1);
	ut_suite_add_test(suite, "test2", test2);
	ut_suite_run_all(suite);
}
