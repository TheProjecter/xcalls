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

UT_BARRIER_T test1_barrier1;

UT_START_TEST_THREAD(test1_thread)
{
	txc_tx_t     *txd;
	int          i;
	int          fd;
	int          result;
	int          ret;
	char         *final_contents;
	volatile int test_retries;
	int          tid = (int) __arg;

	UT_ASSERT_EQUAL(TXC_R_SUCCESS, _TXC_thread_init());
	UT_BARRIER_WAIT(&test1_barrier1);

	txd = txc_tx_get_txd();

	UT_BARRIER_WAIT(&test1_barrier1);

	final_contents = str_create("");
	switch (tid) {
		case 0:
			str_modify(&final_contents, 0, "DEADBEEF");
			str_modify(&final_contents, strlen(final_contents), "MADCOW");
			str_modify(&final_contents, strlen(final_contents), "ZOMBIE");
			break;
		case 1:	
			str_modify(&final_contents, 0, "CYPRESS");
			str_modify(&final_contents, strlen(final_contents), "PALM");
			str_modify(&final_contents, strlen(final_contents), "PINE");
			break;
		case 2:	
			str_modify(&final_contents, 0, "SOCCER");
			str_modify(&final_contents, strlen(final_contents), "BASKETBALL");
			str_modify(&final_contents, strlen(final_contents), "TENNIS");
			break;
		case 3:	
			str_modify(&final_contents, 0, "BARCELONA");
			str_modify(&final_contents, strlen(final_contents), "PANATHINAIKOS");
			str_modify(&final_contents, strlen(final_contents), "A.C.MILAN");
			break;
		default:
	}	


	for (i=0; i<1024; i++) {
		test_retries = 0;
		XACT_BEGIN(xact_1)
			fd  = _XCALL(x_create)(test_file, S_IRUSR|S_IWUSR, NULL);
			switch (tid) {
				case 0:
					ret = _XCALL(x_write_ovr_save)(fd, "DEADBEEF", 8, &result);
					ret = _XCALL(x_write_ovr_save)(fd, "MADCOW", 6, &result);
					ret = _XCALL(x_write_ovr_save)(fd, "ZOMBIE", 6, &result);
					break;
				case 1:
					ret = _XCALL(x_write_ovr_save)(fd, "CYPRESS", 7, &result);
					ret = _XCALL(x_write_ovr_save)(fd, "PALM", 4, &result);
					ret = _XCALL(x_write_ovr_save)(fd, "PINE", 4, &result);
					break;
				case 2:
					ret = _XCALL(x_write_ovr_save)(fd, "SOCCER", 6, &result);
					ret = _XCALL(x_write_ovr_save)(fd, "BASKETBALL", 10, &result);
					ret = _XCALL(x_write_ovr_save)(fd, "TENNIS", 6, &result);
					break;
				case 3:
					ret = _XCALL(x_write_ovr_save)(fd, "BARCELONA", 9, &result);
					ret = _XCALL(x_write_ovr_save)(fd, "PANATHINAIKOS", 13, &result);
					ret = _XCALL(x_write_ovr_save)(fd, "A.C.MILAN", 9, &result);
					break;
				default:
			}	

			XACT_WAIVER {
				UT_ASSERT_EQUAL(txd, txc_sentinel_owner(FD2SENTINEL(fd)));
				UT_ASSERT_NOTEQUAL(TXC_R_FAILURE, SENTINEL_ENLISTED(txd, FD2SENTINEL(fd)));
				UT_ASSERT_EQUAL(UT_TRUE, file_equal_str(test_file, final_contents));
				if (UT_TRUE != file_equal_str(test_file, final_contents)) {
					print_file(test_file);
					fprintf(UT_PRINT_OUT, "\n%s\n", final_contents);
				}

			}
			_XCALL(x_close(fd, NULL));
		XACT_END(xact_1)
	}
}
UT_END_TEST_THREAD


UT_START_TEST(test1_2T)
{
	int i;
	txc_tx_t *txd;
	UT_THREAD_T thread[4];

	UT_ASSERT_EQUAL(TXC_R_SUCCESS, _TXC_global_init());

	UT_BARRIER_INIT(&test1_barrier1, 2);

	UT_ASSERT_EQUAL(0, create_file(test_file, test_file_initial_contents));
	UT_ASSERT_EQUAL(UT_TRUE, file_equal_str(test_file, test_file_initial_contents));

    UT_THREAD_CREATE (&thread[0], NULL, test1_thread, (void *) 0);
    UT_THREAD_CREATE (&thread[1], NULL, test1_thread, (void *) 1);

	UT_THREAD_JOIN(thread[0]);
	UT_THREAD_JOIN(thread[1]);
}
UT_END_TEST


UT_START_TEST(test1_4T)
{
	int i;
	txc_tx_t *txd;
	UT_THREAD_T thread[4];

	UT_ASSERT_EQUAL(TXC_R_SUCCESS, _TXC_global_init());

	UT_BARRIER_INIT(&test1_barrier1, 4);

	UT_ASSERT_EQUAL(0, create_file(test_file, test_file_initial_contents));
	UT_ASSERT_EQUAL(UT_TRUE, file_equal_str(test_file, test_file_initial_contents));

    UT_THREAD_CREATE (&thread[0], NULL, test1_thread, (void *) 0);
    UT_THREAD_CREATE (&thread[1], NULL, test1_thread, (void *) 1);
    UT_THREAD_CREATE (&thread[2], NULL, test1_thread, (void *) 2);
    UT_THREAD_CREATE (&thread[3], NULL, test1_thread, (void *) 3);

	UT_THREAD_JOIN(thread[0]);
	UT_THREAD_JOIN(thread[1]);
	UT_THREAD_JOIN(thread[2]);
	UT_THREAD_JOIN(thread[3]);
}
UT_END_TEST


UT_BARRIER_T test2_barrier1;

UT_START_TEST_THREAD(test2_thread)
{
	txc_tx_t     *txd;
	int          i;
	int          fd;
	int          result;
	int          ret;
	char         *final_contents;
	volatile int test_retries;
	int          tid = (int) __arg;

	UT_ASSERT_EQUAL(TXC_R_SUCCESS, _TXC_thread_init());
	UT_BARRIER_WAIT(&test2_barrier1);

	txd = txc_tx_get_txd();

	UT_BARRIER_WAIT(&test2_barrier1);

	final_contents = str_create("");
	switch (tid) {
		case 0:
			str_modify(&final_contents, 0, "DEADBEEF");
			str_modify(&final_contents, strlen(final_contents), "MADCOW");
			str_modify(&final_contents, strlen(final_contents), "ZOMBIE");
			break;
		case 1:	
			str_modify(&final_contents, 0, "CYPRESS");
			str_modify(&final_contents, strlen(final_contents), "PALM");
			str_modify(&final_contents, strlen(final_contents), "PINE");
			break;
		default:
	}	

	for (i=0; i<128; i++) {
		test_retries = 0;
		XACT_BEGIN(xact_1)
			fd  = _XCALL(x_create)(test_file, S_IRUSR|S_IWUSR, NULL);
			switch (tid) {
				case 0:
					ret = _XCALL(x_write_ovr_save)(fd, "DEADBEEF", 8, &result);
					ret = _XCALL(x_write_ovr_save)(fd, "MADCOW", 6, &result);
					ret = _XCALL(x_write_ovr_save)(fd, "ZOMBIE", 6, &result);
					break;
				case 1:
					ret = _XCALL(x_write_ovr_save)(fd, "CYPRESS", 7, &result);
					ret = _XCALL(x_write_ovr_save)(fd, "PALM", 4, &result);
					ret = _XCALL(x_write_ovr_save)(fd, "PINE", 4, &result);
					break;
				default:
			}	

			XACT_WAIVER {
				UT_ASSERT_EQUAL(txd, txc_sentinel_owner(FD2SENTINEL(fd)));
				UT_ASSERT_NOTEQUAL(TXC_R_FAILURE, SENTINEL_ENLISTED(txd, FD2SENTINEL(fd)));
				if (test_retries++ <= 8) {
					XACT_ABORT(TXC_ABORTREASON_USERRETRY);	
				}
				UT_ASSERT_EQUAL(UT_TRUE, file_equal_str(test_file, final_contents));
			}
			_XCALL(x_close(fd, NULL));
		XACT_END(xact_1)
	}
}
UT_END_TEST_THREAD



UT_START_TEST(test2)
{
	int i;
	txc_tx_t *txd;
	UT_THREAD_T thread[4];

	UT_ASSERT_EQUAL(TXC_R_SUCCESS, _TXC_global_init());

	UT_BARRIER_INIT(&test2_barrier1, 2);

	UT_ASSERT_EQUAL(0, create_file(test_file, test_file_initial_contents));
	UT_ASSERT_EQUAL(UT_TRUE, file_equal_str(test_file, test_file_initial_contents));

    UT_THREAD_CREATE (&thread[0], NULL, test2_thread, (void *) 0);
    UT_THREAD_CREATE (&thread[1], NULL, test2_thread, (void *) 1);

	UT_THREAD_JOIN(thread[0]);
	UT_THREAD_JOIN(thread[1]);
}
UT_END_TEST


int
main(int argc, char *argv[])
{
	ut_suite_t *suite;
	ut_suite_create(&suite, "test_x_create_write_multithread");
	ut_suite_add_test(suite, "test1_2T", test1_2T);
	ut_suite_add_test(suite, "test1_4T", test1_4T);
	ut_suite_add_test(suite, "test2", test2);

	test_file_initial_contents = str_create2(test_file_initial_contents_tbl);

	ut_suite_run_all(suite);
}
