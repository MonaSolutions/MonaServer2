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
#include "Mona/Util.h"

namespace Mona {


struct QueryWriter : DataWriter, virtual Object {
	QueryWriter(Buffer& buffer) : DataWriter(buffer), _query(NULL), _first(true), _isProperty(false) {}

	const char* query() const;

	void   writePropertyName(const char* value);

	void   writeNumber(double value) { String::Append(writer(), value); }
	void   writeString(const char* value, UInt32 size) { Util::EncodeURI(value,size, writer()); }
	void   writeBoolean(bool value) { writer().write(value ? "true" : "false"); }
	void   writeNull() { writer().write("null",4); }
	UInt64 writeDate(const Date& date) { String::Append(writer(), String::Date(date, Date::FORMAT_ISO8601_SHORT)); return 0; }
	UInt64 writeBytes(const UInt8* data, UInt32 size) { Util::ToBase64(data, size, writer()); return 0; }

	void clear() { _isProperty = false; _first = true; _query = NULL;  DataWriter::clear(); }
	
private:
	BinaryWriter& writer();

	bool				_isProperty;
	bool				_first;
	mutable const char* _query;
};



} // namespace Mona
