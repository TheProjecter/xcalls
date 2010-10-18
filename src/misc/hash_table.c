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
 * \file hash_table.c
 *
 * \brief Simple bucket hash table implementation.
 */

#include <misc/result.h>
#include <misc/malloc.h>
#include <misc/hash_table.h>
#include <misc/debug.h>
#include <misc/mutex.h>
#include <pthread.h>

typedef struct txc_hash_table_bucket_list_s txc_hash_table_bucket_list_t;

struct txc_hash_table_bucket_s {
	struct txc_hash_table_bucket_s *next;
	struct txc_hash_table_bucket_s *prev;
	txc_hash_table_key_t           key;
	txc_hash_table_value_t         value;
};

struct txc_hash_table_bucket_list_s {
	txc_mutex_t             mutex;
	txc_hash_table_bucket_t *head;
	txc_hash_table_bucket_t *free;
};

struct txc_hash_table_s {
	unsigned int                 tbl_size;
	txc_hash_table_bucket_list_t *tbl;
	txc_bool_t                   mtsafe;
};


txc_result_t 
txc_hash_table_create(txc_hash_table_t** hp, unsigned int table_size, txc_bool_t mtsafe)
{
	int i;

	*hp = (txc_hash_table_t *) MALLOC(sizeof(txc_hash_table_t));
	if (*hp == NULL) 
	{
		return TXC_R_NOMEMORY;
	}
	(*hp)->tbl = (txc_hash_table_bucket_list_t*) CALLOC (table_size,
	                                               sizeof(txc_hash_table_bucket_list_t));
	if ((*hp)->tbl == NULL) 
	{
		FREE(*hp);
		return TXC_R_NOMEMORY;
	}

	for (i=0; i<table_size; i++) 
	{
		TXC_MUTEX_INIT(&(*hp)->tbl[i].mutex, NULL);
		(*hp)->tbl[i].head = NULL;
		(*hp)->tbl[i].free = NULL;
	}
	(*hp)->tbl_size = table_size;
	(*hp)->mtsafe = mtsafe;

	return TXC_R_SUCCESS;
}


txc_result_t
txc_hash_table_destroy(txc_hash_table_t** hp)
{
	txc_hash_table_bucket_t *bucket;
	txc_hash_table_bucket_t *next_bucket;
	int               i;

	if (*hp == NULL) 
	{
		return TXC_R_SUCCESS;
	}
	for (i=0; i<(*hp)->tbl_size; i++) {
		for (bucket = (*hp)->tbl[i].head; bucket; bucket = next_bucket) {
			next_bucket = bucket->next;
			FREE(bucket);
		}
		for (bucket = (*hp)->tbl[i].free; bucket; bucket = next_bucket) {
			next_bucket = bucket->next;
			FREE(bucket);
		}
	}

	FREE((*hp)->tbl);
	FREE(*hp);
	
	return TXC_R_SUCCESS;
}


static void 
bucket_list_add(txc_hash_table_bucket_t **head, txc_hash_table_bucket_t *bucket)
{
	if (*head)
	{
		(*head)->prev = bucket;
	}
	bucket->next = *head;
	bucket->prev = NULL;
	(*head) = bucket;
}


static void 
bucket_list_remove(txc_hash_table_bucket_t **head, txc_hash_table_bucket_t *bucket)
{
	if (*head == bucket) 
	{
		*head = bucket->next;
		bucket->prev = NULL;
	} else {
		bucket->prev->next = bucket->next;
		if (bucket->next) 
		{
			bucket->next->prev = bucket->prev;
		}	
	}	
}


txc_result_t
txc_hash_table_add(txc_hash_table_t* h, 
                   txc_hash_table_key_t key, 
                   txc_hash_table_value_t value)
{
	txc_result_t                 result;
	txc_hash_table_bucket_t      *bucket;
	txc_hash_table_bucket_list_t *bucket_list = &h->tbl[key % h->tbl_size];

	if (h->mtsafe == TXC_BOOL_TRUE) 
	{
		TXC_MUTEX_LOCK(&(bucket_list->mutex));
	}

	for (bucket = bucket_list->head; 
	     bucket != NULL; 
	     bucket = bucket->next) 
	{
		if (bucket->key == key) 
		{
			result = TXC_R_EXISTS;
			goto done;
		}
	}
	if (bucket_list->free) 
	{
		bucket = bucket_list->free;
		bucket_list_remove(&bucket_list->free, bucket);
	} else {
		bucket = (txc_hash_table_bucket_t *) MALLOC(sizeof(txc_hash_table_bucket_t));
	}	
	bucket->key = key;
	bucket->value = value;
	bucket_list_add(&bucket_list->head, bucket);
	result = TXC_R_SUCCESS;

done:
	if (h->mtsafe == TXC_BOOL_TRUE) 
	{
		TXC_MUTEX_UNLOCK(&(bucket_list->mutex));
	}
	return result;
}


txc_result_t
txc_hash_table_lookup(txc_hash_table_t* h,
                      txc_hash_table_key_t key, 
                      txc_hash_table_value_t *value)
{
	txc_result_t                 result;
	txc_hash_table_bucket_t      *bucket;
	txc_hash_table_bucket_list_t *bucket_list;

	bucket_list = &h->tbl[key % h->tbl_size];

	if (h->mtsafe == TXC_BOOL_TRUE) 
	{
		TXC_MUTEX_LOCK(&(bucket_list->mutex));
	}

	for (bucket = bucket_list->head; 
	     bucket != NULL; 
	     bucket = bucket->next) 
	{
		if (bucket->key == key) 
		{
			if (value) 
			{
				*value = bucket->value;	
			}	
			result = TXC_R_SUCCESS;
			goto done;
		}
	}
	result = TXC_R_NOTEXISTS;

done:
	if (h->mtsafe == TXC_BOOL_TRUE) 
	{
		TXC_MUTEX_UNLOCK(&(bucket_list->mutex));
	}
	return result;

}


txc_result_t
txc_hash_table_remove(txc_hash_table_t* h,
                      txc_hash_table_key_t key, 
                      txc_hash_table_value_t *value)
{
	txc_result_t                 result;
	txc_hash_table_bucket_t      *bucket;
	txc_hash_table_bucket_list_t *bucket_list;

	bucket_list = &h->tbl[key % h->tbl_size];

	if (h->mtsafe == TXC_BOOL_TRUE) 
	{
		TXC_MUTEX_LOCK(&(bucket_list->mutex));
	}

	for (bucket = bucket_list->head; 
	     bucket != NULL; 
	     bucket = bucket->next) 
	{
		if (bucket->key == key) 
		{
			if (value) 
			{
				*value = bucket->value;	
			}	
			bucket_list_remove(&bucket_list->head, bucket);
			bucket_list_add(&bucket_list->free, bucket);
			result = TXC_R_SUCCESS;
			goto done;
		}
	}
	result = TXC_R_NOTEXISTS;
done:
	if (h->mtsafe == TXC_BOOL_TRUE) 
	{
		TXC_MUTEX_UNLOCK(&(bucket_list->mutex));
	}
	return result;
}

/* Iterator is not multithreaded safe */

void
txc_hash_table_iter_init(txc_hash_table_t *hash_table, txc_hash_table_iter_t *iter)
{
	unsigned int            i;
	txc_hash_table_bucket_t *bucket;

	iter->hash_table = hash_table;
	iter->index = 0;
	iter->bucket = NULL;

	for (i=0; i<hash_table->tbl_size; i++) {
		if (bucket = hash_table->tbl[i].head) {
			iter->bucket = bucket;
			iter->index = i;
			break;
		}
	}
}


txc_result_t
txc_hash_table_iter_next(txc_hash_table_iter_t *iter, 
                         txc_hash_table_key_t *key, 
                         txc_hash_table_value_t *value)
{
	unsigned int            i;
	txc_hash_table_bucket_t *bucket;
	txc_hash_table_t        *hash_table = iter->hash_table;

	if (iter->bucket == NULL) {
		*key = 0;
		*value = NULL;
		return TXC_R_NULLITER;
	} 

	*key = iter->bucket->key;
	*value = iter->bucket->value;

	if (iter->bucket->next) {
		iter->bucket = iter->bucket->next;
	} else {	
		iter->bucket = NULL;
		for (i=iter->index+1; i<hash_table->tbl_size; i++) {
			if (bucket = hash_table->tbl[i].head) {
				iter->bucket = bucket;
				iter->index = i;
				break;
			}
		}
	}
	return TXC_R_SUCCESS;

}


void 
txc_hash_table_print(txc_hash_table_t *h) {
	int i;
	txc_hash_table_bucket_list_t *bucket_list;
	txc_hash_table_bucket_t      *bucket;

	fprintf(TXC_DEBUG_OUT, "HASH TABLE: %p\n", h);
	for (i=0;i<h->tbl_size; i++)
	{
		bucket_list = &h->tbl[i];
		if (bucket = bucket_list->head) {
			fprintf(TXC_DEBUG_OUT, "[%d]: head", i);
			for (; bucket != NULL; bucket=bucket->next)
			{
				fprintf(TXC_DEBUG_OUT, " --> (%u, %p)", bucket->key, bucket->value);
			}
			fprintf(TXC_DEBUG_OUT, "\n");
		}
		if (bucket = bucket_list->free) {
			fprintf(TXC_DEBUG_OUT, "[%d]: free", i);
			for (; bucket != NULL; bucket=bucket->next)
			{
				fprintf(TXC_DEBUG_OUT, " --> (%u, %p)", bucket->key, bucket->value);
			}
			fprintf(TXC_DEBUG_OUT, "\n");
		}
	}
}
