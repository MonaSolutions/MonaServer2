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
Thread Safe class to compute ByteRate */
struct LostRate : virtual Object {
	LostRate(ByteRate& byteRate) : _byteRate(byteRate) {}
	/*!
	Add bytes to next byte rate calculation */
	LostRate& operator+=(UInt32 lost) { _lostRate += lost; return *this; }
	LostRate& operator++() { ++_lostRate; return *this; }
	LostRate& operator=(UInt32 lost) { _lostRate = lost; return *this; }
	/*!
	Returns byte rate */
	operator double() const { double lost(_lostRate.exact()); return lost ? (lost / (lost + _byteRate.exact())) : 0; }
	double operator()() const { return this->operator double(); }

private:
	ByteRate& _byteRate;
	ByteRate  _lostRate;
};



} // namespace Mona
