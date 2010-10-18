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
 * \file pool.c
 *
 * \brief Pool allocator implementation.
 *
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <pthread.h>
#include <unistd.h>
#include <stddef.h>
#include <misc/result.h>
#include <misc/malloc.h>
#include <misc/pool.h>
#include <misc/debug.h>
#include <misc/mutex.h>
#include <core/config.h>

struct txc_pool_object_s {
	void   *buf;
	char   status;
	struct txc_pool_object_s *next;
	struct txc_pool_object_s *prev;
};

struct txc_pool_s {
	txc_mutex_t                   mutex;
	void                          *full_buf;			
	txc_pool_object_t             *obj_list;
	txc_pool_object_t             *obj_free_head;
	txc_pool_object_t             *obj_allocated_head;
	unsigned int                  obj_free_num;
	unsigned int                  obj_allocated_num;
	unsigned int                  obj_size;
	unsigned int                  obj_num;
	txc_pool_object_constructor_t obj_constructor;
};


txc_result_t
txc_pool_create(txc_pool_t **poolp, 
                unsigned int obj_size, 
                unsigned int obj_num, 
                txc_pool_object_constructor_t obj_constructor) 
{
	txc_pool_object_t *object_list;
	char *buf; 
	int i;

	*poolp = (txc_pool_t *) MALLOC(sizeof(txc_pool_t));
	if (*poolp == NULL) {
		return TXC_R_NOMEMORY;
	}
	(*poolp)->obj_size = obj_size;
	(*poolp)->obj_num = obj_num;
	object_list = (txc_pool_object_t *) CALLOC(obj_num, 
	                                           sizeof(txc_pool_object_t));
	if (object_list == NULL) {
		FREE(*poolp);
		return TXC_R_NOMEMORY;
	}
	(*poolp)->obj_list = object_list;
	if ((buf = CALLOC(obj_num, obj_size)) == NULL) {  
		FREE(object_list);
		FREE(*poolp);
		return TXC_R_NOMEMORY;
	}
	(*poolp)->full_buf = buf; 
	for (i=0;i<obj_num;i++) {
		object_list[i].buf = &buf[i*obj_size];
		object_list[i].next = &object_list[(i+1)%obj_num];
		object_list[i].prev = &object_list[(i-1)%obj_num];
		object_list[i].status = TXC_POOL_OBJECT_FREE;
		if (obj_constructor != NULL) {
			obj_constructor(object_list[i].buf);
		}
	}
	object_list[0].prev = object_list[obj_num-1].next = NULL;
	(*poolp)->obj_free_num = obj_num;
	(*poolp)->obj_free_head = &object_list[0];	
	(*poolp)->obj_allocated_num = 0;
	(*poolp)->obj_allocated_head = NULL;	
	if (TXC_MUTEX_INIT(&((*poolp)->mutex), NULL) !=0) {
		FREE(buf);
		FREE(object_list);	
		FREE(*poolp);
		return TXC_R_NOTINITLOCK;
	}
	return TXC_R_SUCCESS;
}


txc_result_t 
txc_pool_destroy(txc_pool_t **poolp) 
{
	TXC_ASSERT(*poolp != NULL);
	FREE((*poolp)->full_buf);
	FREE((*poolp)->obj_list);	
	FREE(*poolp);
	*poolp = NULL;
	return TXC_R_SUCCESS;
}


txc_result_t
txc_pool_object_alloc(txc_pool_t *pool, void **objp, int lock) 
{
	txc_result_t result;
	txc_pool_object_t *pool_object;

	if (lock) { 
		TXC_MUTEX_LOCK(&pool->mutex);
	}	
	if (pool->obj_free_num == 0) {
		*objp = NULL;
		result = TXC_R_NOMEMORY;
		goto unlock;
	}
	pool_object = pool->obj_free_head;
	pool->obj_free_head = pool_object->next;
	if (pool->obj_free_head) {
		pool->obj_free_head->prev = NULL;
	}
	pool_object->prev = NULL;
	pool_object->next = pool->obj_allocated_head;
	if (pool->obj_allocated_head) {
		pool->obj_allocated_head->prev = pool_object;
	}
	pool->obj_allocated_head = pool_object; 
	pool->obj_allocated_num++;
	pool->obj_free_num--;
	*objp = pool_object->buf;
	pool_object->status = TXC_POOL_OBJECT_ALLOCATED;
	result = TXC_R_SUCCESS;
unlock:
	if (lock) {
		TXC_MUTEX_UNLOCK(&pool->mutex);
	}	
	return result;
}


void
txc_pool_object_free(txc_pool_t *pool, void **objp, int lock) 
{
	txc_pool_object_t *pool_object;
	unsigned int index;

	TXC_ASSERT(pool);
	TXC_ASSERT(*objp);
	if (lock) {
		TXC_MUTEX_LOCK(&(pool->mutex));
	}	
	index = ((unsigned int) ( (char*) *objp - (char*) pool->full_buf)) / pool->obj_size;
	pool_object = &pool->obj_list[index];
	TXC_ASSERT(pool_object->status == TXC_POOL_OBJECT_ALLOCATED);
	if (pool_object->prev) {
		pool_object->prev->next = pool_object->next;
	} else { 
		pool->obj_allocated_head = pool_object->next;
	}
	if (pool_object->next) {
		pool_object->next->prev = pool_object->prev;
	}
	pool_object->next = pool->obj_free_head;
	pool_object->prev = NULL;
	if (pool->obj_free_head) {
		pool->obj_free_head->prev = pool_object;
	} 
	pool->obj_allocated_num--;
	pool->obj_free_num++;
	pool_object->status = TXC_POOL_OBJECT_FREE;
	pool->obj_free_head = pool_object; 
	if (lock) {
		TXC_MUTEX_UNLOCK(&(pool->mutex));
	}	
	*objp = NULL;
}


txc_pool_object_t *
txc_pool_object_first(txc_pool_t *pool, int obj_status)
{
	switch (obj_status) {
		case TXC_POOL_OBJECT_FREE:
			return pool->obj_free_head;
		case TXC_POOL_OBJECT_ALLOCATED:
			return pool->obj_allocated_head;
		case TXC_POOL_OBJECT_ALLOCATED & TXC_POOL_OBJECT_FREE:
			return pool->obj_list;
		default:
			TXC_INTERNALERROR("Unknown status of pool object");
	}
	return NULL;
}


txc_pool_object_t *
txc_pool_object_next(txc_pool_object_t *obj)
{
	return obj->next;
}


void *
txc_pool_object_of(txc_pool_object_t *obj)
{
	return obj->buf;
}

void
txc_pool_lock(txc_pool_t *pool)
{
	TXC_MUTEX_LOCK(&pool->mutex);
}


void
txc_pool_unlock(txc_pool_t *pool)
{
	TXC_MUTEX_UNLOCK(&pool->mutex);
}




/*
 **************************************************************************
 ***                     DEBUGGING SUPPORT ROUTINES                     ***
 **************************************************************************
 */


void 
txc_pool_print(txc_pool_t *pool, txc_pool_object_print_t printer, int verbose) 
{
	txc_pool_object_t *pool_object;
	unsigned int index;

	if (TXC_DEBUG_POOL) {
		TXC_MUTEX_LOCK(&(pool->mutex));
		fprintf(TXC_DEBUG_OUT, "FREE POOL\n");
		pool_object = pool->obj_free_head;
		while (pool_object) {
			index = (pool_object - pool->obj_list);
			if (verbose) {
				fprintf(TXC_DEBUG_OUT, "Object Index = %u\t", index);
				if (pool_object->prev) {
					index = (pool_object->prev - pool->obj_list);
					fprintf(TXC_DEBUG_OUT, "[prev = %u, ", index);
				} else {
					fprintf(TXC_DEBUG_OUT, "[prev = NULL, ");
				} 
				if (pool_object->next) {
					index = (pool_object->next - pool->obj_list);
					fprintf(TXC_DEBUG_OUT, "next = %u]\n", index);
				} else {
					fprintf(TXC_DEBUG_OUT, "next = NULL]\n");
				}
			}
			if (printer) {
				printer(pool_object->buf);
			} 
			pool_object = pool_object->next;
		}
		fprintf(TXC_DEBUG_OUT, "\nALLOCATED POOL\n");
		pool_object = pool->obj_allocated_head;
		while (pool_object) {
			index = (pool_object - pool->obj_list);
			if (verbose) {
				fprintf(TXC_DEBUG_OUT, "Object Index = %u\t", index);
				if (pool_object->prev) {
					index = (pool_object->prev - pool->obj_list);
					fprintf(TXC_DEBUG_OUT, "[prev = %u, ", index);
				} else {
					fprintf(TXC_DEBUG_OUT, "[prev = NULL, ");
				} 
				if (pool_object->next) {
					index = (pool_object->next - pool->obj_list);
					fprintf(TXC_DEBUG_OUT, "next = %u]\n", index);
				} else {
					fprintf(TXC_DEBUG_OUT, "next = NULL]\n");
				} 
			}
			if (printer) {
				printer(pool_object->buf);
			} 
			pool_object = pool_object->next;
		}
		TXC_MUTEX_UNLOCK(&(pool->mutex));
	}
}
