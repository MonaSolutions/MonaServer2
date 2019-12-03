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
#include <vector>

namespace Mona {


struct XMLRPCWriter : DataWriter, virtual Object {
	XMLRPCWriter(Buffer& buffer);

	UInt64 beginObject(const char* type=NULL);
	void   writePropertyName(const char* value);
	void   endObject();

	UInt64 beginArray(UInt32 size);
	void   endArray();

	
	void   writeNumber(double value);
	void   writeString(const char* value, UInt32 size) { beginValue("string").write(value,size); endValue("string"); }
	void   writeBoolean(bool value) { beginValue("boolean").write(value ? "1" : "0", 1); endValue("boolean"); }
	void   writeNull() { beginValue(NULL); endValue(NULL); }
	UInt64 writeDate(const Date& date);
	UInt64 writeByte(const Packet& bytes) { Util::ToBase64(bytes.data(), bytes.size(), beginValue("base64"),true); endValue("base64"); return 0; }

	void   reset();
	

private:

	enum Type {
		DATA=0,
		OBJECT,
		ARRAY
	};

	void writeInt(int value) { String::Append(beginValue("int"), value); endValue("int"); }

	BinaryWriter&	beginValue(const char* tag);
	void			endValue(const char* tag);

	void start(Type type=DATA);
	void end(bool isContainer=false);
	

	std::vector<bool>	_layers;
};



} // namespace Mona
