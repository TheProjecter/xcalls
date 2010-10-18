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
 * \file txc.h
 *
 * \brief xCalls' library public interface.
 *
 * This is the public header file to be included by any application using xCalls.
 */

#ifndef _TXC_TXC_H
#define _TXC_TXC_H

/* Abort reason */
#ifndef TYPEDEF_TXC_TX_ABORTREASON_T
#define TYPEDEF_TXC_TX_ABORTREASON_T
typedef enum {
	TXC_ABORTREASON_TMCONFLICT = 0, 
    TXC_ABORTREASON_USERABORT = 1, 
	TXC_ABORTREASON_USERRETRY = 2, 
	TXC_ABORTREASON_BUSYSENTINEL = 4, 
	TXC_ABORTREASON_INCONSISTENCY = 8, 
	TXC_ABORTREASON_BUSYTXLOCK = 16, 
} txc_tx_abortreason_t;
#endif /* TYPEDEF_TXC_TX_ABORTREASON_T */

/* Transactional State */
#ifndef TYPEDEF_TXC_TX_XACTSTATE_T
#define TYPEDEF_TXC_TX_XACTSTATE_T
typedef enum {
	TXC_XACTSTATE_NONTRANSACTIONAL = 0, 
    TXC_XACTSTATE_TRANSACTIONAL_RETRYABLE = 1, 
	TXC_XACTSTATE_TRANSACTIONAL_IRREVOCABLE = 2 
} txc_tx_xactstate_t;
#endif /* TYPEDEF_TXC_TX_XACTSTATE_T */

/* Failure manager flags */
#ifndef TXC_FM_FLAGS
# define TXC_FM_FLAGS
# define TXC_FM_NO_RETRY 0x1
#endif


#define __TXC_XSTR(s) __TXC_STR(s)
#define __TXC_STR(s) #s

#define SRCLOC_STR(file,line) file ":" __TXC_XSTR(line)


#define XACT_BEGIN(tag)                                                      \
  _TXC_transaction_pre_begin(SRCLOC_STR(__FILE__, __LINE__),                 \
                             __FILE__,                                       \
                             __FUNCTION__,                                   \
                             __LINE__);                                      \
  __tm_atomic                                                                \
  {                                                                          \
    _TXC_transaction_post_begin();


#define XACT_END(tag)                                                        \
  }

#define XACT_END_NONLEXICAL(tag) /* do nothing for ICC */

#define XACT_WAIVER __tm_waiver

#define XACT_ABORT(abortreason)  _TXC_transaction_abort(abortreason);
#define XACT_RETRY               _TXC_transaction_abort(TXC_ABORTREASON_USERRETRY);

#define TM_WAIVER   __attribute__ ((tm_pure)) 
#define TM_PURE     __attribute__ ((tm_pure)) 
#define TM_CALLABLE __attribute__ ((tm_callable)) 

#define ASSERT_NOT_RUNNING_XACT                                              \
	assert(_TXC_get_xactstate() == TXC_XACTSTATE_NONTRANSACTIONAL);

typedef unsigned int txc_result_t;

TM_WAIVER int _TXC_global_init();
TM_WAIVER int _TXC_global_shutdown();
TM_WAIVER int _TXC_thread_init();
TM_WAIVER void _TXC_transaction_abort(txc_tx_abortreason_t);
TM_WAIVER txc_tx_xactstate_t _TXC_get_xactstate();
TM_WAIVER void _TXC_transaction_pre_begin(const char *srcloc_str, const char *src_file, const char *src_fun, int src_line);
TM_WAIVER int _TXC_transaction_post_begin();
TM_WAIVER void _TXC_fm_register(int flags, int *err);

#ifndef _TXC_COMMIT_FUNCTION_T
#define _TXC_COMMIT_FUNCTION_T
typedef void (*_TXC_commit_function_t)(void *, int *);
#endif
#ifndef _TXC_UNDO_FUNCTION_T
#define _TXC_UNDO_FUNCTION_T
typedef void (*_TXC_undo_function_t)(void *, int *);
#endif

TM_WAIVER int _TXC_register_commit_action(_TXC_commit_function_t function, void *args, int *error_result);
TM_WAIVER int _TXC_register_undo_action(_TXC_undo_function_t function, void *args, int *error_result);

/**
 * xCall API
 *
 * Please note that xCall functions names are defined using XCALL_DEF(xcall).
 * This is to allow flexibility in choosing the library prefix.
 */ 

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define XCALL_DEF(xcall) _TXC_##xcall 
#define _XCALL(xcall) _TXC_##xcall 

TM_WAIVER int     XCALL_DEF(x_close)(int fildes, int *result);
TM_WAIVER int     XCALL_DEF(x_create)(const char *, mode_t, int *);
TM_WAIVER int     XCALL_DEF(x_dup)(int oldfd, int *result);
TM_WAIVER int     XCALL_DEF(x_fsync)(int fildes, int *result);
TM_WAIVER int     XCALL_DEF(x_fdatasync)(int fildes, int *result);
TM_WAIVER off_t   XCALL_DEF(x_lseek)(int fd, off_t offset, int whence, int *result);
TM_WAIVER int     XCALL_DEF(x_open)(const char *pathname, int flags, mode_t mode, int *result); 
TM_WAIVER int     XCALL_DEF(x_pipe)(int fildes[2], int *result);
TM_WAIVER ssize_t XCALL_DEF(x_printf)(const char *format, ...);
TM_WAIVER int     XCALL_DEF(x_pthread_create)(pthread_t *thread, const pthread_attr_t *attr, void *(*start_routine)(void*), void *arg, int *result);
TM_WAIVER int     XCALL_DEF(x_pthread_mutex_init)(pthread_mutex_t *mutex, const pthread_mutexattr_t *attr, int *result);
TM_WAIVER int     XCALL_DEF(x_pthread_mutex_lock)(pthread_mutex_t *mutex, int *result);
TM_WAIVER int     XCALL_DEF(x_pthread_mutex_unlock)(pthread_mutex_t *mutex, int *result);
TM_WAIVER ssize_t XCALL_DEF(x_read)(int fd, void *buf, size_t nbyte, int *result);
TM_WAIVER ssize_t XCALL_DEF(x_read_pipe)(int fd, void *buf, size_t nbyte, int *result);
TM_WAIVER ssize_t XCALL_DEF(x_recvmsg)(int s, struct msghdr *msg, int flags, int *result);
TM_WAIVER int     XCALL_DEF(x_rename)(const char *oldpath, const char *newpath, int *result);
TM_WAIVER int     XCALL_DEF(x_socket)(int domain, int type, int protocol, int *result); 
TM_WAIVER ssize_t XCALL_DEF(x_sendmsg)(int fd, const struct msghdr *msg, int flags, int *result);
TM_WAIVER ssize_t XCALL_DEF(x_write_ovr)(int fd, const void *buf, size_t nbyte, int *result);
TM_WAIVER ssize_t XCALL_DEF(x_write_ovr_save)(int fd, const void *buf, size_t nbyte, int *result);
TM_WAIVER ssize_t XCALL_DEF(x_write_ovr_ignore)(int fd, const void *buf, size_t nbyte, int *result);
TM_WAIVER ssize_t XCALL_DEF(x_write_pipe)(int fd, const void *buf, size_t nbyte, int *result);
TM_WAIVER ssize_t XCALL_DEF(x_write_seq)(int fd, const void *buf, size_t nbyte, int *result);
TM_WAIVER int     XCALL_DEF(x_unlink)(const char *pathname, int *result);

/*
 * Transactional safe conditional variables API
 */

#include <xcalls/condvar/condvar.h>


#endif /* _TXC_TXC_H */
