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

#include <core/tx.h>
#include "util/ut.h"

UT_START_TEST (test1)
{
	txc_tx_t *txd1;

	UT_ASSERT_EQUAL(TXC_R_SUCCESS, _TXC_global_init());
	UT_ASSERT_EQUAL(TXC_R_SUCCESS, txc_tx_create(txc_g_txmgr, &txd1));
	UT_ASSERT_EQUAL(TXC_R_SUCCESS, txc_tx_exists(txc_g_txmgr, txd1));
	UT_ASSERT_EQUAL(TXC_R_SUCCESS, txc_tx_destroy(&txd1));

}
UT_END_TEST


UT_START_TEST (test2)
{
	txc_tx_t *txd1;
	txc_tx_t *txd2;

	UT_ASSERT_EQUAL(TXC_R_SUCCESS, _TXC_global_init());
	UT_ASSERT_EQUAL(TXC_R_SUCCESS, txc_tx_create(txc_g_txmgr, &txd1));
	UT_ASSERT_EQUAL(TXC_R_SUCCESS, txc_tx_create(txc_g_txmgr, &txd2));
	UT_ASSERT_EQUAL(TXC_R_SUCCESS, txc_tx_exists(txc_g_txmgr, txd1));
	UT_ASSERT_EQUAL(TXC_R_SUCCESS, txc_tx_exists(txc_g_txmgr, txd2));
	UT_ASSERT_EQUAL(TXC_R_SUCCESS, txc_tx_destroy(&txd1));
	UT_ASSERT_EQUAL(TXC_R_SUCCESS, txc_tx_destroy(&txd2));
	UT_ASSERT_EQUAL(TXC_R_NOTEXISTS, txc_tx_exists(txc_g_txmgr, txd1));
	UT_ASSERT_EQUAL(TXC_R_NOTEXISTS, txc_tx_exists(txc_g_txmgr, txd2));
}
UT_END_TEST


UT_START_TEST (test3)
{
	txc_tx_t *txd1;
	txc_tx_t *txd2;

	UT_ASSERT_EQUAL(TXC_R_SUCCESS, _TXC_global_init());
	UT_ASSERT_EQUAL(TXC_R_SUCCESS, txc_tx_create(txc_g_txmgr, &txd1));
	UT_ASSERT_EQUAL(TXC_R_SUCCESS, txc_tx_create(txc_g_txmgr, &txd2));
	UT_ASSERT_EQUAL(TXC_R_SUCCESS, txc_tx_exists(txc_g_txmgr, txd1));
	UT_ASSERT_EQUAL(TXC_R_SUCCESS, txc_tx_exists(txc_g_txmgr, txd2));
	UT_ASSERT_EQUAL(TXC_R_SUCCESS, txc_tx_destroy(&txd2));
	UT_ASSERT_EQUAL(TXC_R_SUCCESS, txc_tx_destroy(&txd1));
	UT_ASSERT_EQUAL(TXC_R_NOTEXISTS, txc_tx_exists(txc_g_txmgr, txd1));
	UT_ASSERT_EQUAL(TXC_R_NOTEXISTS, txc_tx_exists(txc_g_txmgr, txd2));
}
UT_END_TEST


UT_START_TEST (test4)
{
	txc_tx_t *txd1;
	txc_tx_t *txd2;
	txc_tx_t *txd3;

	UT_ASSERT_EQUAL(TXC_R_SUCCESS, _TXC_global_init());
	UT_ASSERT_EQUAL(TXC_R_SUCCESS, txc_tx_create(txc_g_txmgr, &txd1));
	UT_ASSERT_EQUAL(TXC_R_SUCCESS, txc_tx_create(txc_g_txmgr, &txd2));
	UT_ASSERT_EQUAL(TXC_R_SUCCESS, txc_tx_create(txc_g_txmgr, &txd3));
	UT_ASSERT_EQUAL(TXC_R_SUCCESS, txc_tx_exists(txc_g_txmgr, txd1));
	UT_ASSERT_EQUAL(TXC_R_SUCCESS, txc_tx_exists(txc_g_txmgr, txd2));
	UT_ASSERT_EQUAL(TXC_R_SUCCESS, txc_tx_exists(txc_g_txmgr, txd3));
	UT_ASSERT_EQUAL(TXC_R_SUCCESS, txc_tx_destroy(&txd2));
	UT_ASSERT_EQUAL(TXC_R_SUCCESS, txc_tx_destroy(&txd1));
	UT_ASSERT_EQUAL(TXC_R_SUCCESS, txc_tx_destroy(&txd3));
	UT_ASSERT_EQUAL(TXC_R_NOTEXISTS, txc_tx_exists(txc_g_txmgr, txd1));
	UT_ASSERT_EQUAL(TXC_R_NOTEXISTS, txc_tx_exists(txc_g_txmgr, txd2));
	UT_ASSERT_EQUAL(TXC_R_NOTEXISTS, txc_tx_exists(txc_g_txmgr, txd3));

}
UT_END_TEST


UT_START_TEST (test5)
{
	txc_tx_t *txd1;
	txc_tx_t *txd2;
	txc_tx_t *txd3;

	UT_ASSERT_EQUAL(TXC_R_SUCCESS, _TXC_global_init());
	UT_ASSERT_EQUAL(TXC_R_SUCCESS, txc_tx_create(txc_g_txmgr, &txd1));
	UT_ASSERT_EQUAL(TXC_R_SUCCESS, txc_tx_create(txc_g_txmgr, &txd2));
	UT_ASSERT_EQUAL(TXC_R_SUCCESS, txc_tx_create(txc_g_txmgr, &txd3));
	UT_ASSERT_EQUAL(TXC_R_SUCCESS, txc_tx_exists(txc_g_txmgr, txd1));
	UT_ASSERT_EQUAL(TXC_R_SUCCESS, txc_tx_exists(txc_g_txmgr, txd2));
	UT_ASSERT_EQUAL(TXC_R_SUCCESS, txc_tx_exists(txc_g_txmgr, txd3));
	UT_ASSERT_EQUAL(TXC_R_SUCCESS, txc_tx_destroy(&txd1));
	UT_ASSERT_EQUAL(TXC_R_SUCCESS, txc_tx_destroy(&txd2));
	UT_ASSERT_EQUAL(TXC_R_SUCCESS, txc_tx_destroy(&txd3));
	UT_ASSERT_EQUAL(TXC_R_NOTEXISTS, txc_tx_exists(txc_g_txmgr, txd1));
	UT_ASSERT_EQUAL(TXC_R_NOTEXISTS, txc_tx_exists(txc_g_txmgr, txd2));
	UT_ASSERT_EQUAL(TXC_R_NOTEXISTS, txc_tx_exists(txc_g_txmgr, txd3));

}
UT_END_TEST


int
main(int argc, char *argv[])
{
	ut_suite_t *suite;
	ut_suite_create(&suite, "test_txmgr");
	ut_suite_add_test(suite, "test1", test1);
	ut_suite_add_test(suite, "test2", test2);
	ut_suite_add_test(suite, "test3", test3);
	ut_suite_add_test(suite, "test4", test4);
	ut_suite_add_test(suite, "test5", test5);
	ut_suite_run_all(suite);
}
