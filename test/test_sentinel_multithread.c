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

#include <txc/txc.h>
#include <misc/result.h>
#include <core/sentinel.h>
#include <core/tx.h>
#include "util/ut.h"

#define SENTINEL_NUM 32


#define SENTINEL_ACQUIRE(__txd__, __id__)                                    \
do {                                                                         \
    txc_result_t __xres;                                                     \
    __xres = txc_sentinel_tryacquire(__txd__, sentinel_table[__id__],        \
		                             TXC_SENTINEL_ACQUIREONRETRY);           \
	if (__xres == TXC_R_BUSYSENTINEL) {                                      \
	    txc_tx_abort_transaction(txd, TXC_ABORTREASON_BUSYSENTINEL);         \
	}                                                                        \	
} while(0);							 


#define SENTINEL_OWNER(__id__)                                               \
    txc_sentinel_owner(sentinel_table[__id__])

#define SENTINEL_ENLISTED(__txd__, __id__)                                   \
    txc_sentinel_is_enlisted(__txd__, sentinel_table[__id__])

txc_sentinel_t *sentinel_table[SENTINEL_NUM];

UT_BARRIER_T test1_barrier1;

int test1_thread1_retries;

UT_START_TEST_THREAD(test1_thread1)
{
	txc_tx_t *txd;

	UT_ASSERT_EQUAL(TXC_R_SUCCESS, _TXC_thread_init());
	UT_BARRIER_WAIT(&test1_barrier1);

	txd = txc_tx_get_txd();
	test1_thread1_retries = 0;

	UT_BARRIER_WAIT(&test1_barrier1);

	usleep(100000);
	XACT_BEGIN(xact_sentinel)
		XACT_WAIVER {
				SENTINEL_ACQUIRE(txd, 2);
				SENTINEL_ACQUIRE(txd, 1);
				SENTINEL_ACQUIRE(txd, 0);
		}
	XACT_END(xact_sentinel)
	UT_ASSERT_NOTEQUAL(txd, SENTINEL_OWNER(0));
	UT_ASSERT_EQUAL(TXC_R_FAILURE, SENTINEL_ENLISTED(txd, 0));
	UT_ASSERT_NOTEQUAL(txd, SENTINEL_OWNER(1));
	UT_ASSERT_EQUAL(TXC_R_FAILURE, SENTINEL_ENLISTED(txd, 1));
	UT_ASSERT_NOTEQUAL(txd, SENTINEL_OWNER(2));
	UT_ASSERT_EQUAL(TXC_R_FAILURE, SENTINEL_ENLISTED(txd, 2));
}
UT_END_TEST_THREAD


int test1_thread2_retries;

UT_START_TEST_THREAD(test1_thread2)
{
	txc_tx_t *txd;

	UT_BARRIER_WAIT(&test1_barrier1);
	UT_ASSERT_EQUAL(TXC_R_SUCCESS, _TXC_thread_init());

	txd = txc_tx_get_txd();
	test1_thread2_retries = 0;

	UT_BARRIER_WAIT(&test1_barrier1);

	XACT_BEGIN(xact_sentinel)
		XACT_WAIVER {
				SENTINEL_ACQUIRE(txd, 2);
				usleep(200000);
		}
	XACT_END(xact_sentinel)

	UT_ASSERT_NOTEQUAL(txd, SENTINEL_OWNER(2));
	UT_ASSERT_EQUAL(TXC_R_FAILURE, SENTINEL_ENLISTED(txd, 2));

	XACT_BEGIN(xact_sentinel)
		XACT_WAIVER {
				SENTINEL_ACQUIRE(txd, 1);
				usleep(200000);
		}
	XACT_END(xact_sentinel)

	UT_ASSERT_NOTEQUAL(txd, SENTINEL_OWNER(1));
	UT_ASSERT_EQUAL(TXC_R_FAILURE, SENTINEL_ENLISTED(txd, 1));
}
UT_END_TEST_THREAD


UT_START_TEST(test1)
{
	int i;
	txc_tx_t *txd;
	UT_THREAD_T thread[4];

	UT_ASSERT_EQUAL(TXC_R_SUCCESS, _TXC_global_init());

	for (i=0; i<SENTINEL_NUM; i++) {
		txc_sentinel_create(txc_g_sentinelmgr, &sentinel_table[i]);
	}

	UT_BARRIER_INIT(&test1_barrier1, 2);

    UT_THREAD_CREATE (&thread[0], NULL, test1_thread1, (void *) 1);
    UT_THREAD_CREATE (&thread[1], NULL, test1_thread2, (void *) 2);

	UT_THREAD_JOIN(thread[0]);
	UT_THREAD_JOIN(thread[1]);
}
UT_END_TEST


int
main(int argc, char *argv[])
{
	ut_suite_t *suite;
	ut_suite_create(&suite, "test_sentinel_multithread");
	ut_suite_add_test(suite, "test1", test1);
	ut_suite_run_all(suite);
}
