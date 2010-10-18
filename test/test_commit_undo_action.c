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
#include "util/ut.h"

TM_WAIVER txc_result_t txc_tx_register_undo_action(txc_tx_t *, txc_tx_commit_function_t, void *, int *, int);
TM_WAIVER txc_result_t txc_tx_register_commit_action(txc_tx_t *, txc_tx_commit_function_t, void *, int *, int);

#define PRINT_DEBUG 0

/*
 * To capture the sequence of operations we use Godel's encoding.    
 */

unsigned int seq_num = 0;	
int retries;
unsigned int test1_compensating_action_1_seq;
unsigned int test1_compensating_action_2_seq;
unsigned long long score_commit; 
unsigned long long score_compensation; 
unsigned long long score_compensation2; 
unsigned int godel[] = {2,3,5,7,11,13,17,23,31}; 

void 
test1_commit_action_1(void *args, int *result)
{
	score_commit *= pow(godel[0], ++seq_num);
}

void 
test1_commit_action_2(void *args, int *result)
{
	score_commit *= pow(godel[1], ++seq_num);
}


void 
test1_undo_action_1(void *args, int *result)
{
	if (test1_compensating_action_1_seq++ == 0) {
		score_compensation *= pow(godel[2], ++seq_num);
#if PRINT_DEBUG 		
		printf("action1: score_compensation = %lld\n", score_compensation);
#endif		
	} else {
		score_compensation2 *= pow(godel[2], ++seq_num);
#if PRINT_DEBUG		
		printf("action1: score_compensation2 = %lld\n", score_compensation2);
#endif
	}
}

void 
test1_undo_action_2(void *args, int *result)
{
	if (test1_compensating_action_2_seq++ == 0) {
		score_compensation *= pow(godel[3], ++seq_num);
#if PRINT_DEBUG		
		printf("action2: score_compensation = %lld\n", score_compensation);
#endif
	} else {
		score_compensation2 *= pow(godel[3], ++seq_num);
#if PRINT_DEBUG		
		printf("action2: score_compensation2 = %lld\n", score_compensation2);
#endif
	}
}

UT_START_TEST(test1)
{
	txc_tx_t *txd;
	int i;
	int stress = 5;
	
	UT_ASSERT_EQUAL(TXC_R_SUCCESS, _TXC_global_init());
	UT_ASSERT_EQUAL(TXC_R_SUCCESS, _TXC_thread_init());

	txd = txc_tx_get_txd();

	for (i=0; i<stress; i++) {
		retries = 0;
		seq_num = 0;
		score_compensation = 1;
		test1_compensating_action_1_seq=0;
		test1_compensating_action_2_seq=0;
		score_commit = 1;
	
		XACT_BEGIN(xact_1)
			XACT_WAIVER { seq_num++;	}
			XACT_BEGIN(xact_2)
				txc_tx_register_commit_action(txd, test1_commit_action_1, NULL, NULL, 0);
				txc_tx_register_undo_action(txd, test1_undo_action_1, NULL, NULL, 0);
			XACT_END(xact_2)
			txc_tx_register_commit_action(txd, test1_commit_action_2, NULL, NULL, 0);
			txc_tx_register_undo_action(txd, test1_undo_action_2, NULL, NULL, 0);
			XACT_WAIVER {
				if (retries++ == 0) {
					XACT_ABORT(TXC_ABORTREASON_USERRETRY);	
				}
			}
		XACT_END(xact_1)

		UT_ASSERT_EQUAL(23328, score_commit);
		UT_ASSERT_EQUAL(6125, score_compensation);
	}
}
UT_END_TEST


UT_START_TEST(test2)
{
	txc_tx_t *txd;
	int i;
	int stress = 5;
	
	UT_ASSERT_EQUAL(TXC_R_SUCCESS, _TXC_global_init());
	UT_ASSERT_EQUAL(TXC_R_SUCCESS, _TXC_thread_init());
	txd = txc_tx_get_txd();

	for (i=0; i<stress; i++) {
		retries = 0;
		seq_num = 0;
		score_compensation = 1;
		score_compensation2 = 1;
		test1_compensating_action_1_seq=0;
		test1_compensating_action_2_seq=0;
		score_commit = 1;
	
		XACT_BEGIN(xact_1)
			XACT_WAIVER { seq_num++;	}
			XACT_BEGIN(xact_2)
				txc_tx_register_commit_action(txd, test1_commit_action_1, NULL, NULL, 0);
				txc_tx_register_undo_action(txd, test1_undo_action_1, NULL, NULL, 0);
			XACT_END(xact_2)
			txc_tx_register_commit_action(txd, test1_commit_action_2, NULL, NULL, 0);
			txc_tx_register_undo_action(txd, test1_undo_action_2, NULL, NULL, 0);
			XACT_WAIVER {
				if (retries++ < 2) {
					XACT_ABORT(TXC_ABORTREASON_USERRETRY);	
				}
			}
		XACT_END(xact_1)
		UT_ASSERT_EQUAL(5038848, score_commit);
		UT_ASSERT_EQUAL(6125, score_compensation);
		UT_ASSERT_EQUAL(262609375, score_compensation2);
	}
}
UT_END_TEST


int
main(int argc, char *argv[])
{
	ut_suite_t *suite;
	ut_suite_create(&suite, "test_commit_undo_action");
	ut_suite_add_test(suite, "test1", test1);
	ut_suite_add_test(suite, "test2", test2);
	ut_suite_run_all(suite);
}
