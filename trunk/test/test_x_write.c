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
char *test_file_initial_contents_tbl[] = { 
	"00: Thermopylae by Constantine P. Cavafy (1903)\n",
	"01: Honor to those who in their lives\n",
	"02: have defined and guard their Thermopylae.\n",
	"03: Never stirring from duty;\n",
	"04: just and upright in all their deeds,\n",
	"05: yet with pity and compassion too;\n",
	"06: generous when they are rich, and when\n",
	"07: they are poor, again a little generous,\n",
	"08: again helping as much as they can;\n",
	"09: always speaking the truth,\n",
	"10: yet without hatred for those who lie.\n",
	"11: And more honor is due to them\n",
	"12: when they foresee (and many do foresee)\n",
	"13: that Ephialtes will finally appear,\n",
	"14: and that the Medes in the end will go through.\n",
	NULL
};


UT_START_TEST(test1)
{
	txc_tx_t     *txd;
	int          fd;
	int          result;
	int          ret;
	char         *final_contents;
	volatile int test_retries;


	UT_ASSERT_EQUAL(TXC_R_SUCCESS, _TXC_global_init());
	UT_ASSERT_EQUAL(TXC_R_SUCCESS, _TXC_thread_init());

	txd = txc_tx_get_txd();

	UT_ASSERT_EQUAL(0, create_file(test_file, test_file_initial_contents));
	UT_ASSERT_EQUAL(UT_TRUE, file_equal_str(test_file, test_file_initial_contents));
	final_contents = str_create("");
	str_modify(&final_contents, 0, "DEADBEEF");
	str_modify(&final_contents, strlen(final_contents), "MADCOW");
	str_modify(&final_contents, strlen(final_contents), "ZOMBIE");

	test_retries = 0;
	fd = _XCALL(x_create)(test_file, S_IRUSR|S_IWUSR, NULL);
	XACT_BEGIN(xact_1)
		XACT_WAIVER {
			if (test_retries > 0) {
				UT_ASSERT_EQUAL(txd, txc_sentinel_owner(FD2SENTINEL(fd)));
				UT_ASSERT_NOTEQUAL(TXC_R_FAILURE, SENTINEL_ENLISTED(txd, FD2SENTINEL(fd)));
			}	
		}	
		ret = _XCALL(x_write_ovr_save)(fd, "DEADBEEF", 8, &result);
		ret = _XCALL(x_write_ovr_save)(fd, "MADCOW", 6, &result);
		ret = _XCALL(x_write_ovr_save)(fd, "ZOMBIE", 6, &result);
		XACT_WAIVER {
			UT_ASSERT_EQUAL(txd, txc_sentinel_owner(FD2SENTINEL(fd)));
			UT_ASSERT_NOTEQUAL(TXC_R_FAILURE, SENTINEL_ENLISTED(txd, FD2SENTINEL(fd)));
			if (test_retries++ <= 8) {
				XACT_ABORT(TXC_ABORTREASON_USERRETRY);	
			}
		}
	XACT_END(xact_1)
	UT_ASSERT_NOTEQUAL(txd, txc_sentinel_owner(FD2SENTINEL(fd)));
	UT_ASSERT_EQUAL(TXC_R_FAILURE, SENTINEL_ENLISTED(txd, FD2SENTINEL(fd)));
	UT_ASSERT_EQUAL(UT_TRUE, file_equal_str(test_file, final_contents));
}
UT_END_TEST


UT_START_TEST(test2)
{
	txc_tx_t     *txd;
	int          fd;
	int          result;
	int          ret;
	char         *final_contents;
	char         *intermediate_contents;
	volatile int test_retries;
	txc_koa_t    *koa;


	UT_ASSERT_EQUAL(TXC_R_SUCCESS, _TXC_global_init());
	UT_ASSERT_EQUAL(TXC_R_SUCCESS, _TXC_thread_init());

	txd = txc_tx_get_txd();

	UT_ASSERT_EQUAL(0, create_file(test_file, test_file_initial_contents));
	UT_ASSERT_EQUAL(UT_TRUE, file_equal_str(test_file, test_file_initial_contents));
	intermediate_contents = str_create("");
	str_modify(&intermediate_contents, 0, "DEADBEEF");
	str_modify(&intermediate_contents, strlen(intermediate_contents), "MADCOW");
	str_modify(&intermediate_contents, strlen(intermediate_contents), "ZOMBIE");
	final_contents = str_create("");

	test_retries = 0;
	fd = _XCALL(x_create)(test_file, S_IRUSR|S_IWUSR, NULL);
	XACT_BEGIN(xact_1)
		ret = _XCALL(x_write_ovr_save)(fd, "DEADBEEF", 8, &result);
		ret = _XCALL(x_write_ovr_save)(fd, "MADCOW", 6, &result);
		ret = _XCALL(x_write_ovr_save)(fd, "ZOMBIE", 6, &result);
		XACT_WAIVER {
			UT_ASSERT_EQUAL(UT_TRUE, file_equal_str(test_file, intermediate_contents));
		}
		XACT_ABORT(TXC_ABORTREASON_USERABORT);	
	XACT_END(xact_1)
	UT_ASSERT_NOTEQUAL(txd, SENTINEL_OWNER(FD2SENTINEL(fd)));
	UT_ASSERT_EQUAL(TXC_R_FAILURE, SENTINEL_ENLISTED(txd, FD2SENTINEL(fd)));
	UT_ASSERT_EQUAL(UT_TRUE, file_equal_str(test_file, final_contents));
}
UT_END_TEST


UT_START_TEST(test3)
{
	txc_tx_t     *txd;
	int          fd;
	int          result;
	int          ret;
	char         *final_contents;
	volatile int test_retries;
	txc_koa_t    *koa;


	UT_ASSERT_EQUAL(TXC_R_SUCCESS, _TXC_global_init());
	UT_ASSERT_EQUAL(TXC_R_SUCCESS, _TXC_thread_init());

	txd = txc_tx_get_txd();

	UT_ASSERT_EQUAL(0, create_file(test_file, test_file_initial_contents));
	UT_ASSERT_EQUAL(UT_TRUE, file_equal_str(test_file, test_file_initial_contents));
	final_contents = str_create("");
	str_modify(&final_contents, 0, "DEADBEEF");
	str_modify(&final_contents, strlen(final_contents), "MADCOW");
	str_modify(&final_contents, strlen(final_contents), "ZOMBIE");
	final_contents = str_create("");

	test_retries = 0;
	fd = _XCALL(x_create)(test_file, S_IRUSR|S_IWUSR, NULL);
	XACT_BEGIN(xact_1)
		ret = _XCALL(x_write_ovr_save)(fd, "DEADBEEF", 8, &result);
		ret = _XCALL(x_write_ovr_save)(fd, "MADCOW", 6, &result);
		ret = _XCALL(x_write_ovr_save)(fd, "ZOMBIE", 6, &result);
	XACT_END(xact_1)
	UT_ASSERT_NOTEQUAL(txd, SENTINEL_OWNER(FD2SENTINEL(fd)));
	UT_ASSERT_EQUAL(TXC_R_FAILURE, SENTINEL_ENLISTED(txd, FD2SENTINEL(fd)));
	UT_ASSERT_EQUAL(UT_TRUE, file_equal_str(test_file, final_contents));
}
UT_END_TEST


UT_START_TEST(test4)
{
	txc_tx_t     *txd;
	int          fd;
	int          result;
	int          ret;
	char         *initial_contents;
	char         *final_contents;
	volatile int test_retries;
	char         *str1 = "DEADBEEF----------------";
	char         *str2 = "MADCOW--------------";
	char         *str3 = "ZOMBIE";
	int          len1 = strlen(str1);
	int          len2 = strlen(str2);
	int          len3 = strlen(str3);


	UT_ASSERT_EQUAL(TXC_R_SUCCESS, _TXC_global_init());
	UT_ASSERT_EQUAL(TXC_R_SUCCESS, _TXC_thread_init());

	txd = txc_tx_get_txd();

	initial_contents = test_file_initial_contents_tbl[0];
	UT_ASSERT_EQUAL(0, create_file(test_file, initial_contents));
	UT_ASSERT_EQUAL(UT_TRUE, file_equal_str(test_file, initial_contents));
	final_contents = str_create(initial_contents);
	str_modify(&final_contents, 0, str1);
	str_modify(&final_contents, len1, str2);
	str_modify(&final_contents, len1+len2, str3);

	test_retries = 0;
	fd = _XCALL(x_open)(test_file, O_RDWR, S_IRUSR|S_IWUSR, NULL);
	XACT_BEGIN(xact_1)
		XACT_WAIVER {
			if (test_retries > 0) {
				UT_ASSERT_EQUAL(txd, txc_sentinel_owner(FD2SENTINEL(fd)));
				UT_ASSERT_NOTEQUAL(TXC_R_FAILURE, SENTINEL_ENLISTED(txd, FD2SENTINEL(fd)));
				UT_ASSERT_EQUAL(UT_TRUE, file_equal_str(test_file, initial_contents));
			}	
		}	
		ret = _XCALL(x_write_ovr_save)(fd, str1, len1, &result);
		ret = _XCALL(x_write_ovr_save)(fd, str2, len2, &result);
		ret = _XCALL(x_write_ovr_save)(fd, str3, len3, &result);
		XACT_WAIVER {
			UT_ASSERT_EQUAL(txd, txc_sentinel_owner(FD2SENTINEL(fd)));
			UT_ASSERT_NOTEQUAL(TXC_R_FAILURE, SENTINEL_ENLISTED(txd, FD2SENTINEL(fd)));
			UT_ASSERT_EQUAL(UT_TRUE, file_equal_str(test_file, final_contents));
			if (test_retries++ <= 8) {
				XACT_ABORT(TXC_ABORTREASON_USERRETRY);	
			}
		}
	XACT_END(xact_1)
	UT_ASSERT_NOTEQUAL(txd, txc_sentinel_owner(FD2SENTINEL(fd)));
	UT_ASSERT_EQUAL(TXC_R_FAILURE, SENTINEL_ENLISTED(txd, FD2SENTINEL(fd)));
	UT_ASSERT_EQUAL(UT_TRUE, file_equal_str(test_file, final_contents));
}
UT_END_TEST


UT_START_TEST(test5)
{
	txc_tx_t     *txd;
	int          fd;
	int          result;
	int          ret;
	char         *initial_contents;
	char         *final_contents;
	volatile int test_retries;
	char         *str1 = "DEADBEEF----------------";
	char         *str2 = "MADCOW--------------";
	char         *str3 = "ZOMBIE";
	int          len1 = strlen(str1);
	int          len2 = strlen(str2);
	int          len3 = strlen(str3);


	UT_ASSERT_EQUAL(TXC_R_SUCCESS, _TXC_global_init());
	UT_ASSERT_EQUAL(TXC_R_SUCCESS, _TXC_thread_init());

	txd = txc_tx_get_txd();

	initial_contents = test_file_initial_contents_tbl[0];
	UT_ASSERT_EQUAL(0, create_file(test_file, initial_contents));
	UT_ASSERT_EQUAL(UT_TRUE, file_equal_str(test_file, initial_contents));
	final_contents = str_create(initial_contents);
	str_modify(&final_contents, 0, str1);
	str_modify(&final_contents, len1, str2);
	str_modify(&final_contents, len1+len2, str3);

	test_retries = 0;
	fd = _XCALL(x_open)(test_file, O_RDWR, S_IRUSR|S_IWUSR, NULL);
	XACT_BEGIN(xact_1)
		XACT_WAIVER {
			if (test_retries > 0) {
				UT_ASSERT_EQUAL(txd, txc_sentinel_owner(FD2SENTINEL(fd)));
				UT_ASSERT_NOTEQUAL(TXC_R_FAILURE, SENTINEL_ENLISTED(txd, FD2SENTINEL(fd)));
				UT_ASSERT_EQUAL(UT_TRUE, file_equal_str(test_file, initial_contents));
			}	
		}	
		ret = _XCALL(x_write_ovr_save)(fd, str1, len1, &result);
		ret = _XCALL(x_write_ovr_save)(fd, str2, len2, &result);
		ret = _XCALL(x_write_ovr_save)(fd, str3, len3, &result);
		XACT_WAIVER {
			UT_ASSERT_EQUAL(txd, txc_sentinel_owner(FD2SENTINEL(fd)));
			UT_ASSERT_NOTEQUAL(TXC_R_FAILURE, SENTINEL_ENLISTED(txd, FD2SENTINEL(fd)));
			UT_ASSERT_EQUAL(UT_TRUE, file_equal_str(test_file, final_contents));
		}
		XACT_ABORT(TXC_ABORTREASON_USERABORT);	
	XACT_END(xact_1)
	UT_ASSERT_NOTEQUAL(txd, txc_sentinel_owner(FD2SENTINEL(fd)));
	UT_ASSERT_EQUAL(TXC_R_FAILURE, SENTINEL_ENLISTED(txd, FD2SENTINEL(fd)));
	UT_ASSERT_EQUAL(UT_TRUE, file_equal_str(test_file, initial_contents));
}
UT_END_TEST


UT_START_TEST(test6)
{
	txc_tx_t     *txd;
	int          fd;
	int          result;
	int          ret;
	char         *initial_contents;
	char         *final_contents;
	volatile int test_retries;
	char         *str1 = "DEADBEEF----------------";
	char         *str2 = "MADCOW--------------";
	char         *str3 = "ZOMBIE";
	int          len1 = strlen(str1);
	int          len2 = strlen(str2);
	int          len3 = strlen(str3);


	UT_ASSERT_EQUAL(TXC_R_SUCCESS, _TXC_global_init());
	UT_ASSERT_EQUAL(TXC_R_SUCCESS, _TXC_thread_init());

	txd = txc_tx_get_txd();

	initial_contents = test_file_initial_contents_tbl[0];
	UT_ASSERT_EQUAL(0, create_file(test_file, initial_contents));
	UT_ASSERT_EQUAL(UT_TRUE, file_equal_str(test_file, initial_contents));
	final_contents = str_create(initial_contents);
	str_modify(&final_contents, 0, str1);
	str_modify(&final_contents, len1, str2);

	test_retries = 0;
	fd = _XCALL(x_open)(test_file, O_RDWR, S_IRUSR|S_IWUSR, NULL);
	XACT_BEGIN(xact_1)
		XACT_WAIVER {
			if (test_retries > 0) {
				UT_ASSERT_EQUAL(txd, txc_sentinel_owner(FD2SENTINEL(fd)));
				UT_ASSERT_NOTEQUAL(TXC_R_FAILURE, SENTINEL_ENLISTED(txd, FD2SENTINEL(fd)));
				UT_ASSERT_EQUAL(UT_TRUE, file_equal_str(test_file, initial_contents));
			}	
		}	
		ret = _XCALL(x_write_ovr_save)(fd, str1, len1, &result);
		ret = _XCALL(x_write_ovr_save)(fd, str2, len2, &result);
		XACT_WAIVER {
			UT_ASSERT_EQUAL(txd, txc_sentinel_owner(FD2SENTINEL(fd)));
			UT_ASSERT_NOTEQUAL(TXC_R_FAILURE, SENTINEL_ENLISTED(txd, FD2SENTINEL(fd)));
			UT_ASSERT_EQUAL(UT_TRUE, file_equal_str(test_file, final_contents));
			if (test_retries++ <= 8) {
				XACT_ABORT(TXC_ABORTREASON_USERRETRY);	
			}
		}
	XACT_END(xact_1)
	UT_ASSERT_NOTEQUAL(txd, txc_sentinel_owner(FD2SENTINEL(fd)));
	UT_ASSERT_EQUAL(TXC_R_FAILURE, SENTINEL_ENLISTED(txd, FD2SENTINEL(fd)));
	UT_ASSERT_EQUAL(UT_TRUE, file_equal_str(test_file, final_contents));
}
UT_END_TEST


UT_START_TEST(test7)
{
	txc_tx_t     *txd;
	int          fd;
	int          result;
	int          ret;
	char         *initial_contents;
	char         *final_contents;
	volatile int test_retries;
	char         *str1 = "DEADBEEF----------------";
	char         *str2 = "MADCOW--------------";
	char         *str3 = "ZOMBIE";
	int          len1 = strlen(str1);
	int          len2 = strlen(str2);
	int          len3 = strlen(str3);


	UT_ASSERT_EQUAL(TXC_R_SUCCESS, _TXC_global_init());
	UT_ASSERT_EQUAL(TXC_R_SUCCESS, _TXC_thread_init());

	txd = txc_tx_get_txd();

	initial_contents = test_file_initial_contents_tbl[0];
	UT_ASSERT_EQUAL(0, create_file(test_file, initial_contents));
	UT_ASSERT_EQUAL(UT_TRUE, file_equal_str(test_file, initial_contents));
	final_contents = str_create(initial_contents);
	str_modify(&final_contents, 0, str1);
	str_modify(&final_contents, len1, str2);

	test_retries = 0;
	fd = _XCALL(x_open)(test_file, O_RDWR, S_IRUSR|S_IWUSR, NULL);
	XACT_BEGIN(xact_1)
		XACT_WAIVER {
			if (test_retries > 0) {
				UT_ASSERT_EQUAL(txd, txc_sentinel_owner(FD2SENTINEL(fd)));
				UT_ASSERT_NOTEQUAL(TXC_R_FAILURE, SENTINEL_ENLISTED(txd, FD2SENTINEL(fd)));
				UT_ASSERT_EQUAL(UT_TRUE, file_equal_str(test_file, initial_contents));
			}	
		}	
		ret = _XCALL(x_write_ovr_save)(fd, str1, len1, &result);
		ret = _XCALL(x_write_ovr_save)(fd, str2, len2, &result);
		XACT_WAIVER {
			UT_ASSERT_EQUAL(txd, txc_sentinel_owner(FD2SENTINEL(fd)));
			UT_ASSERT_NOTEQUAL(TXC_R_FAILURE, SENTINEL_ENLISTED(txd, FD2SENTINEL(fd)));
			UT_ASSERT_EQUAL(UT_TRUE, file_equal_str(test_file, final_contents));
		}
		XACT_ABORT(TXC_ABORTREASON_USERABORT);	
	XACT_END(xact_1)
	UT_ASSERT_NOTEQUAL(txd, txc_sentinel_owner(FD2SENTINEL(fd)));
	UT_ASSERT_EQUAL(TXC_R_FAILURE, SENTINEL_ENLISTED(txd, FD2SENTINEL(fd)));
	UT_ASSERT_EQUAL(UT_TRUE, file_equal_str(test_file, initial_contents));
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
	ut_suite_add_test(suite, "test6", test6);
	ut_suite_add_test(suite, "test7", test7);

	test_file_initial_contents = str_create2(test_file_initial_contents_tbl);

	ut_suite_run_all(suite);
}
