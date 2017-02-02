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

#include "Mona/BinaryWriter.h"
#include "Mona/Util.h"


using namespace std;


namespace Mona {

BinaryWriter::BinaryWriter(UInt8* buffer, UInt32 size,Byte::Order byteOrder) : _offset(0), _pBuffer(new Buffer(buffer,size)), _reference(false) {
	_pBuffer->resize(0);
#if defined(_ARCH_BIG_ENDIAN)
	_flipBytes = byteOrder == Byte::ORDER_LITTLE_ENDIAN;
#else
    _flipBytes = byteOrder == Byte::ORDER_BIG_ENDIAN;;
#endif
}
BinaryWriter::BinaryWriter(Buffer& buffer,  Byte::Order byteOrder) : _offset(buffer.size()), _pBuffer(&buffer), _reference(true) {
#if defined(_ARCH_BIG_ENDIAN)
	_flipBytes = byteOrder == Byte::ORDER_LITTLE_ENDIAN;
#else
    _flipBytes = byteOrder == Byte::ORDER_BIG_ENDIAN;;
#endif
}

BinaryWriter::~BinaryWriter() {
	if (!_reference)
		delete _pBuffer;
}

UInt8* BinaryWriter::buffer(UInt32 size) {
	UInt32 oldSize(_pBuffer->size());
	_pBuffer->resize(oldSize+size);
	return _pBuffer->data() + oldSize;
}

BinaryWriter& BinaryWriter::writeRandom(UInt32 count) {
	while(count--)
		write8(Util::Random<UInt8>());
	return *this;
}

BinaryWriter& BinaryWriter::write7BitEncoded(UInt32 value) {
	do {
		unsigned char c = (unsigned char)(value & 0x7F);
		value >>= 7;
		if (value)
			c |= 0x80;
		write8(c);
	} while (value);
	return *this;
}

BinaryWriter& BinaryWriter::write16(UInt16 value) {
	if (_flipBytes)
		value = Byte::Flip16(value);
	return write(&value, sizeof(value));
}

BinaryWriter& BinaryWriter::write24(UInt32 value) {
	if (_flipBytes)
		value = Byte::Flip24(value);
	return write(&value, 3);
}

BinaryWriter& BinaryWriter::write32(UInt32 value) {
	if (_flipBytes)
		value = Byte::Flip32(value);
	return write(&value, sizeof(value));
}

BinaryWriter& BinaryWriter::write64(UInt64 value) {
	if (_flipBytes)
		value = Byte::Flip64(value);
	return write(&value, sizeof(value));
}

BinaryWriter& BinaryWriter::writeDouble(double value) {
	if (_flipBytes)
		value = Byte::Flip(value);
	return write(&value, sizeof(value));
}
BinaryWriter& BinaryWriter::writeFloat(float value) {
	if (_flipBytes)
		value = Byte::Flip(value);
	return write(&value, sizeof(value));
}


BinaryWriter& BinaryWriter::write7BitValue(UInt32 value) {
	UInt8 shift = (Get7BitValueSize(value)-1)*7;
	bool max = false;
	if(shift>=21) { // 4 bytes maximum
		shift = 22;
		max = true;
	}

	while(shift>=7) {
		write8(0x80 | ((value>>shift)&0x7F));
		shift -= 7;
	}
	return write8(max ? value&0xFF : value&0x7F);
}

BinaryWriter& BinaryWriter::write7BitLongValue(UInt64 value) {
	UInt8 shift = (Get7BitValueSize(value)-1)*7;
	bool max = shift>=63; // Can give 10 bytes!
	if(max)
		++shift;

	while(shift>=7) {
		write8(0x80 | ((value>>shift)&0x7F));
		shift -= 7;
	}
	return write8(max ? value&0xFF : value&0x7F);
}


} // namespace Mona
