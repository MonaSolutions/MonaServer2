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


struct QueryWriter : DataWriter, virtual Object {
	QueryWriter(Buffer& buffer) : DataWriter(buffer), _pBuffer(NULL), _first(true), _isProperty(false),
		separator("&"), dateFormat(Date::FORMAT_ISO8601_SHORT), uriChars(true) {}
	QueryWriter(std::string& buffer) : _pBuffer(&buffer), _first(true), _isProperty(false),
		separator("&"), dateFormat(Date::FORMAT_ISO8601_SHORT), uriChars(true) {}

	bool  uriChars;
	const char* dateFormat;
	const char*	separator;

	void   writePropertyName(const char* value);

	void   writeNumber(double value) { write(value); }
	void   writeString(const char* value, UInt32 size);
	void   writeBoolean(bool value) { write(value ? "true" : "false"); }
	void   writeNull() { write("null"); }
	UInt64 writeDate(const Date& date) { write(String::Date(date, dateFormat)); return 0; }
	UInt64 writeByte(const Packet& bytes);

	void reset();


private:
	template <typename ...Args>
	void write(Args&&... args) {
		if (_isProperty) {
			writeBuffer('=');
			_isProperty = false;
		} else if (!_first)
			writeBuffer(separator); // &
		else
			_first = false;
		writeBuffer(std::forward<Args>(args)...);
	}
	template <typename ...Args>
	void writeBuffer(Args&&... args) {
		if (_pBuffer)
			String::Append(*_pBuffer, std::forward<Args>(args)...);
		else
			String::Append(writer, std::forward<Args>(args)...);
	}

	bool				_isProperty;
	bool				_first;
	std::string*		_pBuffer;
};



} // namespace Mona
