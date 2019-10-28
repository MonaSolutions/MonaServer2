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
#include "Mona/Time.h"
#include "Mona/Net.h"

namespace Mona {

/*!
Tool to compute queue congestion */
struct Congestion : virtual Object {
	NULLABLE(!self(Net::RTO_INIT))

	Congestion() : _lastQueueing(0), _congested(0) {}

	// Wait RTO time by default (3 sec) => sounds right with socket and file
	UInt32 operator()(UInt32 duration = Net::RTO_INIT) const;

	Congestion& operator=(UInt64 queueing);
private:
	UInt64	_lastQueueing;
	Time	_congested;
};

} // namespace Mona

