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

/*!
Stringify data, can be build with a Buffer or whatever with append and clear methods (std::string, etc...)
You can too build it with a Buffer::Null() and overloads reset/append/size/resize to custom writing */
template<typename BufferType=Buffer>
struct StringWriter : DataWriter, virtual Object {

	StringWriter(BufferType& buffer, bool append=true) :
		DataWriter(std::is_same<BufferType, Buffer>::value ? (Buffer&)buffer : Buffer::Null()), _buffer(buffer), _append(append), _initSize(append ? buffer.size() : 0) {}

	void   writePropertyName(const char* name) { append(name, std::strlen(name));  }

	void   writeNumber(double value) { String::Append(self, value); }
	void   writeString(const char* value, UInt32 size) { append(value, size); }
	void   writeBoolean(bool value) { String::Append(self, value); }
	void   writeNull() { append(EXPAND("null")); }
	UInt64 writeDate(const Date& date) { date.format(Date::FORMAT_SORTABLE, self); return 0; }
	UInt64 writeByte(const Packet& bytes) { append(bytes.data(), bytes.size()); return 0; }

	
	virtual void reset() {
		DataWriter::reset();
		if(_buffer.size()>_initSize)
			_buffer.resize(_initSize);
	}
	virtual StringWriter& append(const void* value, UInt32 size) {
		if (!_append) {
			_append = true; // first time just, to clear default value!
			reset();
		}
		if(!writer.append(value, size))
			_buffer.append(STR value, size);
		return self;
	}
protected:
	/*!
	Allow to inherit from StringWriter and stringify something with a custom target other than Buffer or string for example */
	StringWriter() : _buffer(Buffer::Null()), _initSize(0), _append(true) {} // append=true to avoid a call to reset if append is not overloaded

private:
	BufferType& _buffer;
	bool		_append;
	UInt32		_initSize;
};

} // namespace Mona
