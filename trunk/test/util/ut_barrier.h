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

#ifndef _UT_BARRIER_H
#define _UT_BARRIER_H

/* Portable barrier implementation based on POSIX threads */

#define UT_BARRIER_T       ut_barrier_t
#define UT_BARRIER_INIT    ut_barrier_init
#define UT_BARRIER_WAIT    ut_barrier_wait
#define UT_BARRIER_DESTROY ut_barrier_destroy

typedef struct ut_barrier_s ut_barrier_t;

struct ut_barrier_s { 
	int        maxcnt;            /* maximum number of runners    */  
	struct _sb {  
		pthread_cond_t  wait_cv;  /* cv for waiters at barrier    */  
		pthread_mutex_t wait_lk;  /* mutex for waiters at barrier */  
		int             runners;  /* number of running threads    */  
	} sb[2];  
    struct _sb *sbp;              /* current sub-barrier          */  
};


int  
ut_barrier_init(ut_barrier_t *bp, int count)
{  
	int n;  
	int i;  
  
	if (count < 1) { 
		return(EINVAL);  
	}	
  
	bp->maxcnt = count;  
	bp->sbp = &bp->sb[0];  
  
	for (i = 0; i < 2; ++i) {  
		struct _sb *sbp = &( bp->sb[i] );  
		sbp->runners = count;  
  
		if (n = pthread_mutex_init(&sbp->wait_lk, NULL)) {  
			return(n);  
		}	
  
		if (n = pthread_cond_init(&sbp->wait_cv, NULL)) {  
			return(n);  
		}	
   }  
   return(0);  
}  
  

int  
ut_barrier_wait(register ut_barrier_t *bp) 
{  
	register struct _sb *sbp = bp->sbp;  

	pthread_mutex_lock(&sbp->wait_lk);  
  
	if (sbp->runners == 1) {    /* last thread to reach barrier */  
		if (bp->maxcnt != 1) {  
			/* reset runner count and switch sub-barriers */  
			sbp->runners = bp->maxcnt;  
			bp->sbp = (bp->sbp == &bp->sb[0]) ? &bp->sb[1] : &bp->sb[0];  
  
			/* wake up the waiters */  
			pthread_cond_broadcast(&sbp->wait_cv);  
      	}  
    } else {  
		sbp->runners--;    /* one less runner */  
  
		while (sbp->runners != bp->maxcnt) { 
			pthread_cond_wait( &sbp->wait_cv, &sbp->wait_lk);  
		}	
	}  
  
	pthread_mutex_unlock(&sbp->wait_lk);  
  
    return(0);  
}  
  
  
int  
ut_barrier_destroy(ut_barrier_t *bp) {  
	int n;  
	int i;  
  
	for (i=0; i < 2; ++ i) {  
		if (n = pthread_cond_destroy(&bp->sb[i].wait_cv)) {  
			return( n );  
		}	
  
		if (n = pthread_mutex_destroy( &bp->sb[i].wait_lk)) { 
			return(n);  
		}  
	}
	return (0);
}	

#endif
