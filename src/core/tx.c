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
 * \file tx.c
 * 
 * \brief Generic transaction manager implementation.
 *
 * Abstracts the underlying transactional memory system and exposes a 
 * generic transactional memory interface to the rest of the library.
 */

#include <sys/time.h>
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <setjmp.h>
#include <assert.h>
#include <sched.h>
#include <misc/debug.h>
#include <misc/pool.h>
#include <misc/malloc.h>
#include <misc/mutex.h>
#include <core/config.h>
#include <core/sentinel.h>
#include <core/buffer.h>
#include <core/koa.h>
#include <core/fm.h>
#include <core/tx.h>
#include <core/txdesc.h>


static void tx_generic_undo_action(txc_tx_t *);
static void tx_generic_commit_action(txc_tx_t *);


__thread txc_tx_t *txc_l_txd;     /* Thread specific data stored in 
                                   * thread local storage (TLS) */


struct txc_tx_action_list_entry_s {
	txc_tx_function_t function;
	void *args;
	int *error_result;
	int order;
};


struct txc_tx_action_list_s {
	txc_tx_action_list_entry_t *entries;      /* Array of entries */
	unsigned int               num_entries;   /* Number of entries */
	unsigned int               size;          /* Size of array */
};

							  
struct txc_txmgr_s {
	txc_mutex_t     mutex;
	unsigned int    alloc_txd_num;
	txc_tx_t        *alloc_txd_list_head;
	txc_tx_t        *alloc_txd_list_tail;
	txc_pool_t      *pool_txd;
	txc_buffermgr_t *buffermgr;
	txc_statsmgr_t  *statsmgr;
};


/* Include TM system specific functions */
#include <tm/itm.h>
#include <tm/logtm.h>

txc_txmgr_t *txc_g_txmgr;

static inline 
txc_result_t
allocate_action_list_entries(txc_tx_action_list_t *la, int extend)
{
	if (extend) {
		/* Extend action list */
		la->size *= 2;
		if ((la->entries = 
		     (txc_tx_action_list_entry_t *) REALLOC (la->entries, 
		                                             la->size * sizeof(txc_tx_action_list_entry_t)))
		    == NULL)
		{
			return TXC_R_NOMEMORY;
		}
	} else {
		/* Allocate action list */
		if ((la->entries = 
		     (txc_tx_action_list_entry_t *) MALLOC (la->size * sizeof(txc_tx_action_list_entry_t)))
		    == NULL)
		{
			return TXC_R_NOMEMORY;
		}	
	}
	return TXC_R_SUCCESS;
}


static inline 
txc_result_t
deallocate_action_list_entries(txc_tx_action_list_t *la)
{
	FREE(la->entries);
	return TXC_R_SUCCESS;
}


txc_result_t
txc_txmgr_create(txc_txmgr_t **txmgrp, 
                 txc_buffermgr_t *buffermgr, 
                 txc_sentinelmgr_t *sentinelmgr,
                 txc_statsmgr_t *statsmgr)
{
	txc_result_t      result;
	txc_pool_object_t *pool_object;
	txc_tx_t          *txd;
	
	*txmgrp = (txc_txmgr_t *) MALLOC(sizeof(txc_txmgr_t));
	if (*txmgrp == NULL) {
		return TXC_R_NOMEMORY;
	}
	if ((result = txc_pool_create(&((*txmgrp)->pool_txd), 
	                              sizeof(txc_tx_t),
	                              TXC_MAX_NUM_THREADS, 
	                              NULL)) != TXC_R_SUCCESS) 
	{
		FREE(*txmgrp);
		return result;
	}

	/* 
	 * Allocating memory for logs and buffers should be done only once here.
	 * Then descriptors can be recycled by just re-initializing them.
	 */
	for (pool_object = txc_pool_object_first((*txmgrp)->pool_txd, 
	                                         TXC_POOL_OBJECT_ALLOCATED & 
	                                         TXC_POOL_OBJECT_FREE);
	     pool_object != NULL;
		 pool_object = txc_pool_object_next(pool_object))
	{	 
		txd = (txc_tx_t *) txc_pool_object_of(pool_object);
		txd->manager = *txmgrp;
		txc_sentinel_list_create(sentinelmgr, &(txd->sentinel_list));
		txc_sentinel_list_create(sentinelmgr, &(txd->sentinel_list_preacquire));
		txd->commit_action_list = (txc_tx_commit_action_list_t *) MALLOC(sizeof(txc_tx_commit_action_list_t));
		txd->undo_action_list = (txc_tx_undo_action_list_t *) MALLOC(sizeof(txc_tx_undo_action_list_t));
		txd->commit_action_list->size = TXC_ACTION_LIST_SIZE;  
		txd->undo_action_list->size = TXC_ACTION_LIST_SIZE; 
		allocate_action_list_entries(txd->commit_action_list, 0);
		allocate_action_list_entries(txd->undo_action_list, 0);
		txc_buffer_linear_create(buffermgr, &(txd->buffer_linear));
	}

	(*txmgrp)->alloc_txd_num = 0;
	(*txmgrp)->alloc_txd_list_head = (*txmgrp)->alloc_txd_list_tail = NULL;
	(*txmgrp)->buffermgr = buffermgr;
	(*txmgrp)->statsmgr = statsmgr;
	TXC_MUTEX_INIT(&(*txmgrp)->mutex, NULL);

	return TXC_R_SUCCESS;
}


txc_result_t
txc_txmgr_destroy(txc_txmgr_t **txmgrp)
{
	txc_tx_t          *txd;
	txc_pool_object_t *pool_object;

	for (pool_object = txc_pool_object_first((*txmgrp)->pool_txd, 
	                                         TXC_POOL_OBJECT_ALLOCATED & 
	                                         TXC_POOL_OBJECT_FREE);
	     pool_object != NULL;
		 pool_object = txc_pool_object_next(pool_object))
	{	 
		txd = (txc_tx_t *) txc_pool_object_of(pool_object);
		txd->manager = NULL;
		deallocate_action_list_entries(txd->commit_action_list);
		deallocate_action_list_entries(txd->undo_action_list);
		txc_sentinel_list_destroy(&(txd->sentinel_list));
		txc_sentinel_list_destroy(&(txd->sentinel_list_preacquire));
		txc_buffer_linear_destroy(&(txd->buffer_linear));
	}

	txc_pool_destroy(&(*txmgrp)->pool_txd);
	FREE(*txmgrp);
	*txmgrp = NULL;

	return TXC_R_SUCCESS;
}


static inline
txc_result_t
txc_tx_init(txc_tx_t *txd)
{
	txd->sentinelmgr_commit_action_registered = 0;
	txd->sentinelmgr_undo_action_registered = 0;
	txd->statsmgr_commit_action_registered = 0;
	txd->statsmgr_undo_action_registered = 0;
	txd->generic_undo_action_registered = 0;
	txd->generic_commit_action_registered = 0;
	txd->abort_reason = TXC_ABORTREASON_TMCONFLICT;
	txd->commit_action_list->num_entries = 0;
	txd->undo_action_list->num_entries = 0;
	txc_buffer_linear_init(txd->buffer_linear);

	return TXC_R_SUCCESS;
}


txc_result_t
txc_tx_create(txc_txmgr_t *txmgr, txc_tx_t **txdp)
{
	txc_tx_t     *txd;
	txc_result_t result;

	if ((result = txc_pool_object_alloc(txmgr->pool_txd,(void **) &txd, 1)) 
	    != TXC_R_SUCCESS) 
	{
		TXC_INTERNALERROR("Could not create TX object\n");
		return result;
	}

	TXC_MUTEX_LOCK(&(txmgr->mutex));
	if (txmgr->alloc_txd_list_head == NULL) {
		TXC_ASSERT(txmgr->alloc_txd_list_tail == NULL);
		txmgr->alloc_txd_list_head = txd;
		txmgr->alloc_txd_list_tail = txd;
		txd->next = NULL;
		txd->prev = NULL;
	} else {
		txmgr->alloc_txd_list_tail->next = txd;
		txd->prev = txmgr->alloc_txd_list_tail;
		txmgr->alloc_txd_list_tail = txd;
	}
	txmgr->alloc_txd_num++;
	TXC_MUTEX_UNLOCK(&(txmgr->mutex));

	txd->tid_pthread = pthread_self();
	txd->forced_retries = 0;
	txc_tx_init(txd);
	txc_sentinel_list_init(txd->sentinel_list);
	txc_sentinel_list_init(txd->sentinel_list_preacquire);

	/* TM system specific initialization */
	tmsystem_tx_init(txd);
	
	txc_fm_init(txd);

#ifdef _TXC_STATS_BUILD	
	if (txc_runtime_settings.statistics == TXC_BOOL_TRUE) {
		txc_stats_threadstat_create(txd->manager->statsmgr, &(txd->threadstat), txd);
		txc_stats_txstat_create(&(txd->txstat));
	}	
#endif		

	*txdp = txd;

	return TXC_R_SUCCESS;
}


txc_result_t
txc_tx_destroy(txc_tx_t **txdp)
{
	txc_tx_t    *txd;
	txc_txmgr_t *txmgr;

	txd = *txdp;
	txmgr = txd->manager;

	TXC_MUTEX_LOCK(&(txmgr->mutex));
	TXC_ASSERT(txmgr->alloc_txd_list_head != NULL); 
	TXC_ASSERT(txmgr->alloc_txd_list_tail != NULL); 

	if (txmgr->alloc_txd_list_head == txd) {
		txmgr->alloc_txd_list_head = txd->next;
	}
	if (txmgr->alloc_txd_list_tail == txd)	{
		txmgr->alloc_txd_list_tail = txd->prev;
	}
	if (txd->prev) {
		txd->prev->next = txd->next;	
	}
	if (txd->next) {
		txd->next->prev = txd->prev;
	}	

	txmgr->alloc_txd_num--;
	TXC_MUTEX_UNLOCK(&(txmgr->mutex));

	txc_pool_object_free(txmgr->pool_txd, (void **) (&txd), 1);
	*txdp = NULL;

	return TXC_R_SUCCESS;
}


void                                                                          
txc_tx_abort_transaction(txc_tx_t *txd, txc_tx_abortreason_t abort_reason)
{
	TXC_DEBUG_PRINT(TXC_DEBUG_TX, "ABORT\n");
	tmsystem_abort_transaction(txd, abort_reason);
}


static inline txc_result_t
register_action(txc_tx_action_list_t *la, 
                txc_tx_function_t function, void *args, int *error_result, int order) 
{
	txc_result_t ret;

	if (la->num_entries == la->size) {
		if ((ret = allocate_action_list_entries(la, 1)) != TXC_R_SUCCESS) {
			return ret;
		}	
	}

	la->entries[la->num_entries].function = function;
	la->entries[la->num_entries].args = args;
	la->entries[la->num_entries].error_result = error_result;
	la->entries[la->num_entries].order = order;
	la->num_entries++; 

	return TXC_R_SUCCESS;

}


txc_result_t
txc_tx_register_commit_action(txc_tx_t *txd, 
                              txc_tx_commit_function_t function, 
                              void *args, int *error_result, int order) 
{
	txc_result_t                ret;
	txc_tx_commit_action_list_t *la;

	la = txd->commit_action_list;
	if ((ret = register_action(la, function, args, error_result, order)) 
	    != TXC_R_SUCCESS) 
	{
		return ret;
	}
	tmsystem_register_generic_commit_action(txd);

	return TXC_R_SUCCESS;
}


txc_result_t
txc_tx_register_undo_action(txc_tx_t *txd, 
                            txc_tx_undo_function_t function, 
                            void *args, int *error_result, int order) 
{
	txc_result_t              ret;
	txc_tx_undo_action_list_t *la;

	la = txd->undo_action_list;
	if ((ret = register_action(la, function, args, error_result, order)) 
	    != TXC_R_SUCCESS) 
	{
		return ret;
	}

	tmsystem_register_generic_undo_action(txd);

	return TXC_R_SUCCESS;
}


static
void
tx_generic_undo_action(txc_tx_t *txd)
{
	txc_tx_undo_action_list_entry_t *entry;
	int                             i;
	int                             order;
	int                             num_actions_executed;
	int                             temp_error_result;
	int                             first_error_result = 0;

	for (order = 0, num_actions_executed = 0; 
	     num_actions_executed != txd->undo_action_list->num_entries; 
	     order++) 
	{
		for (i = txd->undo_action_list->num_entries-1; i >= 0; i--) {
			entry = &txd->undo_action_list->entries[i];
			if (entry->order == order) {
				/* 
				 * It is possible entry->error_result be NULL because the caller
				 * might have not passed any result variable. Thus, if we simply
				 * pass entry->error_result we might not get informed about any 
				 * error happpened. Temporarily save any error of the action in 
				 * temp_error_result to be able to correctly take any actions 
				 * needed in case of failure (e.g. informing the failure manager
				 */
				temp_error_result = 0; 
				entry->function(entry->args, &temp_error_result);
				if (entry->error_result) {
					*(entry->error_result) = temp_error_result;
				}
				if (first_error_result == 0) {
					first_error_result = temp_error_result;
				}
				num_actions_executed++;
			}	
		}
	}
	txc_tx_init(txd);
	txc_fm_handle_undo_failure(txd, first_error_result);
}


static
void
tx_generic_commit_action(txc_tx_t *txd)
{
	txc_tx_commit_action_list_entry_t *entry;
	int                               i;
	int                               order;
	int                               num_actions_executed;
	int                               temp_error_result;
	int                               first_error_result = 0;

	for (order = 0, num_actions_executed = 0; 
	     num_actions_executed != txd->commit_action_list->num_entries; 
	     order++) 
	{
		for (i = 0; i < txd->commit_action_list->num_entries; i++) {
			entry = &txd->commit_action_list->entries[i];
			if (entry->order == order) {
				/* 
				 * It is possible entry->error_result be NULL because the caller
				 * might have not passed any result variable. Thus, if we simply
				 * pass entry->error_result we might not get informed about any 
				 * error happpened. Temporarily save any error of the action in 
				 * temp_error_result to be able to correctly take any actions 
				 * needed in case of failure (e.g. informing the failure manager
				 */
				temp_error_result = 0; 
				entry->function(entry->args, &temp_error_result);
				if (entry->error_result) {
					*(entry->error_result) = temp_error_result;
				}
				if (first_error_result == 0) {
					first_error_result = temp_error_result;
				}
				num_actions_executed++;
			}	
		}
	}	
	txc_tx_init(txd);
	txd->forced_retries = 0;
	txc_fm_handle_commit_failure(txd, first_error_result);
}


txc_tx_t *
txc_tx_get_txd()
{
	return txc_l_txd;
}


unsigned int
txc_tx_get_tid(txc_tx_t *txd)
{
	if (txd != NULL) {
		return txd->tid;
	} else {	
		return 0;
	}	
}


unsigned int
txc_tx_get_tid_pthread(txc_tx_t *txd)
{
	if (txd != NULL) {
		return txd->tid_pthread;
	} else {	
		return 0;
	}	
}


txc_tx_xactstate_t
txc_tx_get_xactstate(txc_tx_t *txd) 
{
	if (txd != NULL) {
		return tmsystem_get_xactstate(txd);
	} else {	
		return TXC_XACTSTATE_NONTRANSACTIONAL;
	}	
}   


unsigned int
txc_tx_get_forced_retries(txc_tx_t *txd)
{
	return txd->forced_retries;
}


static
void
tx_pre_outerbegin(txc_tx_t * txd, const char *srcloc_str, txc_tx_srcloc_t *srcloc)
{
	txc_tx_init(txd);
	txc_fm_init(txd);
#ifdef _TXC_STATS_BUILD	
	if (txc_runtime_settings.statistics == TXC_BOOL_TRUE) {
		txc_stats_txstat_init(txd->txstat, srcloc_str, srcloc);
	}	
#endif	
	txd->forced_retries = 0;
}


static
void
tx_pre_innerbegin(txc_tx_t * txd, const char *srcloc_str, txc_tx_srcloc_t *srcloc)
{
	/* do nothing */
}


void
txc_tx_pre_begin(txc_tx_t * txd, const char *srcloc_str, txc_tx_srcloc_t *srcloc)
{
	if (txc_tx_get_xactstate(txd) == TXC_XACTSTATE_NONTRANSACTIONAL) {
		tx_pre_outerbegin(txd, srcloc_str, srcloc);
	} else {
		tx_pre_innerbegin(txd, srcloc_str, srcloc);
	}
}


void
txc_tx_post_begin(txc_tx_t * txd)
{
	txc_sentinel_transaction_postbegin(txd);
#ifdef _TXC_STATS_BUILD	
	if (txc_runtime_settings.statistics == TXC_BOOL_TRUE) {
		txc_stats_transaction_postbegin(txd);
	}	
#endif	
	txc_fm_transaction_postbegin(txd);
}


/*
 **************************************************************************
 ***                     DEBUGGING SUPPORT ROUTINES                     ***
 **************************************************************************
 */

void 
txc_txmgr_print(txc_txmgr_t *txmgr)
{
	txc_tx_t *txd;

	TXC_MUTEX_LOCK(&(txmgr->mutex));
	fprintf(TXC_DEBUG_OUT, "TRANSACTION MANAGER: %p\n", txmgr); 
	fprintf(TXC_DEBUG_OUT, "\talloc_txd_list_head =  %p\n", txmgr->alloc_txd_list_head); 
	fprintf(TXC_DEBUG_OUT, "\talloc_txd_list_tail =  %p\n", txmgr->alloc_txd_list_tail); 
	fprintf(TXC_DEBUG_OUT, "========================================\n"); 

	for (txd = txmgr->alloc_txd_list_head; 
	     txd != NULL; 
	     txd = txd->next)
	{
		fprintf(TXC_DEBUG_OUT, "Descriptor: %p\n", txd); 
		fprintf(TXC_DEBUG_OUT, "\ttid: %u\n", txd->tid);
		fprintf(TXC_DEBUG_OUT, "\ttid_pthread: 0x%x\n", txd->tid_pthread);
		fprintf(TXC_DEBUG_OUT, "\tnext: %p\n", txd->next);
		fprintf(TXC_DEBUG_OUT, "\tprev: %p\n", txd->prev);
	}

	fprintf(TXC_DEBUG_OUT, "\n");
	TXC_MUTEX_UNLOCK(&(txmgr->mutex));
}


txc_result_t 
txc_tx_exists(txc_txmgr_t *txmgr, txc_tx_t *txd)
{
	txc_tx_t     *iter_txd;
	txc_result_t result;

	TXC_MUTEX_LOCK(&(txmgr->mutex));
	for (iter_txd = txmgr->alloc_txd_list_head; 
	     iter_txd != NULL; 
	     iter_txd = iter_txd->next)
	{
		if (iter_txd == txd) {
			result = TXC_R_SUCCESS;
			goto done;
		}
	}

	result = TXC_R_NOTEXISTS;
done:
	TXC_MUTEX_UNLOCK(&(txmgr->mutex));
	return result;
}
