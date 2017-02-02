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

#pragma once

#include "Mona/Mona.h"
#include <condition_variable>

namespace Mona {


struct Signal : virtual Object {
	Signal(bool autoReset=true) : _autoReset(autoReset), _set(false) {}

	void set();

	// return true if the event has been set
	bool wait(UInt32 millisec = 0);

	void reset();
	
private:
	bool					_autoReset;
	bool					_set;
	std::condition_variable _condition;
	std::mutex				_mutex;
};


} // namespace Mona
