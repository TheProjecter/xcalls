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
 * \file buffer.c
 * 
 * \brief Buffer manager implementation.
 *
 * This subsystem provides shared and private buffers to store 
 * data for deferred system calls and for in-place system calls that 
 * require compensation. Shared buffers store data when the kernel 
 * cannot undo a data-producing action, such as reading from a pipe. 
 * These buffers persist across transactions and may be accessed by any 
 * thread. Private buffers store data for deferred calls for compensating
 * actions. For example, undoing a random file write replaces the data 
 * in the file with its original contents, which are stored in a private 
 * buffer. As private buffers are discarded when a transaction aborts, 
 * they are organized as a per-thread log (<tt>txc_buffer_linear_t</tt>)
 * to reduce fragmentation and bookkeeping overheads.
 *
 * \todo Move circular buffer management code from 
 * <tt>src/xcalls/x_read_pipe.c</tt> and <tt>src/xcalls/x_sendmsg.c</tt> 
 * into this file. 
 * 
 */

#include <misc/result.h>
#include <misc/malloc.h>
#include <misc/pool.h>
#include <misc/debug.h>
#include <core/buffer.h>

/** Buffer manager */
struct txc_buffermgr_s {
	txc_pool_t *pool_buffer_circular; /**< Pool of circular buffers. */
	txc_pool_t *pool_buffer_linear;   /**< Pool of linear buffers. */
};


txc_buffermgr_t *txc_g_buffermgr;


static
void
circular_buffer_constructor(void *obj)
{
	txc_buffer_circular_t *buffer;
	buffer  = (txc_buffer_circular_t *) obj;
	buffer->size_max = TXC_BUFFER_CIRCULAR_SIZE;
	buffer->buf = (char *) (((char *) obj) + sizeof (txc_buffer_circular_t));
}


static
void
linear_buffer_constructor(void *obj)
{
	txc_buffer_linear_t *buffer;

	buffer  = (txc_buffer_linear_t *) obj;
	buffer->size_max = TXC_BUFFER_LINEAR_SIZE;
	buffer->buf = (char *) (((char *) obj) + sizeof (txc_buffer_linear_t));
}


/**
 * \brief Creates a buffer manager 
 *
 * It preallocates a pool of circular and linear buffers to make allocation 
 * fast.
 *
 * \param[out] buffermgrp The created buffer manager.
 * \return Code indicating success or failure (reason) of the operation.
 */
txc_result_t
txc_buffermgr_create(txc_buffermgr_t **buffermgrp) 
{
	txc_result_t result;

	*buffermgrp = (txc_buffermgr_t *) MALLOC(sizeof(txc_buffermgr_t));
	if (*buffermgrp == NULL) {
		return TXC_R_NOMEMORY;
	}

	if ((result = txc_pool_create(&((*buffermgrp)->pool_buffer_circular), 
	                              TXC_BUFFER_CIRCULAR_SIZE +
                                    sizeof(txc_buffer_circular_t),
	                              TXC_BUFFER_CIRCULAR_NUM, 
                                  circular_buffer_constructor)) != TXC_R_SUCCESS) 
	{
		FREE(*buffermgrp);
		return result;
	}	

	if ((result = txc_pool_create(&((*buffermgrp)->pool_buffer_linear), 
	                              TXC_BUFFER_LINEAR_SIZE + 
                                    sizeof(txc_buffer_linear_t),
	                              TXC_BUFFER_LINEAR_NUM, 
                                  linear_buffer_constructor)) != TXC_R_SUCCESS) 
	{
		txc_pool_destroy(&((*buffermgrp)->pool_buffer_circular));
		FREE(*buffermgrp);
		return result;
	}	

	return TXC_R_SUCCESS;
}


/**
 * \brief Destroys a buffer manager 
 *
 * It deallocates all buffers that it manages.
 *
 * \param[in,out] buffermgrp The buffer manager to be destroyed.
 */
void
txc_buffermgr_destroy(txc_buffermgr_t **buffermgrp)
{
	txc_pool_destroy(&((*buffermgrp)->pool_buffer_circular));
	txc_pool_destroy(&((*buffermgrp)->pool_buffer_linear));
	FREE(*buffermgrp);
	*buffermgrp = NULL;
}


/*
 *****************************************************************************
 ***                    CIRCULAR BUFFER IMPLEMENTATION                     ***  
 *****************************************************************************
 */

/**
 * \struct txc_buffer_circular_s
 *
 * \brief A structure to represent a circular buffer.
 *
 * The circular buffer is implemented using two queues: a
 * primary queue <primary_head, primary_tail> and a secondary queue
 * <secondary_head, secondary_tail>. 
 * The two queues use a contiguous region of the same buffer (buf) and
 * they never overlap. 
 * Data are copied to the primary queue, and when there is no space, data
 * are copied to the secondary queue. There is no space in a queue if the
 * tail would have to extend beyond the end of the buffer or extend into
 * the buffer region of the other queue.
 *
 * When a primary queue is emptied, the secondary queue becomes the new primary
 * queue and the old primary the new secondary.
 *
 * We chose to implement a circular buffer this way instead of simply using
 * a single pair <head, tail> to avoid wrapping the head and tail pointers 
 * around to the beginning of the buffer region. The wrap around would result
 * in a non-contiguous queue region. When reading data from the kernel (using 
 * a system call) would require either to bring the data into the two separate 
 * regions by doing two system calls or bring the data into an intermediate 
 * contiguous buffer to avoid crossing the kernel-user interface two times. 
 * The downside of using two queues is internal fragmentation.
 */ 


/** 
 * \brief Initializes an existing circular buffer.
 *
 * \param[in] buffer The buffer to be initialized.
 * \return Code indicating success or failure (reason) of the operation.
 */
txc_result_t 
txc_buffer_circular_init(txc_buffer_circular_t *buffer)
{

	buffer->primary_head = 0;
	buffer->primary_tail = 0;
	buffer->secondary_head = 0;
	buffer->secondary_tail = 0;
	buffer->state = TXC_BUFFER_STATE_NON_SPECULATIVE;

	return TXC_R_SUCCESS;
}

/** 
 * \brief Creates and initializes a new circular buffer.
 *
 * \param[in] buffermgr The buffer manager responsible for the buffer.
 * \param[out] bufferp The newly created buffer.
 * \return Code indicating success or failure (reason) of the operation.
 */
txc_result_t 
txc_buffer_circular_create(txc_buffermgr_t *buffermgr, 
                           txc_buffer_circular_t **bufferp)
{
	txc_result_t result;
	txc_buffer_circular_t *buffer;

	if ((result = txc_pool_object_alloc(buffermgr->pool_buffer_circular,
	                                    (void **) &(buffer), 1)) 
	    != TXC_R_SUCCESS) 
	{
		TXC_INTERNALERROR("Could not create buffer object\n");
		return result;
	}

	buffer->manager = buffermgr;
	txc_buffer_circular_init(buffer);
	*bufferp = buffer;

	return TXC_R_SUCCESS;
}


/** 
 * \brief Destroys a circular buffer.
 *
 * \param[in, out] bufferp The buffer to be destroyed.
 */
void 
txc_buffer_circular_destroy(txc_buffer_circular_t **bufferp)
{
	txc_buffermgr_t *buffermgr = (*bufferp)->manager;

	txc_pool_object_free(buffermgr->pool_buffer_circular, (void **) bufferp, 1); 
	*bufferp = NULL;
}


/*
 *****************************************************************************
 ***                     LINEAR BUFFER IMPLEMENTATION                      *** 
 *****************************************************************************
 */


/** 
 * \brief Initializes an existing linear buffer.
 *
 * \param[in] buffer The buffer to be initialized.
 * \return Code indicating success or failure (reason) of the operation.
 */

txc_result_t 
txc_buffer_linear_init(txc_buffer_linear_t *buffer)
{
	buffer->state = TXC_BUFFER_STATE_NON_SPECULATIVE;
	buffer->cur_len = 0;

	return TXC_R_SUCCESS;
}


/** 
 * \brief Creates and initializes a new linear buffer.
 *
 * \param[in] buffermgr The buffer manager responsible for the buffer.
 * \param[out] bufferp The newly created buffer.
 * \return Code indicating success or failure (reason) of the operation.
 */
txc_result_t 
txc_buffer_linear_create(txc_buffermgr_t *buffermgr, 
                         txc_buffer_linear_t **bufferp)
{
	txc_result_t result;
	txc_buffer_linear_t *buffer;

	if ((result = txc_pool_object_alloc(buffermgr->pool_buffer_linear,
	                                    (void **) &(buffer), 1)) 
	    != TXC_R_SUCCESS) 
	{
		TXC_INTERNALERROR("Could not create buffer object\n");
		return result;
	}

	buffer->manager = buffermgr;
	txc_buffer_linear_init(buffer);
	*bufferp = buffer;

	return TXC_R_SUCCESS;
}


/** 
 * \brief Destroys a linear buffer.
 *
 * \param[in, out] bufferp The buffer to be destroyed.
 */
void 
txc_buffer_linear_destroy(txc_buffer_linear_t **bufferp)
{
	txc_buffermgr_t *buffermgr = (*bufferp)->manager;

	txc_pool_object_free(buffermgr->pool_buffer_linear, (void **) bufferp, 1); 
	*bufferp = NULL;
}


/**
 * \brief Allocates a linear region from a linear buffer.
 *
 * \param[in] buffer The buffer from which the region is allocated.
 * \param[in] size The size of the region to be allocated.
 * \return A pointer to the allocated region or NULL if allocated failed.
 */
void *
txc_buffer_linear_malloc(txc_buffer_linear_t *buffer, unsigned int size)
{
	void *ptr;

	if (buffer->cur_len + size > buffer->size_max) {
		ptr = NULL;	
	} else {
		ptr = (void *) &buffer->buf[buffer->cur_len];
		buffer->cur_len += size;
	}
	return ptr;
}


/** 
 * \brief Deallocates a linear region previously allocated from a linear 
 * buffer.
 * 
 * Deallocation operations  must be in the reverse order of the allocation 
 * operations. 
 *
 * \buffer[in] buffer The buffer from which the region was allocated.
 * \size[in] size The size of the region to be allocated.
 */
void
txc_buffer_linear_free(txc_buffer_linear_t *buffer, unsigned int size)
{
	buffer->cur_len -= size;
}
