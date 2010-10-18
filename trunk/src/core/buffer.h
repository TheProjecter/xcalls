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
 * \file buffer.h
 * 
 * \brief Buffer manager interface.
 */

#ifndef _TXC_BUFFER_H
#define _TXC_BUFFER_H

#include <misc/result.h>

#define TXC_BUFFER_CIRCULAR_SIZE        1024*(64*2)*64
#define TXC_BUFFER_CIRCULAR_NUM         32
#define TXC_BUFFER_LINEAR_SIZE          1024*256
#define TXC_BUFFER_LINEAR_NUM           32

typedef enum {
	TXC_BUFFER_STATE_NON_SPECULATIVE = 0,
	TXC_BUFFER_STATE_SPECULATIVE = 1
} txc_buffer_state_t;

typedef struct txc_buffermgr_s txc_buffermgr_t;
typedef struct txc_buffer_circular_s txc_buffer_circular_t;
typedef struct txc_buffer_linear_s txc_buffer_linear_t;
typedef struct txc_buffer_circular_memento_s txc_buffer_circular_memento_t;
typedef struct txc_buffer_linear_memento_s txc_buffer_linear_memento_t;

extern txc_buffermgr_t *txc_g_buffermgr;

/* 
 * Break the abstraction and make the buffer's internal structure visible 
 * for better performance.
 */

struct txc_buffer_circular_s {
	txc_buffermgr_t    *manager;
	char               *buf;
	unsigned int       size_max;
	unsigned int       primary_head;
	unsigned int       primary_tail;
	unsigned int       secondary_head;
	unsigned int       secondary_tail;
	unsigned int       speculative_primary_head;
	unsigned int       speculative_primary_tail;
	unsigned int       speculative_secondary_head;
	unsigned int       speculative_secondary_tail;
	txc_buffer_state_t state;
};


/**
 * A structure to represent a linear buffer. 
 */
struct txc_buffer_linear_s {
	txc_buffermgr_t    *manager;
	char               *buf;
	unsigned int       size_max;
	unsigned int       cur_len;
	txc_buffer_state_t state;
};



txc_result_t txc_buffermgr_create(txc_buffermgr_t **);
void txc_buffermgr_destroy(txc_buffermgr_t **);

txc_result_t txc_buffer_circular_create(txc_buffermgr_t *buffermgr, txc_buffer_circular_t **bufferp);
void txc_buffer_circular_destroy(txc_buffer_circular_t **bufferp);
txc_result_t txc_buffer_circular_init(txc_buffer_circular_t *buffer);

txc_result_t txc_buffer_linear_create(txc_buffermgr_t *buffermgr, txc_buffer_linear_t **bufferp);
void txc_buffer_linear_destroy(txc_buffer_linear_t **bufferp);
txc_result_t txc_buffer_linear_init(txc_buffer_linear_t *buffer);
void *txc_buffer_linear_malloc(txc_buffer_linear_t *buffer, unsigned int size);
void txc_buffer_linear_free(txc_buffer_linear_t *buffer, unsigned int size);

#endif
