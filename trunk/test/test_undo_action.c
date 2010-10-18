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

TM_WAIVER txc_result_t txc_tx_register_undo_action(txc_tx_t *, txc_tx_undo_function_t, void *, int *, int);


/*
 * To capture the sequence of operations we use Godel's encoding.    
 */

int seq_num = 0;	
int retries;
int score; 
int godel[] = {2,3,5,7,11,13,17,23,31}; 

void 
test1_undo_action_1(void *args, int *result)
{
	score *= pow(godel[0], ++seq_num);
}

void 
test1_undo_action_2(void *args, int *result)
{
	score *= pow(godel[1], ++seq_num);
}

UT_START_TEST(test1)
{
	txc_tx_t *txd;
	retries = 0;
	seq_num = 0;
	score = 1;

	UT_ASSERT_EQUAL(TXC_R_SUCCESS, _TXC_global_init());
	UT_ASSERT_EQUAL(TXC_R_SUCCESS, _TXC_thread_init());

	txd = txc_tx_get_txd();

	XACT_BEGIN(xact_undo_action1)
		XACT_WAIVER { seq_num++;	}
		XACT_BEGIN(xact_undo_action2)
			txc_tx_register_undo_action(txd, 
			                            test1_undo_action_1, 
			                            NULL, NULL, 0);
			XACT_WAIVER {
				if (retries++ == 0) {
					XACT_ABORT(TXC_ABORTREASON_USERRETRY);	
				}
			}
		XACT_END(xact_undo_action2)
		txc_tx_register_undo_action(txd, 
                                    test1_undo_action_2, 
	                                NULL, NULL, 0);
	XACT_END(xact_undo_action1)

	UT_ASSERT_EQUAL(4, score);
}
UT_END_TEST

UT_START_TEST(test2)
{
	txc_tx_t *txd;
	retries = 0;
	seq_num = 0;
	score = 1;
	
	UT_ASSERT_EQUAL(TXC_R_SUCCESS, _TXC_global_init());
	UT_ASSERT_EQUAL(TXC_R_SUCCESS, _TXC_thread_init());

	txd = txc_tx_get_txd();

	XACT_BEGIN(xact_undo_action2)
		XACT_WAIVER { seq_num++;	}
		XACT_BEGIN(xact_undo_action1)
			txc_tx_register_undo_action(txd, test1_undo_action_1, NULL, NULL, 0);
		XACT_END(xact_undo_action1)
		txc_tx_register_undo_action(txd, test1_undo_action_2, NULL, NULL, 0);
		XACT_WAIVER {
			if (retries++ == 0) {
				XACT_ABORT(TXC_ABORTREASON_USERRETRY);
			}
		}
	XACT_END(xact_undo_action2)

	UT_ASSERT_EQUAL(72, score);
}
UT_END_TEST

void 
test3_undo_action_1(void *args, int *result)
{
	int my_seq_num = (int) args;
	score *= pow(godel[0], my_seq_num);
}

void 
test3_undo_action_2(void *args, int *result)
{
	int my_seq_num = (int) args;
	score *= pow(godel[1], my_seq_num);
}

UT_START_TEST(test3)
{
	txc_tx_t *txd;
	retries = 0;
	seq_num = 0;
	score = 1;

	UT_ASSERT_EQUAL(TXC_R_SUCCESS, _TXC_global_init());
	UT_ASSERT_EQUAL(TXC_R_SUCCESS, _TXC_thread_init());

	txd = txc_tx_get_txd();
	
	XACT_BEGIN(xact_undo_action2)
		XACT_WAIVER { seq_num++;	}
		XACT_BEGIN(xact_undo_action1)
			XACT_WAIVER { seq_num++; }
			txc_tx_register_undo_action(txd, test3_undo_action_1, (void *) seq_num, NULL, 0);
		XACT_END(xact_undo_action1)
		XACT_WAIVER { seq_num++; }
		txc_tx_register_undo_action(txd, test3_undo_action_2, (void *) seq_num, NULL, 0);
		XACT_WAIVER {
			if (retries++ == 0) {
				XACT_ABORT(TXC_ABORTREASON_USERRETRY);
			}
		}
	XACT_END(xact_undo_action2)

	UT_ASSERT_EQUAL(108, score);
}
UT_END_TEST


int
main(int argc, char *argv[])
{
	ut_suite_t *suite;
	ut_suite_create(&suite, "test_undo_action");
	ut_suite_add_test(suite, "test1", test1);
	ut_suite_add_test(suite, "test2", test2);
	ut_suite_add_test(suite, "test3", test3);
	ut_suite_run_all(suite);
}
