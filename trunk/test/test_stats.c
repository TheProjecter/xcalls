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

#include <core/stats.h>
#include <misc/result.h>
#include <txc/txc.h>
#include "util/ut.h"

UT_START_TEST (test1)
{
	txc_tx_t *txd;
	int      i;

	UT_ASSERT_EQUAL(TXC_R_SUCCESS, _TXC_global_init());
	UT_ASSERT_EQUAL(TXC_R_SUCCESS, _TXC_thread_init());
	
	txd = txc_tx_get_txd();

	for (i=0; i<4; i++) {
	XACT_BEGIN(xact1)
		XACT_WAIVER  {
			txc_stats_txstat_increment(txd, XCALL, x_create, 1)
			txc_stats_txstat_increment(txd, XCALL, x_create, 1)
		}

	XACT_END(xact1)
	}

	XACT_BEGIN(xact2)
		XACT_WAIVER  {
			txc_stats_txstat_increment(txd, XCALL, x_read, 1)
		}

	XACT_END(xact2)


	UT_ASSERT_EQUAL(TXC_R_SUCCESS, _TXC_global_shutdown());
}
UT_END_TEST


UT_START_TEST_THREAD(test2_thread)
{
	txc_tx_t     *txd;
	unsigned int tid = (void *) __arg;
	int          i;
	int          n;

	UT_ASSERT_EQUAL(TXC_R_SUCCESS, _TXC_thread_init());

	txd = txc_tx_get_txd();

	n = (tid == 1 ? 3 : 1);
	for (i=0; i<n; i++) {
		XACT_BEGIN(xact1)
			XACT_WAIVER  {
				txc_stats_txstat_increment(txd, XCALL, x_read, i+1);
				if (tid == 1) {
					txc_stats_txstat_increment(txd, XCALL, x_create, 1);
				}
	
			}
		XACT_END(xact1)
	}
}
UT_END_TEST_THREAD


UT_START_TEST(test2)
{
	int i;
	txc_tx_t *txd;
	UT_THREAD_T thread[4];

	UT_ASSERT_EQUAL(TXC_R_SUCCESS, _TXC_global_init());

    UT_THREAD_CREATE (&thread[0], NULL, test2_thread, (void *) 1);
    UT_THREAD_CREATE (&thread[1], NULL, test2_thread, (void *) 2);

	UT_THREAD_JOIN(thread[0]);
	UT_THREAD_JOIN(thread[1]);

	UT_ASSERT_EQUAL(TXC_R_SUCCESS, _TXC_global_shutdown());
}
UT_END_TEST




int
main(int argc, char *argv[])
{
	ut_suite_t *suite;
	ut_suite_create(&suite, "test_stats");
	ut_suite_add_test(suite, "test1", test1);
	ut_suite_add_test(suite, "test2", test2);
	ut_suite_run_all(suite);
}
