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
 * \file hash_table.h
 *
 * \brief Simple bucket hash table interface.
 */

#ifndef _TXC_HASH_H
#define _TXC_HASH_H

#include <misc/result.h>
#include <misc/generic_types.h>

#define HASH_FACTOR 4

/* Opaque structure used to represent hash table. */
typedef struct txc_hash_table_s txc_hash_table_t;

/* Opaque structure used to represent hash table iterator. */
typedef struct txc_hash_table_iter_s txc_hash_table_iter_t;

typedef unsigned int txc_hash_table_key_t;
typedef void *txc_hash_table_value_t;
typedef struct txc_hash_table_bucket_s txc_hash_table_bucket_t;

struct txc_hash_table_iter_s {
	txc_hash_table_t        *hash_table;
	unsigned int            index;
	txc_hash_table_bucket_t *bucket;
};


txc_result_t txc_hash_table_create(txc_hash_table_t**, unsigned int, txc_bool_t);
txc_result_t txc_hash_table_destroy(txc_hash_table_t**);
txc_result_t txc_hash_table_add(txc_hash_table_t*, txc_hash_table_key_t, txc_hash_table_value_t);
txc_result_t txc_hash_table_lookup(txc_hash_table_t*, txc_hash_table_key_t, txc_hash_table_value_t *);
txc_result_t txc_hash_table_remove(txc_hash_table_t*, txc_hash_table_key_t, txc_hash_table_value_t *);
void txc_hash_table_iter_init(txc_hash_table_t *hash_table, txc_hash_table_iter_t *iter);
txc_result_t txc_hash_table_iter_next(txc_hash_table_iter_t *iter, txc_hash_table_key_t *key, txc_hash_table_value_t *value);
void txc_hash_table_print(txc_hash_table_t *);
#endif
