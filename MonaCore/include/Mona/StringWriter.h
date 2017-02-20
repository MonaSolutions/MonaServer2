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


struct StringWriter : DataWriter, virtual Object {

	StringWriter(Buffer& buffer) : _pString(NULL),DataWriter(buffer) {}
	StringWriter(std::string& buffer) : _pString(&buffer) {}

	UInt64 beginObject(const char* type = NULL) { return 0; }
	void   endObject() {}

	void   writePropertyName(const char* name) { append(name);  }

	UInt64 beginArray(UInt32 size) { return 0; }
	void   endArray(){}

	void   writeNumber(double value) { append(value); }
	void   writeString(const char* value, UInt32 size) { append(value,size); }
	void   writeBoolean(bool value) { append( value ? "true" : "false"); }
	void   writeNull() { writer.write("null",4); }
	UInt64 writeDate(const Date& date) { append(String::Date(date, Date::FORMAT_SORTABLE)); return 0; }
	UInt64 writeBytes(const UInt8* data, UInt32 size) { append(data, size); return 0; }

	void   clear() { if (_pString) _pString->clear(); else writer.clear(); }
private:
	void append(const void* value, UInt32 size) {
		if (_pString)
			_pString->append(STR value, size);
		else
			writer.write(value, size);
	}

	template<typename ValueType>
	void append(const ValueType& value) {
		if (_pString)
			String::Append(*_pString, value);
		else
			String::Append(writer, value);
	}

	std::string* _pString;

};



} // namespace Mona
