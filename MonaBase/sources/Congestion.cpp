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
#include "Mona/Net.h"

namespace Mona {

Congestion::operator UInt32() const {
	Int64 congested = _congested;
	if (_queueRate > _dequeRate) {
		// congestion
		if (congested) {
			UInt32 elapsed = UInt32(Time::Now() - congested);
			if(elapsed > Net::RTO_INIT)
				return elapsed; // Wait RTO time (to expect response ping time)
		} else
			_congested = Time::Now();
	} else if (congested)
		_congested = 0;
	return 0;
}

} // namespace Mona
