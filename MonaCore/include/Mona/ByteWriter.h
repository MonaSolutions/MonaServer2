/*
This file is a part of MonaSolutions Copyright 2017
mathieu.poux[a]gmail.com
jammetthomas[a]gmail.com

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License received along this program for more
details (or else see http://www.gnu.org/licenses/).

*/

#pragma once

#include "Mona/Mona.h"
#include "Mona/DataWriter.h"

namespace Mona {

struct ByteWriter : DataWriter {
	ByteWriter(Packet& bytes) : _bytes(bytes), _initBytes(bytes) {}
	void			writePropertyName(const char* value) { writeString(value, std::strlen(value)); }
	UInt64			writeDate(const Date& date) { Int64 time = date; writeByte(Packet(&time, sizeof(time))); return 0; }
	void			writeNumber(double value) { writeByte(Packet(&value, sizeof(value))); }
	void			writeString(const char* value, UInt32 size) { writeByte(Packet(value, size)); }
	void			writeBoolean(bool value) { writeByte(value ? Packet(EXPAND("1")) : Packet(EXPAND("0"))); }
	void			writeNull() { writeByte(Packet(EXPAND("\0"))); }
	virtual UInt64	writeByte(const Packet& bytes) { _bytes = std::move(bytes); return 0; }
	void			reset() { _bytes = _initBytes; }
private:
	Packet&  _bytes;
	Packet	 _initBytes;
};

} // namespace Mona
