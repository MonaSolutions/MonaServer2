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
#include "Mona/ByteRate.h"

namespace Mona {

/*!
Tool to compute queue congestion */
struct Congestion : public virtual Object {
	
	Congestion() : _congested(0) {}

	operator UInt32() const;

	Congestion& operator+=(UInt32 bytes) { _queueRate += bytes; return *this; }
	Congestion& operator++() { ++_queueRate; return *this; }

	Congestion& operator-=(UInt32 bytes) { _dequeRate += bytes; return *this; }
	Congestion& operator--() { ++_dequeRate; return *this; }

	static const Congestion& Null() { static Congestion Congestion;  return Congestion; }

private:
	ByteRate					_queueRate;
	ByteRate					_dequeRate;
	mutable std::atomic<UInt64>	_congested;
};

} // namespace Mona

