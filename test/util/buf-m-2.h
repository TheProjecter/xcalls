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

/* ---------------------------------------------------------------- 
 * MONITOR  Buffer:                                                 
 *    This file simulates a bounded buffer.  It consists of the     
 * following procedures:                                            
 *    (1)  BufferInit() : initialize the monitor                    
 *    (2)  GET()        : retrieve the next available item          
 *    (3)  PUT()        : insert an item into the buffer            
 * ---------------------------------------------------------------- 
 */

#ifndef   _CK_buffer_m_h
#define   _CK_buffer_m_h

void  BufferInit(void);         /* initializes the monitor  */
__int64   GET(void);            /* get an item from buffer  */
__int64   PUT(__int64  value);  /* add an item to buffer    */
int GETSIZE(void);


#define   BUF_SIZE   5                    /* buffer size              */

static __int64            Buf[BUF_SIZE];  /* the actual buffer        */
static int                in;             /* next empty location      */
static int                out;            /* next filled location     */
static int                count;          /* no. of items in buffer   */
static txc_cond_event_t   UntilNotFull;   /* wait until full cv       */
static txc_cond_event_t   UntilNotEmpty;  /* wait until full cv       */

/* ----------------------------------------------------------------
 * FUNCTION  BufferInit():                                         
 *   This function initializes the buffer                          
 * ----------------------------------------------------------------
 */
void  BufferInit(void)
{
	in = out = count = 0;

	txc_cond_event_init(&UntilNotFull);
	txc_cond_event_init(&UntilNotEmpty);
}

/* ----------------------------------------------------------------- 
 * FUNCTION  GET():                                                 
 *   This function retrieves the next available item from the buffer
 * -----------------------------------------------------------------
 */
__int64  GET(void)
{
	__int64  value;

	COND_XACT_BEGIN;
		while (count == 0) {            /* while nothing in buffer  */
			/* Set up for the wait */
			txc_cond_wait(&UntilNotEmpty);
		}
		  
		value = Buf[out];             /* retrieve an item         */
		out = (out + 1) % BUF_SIZE;   /* advance out pointer      */
		count--;                      /* decrease counter         */
		txc_cond_event_tm_signal(&UntilNotFull);   /* signal consumers         */
	COND_XACT_END;
  return  value;                     /* return retrieved vague   */
}


/* ----------------------------------------------------------------
 * FUNCTION  PUT(__int64):                                         
 *   This function adds an item to the buffer                      
 * ----------------------------------------------------------------
 */
__int64  PUT(__int64  value)
{
	COND_XACT_BEGIN;
	{
    	while (count == BUF_SIZE) {     /* while buffer is full     */
			txc_cond_wait(&UntilNotFull); /* then wait */
		}	
		Buf[in] = value;              /* add the value to buffer  */
		in = (in + 1) % BUF_SIZE;     /* advance in pointer       */
		count++;                      /* increase count           */
		txc_cond_event_tm_signal(&UntilNotEmpty);  /* signal producers         */
	}
	COND_XACT_END;
	return  value;                     /* return the value         */
}

int GETSIZE(void)
{
	return(count);
}

#endif
