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

#include <misc/hash_table.h>
#include <misc/result.h>
#include "util/ut.h"

#define TBL_SIZE 128

txc_hash_table_t *hp;

UT_START_TEST (test1)
{
	unsigned int key1, key2, key3;
	void *val1, *val2, *val3;
	key1 = 4;            val1 = (void *) (key1+1);
	key2 = 4+TBL_SIZE;   val2 = (void *) (key2+1);
	key3 = 4+2*TBL_SIZE; val3 = (void *) (key3+1);

	UT_ASSERT_EQUAL(TXC_R_SUCCESS, txc_hash_table_create(&hp, TBL_SIZE, TXC_BOOL_TRUE));


	UT_ASSERT_EQUAL(TXC_R_SUCCESS, txc_hash_table_add(hp, key1, val1));
	UT_ASSERT_EQUAL(TXC_R_SUCCESS, txc_hash_table_add(hp, key2, val2)); 
	UT_ASSERT_EQUAL(TXC_R_SUCCESS, txc_hash_table_add(hp, key3, val3)); 
	UT_ASSERT_EQUAL(TXC_R_SUCCESS, txc_hash_table_remove(hp, key1, NULL)); 
	UT_ASSERT_EQUAL(TXC_R_NOTEXISTS, txc_hash_table_remove(hp, key1, NULL)); 
	UT_ASSERT_EQUAL(TXC_R_SUCCESS, txc_hash_table_lookup(hp, key2, NULL)); 
	UT_ASSERT_EQUAL(TXC_R_SUCCESS, txc_hash_table_lookup(hp, key3, NULL)); 

	UT_ASSERT_EQUAL(TXC_R_SUCCESS, txc_hash_table_destroy(&hp));
}
UT_END_TEST


UT_START_TEST (test2)
{
	unsigned int key1, key2, key3;
	void *val1, *val2, *val3;
	key1 = 4;            val1 = (void *) (key1+1);
	key2 = 4+TBL_SIZE;   val2 = (void *) (key2+1);
	key3 = 4+2*TBL_SIZE; val3 = (void *) (key3+1);

	UT_ASSERT_EQUAL(TXC_R_SUCCESS, txc_hash_table_create(&hp, TBL_SIZE, TXC_BOOL_TRUE));

	UT_ASSERT_EQUAL(TXC_R_SUCCESS, txc_hash_table_add(hp, key1, val1));
	UT_ASSERT_EQUAL(TXC_R_SUCCESS, txc_hash_table_add(hp, key2, val2)); 
	UT_ASSERT_EQUAL(TXC_R_SUCCESS, txc_hash_table_add(hp, key3, val3)); 
	UT_ASSERT_EQUAL(TXC_R_SUCCESS, txc_hash_table_remove(hp, key2, NULL)); 
	UT_ASSERT_EQUAL(TXC_R_NOTEXISTS, txc_hash_table_remove(hp, key2, NULL)); 
	UT_ASSERT_EQUAL(TXC_R_SUCCESS, txc_hash_table_lookup(hp, key1, NULL)); 
	UT_ASSERT_EQUAL(TXC_R_SUCCESS, txc_hash_table_lookup(hp, key3, NULL)); 

	UT_ASSERT_EQUAL(TXC_R_SUCCESS, txc_hash_table_destroy(&hp));
}
UT_END_TEST


UT_START_TEST (test3) 
{
	unsigned int key1, key2, key3;
	void *val1, *val2, *val3;
	key1 = 4;            val1 = (void *) (key1+1);
	key2 = 4+TBL_SIZE;   val2 = (void *) (key2+1);
	key3 = 4+2*TBL_SIZE; val3 = (void *) (key3+1);

	UT_ASSERT_EQUAL(TXC_R_SUCCESS, txc_hash_table_create(&hp, TBL_SIZE, TXC_BOOL_TRUE));

	UT_ASSERT_EQUAL(TXC_R_SUCCESS, txc_hash_table_add(hp, key1, val1));
	UT_ASSERT_EQUAL(TXC_R_SUCCESS, txc_hash_table_add(hp, key2, val2)); 
	UT_ASSERT_EQUAL(TXC_R_SUCCESS, txc_hash_table_add(hp, key3, val3)); 
	UT_ASSERT_EQUAL(TXC_R_SUCCESS, txc_hash_table_remove(hp, key3, NULL)); 
	UT_ASSERT_EQUAL(TXC_R_NOTEXISTS, txc_hash_table_remove(hp, key3, NULL)); 
	UT_ASSERT_EQUAL(TXC_R_SUCCESS, txc_hash_table_lookup(hp, key1, NULL)); 
	UT_ASSERT_EQUAL(TXC_R_SUCCESS, txc_hash_table_lookup(hp, key2, NULL)); 

	UT_ASSERT_EQUAL(TXC_R_SUCCESS, txc_hash_table_destroy(&hp));
}
UT_END_TEST


UT_START_TEST (test4)
{
	unsigned int key1, key2, key3;
	void *val1, *val2, *val3;
	key1 = 4;            val1 = (void *) (key1+1);
	key2 = 4+TBL_SIZE;   val2 = (void *) (key2+1);
	key3 = 4+2*TBL_SIZE; val3 = (void *) (key3+1);

	UT_ASSERT_EQUAL(TXC_R_SUCCESS, txc_hash_table_create(&hp, TBL_SIZE, TXC_BOOL_TRUE));

	UT_ASSERT_EQUAL(TXC_R_SUCCESS, txc_hash_table_add(hp, key1, val1));
	UT_ASSERT_EQUAL(TXC_R_SUCCESS, txc_hash_table_add(hp, key2, val2)); 
	UT_ASSERT_EQUAL(TXC_R_SUCCESS, txc_hash_table_add(hp, key3, val3)); 
	UT_ASSERT_EQUAL(TXC_R_SUCCESS, txc_hash_table_remove(hp, key1, NULL)); 
	UT_ASSERT_EQUAL(TXC_R_SUCCESS, txc_hash_table_remove(hp, key2, NULL)); 
	UT_ASSERT_EQUAL(TXC_R_SUCCESS, txc_hash_table_remove(hp, key3, NULL)); 
	UT_ASSERT_EQUAL(TXC_R_NOTEXISTS, txc_hash_table_remove(hp, key1, NULL)); 
	UT_ASSERT_EQUAL(TXC_R_NOTEXISTS, txc_hash_table_remove(hp, key2, NULL)); 
	UT_ASSERT_EQUAL(TXC_R_NOTEXISTS, txc_hash_table_remove(hp, key3, NULL)); 
	UT_ASSERT_EQUAL(TXC_R_SUCCESS, txc_hash_table_add(hp, key1, val1)); 
	UT_ASSERT_EQUAL(TXC_R_SUCCESS, txc_hash_table_add(hp, key2, val2)); 
	UT_ASSERT_EQUAL(TXC_R_SUCCESS, txc_hash_table_lookup(hp, key1, NULL)); 
	UT_ASSERT_EQUAL(TXC_R_SUCCESS, txc_hash_table_lookup(hp, key2, NULL)); 

	UT_ASSERT_EQUAL(TXC_R_SUCCESS, txc_hash_table_destroy(&hp));
}
UT_END_TEST


UT_START_TEST (test5)
{
	unsigned int key1, key2, key3, key4;
	void *val1, *val2, *val3, *val4;
	key1 = 4;            val1 = (void *) (key1+1);
	key2 = 4+TBL_SIZE;   val2 = (void *) (key2+1);
	key3 = 4+2*TBL_SIZE; val3 = (void *) (key3+1);
	key4 = 4+3*TBL_SIZE; val4 = (void *) (key4+1);

	UT_ASSERT_EQUAL(TXC_R_SUCCESS, txc_hash_table_create(&hp, TBL_SIZE, TXC_BOOL_TRUE));

	UT_ASSERT_EQUAL(TXC_R_SUCCESS, txc_hash_table_add(hp, key1, val1));
	UT_ASSERT_EQUAL(TXC_R_SUCCESS, txc_hash_table_add(hp, key2, val2)); 
	UT_ASSERT_EQUAL(TXC_R_SUCCESS, txc_hash_table_add(hp, key3, val3)); 
	UT_ASSERT_EQUAL(TXC_R_SUCCESS, txc_hash_table_remove(hp, key1, NULL)); 
	UT_ASSERT_EQUAL(TXC_R_SUCCESS, txc_hash_table_remove(hp, key2, NULL)); 
	UT_ASSERT_EQUAL(TXC_R_SUCCESS, txc_hash_table_remove(hp, key3, NULL)); 
	UT_ASSERT_EQUAL(TXC_R_NOTEXISTS, txc_hash_table_remove(hp, key1, NULL)); 
	UT_ASSERT_EQUAL(TXC_R_NOTEXISTS, txc_hash_table_remove(hp, key2, NULL)); 
	UT_ASSERT_EQUAL(TXC_R_NOTEXISTS, txc_hash_table_remove(hp, key3, NULL)); 
	UT_ASSERT_EQUAL(TXC_R_SUCCESS, txc_hash_table_add(hp, key1, val1)); 
	UT_ASSERT_EQUAL(TXC_R_SUCCESS, txc_hash_table_add(hp, key2, val2)); 
	UT_ASSERT_EQUAL(TXC_R_SUCCESS, txc_hash_table_add(hp, key3, val3)); 
	UT_ASSERT_EQUAL(TXC_R_SUCCESS, txc_hash_table_lookup(hp, key1, NULL)); 
	UT_ASSERT_EQUAL(TXC_R_SUCCESS, txc_hash_table_lookup(hp, key2, NULL)); 
	UT_ASSERT_EQUAL(TXC_R_SUCCESS, txc_hash_table_lookup(hp, key3, NULL)); 

	UT_ASSERT_EQUAL(TXC_R_SUCCESS, txc_hash_table_destroy(&hp));
}
UT_END_TEST


UT_START_TEST (test6)
{
	unsigned int key1, key2, key3, key4;
	void *val1, *val2, *val3, *val4;
	key1 = 4;            val1 = (void *) (key1+1);
	key2 = 4+TBL_SIZE;   val2 = (void *) (key2+1);
	key3 = 4+2*TBL_SIZE; val3 = (void *) (key3+1);
	key4 = 4+3*TBL_SIZE; val4 = (void *) (key4+1);

	UT_ASSERT_EQUAL(TXC_R_SUCCESS, txc_hash_table_create(&hp, TBL_SIZE, TXC_BOOL_TRUE));

	UT_ASSERT_EQUAL(TXC_R_SUCCESS, txc_hash_table_add(hp, key1, val1));
	UT_ASSERT_EQUAL(TXC_R_SUCCESS, txc_hash_table_add(hp, key2, val2)); 
	UT_ASSERT_EQUAL(TXC_R_SUCCESS, txc_hash_table_add(hp, key3, val3)); 
	UT_ASSERT_EQUAL(TXC_R_SUCCESS, txc_hash_table_remove(hp, key1, NULL)); 
	UT_ASSERT_EQUAL(TXC_R_SUCCESS, txc_hash_table_remove(hp, key2, NULL)); 
	UT_ASSERT_EQUAL(TXC_R_SUCCESS, txc_hash_table_remove(hp, key3, NULL)); 
	UT_ASSERT_EQUAL(TXC_R_NOTEXISTS, txc_hash_table_remove(hp, key1, NULL)); 
	UT_ASSERT_EQUAL(TXC_R_NOTEXISTS, txc_hash_table_remove(hp, key2, NULL)); 
	UT_ASSERT_EQUAL(TXC_R_NOTEXISTS, txc_hash_table_remove(hp, key3, NULL)); 
	UT_ASSERT_EQUAL(TXC_R_SUCCESS, txc_hash_table_add(hp, key1, val1)); 
	UT_ASSERT_EQUAL(TXC_R_SUCCESS, txc_hash_table_add(hp, key2, val2)); 
	UT_ASSERT_EQUAL(TXC_R_SUCCESS, txc_hash_table_add(hp, key3, val3)); 
	UT_ASSERT_EQUAL(TXC_R_SUCCESS, txc_hash_table_add(hp, key4, val4)); 
	UT_ASSERT_EQUAL(TXC_R_SUCCESS, txc_hash_table_lookup(hp, key1, NULL)); 
	UT_ASSERT_EQUAL(TXC_R_SUCCESS, txc_hash_table_lookup(hp, key2, NULL)); 
	UT_ASSERT_EQUAL(TXC_R_SUCCESS, txc_hash_table_lookup(hp, key3, NULL)); 
	UT_ASSERT_EQUAL(TXC_R_SUCCESS, txc_hash_table_lookup(hp, key4, NULL)); 

	UT_ASSERT_EQUAL(TXC_R_SUCCESS, txc_hash_table_destroy(&hp));
}
UT_END_TEST


UT_START_TEST (test7)
{
	unsigned int key1, key2, key3, key4;
	void *val1, *val2, *val3, *val4;
	key1 = 4;            val1 = (void *) (key1+1);
	key2 = 4+TBL_SIZE;   val2 = (void *) (key2+1);
	key3 = 4+2*TBL_SIZE; val3 = (void *) (key3+1);
	key4 = 4+3*TBL_SIZE; val4 = (void *) (key4+1);

	UT_ASSERT_EQUAL(TXC_R_SUCCESS, txc_hash_table_create(&hp, TBL_SIZE, TXC_BOOL_TRUE));

	UT_ASSERT_EQUAL(TXC_R_SUCCESS, txc_hash_table_add(hp, key1, val1));
	UT_ASSERT_EQUAL(TXC_R_SUCCESS, txc_hash_table_lookup(hp, key1, NULL)); 
	UT_ASSERT_EQUAL(TXC_R_SUCCESS, txc_hash_table_remove(hp, key1, NULL)); 
	UT_ASSERT_EQUAL(TXC_R_NOTEXISTS, txc_hash_table_remove(hp, key1, NULL)); 
	UT_ASSERT_EQUAL(TXC_R_SUCCESS, txc_hash_table_add(hp, key1, val1)); 
	UT_ASSERT_EQUAL(TXC_R_SUCCESS, txc_hash_table_lookup(hp, key1, NULL)); 

	UT_ASSERT_EQUAL(TXC_R_SUCCESS, txc_hash_table_destroy(&hp));
}
UT_END_TEST


int
main(int argc, char *argv[])
{
	ut_suite_t *suite;
	ut_suite_create(&suite, "test_hash");
	ut_suite_add_test(suite, "test1", test1);
	ut_suite_add_test(suite, "test2", test2);
	ut_suite_add_test(suite, "test3", test3);
	ut_suite_add_test(suite, "test4", test4);
	ut_suite_add_test(suite, "test5", test5);
	ut_suite_add_test(suite, "test6", test6);
	ut_suite_add_test(suite, "test7", test7);
	ut_suite_run_all(suite);
}
