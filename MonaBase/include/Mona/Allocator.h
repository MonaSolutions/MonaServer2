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

struct Allocator : virtual Object {
	virtual UInt8* allocate(UInt32& size) const { return new UInt8[size](); }
	virtual void   deallocate(UInt8* buffer, UInt32 size) const { delete [] buffer; }

	static Allocator& Default() { static Allocator Default; return Default; }
};


} // namespace Mona
