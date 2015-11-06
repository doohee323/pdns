/*  =========================================================================
from: https://github.com/imatix/zguide/raw/master/examples/C++/zhelpers.hpp
    zhelpers.h - ZeroMQ helpers for example applications

    Copyright (c) 1991-2010 iMatix Corporation and contributors

    This is free software; you can redistribute it and/or modify it under
    the terms of the Lesser GNU General Public License as published by
    the Free Software Foundation; either version 3 of the License, or
    (at your option) any later version.

    This software is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    Lesser GNU General Public License for more details.

    You should have received a copy of the Lesser GNU General Public License
    along with this program. If not, see <http://www.gnu.org/licenses/>.
    =========================================================================
*/

// Olivier Chamoux <olivier.chamoux@fr.thalesgroup.com>


#ifndef __ZHELPERS_HPP_INCLUDED__
#define __ZHELPERS_HPP_INCLUDED__

//  Include a bunch of headers that we will need in the examples

#include "zmq.hpp"

#include <iostream>
#include <iomanip>
#include <string>
#include <sstream>

#include <sys/time.h>
#include <time.h>
#include <assert.h>
#include <pthread.h>
#include <stdlib.h>	// random()  RAND_MAX
#include <stdio.h>
#include <stdarg.h>
#include <signal.h>

//  Bring Windows MSVC up to C99 scratch
#if (defined (__WINDOWS__))
    typedef unsigned long ulong;
    typedef unsigned int  uint;
    typedef __int64 int64_t;
#endif

//  Provide random number from 0..(num-1)
#define within(num) (int) ((float) (num) * random () / (RAND_MAX + 1.0))

namespace zmq {
	std::string s_recv (zmq::socket_t & socket);
	bool s_send (zmq::socket_t & socket, const std::string & string, const int flags = 0);
	bool s_sendmore (zmq::socket_t & socket, const std::string & string);
	void s_dump (zmq::socket_t & socket);
	std::string s_set_id (zmq::socket_t & socket);
	void s_version (void);
	void s_version_assert (int want_major, int want_minor);
	int64_t s_clock (void);
	void s_sleep (int msecs);
}

#endif
