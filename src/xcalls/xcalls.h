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
 * \file xcalls.h
 * 
 * \brief xCalls' declarations.
 *
 * The error codes returned by xCalls are the ones returned by the 
 * corresponding system call.
 *
 * \todo Implement x_send by reusing code of x_sendmsg.
 * \todo Implement x_recv by reusing code of x_recvmsg.
 */

#ifndef _TXC_XCALLS_H
#define _TXC_XCALLS_H

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define XCALL_DEF(xcall) _TXC_##xcall 
#define _XCALL(xcall) _TXC_##xcall 

int     XCALL_DEF(x_close)(int fildes, int *result);
int     XCALL_DEF(x_create)(const char *, mode_t, int *);
int     XCALL_DEF(x_dup)(int oldfd, int *result);
int     XCALL_DEF(x_fsync)(int fildes, int *result);
int     XCALL_DEF(x_fdatasync)(int fildes, int *result);
off_t   XCALL_DEF(x_lseek)(int fd, off_t offset, int whence, int *result);
int     XCALL_DEF(x_open)(const char *pathname, int flags, mode_t mode, int *result); 
int     XCALL_DEF(x_pipe)(int fildes[2], int *result);
int     XCALL_DEF(x_printf)(const char *format, ...);
int     XCALL_DEF(x_pthread_create)(pthread_t *thread, const pthread_attr_t *attr, void *(*start_routine)(void*), void *arg, int *result);
int     XCALL_DEF(x_pthread_mutex_init)(pthread_mutex_t *mutex, const pthread_mutexattr_t *attr, int *result);
int     XCALL_DEF(x_pthread_mutex_lock)(pthread_mutex_t *mutex, int *result);
int     XCALL_DEF(x_pthread_mutex_unlock)(pthread_mutex_t *mutex, int *result);
ssize_t XCALL_DEF(x_read)(int fd, void *buf, size_t nbyte, int *result);
ssize_t XCALL_DEF(x_read_pipe)(int fd, void *buf, size_t nbyte, int *result);
ssize_t XCALL_DEF(x_recvmsg)(int s, struct msghdr *msg, int flags, int *result);
int     XCALL_DEF(x_rename)(const char *oldpath, const char *newpath, int *result);
int     XCALL_DEF(x_socket)(int domain, int type, int protocol, int *result); 
ssize_t XCALL_DEF(x_write_ovr)(int fd, const void *buf, size_t nbyte, int *result);
ssize_t XCALL_DEF(x_write_ovr_save)(int fd, const void *buf, size_t nbyte, int *result);
ssize_t XCALL_DEF(x_write_ovr_ignore)(int fd, const void *buf, size_t nbyte, int *result);
ssize_t XCALL_DEF(x_write_pipe)(int fd, const void *buf, size_t nbyte, int *result);
ssize_t XCALL_DEF(x_sendmsg)(int fd, const struct msghdr *msg, int flags, int *result);
ssize_t XCALL_DEF(x_write_seq)(int fd, const void *buf, size_t nbyte, int *result);
int     XCALL_DEF(x_unlink)(const char *pathname, int *result);

#endif
