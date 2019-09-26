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
#include <atomic>

namespace Mona {

/*!
Thread Safe class to compute ByteRate */
struct ByteRate : virtual Object {
	ByteRate(UInt8 delta=1) : _delta(delta*1000), _bytes(0), _rate(0), _time(Time::Now()) {}

	/*!
	Add bytes to next byte rate calculation */
	ByteRate& operator+=(UInt32 bytes) { _bytes += bytes; return *this; }
	ByteRate& operator++() { ++_bytes; return *this; }
	ByteRate& operator=(UInt32 bytes) { _bytes = bytes; return *this; }

	/*!
	Returns byte rate */
	operator UInt64() const { compute();  return UInt64(_rate); }
	UInt64 operator()() const { return this->operator UInt64(); }
	double   exact() const { compute(); return double(_rate); }

private:
	void compute() const {
		Int64 elapsed(Time::Now());
		elapsed = elapsed - _time.exchange(elapsed);
		if (elapsed <= _delta) // wait "_delta" before next compute rate
			_time -= elapsed;
		else
			_rate = _bytes.exchange(0) * 1000.0 / elapsed;
	}

	mutable std::atomic<UInt64>	_bytes;
	mutable std::atomic<double>	_rate;
	mutable std::atomic<Int64>	_time;
	const Int32					_delta;
};



} // namespace Mona
