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

#include "Mona/Allocator.h"

using namespace std;

namespace Mona {


UInt8* Allocator::allocate(UInt32& size) {
	UInt32 capacity = ComputeCapacity(size);
	return new UInt8[capacity ? (size=capacity) : size]; // if =0 exceeds UInt32, unchange size wanted!
}


UInt32 Allocator::ComputeCapacity(UInt32 size) {
	if (size <= 16) // at minimum allocate 16 bytes!
		return 16;
	// fast compute the closer upper power of two for new capacity
	UInt32 capacity = size - 1; // at minimum allocate 16 bytes!
	capacity |= capacity >> 1;
	capacity |= capacity >> 2;
	capacity |= capacity >> 4;
	capacity |= capacity >> 8;
	capacity |= capacity >> 16;
	return ++capacity;  // if =0 exceeds UInt32
}

} // namespace Mona
