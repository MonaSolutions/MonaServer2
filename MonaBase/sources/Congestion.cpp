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

#include "Mona/Congestion.h"

using namespace std;

namespace Mona {

bool Congestion::operator()(UInt64 queueing, UInt32 duration) {
	bool congested(queueing && queueing>=_lastQueueing); // > or = because control a state, so a double call to Congestion(queueing) is accepted and must not cancel current congestion
	_lastQueueing = queueing;
	if (congested) {
		// congestion
		if (_congested) {
			UInt32 elapsed = UInt32(_congested.elapsed());
			if (elapsed > duration)
				return true;
		} else
			_congested.update();
	} else
		_congested = 0;
	return false;
}


} // namespace Mona
