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
 * \file syscalls.h
 *
 * \brief Wrapper functions for system calls used by the xCalls library.
 *
 * \todo Use lazy symbol evaluation for system call names: 
 * <tt>handle = dlopen("/lib/libc.so.6", RTLD_LAZY); dlsym(handle, #name); </tt>
 */

#ifndef _TXC_LIBC_SYSCALLS_H
#define _TXC_LIBC_SYSCALLS_H

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>



static inline
int 
txc_libc_creat(const char *pathname, mode_t mode)
{
	return creat(pathname, mode);
}


static inline
int 
txc_libc_open(const char *pathname, int flags, mode_t mode)
{
	return open(pathname, flags, mode);
}


static inline
int 
txc_libc_close(int fd)
{
	return close(fd);
}


static inline
int 
txc_libc_rename(const char *oldpath, const char *newpath)
{
	return rename(oldpath, newpath);
}


static inline
int
txc_libc_stat(const char *path, struct stat *buf)
{
	return stat(path, buf);
}


static inline
int
txc_libc_unlink(const char *path)
{
	return unlink(path);
}


static inline
ssize_t 
txc_libc_write(int fd, const void *buf, size_t nbyte)
{
	return write(fd, buf, nbyte);
}


static inline
ssize_t
txc_libc_read(int fd, void *buf, size_t nbyte)
{
	return read(fd, buf, nbyte);
}


static inline
off_t 
txc_libc_lseek(int fildes, off_t offset, int whence)
{
	return lseek(fildes, offset, whence);
}


static inline
size_t 
txc_libc_pread(int fd, void *buf, size_t count, off_t offset)
{
	return pread(fd, buf, count, offset);
}


static inline
ssize_t 
txc_libc_pwrite(int fd, const void *buf, size_t count, off_t offset)
{
	return pwrite(fd, buf, count, offset);
}


static inline
int 
txc_libc_ftruncate(int fd, off_t length)
{
	return ftruncate(fd, length);
}


static inline
int 
txc_libc_link(const char *oldpath, const char *newpath)
{
	return link(oldpath, newpath);
}


static inline
int
txc_libc_dup(int oldfd)
{
	return dup(oldfd);
}


static inline
int 
txc_libc_pipe(int filedes[2])
{
	return pipe(filedes);
}


static inline
int 
txc_libc_socket(int domain, int type, int protocol)
{
	return socket(domain, type, protocol);
}


static inline
ssize_t 
txc_libc_recv(int s, void *buf, size_t len, int flags)
{
	return recv(s, buf, len, flags);
}


static inline
ssize_t 
txc_libc_recvfrom(int s, void *buf, size_t len, int flags,
                  struct sockaddr *from, socklen_t *fromlen)
{
	return recvfrom(s, buf, len, flags, from, fromlen);
}


static inline
ssize_t 
txc_libc_recvmsg(int s, struct msghdr *msg, int flags)
{
	return recvmsg(s, msg, flags);
}


static inline
ssize_t 
txc_libc_send(int s, const void *buf, size_t len, int flags)
{
	return send(s, buf, len, flags);
}


static inline
ssize_t 
txc_libc_sendto(int s, const void *buf, size_t len, int flags, 
                const struct sockaddr *to, socklen_t tolen)
{
	return sendto(s, buf, len, flags, to, tolen);
}


static inline
ssize_t 
txc_libc_sendmsg(int s, const struct msghdr *msg, int flags)
{
	return sendmsg(s, msg, flags);
}


static inline
int 
txc_libc_fsync(int fd)
{
	return fsync(fd);
}


static inline
int 
txc_libc_fdatasync(int fd)
{
	return fdatasync(fd);
}


#endif
