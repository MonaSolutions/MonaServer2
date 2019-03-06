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

BinaryWriter::BinaryWriter(UInt8* buffer, UInt32 size, Byte::Order byteOrder) :
	_offset(0), _pBuffer(new Buffer(size, buffer)), _reference(false), _flipBytes(byteOrder != Byte::ORDER_NATIVE) {
	_pBuffer->resize(0);
}
BinaryWriter::BinaryWriter(Buffer& buffer, Byte::Order byteOrder) :
	_offset(buffer.size()), _pBuffer(&buffer), _reference(true), _flipBytes(byteOrder != Byte::ORDER_NATIVE) {
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

template<typename ValueType>
BinaryWriter& BinaryWriter::write7Bit(typename common_type<ValueType>::type value, UInt8 bytes) {
	if (!bytes)
		return self;
	UInt8 bits = (bytes - 1) * 7 + 1;
	if (!(value >> (bits - 1))) {
		bits -= 8;
		while (!(value >> bits) && (bits -= 7));
	}
	while (bits>1) {
		write8(0x80 | UInt8(value >> bits));
		bits -= 7;
	}
	return write8(value & (bits ? 0xFF : 0x7F));
}
template BinaryWriter& BinaryWriter::write7Bit<UInt16>(common_type<UInt16>::type value, UInt8 bytes);
template BinaryWriter& BinaryWriter::write7Bit<UInt32>(common_type<UInt32>::type value, UInt8 bytes);
template BinaryWriter& BinaryWriter::write7Bit<UInt64>(common_type<UInt64>::type value, UInt8 bytes);


} // namespace Mona
