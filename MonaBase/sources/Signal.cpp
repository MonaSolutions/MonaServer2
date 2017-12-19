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

#include "Mona/Signal.h"
#include "Mona/Exceptions.h"


namespace Mona {

using namespace std;

bool Signal::wait(UInt32 millisec) {
	unique_lock<mutex> lock(_mutex);
#if !defined(_DEBUG)
	try {
#endif
		if (millisec) {
			auto timeout(chrono::system_clock::now() + chrono::milliseconds(millisec));
			while (!_set && _condition.wait_until(lock, timeout) == cv_status::no_timeout);
		} else while (!_set)
			_condition.wait(lock);

#if !defined(_DEBUG)
	} catch (exception& exc) {
		FATAL_ERROR("Wait signal failed, ", exc.what());
	} catch (...) {
		FATAL_ERROR("Wait signal failed, unknown error");
	}
#endif
	if (!_set)
		return false;
	if (_autoReset)
		_set = false;
	return true;
}

void Signal::set() {
	lock_guard<mutex> lock(_mutex);
	_set = true;
	_condition.notify_all();
}


void Signal::reset() {
	lock_guard<mutex> lock(_mutex);
	_set = false;
}


} // namespace Mona
