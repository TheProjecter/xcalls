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
 * \file mainpage.h
 *
 * \brief Main Doxygen documentation.
 */

/*****************************************************************************
 *                            INTRODUCTION TO XCALLS                         *
 *****************************************************************************/ 

/**
@mainpage Introduction to xCalls

Memory transactions, similar to database transactions, allow
a programmer to focus on the logic of their program and let
the system ensures that transactions are atomic and isolated.
Thus, programs using transactions do not suffer from deadlock. 
However, when a transaction performs I/O or accesses
kernel resources, the atomicity and isolation guarantees from
the TM system do not apply to the kernel.

The xCall interface is a new API that provides transactional 
semantics for system calls. With a combination of deferral 
and compensation, xCalls enable transactional memory programs 
to use common OS functionality within transactions.



@section mainpage_tutorial Tutorial
\li \ref getting_started explains how to download and build xCalls, includes 
a number of sample programs, and provides an overview of the source code structure.


@section main_page_see_also See also
\li The xCalls research paper: Volos, H., Tack, A. J., Goyal, N., Swift, M. M., and Welc, A. 2009. xCalls: safe I/O in memory transactions. In Proceedings of the 4th ACM European Conference on Computer Systems (Nuremberg, Germany, April 01 - 03, 2009). EuroSys '09. ACM, New York, NY, 247-260. http://doi.acm.org/10.1145/1519065.1519093 
\li A short and comprehensive introduction to transactional memory: Adl-Tabatabai, A., Kozyrakis, C., and Saha, B. 2006. Unlocking Concurrency. Queue 4, 10 (Dec. 2006), 24-33. http://doi.acm.org/10.1145/1189276.1189288 
*/





/*****************************************************************************
 *                                GETTING STARTED                            *
 *****************************************************************************/ 

/** 

@page getting_started Getting Started

@section getting_started_building Building

Prerequisites (Linux):
\li Intel C/C++ STM Compiler, Prototype Edition 3.0
\li SCons: A software construction tool

To build xCalls, first check out a copy with SVN. If you have commit access:


\verbatim
% svn co --username username https://xcalls.googlecode.com/svn/trunk $XCALLS
\endverbatim

For anonymous checkout:

\verbatim
% svn co https://xcalls.googlecode.com/svn/trunk $XCALLS
\endverbatim

then open file $XCALLS/SConstruct with your favorite editor and set the environment
variable CC to point to the Intel C Compiler (icc):

\verbatim
env['CC'] = '/path/to/intel/c/compiler/icc'
\endverbatim

and finally build xCalls:

\verbatim
% cd $XCALLS
% scons
\endverbatim

All generated objects and binaries are placed under $XCALLS/build.

You may then install the library into the location of your choice $INSTALL_DIR:

\verbatim
% scons install prefix=$INSTALL_DIR
\endverbatim


@subsection getting_started_build_config Build configuration

You may configure the build process using the following Scons parameters:
\li test=(yes|no): Run unit tests.
\li select_test=TEST: Run just test suite TEST instead of all tests.
\li	ubench=(yes|no): Build library performance microbenchmarks.
\li	mode=(debug|release): Set library type.
\li linkage=(static|dynamic): Set library linkage.
\li tm_system=(itm|logtm): Set transactional memory system.
\li stats=(yes|no): Build library with statistics support.
\li prefix=INSTALL_DIR: Install library into INSTALL_DIR


@subsection getting_started_test_suite Test Suite
xCalls includes an extensive unit test suite which may be invoked by 
running 'scons test=yesk' in xCalls' root directory. 
Running 'scons test=yes select_test=TEST' runs just the single unit 
test suite TEST instead of all the unit tests. 


@section getting_started_using_xcalls Using xCalls

The following example demonstrates the major points of xCalls. 
Function <tt>create_file_with_header</tt> atomically creates a file 
and writes a header with a generation ID. The generation ID
is kept in the shared variable <tt>generation</tt>.

\code
#include <txc/txc.h>

volatile int *generation;

int create_file_with_header(char *path, char *header, int header_len)
	int err;
	int fd;

	_TXC_global_init()); 
	_TXC_thread_init()); 

	XACT_BEGIN(xact_create_file)
		_TXC_fm_register(TXC_FM_NO_RETRY, &err);
		fd = _TXC_x_create(path, S_IRUSR|S_IWUSR, &err);
		generation++;
		_TXC_x_write(fd, header, header_len, &err);
		_TXC_x_write(fd, num_creations, sizeof(int), &err);
		_TXC_x_close(fd, NULL);
	XACT_END(xact_create_file)

	if (err) {
		_TXC_x_unlink(path, NULL);
		__tm_atomic {
			generation--;
		}
		return -1;
	}

	return 0;
}	

\endcode

When compiling a program using xCalls, you should include the public header file <txc/txc.h>
which is located under $XCALLS/src/inc. Please make sure that this directory is 
present in your preprocessor's search path. Please also make sure that if you
use the shared library, the library's directory is searched by the dynamic linker.
If the library is not located in a standard directory, then you can set 
the <tt>LD_LIBRARY_PATH</tt> environment variable with the directory containing
the xCalls library.

Before using xCalls, you must properly initialize the library:
\li <tt>_TXC_global_init</tt> performs global library initialization and 
has to be called once by any thread of the program. While this function is
idempotent, multiple calls are not recommended to avoid paying unnecessary 
call overheads. It is recommended that this function  is called once by 
the main routine of your program. 
\li <tt>_TXC_thread_init</tt> performs thread initialization and has to be called
once before a thread invokes an xCall. This function is also idempotent, so
even if multiple calls are not recommended, they do not break correctness.
It is recommended that this function is called once by the start routine of 
the thread.

To begin and end an xCalls aware transaction, one has to use the MACROs
XACT_BEGIN and XACT_END respectively. The use of these MACROs has to be
lexically scoped. 

The last argument of all xCalls is a pointer to a memory location where
the error code of an asynchronous failure should be stored. An asynchronous
failure is one that happens during abort or commit. In our example, we
pass a pointer to the local variable &err. To simplify error recovery 
we use <tt>_TXC_fm_register</tt> to request that the transaction is not 
retried in case of an asynchronous failure. This allows us to recover 
from the error by simply removing the file and restoring the counter
to its initial value by decrementing it by one.



@section getting_starting_source_code Source Code Structure

The xCalls library is implemented in C.
\li <tt>$STASIS/src</tt>: The xCalls library source tree.
\li <tt>$STASIS/src/inc</tt>: The xCalls library public header files.
\li <tt>$STASIS/src/core</tt>: The library's core functionality.
\li <tt>$STASIS/src/libc</tt>: Transactional versions of some C library 
functions we found useful in our workloads.
\li <tt>$STASIS/src/misc</tt>: Some helper functions such as pool allocator, 
debugging facilities, hash table.
\li <tt>$STASIS/src/tm</tt>: Transactional memory system backends. 
\li <tt>$STASIS/src/xcalls</tt>: xCalls API implementation.


*/ 
