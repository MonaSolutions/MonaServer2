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

namespace Mona {

struct Binary : virtual NullableObject {
	virtual const UInt8*	data() const = 0;
	virtual UInt32			size() const = 0;

	explicit operator bool() const { return data() && size(); }

	// max size = 5
	static UInt8 Get7BitValueSize(UInt32 value) { UInt8 result(1); while (value >>= 7) ++result; return result; }
	// max size = 10
	static UInt8 Get7BitValueSize(UInt64 value) { UInt8 result(1); while (value >>= 7) ++result; return result; }
};


} // namespace Mona
