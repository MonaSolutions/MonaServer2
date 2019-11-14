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

template<typename BufferType=Buffer>
struct StringWriter : DataWriter, virtual Object {

	StringWriter(Buffer& buffer, bool append=true) : DataWriter(buffer), _buffer(Buffer::Null()), _append(append) {}
	StringWriter(typename std::conditional<std::is_same<BufferType, Buffer>::value, std::nullptr_t, BufferType>::type& buffer, bool append = true) : _buffer(buffer), _append(append) {}

	void   writePropertyName(const char* name) { append(name, std::strlen(name));  }

	void   writeNumber(double value) { String::Append(self, value); }
	void   writeString(const char* value, UInt32 size) { append(value, size); }
	void   writeBoolean(bool value) { if (value) append(EXPAND("true")); else append(EXPAND("false")); }
	void   writeNull() { append(EXPAND("null")); }
	UInt64 writeDate(const Date& date) { date.format(Date::FORMAT_SORTABLE, self); return 0; }
	UInt64 writeByte(const Packet& bytes) { append(bytes.data(), bytes.size()); return 0; }

	virtual void clear() { _buffer.clear(); writer.clear(); }
	virtual StringWriter& append(const void* value, UInt32 size) { if (!_append) clear(); _buffer.append(STR value, size); writer.append(value, size); return self; }

protected:
	StringWriter() : _buffer(Buffer::Null()) {}
private:
	BufferType& _buffer;
	bool		_append;
};

} // namespace Mona
