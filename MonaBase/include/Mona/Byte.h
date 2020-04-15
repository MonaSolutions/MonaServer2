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

struct Byte : virtual Static {

	 enum Order {
        ORDER_BIG_ENDIAN=1, // network order!
		ORDER_LITTLE_ENDIAN,
		ORDER_NETWORK=ORDER_BIG_ENDIAN,
#if (__BIG_ENDIAN__)
		ORDER_NATIVE=ORDER_BIG_ENDIAN
#else
		ORDER_NATIVE=ORDER_LITTLE_ENDIAN
#endif
	 };

	
	static UInt16 Flip16(UInt16 value) { return ((value >> 8) & 0x00FF) | ((value << 8) & 0xFF00); }
	static UInt32 Flip24(UInt32 value) { return ((value >> 16) & 0x000000FF) | (value & 0x0000FF00) | ((value << 16) & 0x00FF0000); }
	static UInt32 Flip32(UInt32 value) { return ((value >> 24) & 0x000000FF) | ((value >> 8) & 0x0000FF00) | ((value << 8) & 0x00FF0000) | ((value << 24) & 0xFF000000); }
	static UInt64 Flip64(UInt64 value) { UInt32 hi = UInt32(value >> 32); UInt32 lo = UInt32(value & 0xFFFFFFFF); return UInt64(Flip32(hi)) | (UInt64(Flip32(lo)) << 32); }
	template<typename NumType>
	static NumType Flip(NumType value) { UInt8 *lo = (UInt8*)&value; UInt8 *hi = (UInt8*)&value + sizeof(value) - 1; UInt8 swap; while (lo < hi) { swap = *lo; *lo++ = *hi; *hi-- = swap; } return value; }

	static UInt16 From16Network(UInt16 value) { return From16BigEndian(value); }
	static UInt32 From24Network(UInt32 value) { return From24BigEndian(value); }
	static UInt32 From32Network(UInt32 value) { return From32BigEndian(value); }
	static UInt64 From64Network(UInt64 value) { return From64BigEndian(value); }
	template<typename NumType>
	static NumType FromNetwork(NumType value) { return FromBigEndian(value); }

	static UInt16 To16Network(UInt16 value) { return From16BigEndian(value); }
	static UInt32 To24Network(UInt32 value) { return From24BigEndian(value); }
	static UInt32 To32Network(UInt32 value) { return From32BigEndian(value); }
	static UInt64 To64Network(UInt64 value) { return From64BigEndian(value); }
	template<typename NumType>
	static NumType ToNetwork(NumType value) { return FromBigEndian(value); }

	static UInt16 From16BigEndian(UInt16 value) { return ORDER_NATIVE==ORDER_BIG_ENDIAN ? value : Flip16(value); }
	static UInt32 From24BigEndian(UInt32 value) { return ORDER_NATIVE==ORDER_BIG_ENDIAN ? value : Flip24(value); }
	static UInt32 From32BigEndian(UInt32 value) { return ORDER_NATIVE==ORDER_BIG_ENDIAN ? value : Flip32(value); }
	static UInt64 From64BigEndian(UInt64 value) { return ORDER_NATIVE==ORDER_BIG_ENDIAN ? value : Flip64(value); }
	template<typename NumType>
	static NumType FromBigEndian(NumType value) { return ORDER_NATIVE == ORDER_BIG_ENDIAN ? value : Flip(value); }

	static UInt16 To16BigEndian(UInt16 value) { return ORDER_NATIVE==ORDER_BIG_ENDIAN ? value : Flip16(value); }
	static UInt32 To24BigEndian(UInt32 value) { return ORDER_NATIVE==ORDER_BIG_ENDIAN ? value : Flip24(value); }
	static UInt32 To32BigEndian(UInt32 value) { return ORDER_NATIVE==ORDER_BIG_ENDIAN ? value : Flip32(value); }
	static UInt64 To64BigEndian(UInt64 value) { return ORDER_NATIVE==ORDER_BIG_ENDIAN ? value : Flip64(value); }
	template<typename NumType>
	static NumType ToBigEndian(NumType value) { return ORDER_NATIVE == ORDER_BIG_ENDIAN ? value : Flip(value); }

	static UInt16 From16LittleEndian(UInt16 value) { return ORDER_NATIVE==ORDER_LITTLE_ENDIAN ? value : Flip16(value); }
	static UInt32 From24LittleEndian(UInt32 value) { return ORDER_NATIVE==ORDER_LITTLE_ENDIAN ? value : Flip24(value); }
	static UInt32 From32LittleEndian(UInt32 value) { return ORDER_NATIVE==ORDER_LITTLE_ENDIAN ? value : Flip32(value); }
	static UInt64 From64LittleEndian(UInt64 value) { return ORDER_NATIVE==ORDER_LITTLE_ENDIAN ? value : Flip64(value); }
	template<typename NumType>
	static NumType FromLittleEndian(NumType value) { return ORDER_NATIVE == ORDER_LITTLE_ENDIAN ? value : Flip(value); }

	static UInt16 To16LittleEndian(UInt16 value) { return ORDER_NATIVE==ORDER_LITTLE_ENDIAN ? value : Flip16(value); }
	static UInt32 To24LittleEndian(UInt32 value) { return ORDER_NATIVE==ORDER_LITTLE_ENDIAN ? value : Flip24(value); }
	static UInt32 To32LittleEndian(UInt32 value) { return ORDER_NATIVE==ORDER_LITTLE_ENDIAN ? value : Flip32(value); }
	static UInt64 To64LittleEndian(UInt64 value) { return ORDER_NATIVE==ORDER_LITTLE_ENDIAN ? value : Flip64(value); }
	template<typename NumType>
	static NumType ToLittleEndian(NumType value) { return ORDER_NATIVE == ORDER_LITTLE_ENDIAN ? value : Flip(value); }
};


} // namespace Mona
