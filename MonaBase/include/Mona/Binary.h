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

struct Binary : virtual Object {
	NULLABLE(!size())

	virtual const UInt8*	data() const = 0;
	virtual UInt32			size() const = 0;

	template<typename ValueType>
	static UInt8 Get7BitSize(typename std::common_type<ValueType>::type value, UInt8 bytes = sizeof(ValueType) + 1) {
		UInt8 result(1);
		while ((value >>= 7) && result++<bytes);
		return result-(value ? 1 : 0); // 8th bit
	}
};


} // namespace Mona
