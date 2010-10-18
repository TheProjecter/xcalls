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
 * \file koa.c
 *
 * \brief KOA manager implementation.
 *
 * A KOA (Kernel Object Abstraction) is the user-mode representation of a 
 * logical kernel object. It is used for serializing accesses to the kernel
 * object (via sentinel) and for bufferring uncommitted state (buffer).
 *
 * As shown in the figure below, all KOAs are accessible through
 * the <em>file descriptor to KOA map</em>. Additionally, file KOAs
 * are accessible through the alias cache's hash table by indexing the table
 * using the file's inode.
 * 
 * \image html alias_cache.jpg "Alias cache and file descriptor to KOA map."
 *
 * <b>Locking Protocol</b>
 *
 * There are three types of locks of interest:
 * \li Alias cache serialization lock: Serializes directory operations 
 * (open, create, rename, unlink) and any KOA creation/destruction caused 
 * by these operations.
 * \li FD2KOA mutex lock: Synchronizes accesses to KOA through a file 
 * descriptor. Holding this lock ensures that a KOA does not disappear after
 * following a valid reference through a file descriptor. This is because a KOA
 * cannot be destroyed without holding locks on all the file descriptors
 * referencing the KOA.
 * \li KOA mutex lock: Synchronizes KOA accesses when not holding the alias or
 * file descriptor lock. This can happen when deattaching from a KOA/sentinel
 * when a transaction commits/aborts. \todo This lock might be unnecessary.
 * 
 * <em>More on file descriptor locks</em>
 * \li Operations that use a file descriptor don't acquire the KOA cache lock
 * \li Operations on file descriptors acquire a short lock on the FD to properly 
 * synchronize accesses to metadata. They don't acquire the alias lock.
 * \li Operations that operate on pathnames must ensure they are correctly 
 * sycnhronized with operations on FDs. So they need to acquire a short lock
 * on all FDs referenced by a KOA. Operations such as copy require this 
 * isolation. But operations such as unlink could rely on UNIX semantics for 
 * isolation.
 * \li Operations such as opening an existing file that has a KOA in the alias 
 * cache must acquire the locks on all the file descriptors referencing the file
 * to properly synchronize with writes/reads when accessing the metadata.
 *
 * \todo Relax aliasing in favor of concurrency.
 */


#include <sys/types.h>
#include <sys/stat.h>
#include <libc/syscalls.h>
#include <misc/malloc.h>
#include <misc/result.h>
#include <misc/debug.h>
#include <misc/pool.h>
#include <misc/hash_table.h>
#include <misc/mutex.h>
#include <core/config.h>
#include <core/koa.h>
#include <core/sentinel.h>
#include <core/buffer.h>
#include <stdlib.h>
#include <pthread.h>
#include <stddef.h>


typedef struct txc_koa_file_s txc_koa_file_t;
typedef struct txc_koa_sock_dgram_s txc_koa_sock_dgram_t;
typedef struct txc_koa_pipe_read_end_s txc_koa_pipe_read_end_t;


/** File KOA */
struct txc_koa_file_s {
	ino_t st_ino;   /**< Inode number  */
	dev_t st_dev;   /**< Device        */
	dev_t st_rdev;  /**< Device type   */
};	


/** Datagram socket KOA */
struct txc_koa_sock_dgram_s {
	txc_buffer_circular_t   *buffer_circular_input; 
};


/** Pipe read end KOA */
struct txc_koa_pipe_read_end_s {
	txc_buffer_circular_t   *buffer_circular_input; 
};


/** 
 * KOA (Kernel Object Abstraction): A user-mode representation of a 
 * logical kernel object.
 */
struct txc_koa_s {
	txc_koamgr_t                *manager;                   /**< Manager responsible for this KOA */
	txc_mutex_t                 mutex;                      /**< Mutex for synchronizing access to the fields of this KOA */
	txc_sentinel_t              *sentinel;                  /**< User level sentinel providing transactional isolation for this KOA */
	struct {
		int                     fd[TXC_MAX_NUM_FDREFS_PER_KOA]; /**< File descriptors referencing the kernel object of this KOA */
		int                     refcnt;                         /**< Number of file descriptors referencing the kernel object of this KOA */ 
	} fdref;
	int                         refcnt;                     /**< Total reference count */
	int                         type;                       /**< Type of object */
	union {
		txc_koa_file_t          file;                       /**< File specific fields */
		txc_koa_sock_dgram_t    sock_dgram;                 /**< Datagram socket specific fields */
		txc_koa_pipe_read_end_t pipe_read_end;              /**< Pipe read end specific rields */
	};	
}; 

typedef struct txc_fd2koa_s txc_fd2koa_t;


/** Maps a file/pipe/socket descriptor to a KOA. */
struct txc_fd2koa_s {
	txc_mutex_t mutex;
	txc_koa_t   *koa;
};

typedef struct txc_alias_cache_s txc_alias_cache_t;


/** 
 * Alias cache: A hash table that keeps pointers to the KOAs of all 
 * live files. The hash table is indexed by inode number and returns 
 * the KOA of a file if a file has already been opened/created 
 *(i.e. being live). Accesses to the alias cache are serialized using
 * the cache's mutex lock to ensure that no two transactions could race 
 * during the execution of directory operations (e.g. creation of a file).
 */
struct txc_alias_cache_s {
	txc_mutex_t      mutex;                  /* mutex */
	txc_hash_table_t *hash_tbl;              /* hash table */
};


/** KOA Manager */
struct txc_koamgr_s {
	txc_alias_cache_t alias_cache;            /**< Alias cache                     */
	txc_fd2koa_t      map[TXC_KOA_MAP_SIZE];  /**< Maps file descriptor to KOA.    */
	txc_sentinelmgr_t *sentinelmgr;           /**< Pointer to the sentinel manager *
	                                           *   providing the sentinels.        */
	txc_buffermgr_t   *buffermgr;             /**< Pointer to the buffer manager   *
	                                           *   that provides buffers to the    *
											   *   KOA objects.                    */
	txc_pool_t        *pool_koa_obj;          /**< Pool from where we allocate     *
	                                           *   KOA objects.                    */
};


txc_koamgr_t *txc_g_koamgr;


/**
 * \brief Creates a KOA manager.
 *
 * It creates a pool of KOAs and an alias cache for file KOAs.
 * It also precreates pipe KOAs for standard input, output, error.
 *
 * \param[out] koamgrp Pointer to the create manager.
 * \param[in] sentinelmgr The sentinel manager backing this 
 *            manager's KOAs with sentinels.
 * \param[in] buffermgr The buffer manager backing this 
 *            manager's KOAs with buffers.
 * \return Code indicating success or failure (reason) of the operation.
 */
txc_result_t
txc_koamgr_create(txc_koamgr_t **koamgrp, 
                  txc_sentinelmgr_t *sentinelmgr, 
                  txc_buffermgr_t *buffermgr) 
{
	int          i;
	txc_result_t result;
	txc_koa_t    *koa;

	*koamgrp = (txc_koamgr_t *) MALLOC(sizeof(txc_koamgr_t));
	if (*koamgrp == NULL) {
		return TXC_R_NOMEMORY;
	}
	if ((result = txc_pool_create(&((*koamgrp)->pool_koa_obj), 
	                              sizeof(txc_koa_t),
	                              TXC_KOA_NUM, 
	                              NULL)) != TXC_R_SUCCESS) 
	{
		FREE(*koamgrp);
		return result;
	}

	if ((result = txc_hash_table_create(&((*koamgrp)->alias_cache.hash_tbl), 
	                                    TXC_KOA_CACHE_HASHTBL_SIZE,
	                                    TXC_BOOL_FALSE)) != TXC_R_SUCCESS) 
	{
		txc_pool_destroy(&((*koamgrp)->pool_koa_obj));
		FREE(*koamgrp);
		return result;
	}

	for (i=0; i<TXC_KOA_MAP_SIZE; i++) {
		TXC_MUTEX_INIT(&((*koamgrp)->map[i].mutex), NULL);
		(*koamgrp)->map[i].koa = NULL;
	}

	(*koamgrp)->sentinelmgr = sentinelmgr;
	(*koamgrp)->buffermgr = buffermgr;

	/* Precreate standard KOAs: input, output, error */
	txc_koa_create(*koamgrp, &koa, TXC_KOA_IS_PIPE_READ_END, NULL); 
	txc_koa_attach_fd(koa, 0, 0);
	txc_koa_create(*koamgrp, &koa, TXC_KOA_IS_PIPE_WRITE_END, NULL);
	txc_koa_attach_fd(koa, 1, 0);
	txc_koa_create(*koamgrp, &koa, TXC_KOA_IS_PIPE_WRITE_END, NULL);
	txc_koa_attach_fd(koa, 2, 0);

	return TXC_R_SUCCESS;
}


/**
 * \brief Destroys a KOA manager.
 *
 * \param[in,out] koamgrp A pointer to the KOA manager to be destroyed.
 * \return Code indicating success or failure (reason) of the operation.
 */
void
txc_koamgr_destroy(txc_koamgr_t **koamgrp) 
{
	txc_pool_destroy(&((*koamgrp)->pool_koa_obj));
	txc_hash_table_destroy(&((*koamgrp)->alias_cache.hash_tbl));
	FREE(*koamgrp);
	*koamgrp = NULL;
}


/**
 * \brief Locks the alias cache's serialization mutex.
 * 
 * \param[in] koamgr KOA manager.
 * \return Code indicating success or failure (reason) of the operation.
 */
txc_result_t
txc_koa_lock_alias_cache(txc_koamgr_t *koamgr) 
{
	TXC_MUTEX_LOCK(&(koamgr->alias_cache.mutex));
	return TXC_R_SUCCESS;
}


/**
 * \brief Unlocks the alias cache's serialization mutex.
 * 
 * \param[in] koamgr KOA manager.
 * \return Code indicating success or failure (reason) of the operation.
 */
txc_result_t
txc_koa_unlock_alias_cache(txc_koamgr_t *koamgr) 
{
	TXC_MUTEX_UNLOCK(&(koamgr->alias_cache.mutex));
	return TXC_R_SUCCESS;
}


/**
 * \brief Inserts a KOA into an alias cache. 
 *
 * Caller must hold the alias cache's serialization mutex.
 *
 * \param[in] koamgr Alias cache's KOA manager.
 * \param[in] koa File KOA to insert in the alias cache.
 * \return Code indicating success or failure (reason) of the operation.
 */
static inline
txc_result_t
alias_cache_insert(txc_koamgr_t *koamgr, txc_koa_t *koa)
{
	txc_hash_table_t *h = koamgr->alias_cache.hash_tbl;
	txc_result_t     ret;
	ino_t            inode_number = koa->file.st_ino;

	TXC_ASSERT(koa->type == TXC_KOA_IS_FILE);

	if ((ret = txc_hash_table_add(h, (unsigned int) inode_number, (void *) koa)) 
	    != TXC_R_SUCCESS)
	{
		return ret;
	}
	return TXC_R_SUCCESS;
}


/**
 * \brief Remove a KOA from an alias cache. 
 *
 * Caller must hold the alias cache's serialization mutex.
 *
 * \param[in] koamgr Alia cache's KOA manager.
 * \param[in] koa File KOA to insert in the alias cache.
 * \return Code indicating success or failure (reason) of the operation.
 */
static inline
txc_result_t
alias_cache_remove(txc_koamgr_t *koamgr, txc_koa_t *koa)
{
	txc_hash_table_t *h = koamgr->alias_cache.hash_tbl;
	txc_result_t     ret;
	ino_t            inode_number = koa->file.st_ino;

	if ((ret = txc_hash_table_remove(h, (unsigned int) inode_number, NULL)) 
	    != TXC_R_SUCCESS)
	{
		return ret;
	}
	return TXC_R_SUCCESS;
}


/**
 * \brief Looks up the alias cache for a file KOA. 
 *
 * \param[in] koamgr Alias cache's KOA manager.
 * \param[in] inode_number Inode number of the file to lookup.
 * \param[out] koap Pointer to the KOA if file has been found in the alias cache.
 * \return Code indicating success or failure (reason) of the operation.
 */
txc_result_t 
txc_koa_alias_cache_lookup_inode(txc_koamgr_t *koamgr, ino_t inode_number, txc_koa_t **koap)
{
	txc_koa_t *koa;

	if (txc_hash_table_lookup(koamgr->alias_cache.hash_tbl, 
	                          (unsigned int) inode_number, (void **) &koa)
	    != TXC_R_SUCCESS) 
	{
		return TXC_R_NOTEXISTS;
	} else {
		*koap = koa;
		return TXC_R_SUCCESS;
	}
	
}


/**
 * \brief Creates a KOA.
 *
 * \param[in] koamgr The KOA manager creating and managing the KOA.
 * \param[out] koap Pointer to the created KOA.
 * \param[in] type Type of the KOA to be created: 
 *                   TXC_KOA_IS_FILE, 
 *                   TXC_KOA_IS_SOCK_DGRAM, 
 *                   TXC_KOA_IS_PIPE_READ_END
 * \param[in] args Arguments specific to the type of KOA created. 
 *                 For file KOA this is the inode of the file.
 * \return Code indicating success or failure (reason) of the operation.
 */
txc_result_t
txc_koa_create(txc_koamgr_t *koamgr, txc_koa_t **koap, int type, void *args) 
{
	txc_koa_t      *koa;
	txc_sentinel_t *sentinel;
	txc_result_t   result;

	if ((result = txc_pool_object_alloc(koamgr->pool_koa_obj,(void **) &koa, 1)) 
	    != TXC_R_SUCCESS) 
	{
		TXC_INTERNALERROR("Could not create KOA object\n");
		return result;
	}
	if ((result = txc_sentinel_create(koamgr->sentinelmgr, &sentinel)) 
	    != TXC_R_SUCCESS)
	{	
		TXC_INTERNALERROR("Could not create sentinel for KOA object\n");
		return result;
	}	
	TXC_MUTEX_INIT(&(koa->mutex), NULL); 
	koa->manager = koamgr;
	koa->sentinel = sentinel;
	koa->type = type;
	koa->refcnt = 0;
	koa->fdref.refcnt = 0;

	switch(type) {
		case TXC_KOA_IS_FILE:
			koa->file.st_ino = (ino_t) args;
			break;
		case TXC_KOA_IS_SOCK_DGRAM:
			txc_buffer_circular_create(koa->manager->buffermgr, 
			                           &(koa->sock_dgram.buffer_circular_input));
			break;
		case TXC_KOA_IS_PIPE_READ_END:
			txc_buffer_circular_create(koa->manager->buffermgr, 
			                           &(koa->pipe_read_end.buffer_circular_input));
			break;
		default:
			break; /* do nothing */
	}

	*koap = koa;
	return TXC_R_SUCCESS;
}


/**
 * \brief Destroy a KOA.
 *
 * \param[in,out] Pointer to the KOA to be destroyed. 
 */
void
txc_koa_destroy(txc_koa_t **koap) 
{
	txc_koa_t *koa = *koap;

	/* 
	 * txc_sentinel_detach will destroy the sentinel if after the
 	 * detach operation is (sentinel->refcnt == 0) 
 	 */
	txc_sentinel_detach((*koap)->sentinel);

	switch((*koap)->type) {
		case TXC_KOA_IS_FILE:
			break;
		case TXC_KOA_IS_SOCK_DGRAM:
			txc_buffer_circular_destroy(&((*koap)->sock_dgram.buffer_circular_input));
			break;
		case TXC_KOA_IS_PIPE_READ_END:
			txc_buffer_circular_destroy(&((*koap)->pipe_read_end.buffer_circular_input));
			break;
		default:
			break; /* do nothing */
	}
	txc_pool_object_free((*koap)->manager->pool_koa_obj, 
	                     (void **) &koa, 1);
}


/**
 * \brief Attach a file descriptor to a KOA.
 *
 * \param[in] koa The KOA the file descriptor is attached to.
 * \param[in] fd The file descriptor attached to KOA.
 * \param[in] lock If set then this function acquires the lock on the file descriptor
 * \return Code indicating success or failure (reason) of the operation.
 */
txc_result_t
txc_koa_attach_fd(txc_koa_t *koa, int fd, int lock)
{
	txc_koamgr_t *koamgr;
	int          first_attach;

	TXC_ASSERT(koa != NULL);
	TXC_ASSERT(fd < TXC_KOA_MAP_SIZE);

	koamgr = koa->manager;

	if (lock) {
		TXC_MUTEX_LOCK(&(koamgr->map[fd].mutex));
	}	
	TXC_ASSERT(koamgr->map[fd].koa == NULL);
	koamgr->map[fd].koa = koa;
	koa->refcnt++;
	first_attach = (koa->refcnt == 1) ? 1 : 0;
	if (first_attach) {
		/** First attach -- insert it into the alias cache if aliasable KOA. */
		if (koa->type == TXC_KOA_IS_FILE) {
			alias_cache_insert(koamgr, koa);
		}	
	}

	if (koa->fdref.refcnt >= TXC_MAX_NUM_FDREFS_PER_KOA) {
		TXC_INTERNALERROR("Reached maximum number of file descriptors per KOA.\n");
	}	
	koa->fdref.fd[koa->fdref.refcnt++] = fd;

	TXC_DEBUG_PRINT(TXC_DEBUG_KOA, 
	                "txc_koa_attach_fd: koa = %p, fd = %d, refcnt = %d, sentinel = %p\n", 
	                koa, fd, koa->refcnt, koa->sentinel);

	if (lock) {
		TXC_MUTEX_UNLOCK(&(koamgr->map[fd].mutex));
	}	

	return TXC_R_SUCCESS;
}


/**
 * \brief Detach a file descriptor from a KOA.
 *
 * \param[in] koa The KOA the file descriptor is detached from.
 * \param[in] fd The file descriptor detached from KOA.
 * \param[in] lock If set then this function acquires the lock on the file descriptor
 * \return Code indicating success or failure (reason) of the operation.
 */
txc_result_t
txc_koa_detach_fd(txc_koa_t *koa, int fd, int lock)
{
	int          i;
	int          index = -1;
	int          last_detach;
	txc_koamgr_t *koamgr;

	TXC_ASSERT(koa != NULL);
	TXC_ASSERT(fd < TXC_KOA_MAP_SIZE);

	koamgr = koa->manager;

	if (lock) {
		TXC_MUTEX_LOCK(&(koamgr->map[fd].mutex));
	}	
	if (koamgr->map[fd].koa == NULL) {
		return TXC_R_FAILURE;
	}
	koamgr->map[fd].koa = NULL;
	/* Remove backward pointer from KOA to file descriptor */
	for (i=0; i<koa->fdref.refcnt; i++) {
		if (koa->fdref.fd[i] == fd) {
			index = i;
			break;
		}
	}
	if (index < 0) {
		if (lock) {
			TXC_MUTEX_UNLOCK(&(koamgr->map[fd].mutex));
		}	
		return TXC_R_FAILURE;
	}
	koa->fdref.fd[index] = koa->fdref.fd[koa->fdref.refcnt];
	koa->fdref.refcnt--;

	koa->refcnt--;
	last_detach = (koa->refcnt == 0) ? 1 : 0;
	if (last_detach) {
		/* Last detach -- remove it from the alias cache if aliasable KOA. */
		if (koa->type == TXC_KOA_IS_FILE) {
			alias_cache_remove(koamgr, koa);
		}
		txc_koa_destroy(&koa);
	}	


	TXC_DEBUG_PRINT(TXC_DEBUG_KOA, 
	                "txc_koa_detach_fd: koa = %p, fd = %d, refcnt = %d, sentinel = %p\n", 
	                koa, fd, koa->refcnt, koa->sentinel);

	if (lock) {
		TXC_MUTEX_UNLOCK(&(koamgr->map[fd].mutex));
	}	

	return TXC_R_SUCCESS;
}


/**
 * \brief Finds the KOA the file descriptor is mapped to. 
 *
 * \param[in] koamgr KOA manager.
 * \param[in] fd File descriptor.
 * \param[out] koap Pointer to the KOA the file descriptor is mapped to.
 * \return Code indicating success or failure (reason) of the operation.
 */
txc_result_t
txc_koa_lookup_fd2koa(txc_koamgr_t *koamgr, int fd, txc_koa_t **koap) 
{
	TXC_ASSERT(fd < TXC_KOA_MAP_SIZE);
	*koap = koamgr->map[fd].koa;
	if (*koap == NULL) {
		return TXC_R_FAILURE;
	}
	return TXC_R_SUCCESS;
}


/** 
 * \brief Locks a file descriptor.
 *
 * \param[in] koamgr KOA manager.
 * \param[in] File descriptor to lock.
 * \return Code indicating success or failure (reason) of the operation.
 */
txc_result_t
txc_koa_lock_fd(txc_koamgr_t *koamgr, int fd)
{
	if (0 == TXC_MUTEX_LOCK(&(koamgr->map[fd].mutex))) {
		return TXC_R_SUCCESS;
	} else {
		return TXC_R_FAILURE;
	}	
}


/** 
 * \brief Unlocks a file descriptor.
 *
 * \param[in] koamgr KOA manager.
 * \param[in] File descriptor to unlock.
 * \return Code indicating success or failure (reason) of the operation.
 */
txc_result_t
txc_koa_unlock_fd(txc_koamgr_t *koamgr, int fd)
{
	if (0 == TXC_MUTEX_UNLOCK(&(koamgr->map[fd].mutex))) {
		return TXC_R_SUCCESS;
	} else {
		return TXC_R_FAILURE;
	}	
}


/** 
 * \brief Locks all the file descriptors mapped to a KOA.
 *
 * \param[in] koa The KOA of which to lock the file descriptors.
 * \return Code indicating success or failure (reason) of the operation.
 */
txc_result_t
txc_koa_lock_fds_refby_koa(txc_koa_t *koa)
{
	int          i;
	int          fd;
	txc_fd2koa_t *entry; 
	txc_koamgr_t *koamgr = koa->manager;

    /*
	 * FIXME: Is the following still true ???
	 *
	 * This is the only point where we acquire multiple FD locks.
	 * Therefore we don't need to acquire locks in some total order. 
	 */
	for (i=0; i<koa->fdref.refcnt; i++) {
		fd = koa->fdref.fd[i];
		entry = &koamgr->map[fd];
		TXC_MUTEX_LOCK(&(entry->mutex));
	}
	return TXC_R_SUCCESS;
}


/** 
 * \brief Unlocks all the file descriptors mapped to a KOA.
 *
 * \param[in] koa The KOA of which to unlock the file descriptors.
 * \return Code indicating success or failure (reason) of the operation.
 */
txc_result_t
txc_koa_unlock_fds_refby_koa(txc_koa_t *koa)
{
	int          i;
	int          fd;
	txc_fd2koa_t *entry; 
	txc_koamgr_t *koamgr = koa->manager;

	for (i=0; i<koa->fdref.refcnt; i++) {
		fd = koa->fdref.fd[i];
		entry = &koamgr->map[fd];
		TXC_MUTEX_UNLOCK(&(entry->mutex));
	}
	return TXC_R_SUCCESS;
}


/**
 * \brief Finds the inode of the file pointed by path.
 *
 * \param[in] path The pathanme of the file to look for.
 * \param[out] inode_number The inode of the file.
 * \return Code indicating success or failure (reason) of the operation.
 */
txc_result_t
txc_koa_path2inode(const char *path, ino_t *inode_number) 
{
	int         ret;
	struct stat stat_buf;

	ret = txc_libc_stat(path, &stat_buf);
	if (ret==0) {
		*inode_number = stat_buf.st_ino;
	} else {
		*inode_number = (ino_t) 0;
	}

	return TXC_R_SUCCESS;
}


/** 
 * \brief Attaches to a KOA.
 *
 * Logically attaches to a KOA by incrementing the KOA's reference 
 * counter by one.
 *
 * \param[in] koa The KOA to attach to.
 * \return Code indicating success or failure (reason) of the operation.
 */
txc_result_t
txc_koa_attach(txc_koa_t *koa) 
{
	int          first_attach;
	txc_koamgr_t *koamgr;

	TXC_ASSERT(koa != NULL);

	koamgr = koa->manager;
	TXC_MUTEX_LOCK(&(koa->mutex));
	koa->refcnt++;
	first_attach = (koa->refcnt == 1) ? 1 : 0;
	if (first_attach) {
		/** 
          * If this is the first attach, then insert KOA into the alias 
		  * cache if it is an aliasable KOA (e.g. file). 
		  */
		if (koa->type == TXC_KOA_IS_FILE) {
			alias_cache_insert(koamgr, koa);
		}	
	}

	TXC_DEBUG_PRINT(TXC_DEBUG_KOA, 
	                "txc_koa_attach: koa = %p, refcnt = %d, sentinel = %p\n", 
	                koa, koa->refcnt, koa->sentinel);
	TXC_MUTEX_UNLOCK(&(koa->mutex));

	return TXC_R_SUCCESS;
}


/** 
 * \brief Detaches from a KOA.
 *
 * It logically detaches from a KOA by decrementing the KOA's reference 
 * counter by one.
 * It destroys a KOA after detach if reference counter gets zero.
 *
 * \param[in] koa The KOA to detach from.
 * \return Code indicating success or failure (reason) of the operation.
 */
txc_result_t
txc_koa_detach(txc_koa_t *koa) 
{
	int          last_detach;
	txc_koamgr_t *koamgr;

	TXC_ASSERT(koa != NULL);

	koamgr = koa->manager;
	TXC_MUTEX_LOCK(&(koa->mutex));
	koa->refcnt--;
	last_detach = (koa->refcnt == 0) ? 1 : 0;
	if (last_detach) {
		/** Last detach -- remove it from the alias cache if aliasable KOA. */
		if (koa->type == TXC_KOA_IS_FILE) {
			alias_cache_remove(koamgr, koa);
		}	
		txc_koa_destroy(&koa);
	}	

	TXC_DEBUG_PRINT(TXC_DEBUG_KOA, 
	                "txc_koa_detach: koa = %p, refcnt = %d, sentinel = %p\n",
	                koa, koa->refcnt, koa->sentinel);
	TXC_MUTEX_UNLOCK(&(koa->mutex));

	return TXC_R_SUCCESS;
}


/** 
 * Gets the sentinel of a KOA.
 * 
 * \param[in] koa The KOA of which to get the sentinel.
 * \return The sentinel of the KOA.
 */
txc_sentinel_t *
txc_koa_get_sentinel(txc_koa_t *koa)
{
	return koa->sentinel;
}


/** 
 * Gets the KOA manager of a KOA.
 * 
 * \param[in] koa The KOA of which to get the sentinel.
 * \return The KOA manager of the KOA.
 */
txc_koamgr_t *
txc_koa_get_koamgr(txc_koa_t *koa)
{
	return koa->manager;
}


/** 
 * Gets the buffer of a KOA.
 * 
 * \param[in] koa The KOA of which to get the sentinel.
 * \return The buffer of the KOA.
 */
void *
txc_koa_get_buffer(txc_koa_t *koa)
{
	void *ret;

	switch (koa->type) {
		case TXC_KOA_IS_SOCK_DGRAM:
			ret = (void *) koa->sock_dgram.buffer_circular_input;
		case TXC_KOA_IS_PIPE_READ_END:
			ret = (void *) koa->pipe_read_end.buffer_circular_input;
	}
	return ret;
}
