/*
This file is a part of MonaSolutions Copyright 2017
mathieu.poux[a]gmail.com
jammetthomas[a]gmail.com

This program is free software: you can redistribute it and/or
modify it under the terms of the the Mozilla Public License v2.0.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
Mozilla Public License v. 2.0 received along this program for more
details (or else see http://mozilla.org/MPL/2.0/).

*/


#include "Mona/TerminateSignal.h"
#if !defined(_WIN32)
	#include <unistd.h>
#endif

namespace Mona {


#if defined(_WIN32)

TerminateSignal::TerminateSignal() {}


void TerminateSignal::set() {
	_terminate.set();
}

void TerminateSignal::wait() {
	_terminate.wait();
}

#else

TerminateSignal::TerminateSignal() {
	// In the constructor (must be done in first) because it's inheriting between threads!
	sigemptyset(&_signalSet);
	sigaddset(&_signalSet, SIGINT);
	sigaddset(&_signalSet, SIGQUIT);
	sigaddset(&_signalSet, SIGTERM);
	pthread_sigmask(SIG_BLOCK, &_signalSet, NULL);
}

void TerminateSignal::set() {
	kill(getpid(),SIGTERM);
}

void TerminateSignal::wait() {
	int signal;
	sigwait(&_signalSet, &signal);
}


#endif

} // namespace Mona
