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

#include "Mona/BinaryReader.h"

using namespace std;

namespace Mona {

BinaryReader BinaryReader::Null(NULL,0);

BinaryReader::BinaryReader(const UInt8* data, UInt32 size, Byte::Order byteOrder) :
	_end(data+size),_data(data),_size(size),_current(data), _flipBytes(byteOrder != Byte::ORDER_NATIVE) {
}

UInt32 BinaryReader::next(UInt32 count) {
	if (count > available())
		count = available();
	_current += count; 
	return count;
}

UInt32 BinaryReader::shrink(UInt32 available) {
	UInt32 rest(this->available());
	if (available > rest)
		available = rest;
	_end = _current+ available;
	_size = _end-_data;
	return available;
}

UInt8* BinaryReader::read(UInt32 size, UInt8* value) {
	UInt32 available(this->available());
	if (size > available)
		return value; // don't read if no size requested available (allow to know that if a read32 has worked for example, for sure 4 bytes has been readen!)
	memcpy(value, _current,size);
	_current += size;
	return value;
}

UInt32 BinaryReader::read7BitEncoded() {
	UInt8 c;
	UInt32 value = 0;
	int s = 0;
	do {
		c = read8();
		UInt32 x = (c & 0x7F);
		x <<= s;
		value += x;
		s += 7;
	} while (c & 0x80);
	return value;
}

UInt16 BinaryReader::read16() {
	UInt16 value(0);
	read(sizeof(value),BIN &value);
	if (_flipBytes)
		return Byte::Flip16(value);
	return value;
}

UInt32 BinaryReader::read24() {
	UInt32 value(0);
	read(3, BIN &value);
	if (_flipBytes)
		return Byte::Flip24(value);
	return value;
}

UInt32 BinaryReader::read32() {
	UInt32 value(0);
	read(sizeof(value), BIN &value);
	if (_flipBytes)
		return Byte::Flip32(value);
	return value;
}


UInt64 BinaryReader::read64() {
	UInt64 value(0);
	read(sizeof(value), BIN &value);
	if (_flipBytes)
		return Byte::Flip64(value);
	return value;
}

double BinaryReader::readDouble() {
	double value(0);
	read(sizeof(value), BIN &value);
	if (_flipBytes)
		return Byte::Flip(value);
	return value;
}

float BinaryReader::readFloat() {
	float value(0);
	read(sizeof(value), BIN &value);
	if (_flipBytes)
		return Byte::Flip(value);
	return value;
}

UInt32 BinaryReader::read7BitValue() {
	UInt8 n = 0;
    UInt8 b = read8();
    UInt32 result = 0;
    while ((b&0x80) && n < 3) {
        result <<= 7;
        result |= (b&0x7F);
        b = read8();
        ++n;
    }
    result <<= ((n<3) ? 7 : 8); // Use all 8 bits from the 4th byte
    result |= b;
	return result;
}

UInt64 BinaryReader::read7BitLongValue() {
	UInt8 n = 0;
    UInt8 b = read8();
    UInt64 result = 0;
    while ((b&0x80) && n < 8) {
        result <<= 7;
        result |= (b&0x7F);
        b = read8();
        ++n;
    }
    result <<= ((n<8) ? 7 : 8); // Use all 8 bits from the 4th byte
    result |= b;
	return result;
}


} // namespace Mona
