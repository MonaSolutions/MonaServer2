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
#include "Mona/Packet.h"

namespace Mona {

/*!
Allow to get a binary content without any copy, so make senses or "bytes" and "string" just, reset packet for any other type (means no binary or no string data!) */
struct PacketWriter : DataWriter, Packet, virtual Object {
	using Packet::Packet;

	void   writePropertyName(const char* value) {}

	void   writeNumber(double value) { reset(); }
	void   writeString(const char* value, UInt32 size) { set(*this, BIN value, size); }
	void   writeBoolean(bool value) { reset(); }
	void   writeNull() { reset(); }
	UInt64 writeDate(const Date& date) { reset();  return 0; }
	UInt64 writeBytes(const UInt8* data, UInt32 size) { set(*this, data, size); return 0; }

	operator bool() const { return Packet::operator bool(); }
	void   clear() { reset(); }
private:
	PacketWriter() {} // no sense without input data!
};


} // namespace Mona
