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

/**
 * \file config.h
 *
 * \brief Runtime configuration.
 *
 * Defines the runtime parameters. Simply extend FOREACH_RUNTIME_CONFIG_OPTION to
 * add new runtime configuration parameters.
 */

#ifndef _TXC_CONFIG_H
#define _TXC_CONFIG_H

#include <misc/result.h>
#include <misc/generic_types.h>


/*
 ****************************************************************************
 ***                     RUNTIME DYNAMIC CONFIGURATION                    ***
 ****************************************************************************
 */


#define VALIDVAL0	{} 
#define VALIDVAL2(val1, val2)	{val1, val2} 

/** Runtime configuration parameters. */
#define FOREACH_RUNTIME_CONFIG_OPTION(ACTION)                                \
  ACTION(debug_all, boolean, txc_bool_t, char *, TXC_BOOL_FALSE,             \
         VALIDVAL2("enable", "disable"), 2)                                  \
  ACTION(debug_koa, boolean, txc_bool_t, char *, TXC_BOOL_FALSE,             \
         VALIDVAL2("enable", "disable"), 2)                                  \
  ACTION(debug_pool, boolean, txc_bool_t, char *, TXC_BOOL_FALSE,            \
         VALIDVAL2("enable", "disable"), 2)                                  \
  ACTION(debug_sentinel, boolean, txc_bool_t, char *, TXC_BOOL_FALSE,        \
         VALIDVAL2("enable", "disable"), 2)                                  \
  ACTION(debug_tx, boolean, txc_bool_t, char *, TXC_BOOL_FALSE,              \
         VALIDVAL2("enable", "disable"), 2)                                  \
  ACTION(debug_xcall, boolean, txc_bool_t, char *, TXC_BOOL_FALSE,           \
         VALIDVAL2("enable", "disable"), 2)                                  \
  ACTION(printconf, boolean, txc_bool_t, char *, TXC_BOOL_FALSE,             \
         VALIDVAL2("enable", "disable"), 2)                                  \
  ACTION(statistics, boolean, txc_bool_t, char *, TXC_BOOL_TRUE,             \
         VALIDVAL2("enable", "disable"), 2)                                  \
  ACTION(statistics_file, string, char *, char *, "txc.stats",               \
         VALIDVAL0, 0)                                                       \
  ACTION(sentinel_max_spin_retries, integer, int, int, 0,                    \
         VALIDVAL2(0, 1000), 2)                                              \
  ACTION(sentinel_max_backoff_time, integer, int, int, 0,                    \
         VALIDVAL2(0, 1000), 2)                                              


#define CONFIG_OPTION_ENTRY(name,                                            \
                            type_name,                                       \
                            store_type,                                      \
                            parse_type,                                      \
                            defvalue,                                        \
                            validvalues,                                     \
                            num_validvalues)                                 \
  store_type name;

typedef struct txc_runtime_settings_s txc_runtime_settings_t;

struct txc_runtime_settings_s {
	FOREACH_RUNTIME_CONFIG_OPTION(CONFIG_OPTION_ENTRY)  
};
 
extern txc_runtime_settings_t txc_runtime_settings;


/* Interface functions */
txc_result_t txc_config_init ();
txc_result_t txc_config_set_option(char *option, char *value);



/*
 ****************************************************************************
 ***                    BUILD TIME STATIC CONFIGURATION                   ***
 ****************************************************************************
 */

/** Maximum number of threads/descriptors. */
#define TXC_MAX_NUM_THREADS 32

/** Initial sizes of the per descriptor commit and undo action lists. */
#define TXC_ACTION_LIST_SIZE 32

/** Number of sentinels */
#define TXC_SENTINEL_NUM                    512	

/** Number of KOA objects */
#define TXC_KOA_NUM                         512

/** Size of the per thread stat hash table */
#define TXC_STATS_THREADSTAT_HASHTABLE_SIZE 512

/** Initial size of the per descriptor sentinel list. */
#define TXC_SENTINEL_LIST_SIZE              32

/** Maximum number of mapped file descriptors to KOA objects. */
#define TXC_KOA_MAP_SIZE                    1024

/** Size of the hash table used by the KOA cache. */
#define TXC_KOA_CACHE_HASHTBL_SIZE          256

/** Maximum number of file descriptors referencing a KOA */
#define TXC_MAX_NUM_FDREFS_PER_KOA          8

/** Maximum length of pathname */
#define TXC_MAX_LEN_PATHNAME                128


/** Commit and undo actions execution levels. */
#define TXC_TX_REGULAR_COMMIT_ACTION_ORDER  0
#define TXC_TX_REGULAR_UNDO_ACTION_ORDER    0
#define TXC_KOA_CREATE_COMMIT_ACTION_ORDER  0
#define TXC_KOA_CREATE_UNDO_ACTION_ORDER    0
#define TXC_KOA_DESTROY_COMMIT_ACTION_ORDER 1
#define TXC_KOA_DESTROY_UNDO_ACTION_ORDER   1
#define TXC_SENTINELMGR_COMMIT_ACTION_ORDER 2
#define TXC_SENTINELMGR_UNDO_ACTION_ORDER   2
#define TXC_STATSMGR_COMMIT_ACTION_ORDER    3
#define TXC_STATSMGR_UNDO_ACTION_ORDER      3

#endif /* _TXC_CONFIG_H */
