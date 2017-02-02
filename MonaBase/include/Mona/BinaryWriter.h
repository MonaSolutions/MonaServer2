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
#include "Mona/Byte.h"
#include "Mona/Buffer.h"

namespace Mona {

struct BinaryWriter : Binary, virtual Object {

	BinaryWriter(UInt8* buffer, UInt32 size, Byte::Order byteOrder = Byte::ORDER_NETWORK);
	BinaryWriter(Buffer& buffer, Byte::Order byteOrder = Byte::ORDER_NETWORK);
	virtual ~BinaryWriter();

	BinaryWriter& append(const void* data, UInt32 size) { _pBuffer->append(data, size); return *this; }
	BinaryWriter& write(const void* data, UInt32 size) {return append(data, size); }
	BinaryWriter& write(const char* value) { return write(value, strlen(value)); }
	BinaryWriter& write(char* value)	   { return write(value, strlen(value)); } // required cause the following template signature which catch "char *" on posix
	BinaryWriter& write(const std::string& value) { return write(value.data(), (UInt32)value.size()); }
	BinaryWriter& write(const Binary& value) { return write(value.data(),(UInt32)value.size()); }
	BinaryWriter& write(char value) { return write(&value, sizeof(value)); }

	BinaryWriter& write8(UInt8 value) { return write(&value, sizeof(value)); }
	BinaryWriter& write16(UInt16 value);
	BinaryWriter& write24(UInt32 value);
	BinaryWriter& write32(UInt32 value);
	BinaryWriter& write64(UInt64 value);
	BinaryWriter& writeDouble(double value);
	BinaryWriter& writeFloat(float value);
	BinaryWriter& writeString(const std::string& value) { write7BitEncoded(value.size()); return write(value); }
	BinaryWriter& writeString(const char* value) { UInt32 size(strlen(value)); write7BitEncoded(size); return write(value, size); }
	BinaryWriter& write7BitEncoded(UInt32 value);
	BinaryWriter& write7BitValue(UInt32 value);
	BinaryWriter& write7BitLongValue(UInt64 value);
	BinaryWriter& writeBool(bool value) { return write8(value ? 1 : 0); }

	BinaryWriter& writeRandom(UInt32 count=1);

	BinaryWriter& next(UInt32 count = 1) { return resize(size() + count); }
	BinaryWriter& clear(UInt32 size = 0) { return resize(size); }
	BinaryWriter& resize(UInt32 size) { if (_pBuffer->data()) _pBuffer->resize(_offset + size,true); return *this; }
	BinaryWriter& clip(UInt32 offset) { if (_pBuffer->data()) _pBuffer->clip(_offset + offset); return *this; }

	// method doesn't supported by BinaryWriter::Null or BinaryWriter static, raise an exception
	UInt8*	buffer(UInt32 size);

	// beware, data() can be null in the BinaryWriter::Null case
	const UInt8*  data() const { return _pBuffer->data() + _offset; }
	UInt32		  size() const { return _pBuffer->size() - _offset; }
	Buffer&		  buffer() { return *_pBuffer; }

	static BinaryWriter& Null() { static BinaryWriter Null(Buffer::Null()); return Null; }

private:

	bool			_flipBytes;
	mutable Buffer*	_pBuffer;
	bool			_reference;
	UInt32			_offset;
};



} // namespace Mona
