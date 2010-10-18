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
 * \file pool.h
 *
 * \brief Pool allocator interface.
 *
 * This allocator is used by library subsystems that need to preallocate 
 * and preinitialize a pool of objects. 
 *
 * \todo Consider a scalable multithreaded generic allocator such as Hoard.
 */

#ifndef _TXC_POOL_H
#define _TXC_POOL_H

#include <misc/result.h>

#define TXC_POOL_OBJECT_FREE         0x1	
#define TXC_POOL_OBJECT_ALLOCATED    0x2	

/**
 * The pool works as a slab allocator. That is when allocating an objects 
 * you can assume that its state is the same as when the object was 
 * returned back to the pool.
 *
 * Objects are allocated from the head 
 */

typedef struct txc_pool_object_s txc_pool_object_t;
typedef struct txc_pool_s txc_pool_t;
typedef void (*txc_pool_object_constructor_t)(void *obj); 
typedef void (*txc_pool_object_print_t)(void *obj); 

txc_result_t txc_pool_create(txc_pool_t **poolp, 
                             unsigned int obj_size,
                             unsigned int obj_num, 
                             txc_pool_object_constructor_t obj_constructor);
txc_result_t txc_pool_destroy(txc_pool_t **poolp);
txc_result_t txc_pool_object_alloc(txc_pool_t *pool, void **objp, int lock);
void txc_pool_object_free(txc_pool_t *pool, void **objp, int lock);
txc_pool_object_t *txc_pool_object_first(txc_pool_t *pool, int obj_status);
txc_pool_object_t *txc_pool_object_next(txc_pool_object_t *obj);
void *txc_pool_object_of(txc_pool_object_t *obj);
void txc_pool_lock(txc_pool_t *pool);
void txc_pool_unlock(txc_pool_t *pool);
void txc_pool_print(txc_pool_t *pool, txc_pool_object_print_t printer, int verbose);

#endif    /* _TXC_POOL_H */
