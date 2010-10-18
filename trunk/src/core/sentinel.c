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
 * \file sentinel.c
 *
 * \brief Sentinel manager implementation.
 *
 * <b> Sentinels overview </b>
 *
 * Most transactional memory systems operate only at user level and do not 
 * isolate changes to kernel data made within a transaction. For example, a 
 * thread may view speculative data written to a file by a transaction that 
 * later aborts. Such transient effects do not occur with locks and may cause 
 * the program to behave incorrectly.
 * 
 * The xCall interface provides isolation for kernel objects with sentinels. 
 * A sentinel is a lightweight, revocable user-level lock for a kernel object. 
 * The purpose of a sentinel is to isolate the effects of system calls from 
 * other threads in the same process. Similar to a database lock, sentinels are
 * centrally managed to detect deadlocks and can be revoked to recover from 
 * deadlocks.
 * 
 * The xCall APIs associate sentinels with distinct kernel objects, such as 
 * sockets and file descriptors. Sentinels are released only when the 
 * transaction commits or aborts to implement two-phase locking. However, 
 * sentinels do not protect programs with inherent race conditions, such as 
 * when a non-transactional thread reads data from a file written by a
 * transaction.
 *
 * Before invoking a system call, an xCall acquires the sentinel that isolates 
 * the underlying kernel object accessed by the call. Sentinels only lock the 
 * logical state of the kernel that is visible through system calls. Internally, 
 * kernel queues and buffers may have different contents as long as system
 * calls do not observe a change.
 *
 * The sentinel implementation must detect deadlock when two transactions acquire 
 * the same set of sentinels concurrently in different orders. On deadlock, one 
 * transaction aborts and releases its sentinels
 *
 * <b> Implementation </b>
 *
 * The sentinel manager subsystem allocates and maps sentinels to logical kernel 
 * objects (KOA) in user mode. We implement process-wide sentinels with POSIX 
 * mutex locks.
 *
 * The manager maintains lists of the sentinels acquired by each 
 * transaction and releases them at commit or abort. The sentinel list is 
 * actually kept in the transaction's descriptor but managed by the sentinel
 * manager. The sentinel manager is informed about commit/abort/retry events
 * indirectly by registering commit/undo actions. This allows to incrementally
 * pay the cost when the transaction acquires any sentinels.
 *
 * To prevent deadlocks, the sentinel manager enforces a canonical global 
 * order over all sentinels based on the sentinel’s index in the global table 
 * of allocated sentinels. When a transaction cannot acquire a sentinel, the 
 * sentinel manager aborts the transaction and releases the acquired sentinels.
 * The transaction reacquires all the sentinels it encountered in canonical 
 * order before restarting. The transaction holds on to the sentinels until 
 * commit, even if it does not require the same sentinels when reexecuted.
 *
 * <em>Attach/Detach operations:</em> 
 *
 * Whenever creating and enlisting a sentinel,
 * we logically attach to the sentinel by calling <tt>sentinel_attach</tt>. 
 * Attaching to a sentinel increments the sentinel's reference counter by one. 
 * Whenever releasing a sentinel, we logically detach from the sentinel by
 * calling <tt>sentinel_detach</tt>. Detaching from a sentinel decrements the 
 * sentinel's reference counter by one. Attach and detach operations prevent 
 * someone from destroying the sentinel as long as there are transactions 
 * interested in the sentinel. Here are some races which are prevented through
 * this scheme: 
 * \li A transaction wants to destroy a KOA(and therefore destroy its sentinel 
 * as well) as part of a <tt>close</tt> operation. Other transactions might have tried 
 * to acquire it but failed to do so and enlisted the sentinel to acquire it 
 * after a restart. If the sentinel is gone then the transaction may fail acquiring
 * the sentinel after a restart.
 * \li A transaction creates a KOA, assigns a sentinel and later aborts.
 * When it aborts it detaches from the sentinel and destroys (because last) 
 * Another transaction sees the KOA, and tries to acquire the sentinel.
 *
 * Here is how our scheme prevents such races. A sentinel is only destroyed 
 * when its reference counter reaches to zero and we release the sentinel of 
 * a KOA we created after we destroy the KOA. Someone who sees the KOA may also 
 * attach to the sentinel if interested. This is done atomically. So when 
 * you release sentinels, and detach from them is guaranteed that someone 
 * interested in the sentinel would have a valid reference to it. Of course 
 * the sentinel at that time protects no object which is fine. What we really 
 * avoid is the sentinel to protect another KOA between the time we see a 
 * KOA and we acquire the sentinel.
 *
 */


#include <sys/types.h>
#include <sys/stat.h>
#include <pthread.h>
#include <unistd.h>
#include <stddef.h>
#include <misc/result.h>
#include <misc/pool.h>
#include <misc/malloc.h>
#include <misc/debug.h>
#include <misc/mutex.h>
#include <core/config.h>
#include <core/sentinel.h>
#include <core/tx.h>
#include <core/txdesc.h>

#define TXC_SENTINEL_NOOWNER                0x0

/** Sentinel. */
struct txc_sentinel_s {
	txc_mutex_t       sentinel_mutex;  /**< The actual lock backing the sentinel. */
	txc_mutex_t       synch_mutex;     /**< The latch that synchronizes metadata updates. */
	int               id;              /**< Sentinel identifier used to acquire sentinels in order to prevent deadlock. */ 
	int               refcnt;          /**< Reference counter counting entities logically attached to the sentinel. */
	txc_tx_t          *owner;          /**< Transaction holding the sentinel. */
	txc_sentinelmgr_t *manager;        /**< Sentinel manager responsible for the sentinel. */
};

/** Sentinel list entry. */
struct txc_sentinel_list_entry_s {
	txc_sentinel_t *sentinel;          /**< Sentinel */
	txc_result_t   status;             /**< TXC_SENTINEL_ACQUIRED, TXC_SENTINEL_ACQUIRED */
};

/** List of sentinels. */
struct txc_sentinel_list_s {
	txc_sentinel_list_entry_t *entries;      /**< Array of entries. */
	unsigned int              num_entries;   /**< Number of entries. */
	unsigned int              size;          /**< Size of array. */
	txc_sentinelmgr_t         *manager;      /**< Sentinel manager. */
};


/** Sentinel manager */
struct txc_sentinelmgr_s {
	txc_mutex_t mutex;
	txc_pool_t  *pool_sentinel;
};


static inline void sentinel_list_print(txc_sentinel_list_t *sentinel_list, char *heading);
static inline txc_result_t enlist_sentinel(txc_sentinel_list_t *sentinel_list, txc_sentinel_t *sentinel, txc_result_t status);
static inline void backoff(txc_tx_t *txd);


txc_sentinelmgr_t *txc_g_sentinelmgr;


/**
 * \brief Creates a sentinel manager 
 *
 * It preallocates a pool of sentinels to make sentinel allocation fast.
 *
 * \param[out] sentinelmgrp Pointer to the created sentinel manager.
 * \return Code indicating success or failure (reason) of the operation.
 */
txc_result_t
txc_sentinelmgr_create(txc_sentinelmgr_t **sentinelmgrp) 
{
	txc_result_t      result;
	txc_pool_object_t *pool_object;
	txc_sentinel_t    *sentinel;
	int               i;

	*sentinelmgrp = (txc_sentinelmgr_t *) MALLOC(sizeof(txc_sentinelmgr_t));
	if (*sentinelmgrp == NULL) {
		return TXC_R_NOMEMORY;
	}

	if ((result = txc_pool_create(&((*sentinelmgrp)->pool_sentinel), 
	                              sizeof(txc_sentinel_t),
	                              TXC_SENTINEL_NUM, 
                                  NULL)) != TXC_R_SUCCESS) 
	{
		FREE(*sentinelmgrp);
		return result;
	}
	TXC_MUTEX_INIT(&((*sentinelmgrp)->mutex), NULL);
	for (i = 0,
	     pool_object = txc_pool_object_first((*sentinelmgrp)->pool_sentinel,
	                                         TXC_POOL_OBJECT_ALLOCATED & 
	                                         TXC_POOL_OBJECT_FREE);
	     pool_object != NULL;
		 i++, pool_object = txc_pool_object_next(pool_object))
	{
		sentinel = (txc_sentinel_t *) txc_pool_object_of(pool_object);
		TXC_MUTEX_INIT(&sentinel->synch_mutex, NULL);
		TXC_MUTEX_INIT(&sentinel->sentinel_mutex, NULL);
		sentinel->id = i;
		sentinel->manager = *sentinelmgrp;
		sentinel->owner = TXC_SENTINEL_NOOWNER;  
	}
	
	return TXC_R_SUCCESS;
}


/**
 * \brief Destroys a sentinel manager 
 *
 * It deallocates all sentinels that it manages.
 *
 * \param[in,out] sentinelmgrp Poitner to the sentinel manager to be destroyed.
 */
void
txc_sentinelmgr_destroy(txc_sentinelmgr_t **sentinelmgrp)
{
	txc_pool_destroy(&((*sentinelmgrp)->pool_sentinel));
	FREE(*sentinelmgrp);
	*sentinelmgrp = NULL;
}


txc_result_t
txc_sentinel_create(txc_sentinelmgr_t *sentinelmgr, txc_sentinel_t **sentinelp) 
{
	txc_result_t   result;
	txc_sentinel_t *sentinel;

	if ((result = txc_pool_object_alloc(sentinelmgr->pool_sentinel, 
	                                    (void **) &sentinel, 1)) 
	    != TXC_R_SUCCESS) 
	{
		TXC_INTERNALERROR("Could not create sentinel object\n");
		return result;
	}

	sentinel->owner = TXC_SENTINEL_NOOWNER;
	sentinel->refcnt = 1;
	*sentinelp = sentinel;

	return TXC_R_SUCCESS;
}


static inline
void 
sentinel_destroy(txc_sentinel_t *sentinel) 
{
	txc_sentinel_t **sentinelp = &sentinel;

	txc_pool_object_free(sentinel->manager->pool_sentinel, 
	                     (void **) sentinelp, 1);
}

/**
 * \brief Destroys a sentinel.
 * 
 * \param[in] sentinel The sentinel to destroy (deallocate).
 */
void 
txc_sentinel_destroy(txc_sentinel_t *sentinel) 
{
	TXC_ASSERT(sentinel != NULL);
	TXC_MUTEX_LOCK(&sentinel->synch_mutex);
	sentinel_destroy(sentinel);
	TXC_MUTEX_UNLOCK(&sentinel->synch_mutex);
}


static 
void
sentinel_attach(txc_sentinel_t *sentinel)
{
	sentinel->refcnt++;
}


/*
 * Assumes lock sentinel->synch_mutex is held by the caller.
 */
static
txc_result_t 
sentinel_detach(txc_sentinel_t *sentinel)
{
	if (--sentinel->refcnt==0) {
		sentinel_destroy(sentinel);	
	}
	return TXC_R_SUCCESS;
}


/**
 * \brief Detaches from a sentinel.
 * 
 * It logically detaches from the sentinel by decrementing the sentinel's 
 * refererence counter. If reference counter becomes zero then sentinel is
 * destroyed. 
 * 
 * \param [in] sentinel The sentinel to detach from.
 * \return Code indicating success or failure (reason) of the operation.
 */
txc_result_t
txc_sentinel_detach(txc_sentinel_t *sentinel)
{
	txc_result_t ret;

	TXC_MUTEX_LOCK(&sentinel->synch_mutex);
	ret = sentinel_detach(sentinel);
	TXC_MUTEX_UNLOCK(&sentinel->synch_mutex);
	return ret;
}


/** 
 * Acquires all the sentinels enlisted in a sentinel list.
 *
 * This is a blocking operation so it should not be called from a 
 * non-blocking context (e.g. transaction).
 * Sentinels should be sorted by sentinel identifier to prevent deadlocks.
 *
 * \param[in] txd Transaction descriptor. 
 * \param[in] list_acquire The sentinel list enlisting the sentinels to acquire.
 */
static inline
void
sentinel_list_acquire(txc_tx_t *txd, txc_sentinel_list_t *list_acquire)
{
	txc_sentinel_list_entry_t *entry;
	int                       i;

	TXC_DEBUG_PRINT(TXC_DEBUG_SENTINEL, "SENTINEL LIST: ACQUIRE SENTINELS\n");
	sentinel_list_print(list_acquire, "SENTINEL_LIST_ACQUIRE");
	for (i=0; i<list_acquire->num_entries; i++) {
		entry = &list_acquire->entries[i];
		TXC_DEBUG_PRINT(TXC_DEBUG_SENTINEL, 
		                "ACQUIRE SENTINEL %3d: MAY BLOCK\n",
		                entry->sentinel->id);
		TXC_MUTEX_LOCK(&(entry->sentinel->sentinel_mutex));
		entry->sentinel->owner = txd;
		enlist_sentinel(txd->sentinel_list, entry->sentinel, 
		                TXC_SENTINEL_ACQUIRED | 
						TXC_SENTINEL_ACQUIREONRETRY);

		TXC_DEBUG_PRINT(TXC_DEBUG_SENTINEL, 
		                "ACQUIRE SENTINEL %3d: SUCCESS\n",
		                entry->sentinel->id);
	}
}


/**
 * \brief Release sentinels enlisted in a sentinel list.
 *
 * \param[in] sentinel_list The sentinel list enlisting the sentinels to release.
 * \param[in] transaction_completion Whether the transaction completes execution.
 *            Transaction completes when it commits or aborts. Restart (retry) does
 *            not complete a transaction.
 *
 */
static
void
sentinel_list_release(txc_sentinel_list_t *sentinel_list, int transaction_completion)
{
	txc_sentinel_list_entry_t *entry;
	int                       i;

	TXC_DEBUG_PRINT(TXC_DEBUG_SENTINEL, "SENTINEL LIST: RELEASE SENTINELS\n");
	for (i=0; i<sentinel_list->num_entries; i++) {
		entry = &sentinel_list->entries[i];
		TXC_MUTEX_LOCK(&(entry->sentinel->synch_mutex));
		if (entry->status & TXC_SENTINEL_ACQUIRED) {
			entry->sentinel->owner = TXC_SENTINEL_NOOWNER;
			TXC_MUTEX_UNLOCK(&(entry->sentinel->sentinel_mutex));
			TXC_DEBUG_PRINT(TXC_DEBUG_SENTINEL, 
			                "RELEASE SENTINEL %3d: SUCCESS\n",
			                entry->sentinel->id);
		}
		/**
		 * If transaction completes then we need to detach from all sentinels.
		 * Otherwise we detach just from the sentinels we won't preacquire on
		 * restart.
		 */
		if (transaction_completion) {
			/* 
		 	 * sentinel_detach will destroy the sentinel if after the
		 	 * detach operation (sentinel->refcnt == 0) 
		 	 */
			sentinel_detach(entry->sentinel);
		} else {
			if (!(entry->status & TXC_SENTINEL_ACQUIREONRETRY)) {
				sentinel_detach(entry->sentinel);
			}
		}
		TXC_MUTEX_UNLOCK(&(entry->sentinel->synch_mutex));
	}
}


static 
void
sentinel_list_sort(txc_sentinel_list_t *sentinel_list) 
{
	int                       i;
	int                       n = sentinel_list->num_entries;
	txc_sentinel_list_entry_t entry_temp;
	unsigned int              swapped;

	TXC_DEBUG_PRINT(TXC_DEBUG_SENTINEL, "SENTINEL LIST: SORT\n");
	do {
		swapped = 0;
		for (i = 0; i<n-1; i++) {
			if (sentinel_list->entries[i].sentinel->id >
			    sentinel_list->entries[i+1].sentinel->id) 
			{
				entry_temp = sentinel_list->entries[i+1];
				sentinel_list->entries[i+1] = sentinel_list->entries[i];
				sentinel_list->entries[i] = entry_temp;
				swapped = 1;
			}
		}
		n = n-1;
	} while (swapped);
}



static inline
void
sentinelmgr_transaction_on_complete(txc_tx_t *txd)
{
#ifdef _TXC_DEBUG_BUILD
	sentinel_list_print(txd->sentinel_list, "TRANSACTION ON COMPLETE");
	sentinel_list_print(txd->sentinel_list, "SENTINEL LIST (MAIN LIST)");
#endif
	sentinel_list_release(txd->sentinel_list, 1);
	txc_sentinel_list_init(txd->sentinel_list);
	txd->sentinelmgr_undo_action_registered = 0;							  
	txd->sentinelmgr_commit_action_registered = 0;							  
}


static 
void
sentinelmgr_transaction_before_retry(txc_tx_t *txd)
{
	int                       i;
	txc_sentinel_list_entry_t *entry;

#ifdef _TXC_DEBUG_BUILD
	sentinel_list_print(txd->sentinel_list, "TRANSACTION BEFORE RETRY");
	sentinel_list_print(txd->sentinel_list, "SENTINEL_LIST (MAIN LIST)");
#endif
	txd->sentinel_list_preacquire->num_entries = 0;
	for (i=0; i<txd->sentinel_list->num_entries; i++) {
		entry = &txd->sentinel_list->entries[i];
		if (entry->status & TXC_SENTINEL_ACQUIREONRETRY) {
			enlist_sentinel(txd->sentinel_list_preacquire, 
			                entry->sentinel, 
			                0 & ~TXC_SENTINEL_ACQUIRED);
		}
	}
	sentinel_list_sort(txd->sentinel_list_preacquire);
	/* 
	 * Ask sentinel_list_release to release all the sentinels but only drop 
	 * the references to sentinels that we won't reacquire after transaction 
	 * restarts. This is because we continue logically referencing sentinels 
	 * we'll acquire after transaction restarts. 
	 */
	 //FIXME: Intel Prototype 3.0 seems to behave differently from Intel STM 2.0
	 //when aborting a transaction: Undo actions are executed with isolatil held???
	 //this leads to a deadlock where an aborted transaction waits for a sentinel held by
	 //a transaction waiting on me???
	sentinel_list_release(txd->sentinel_list, 0);
	#error
	//FIXME: this is a dirty quick fix: sentinel_list_release(txd->sentinel_list, 1);
#ifdef _TXC_DEBUG_BUILD
	sentinel_list_print(txd->sentinel_list_preacquire, 
	                    "SENTINEL LIST (PREACQUIRE LIST)");
#endif	
	txc_sentinel_list_init(txd->sentinel_list);
	txd->sentinelmgr_undo_action_registered = 0;							  
	txd->sentinelmgr_commit_action_registered = 0;
	backoff(txd);
	sentinel_list_acquire(txd, txd->sentinel_list_preacquire);
}


static
void 
sentinelmgr_commit_action(void *args, int *result)
{
	txc_tx_t *txd = (txc_tx_t *) args;
	sentinelmgr_transaction_on_complete(txd);

	if (result) {
		*result = 0;
	}
}


static 
void 
sentinelmgr_undo_action(void *args, int *result)
{
	txc_tx_t *txd = (txc_tx_t *) args;

	switch (txd->abort_reason) {
		case TXC_ABORTREASON_USERABORT:
			sentinelmgr_transaction_on_complete(txd);
			break;
		case TXC_ABORTREASON_TMCONFLICT:
		case TXC_ABORTREASON_USERRETRY:
		case TXC_ABORTREASON_INCONSISTENCY:
		case TXC_ABORTREASON_BUSYSENTINEL:
		case TXC_ABORTREASON_BUSYTXLOCK:
			sentinelmgr_transaction_before_retry(txd);
			break;
		default:
			TXC_INTERNALERROR("Unknown abort reason\n");
			/* never returns here */
	}

	if (result) {
		*result = 0;
	}
}


static inline
void
register_sentinelmgr_commit_action(txc_tx_t *txd)
{
	if (txd->sentinelmgr_commit_action_registered == 0) {
		txc_tx_register_commit_action(txd, 
		                              sentinelmgr_commit_action, 
		                              (void *) txd,
		                              NULL,
	                                  TXC_SENTINELMGR_COMMIT_ACTION_ORDER);
		txd->sentinelmgr_commit_action_registered = 1;							  
	}
}


static inline
void
register_sentinelmgr_undo_action(txc_tx_t *txd)
{
	if (txd->sentinelmgr_undo_action_registered == 0) {
		txc_tx_register_undo_action(txd, 
		                              sentinelmgr_undo_action,
		                              (void *) txd,
		                              NULL,
	                                  TXC_SENTINELMGR_UNDO_ACTION_ORDER);
		txd->sentinelmgr_undo_action_registered = 1;							  
	}
}


void
txc_sentinel_register_sentinelmgr_commit_action(txc_tx_t *txd)
{
	register_sentinelmgr_commit_action(txd);
}


void
txc_sentinel_register_sentinelmgr_undo_action(txc_tx_t *txd)
{
	register_sentinelmgr_undo_action(txd);
}


void
txc_sentinel_transaction_postbegin(txc_tx_t *txd)
{
	/* 
	 * If we have preacquired sentinels then we need to register commit and
	 * undo actions so that we release the sentinels when the transaction
	 * commits/aborts/retries.
	 */
	if (txd->sentinel_list->num_entries) { 	
		register_sentinelmgr_commit_action(txd);
		register_sentinelmgr_undo_action(txd);
	}	
}


static inline
txc_result_t
allocate_sentinel_list_entries(txc_sentinel_list_t *la, int extend)
{
	if (extend) {
		/* Extend action list */
		la->size *= 2;
		if ((la->entries = 
		     (txc_sentinel_list_entry_t *) REALLOC (la->entries, 
		                                             la->size * sizeof(txc_sentinel_list_entry_t)))
		    == NULL)
		{
			return TXC_R_NOMEMORY;
		}
	} else {
		/* Allocate action list */
		if ((la->entries = 
		     (txc_sentinel_list_entry_t *) MALLOC (la->size * sizeof(txc_sentinel_list_entry_t)))
		    == NULL)
		{
			return TXC_R_NOMEMORY;
		}	
	}
	return TXC_R_SUCCESS;
}


static inline
txc_result_t
deallocate_sentinel_list_entries(txc_sentinel_list_t *la)
{
	FREE(la->entries);
	return TXC_R_SUCCESS;
}


/**
 * \brief Create a sentinel list.
 *
 * \param[in] sentinelmgr Sentinel manager responsible for the sentinel list.
 * \param[in,out] sentinel_list Pointer to the created sentinel list. 
 * \return Code indicating success or failure (reason) of the operation.
 *
 */
txc_result_t
txc_sentinel_list_create(txc_sentinelmgr_t *sentinelmgr, 
                         txc_sentinel_list_t **sentinel_list)
{
	txc_result_t result;

	if ((*sentinel_list = (txc_sentinel_list_t *) MALLOC(sizeof(txc_sentinel_list_t)))
	    == NULL) 
	{
		return TXC_R_NOMEMORY;
	}

	(*sentinel_list)->manager = sentinelmgr;
	(*sentinel_list)->num_entries = 0;
	(*sentinel_list)->size = TXC_SENTINEL_LIST_SIZE;

	if ((result = allocate_sentinel_list_entries(*sentinel_list, 0)) 
	    != TXC_R_SUCCESS) 
	{
		FREE(*sentinel_list);
		return result;
	}

	return TXC_R_SUCCESS;
}

/**
 * \brief Initialize a sentinel list.
 *
 * \param[in] sentinel_list Sentinel list to deallocate. 
 * \return Code indicating success or failure (reason) of the operation.
 *
 */
txc_result_t
txc_sentinel_list_init(txc_sentinel_list_t *sentinel_list)
{
	sentinel_list->num_entries = 0;

	return TXC_R_SUCCESS;
}

/**
 * \brief Destroy a sentinel list.
 *
 * Deallocate sentinel list entries and then deallocates the list itself 
 * 
 * \param[in, out] sentinel_list Pointer to the sentinel list to deallocate. 
 *                 Pointer will point to NULL after return.
 * \return Code indicating success or failure (reason) of the operation.
 *
 */
txc_result_t
txc_sentinel_list_destroy(txc_sentinel_list_t **sentinel_list) 
{
	txc_result_t result;

	if ((result = deallocate_sentinel_list_entries(*sentinel_list))
	    != TXC_R_SUCCESS)
	{
		return result;
	}	
	FREE(*sentinel_list);
	*sentinel_list = NULL;
	return TXC_R_SUCCESS;
}


/** 
 * \brief Enlist the sentinel in the transaction's sentinel list.
 *
 * It does not attach to the sentinel.
 *
 * \param[in] txd Transactional descriptor.
 * \param[in] sentinel Sentinel to enlist.
 * \param[in] status TXC_SENTINEL_ACQUIREONRETRY 
 * \return Code indicating success or failure (reason) of the operation.
 */
static inline 
txc_result_t
enlist_sentinel(txc_sentinel_list_t *sentinel_list, 
                txc_sentinel_t *sentinel, 
                txc_result_t status)
{
	txc_result_t ret;

	if (sentinel_list->num_entries == sentinel_list->size) {
		if ((ret = allocate_sentinel_list_entries(sentinel_list, 1)) 
		    != TXC_R_SUCCESS) 
		{
			return ret;
		}	
	}

	sentinel_list->entries[sentinel_list->num_entries].sentinel = sentinel;
	sentinel_list->entries[sentinel_list->num_entries].status = status;
	sentinel_list->num_entries++; 

	return TXC_R_SUCCESS;
}


/** 
 * \brief Enlist the sentinel in the transaction's sentinel list.
 *
 * It also attaches to the sentinel by bumping up the sentinel's
 * refcount via a call to sentinel_attach().
 *
 * \param[in] txd Transactional descriptor.
 * \param[in] sentinel Sentinel to enlist.
 * \param[in] flags TXC_SENTINEL_ACQUIREONRETRY
 * \return Code indicating success or failure (reason) of the operation.
 */
txc_result_t 
txc_sentinel_enlist(txc_tx_t *txd, txc_sentinel_t *sentinel, int flags) 
{
	int acquire_on_retry;

	acquire_on_retry = (flags & TXC_SENTINEL_ACQUIREONRETRY) > 0 ? 
	                   TXC_SENTINEL_ACQUIREONRETRY : 0;

	TXC_MUTEX_LOCK(&sentinel->synch_mutex);
	sentinel_attach(sentinel);
	TXC_MUTEX_UNLOCK(&sentinel->synch_mutex);
	enlist_sentinel(txd->sentinel_list, sentinel, acquire_on_retry);
	return TXC_R_SUCCESS;
}


/**
 * \brief Try to acquire a sentinel.
 *
 * It will try to acquire the sentinel. If sentinel is acquired it will enlist
 * it in the transaction's sentinel list as acquired, otherwise if the sentinel
 * is not acquired (busy) it will return without block-waiting for the sentinel
 * to become free (prevent deadlock). 
 *
 * \param[in] txd Transactional descriptor.
 * \param[in] sentinel Sentinel to acquire.
 * \param[in] flags TXC_SENTINEL_ACQUIRED, TXC_SENTINEL_ACQUIREONRETRY
 * \return Code indicating success or failure (reason) of the operation.
 */
txc_result_t 
txc_sentinel_tryacquire(txc_tx_t *txd, txc_sentinel_t *sentinel, int flags) 
{
	int          ret;
	int          num_spin_retries;
	int          acquire_on_retry;
	txc_result_t result;

	TXC_ASSERT(sentinel != NULL);
	TXC_ASSERT(txd != NULL);
	acquire_on_retry = (flags & TXC_SENTINEL_ACQUIREONRETRY) > 0 ? 
	                   TXC_SENTINEL_ACQUIREONRETRY : 0;


	if (sentinel->owner != txd) {
		switch (txc_tx_get_xactstate(txd)) {
			case TXC_XACTSTATE_NONTRANSACTIONAL:
				TXC_INTERNALERROR("Acquiring a sentinel outside of a transaction not supported\n");
				/* never gets here */
			case TXC_XACTSTATE_TRANSACTIONAL_IRREVOCABLE:
				/* 
				 * Do not need to acquire any sentinels. 
				 * Irrevocable global lock already provides atomicity & isolation. 
				 */
				result = TXC_R_SUCCESS;
				TXC_DEBUG_PRINT(TXC_DEBUG_SENTINEL, 
				                "ACQUIRE SENTINEL %3d: SUCCESS (GLOCK)\n",
				                sentinel->id);
				break;
			case TXC_XACTSTATE_TRANSACTIONAL_RETRYABLE:
				register_sentinelmgr_undo_action(txd);
				register_sentinelmgr_commit_action(txd);
				num_spin_retries = txc_runtime_settings.sentinel_max_spin_retries;
				do {
					ret = TXC_MUTEX_TRYLOCK(&sentinel->sentinel_mutex);
				} while (--num_spin_retries >= 0 && ret != 0);
				if (ret == 0) {
					TXC_MUTEX_LOCK(&sentinel->synch_mutex);
					sentinel_attach(sentinel);
					TXC_MUTEX_UNLOCK(&sentinel->synch_mutex);
					sentinel->owner = txd;
					enlist_sentinel(txd->sentinel_list, sentinel, 
					                TXC_SENTINEL_ACQUIRED | 
					                acquire_on_retry);
					TXC_DEBUG_PRINT(TXC_DEBUG_SENTINEL, 
					                "ACQUIRE SENTINEL %3d: SUCCESS\n",
					                sentinel->id);
					result = TXC_R_SUCCESS;
				} else {
					TXC_MUTEX_LOCK(&sentinel->synch_mutex);
					sentinel_attach(sentinel);
					TXC_MUTEX_UNLOCK(&sentinel->synch_mutex);
					enlist_sentinel(txd->sentinel_list, sentinel, 
					                acquire_on_retry);
					TXC_DEBUG_PRINT(TXC_DEBUG_SENTINEL, 
					                "ACQUIRE SENTINEL %3d: BUSY\n",
					                sentinel->id);
					result = TXC_R_BUSYSENTINEL;
				}
				break;
			default:
				TXC_INTERNALERROR("Unknown transaction state\n");
		}
	} else {
#ifdef _TXC_DEBUG_BUILD
		TXC_ASSERT(txc_sentinel_owner(sentinel) == txd);
		TXC_ASSERT(txc_sentinel_is_enlisted(txd, sentinel) == TXC_R_SUCCESS);
		TXC_DEBUG_PRINT(TXC_DEBUG_SENTINEL, 
		                "ACQUIRE SENTINEL %3d: SUCCESS (HAVE IT)\n",
		                sentinel->id);	
#endif													
	}
	return result;
}


static inline
void
backoff(txc_tx_t *txd)
{
	useconds_t wait_time;

	if (txc_runtime_settings.sentinel_max_backoff_time>0) {
		wait_time = (useconds_t) txc_tx_get_forced_retries(txd) * 32;
		if (wait_time < txc_runtime_settings.sentinel_max_backoff_time) {
			usleep(wait_time);
		} else {
			usleep(txc_runtime_settings.sentinel_max_backoff_time);
		}
	}
}


/*
 ****************************************************************************
 ***                      DEBUGGING SUPPORT ROUTINES                      ***
 ****************************************************************************
 */


static inline
void
sentinel_print(void *addr)
{
	txc_sentinel_t *sentinel = (txc_sentinel_t *) addr;

	fprintf(TXC_DEBUG_OUT, "SENTINEL %3d: owner = %p (TID = %u)\n", 
	        sentinel->id, 
	        sentinel->owner,
	        sentinel->owner->tid);

}

static inline
void
sentinel_list_print(txc_sentinel_list_t *sentinel_list, char *heading)
{
	int                       i;
	txc_sentinel_list_entry_t *entry;
	txc_sentinel_t            *sentinel;

	if (TXC_DEBUG_SENTINEL) {
		TXC_DEBUG_PRINT(TXC_DEBUG_SENTINEL, "%s\n", heading);
		TXC_DEBUG_PRINT(TXC_DEBUG_SENTINEL, "====================================\n");
		for (i=0; i<sentinel_list->num_entries; i++) {
			entry = &sentinel_list->entries[i];
			sentinel = entry->sentinel;
			TXC_DEBUG_PRINT(TXC_DEBUG_SENTINEL, 
			                "SENTINEL %3d: owner = %p (TID = %d) entry_status = %x\n", 
			                sentinel->id, 
			                sentinel->owner,
			                (sentinel->owner == NULL) ? -1 : sentinel->owner->tid,
			                entry->status);
		}
	}
}


void 
txc_sentinelmgr_print_pools(txc_sentinelmgr_t *sentinelmgr) {
	if (TXC_DEBUG_SENTINEL) {
		TXC_DEBUG_PRINT(TXC_DEBUG_SENTINEL, "SENTINEL POOL\n");
		TXC_DEBUG_PRINT(TXC_DEBUG_SENTINEL, "====================================\n");
		txc_pool_print(sentinelmgr->pool_sentinel, sentinel_print, 0);
	}
}


txc_tx_t * 
txc_sentinel_owner(txc_sentinel_t *sentinel)
{
	return sentinel->owner;
}


txc_result_t 
txc_sentinel_is_enlisted(txc_tx_t *txd, txc_sentinel_t *sentinel)
{
	int            i;
	txc_sentinel_t *sentinel_iter;

	/* 
	 * Sentinel must be found in the list of acquired sentinels and the
	 * sentinel's owner field be the descriptor txd.
	 */
	for (i=0; i<txd->sentinel_list->num_entries; i++) {
		sentinel_iter = txd->sentinel_list->entries[i].sentinel;
		if (sentinel_iter == sentinel) {
			return TXC_R_SUCCESS;
		}
	}
	return TXC_R_FAILURE;
}
