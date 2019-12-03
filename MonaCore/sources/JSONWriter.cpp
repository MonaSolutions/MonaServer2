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

#include "Mona/JSONWriter.h"
#include "Mona/Util.h"
#include "Mona/Logs.h"


using namespace std;


namespace Mona {


JSONWriter::JSONWriter(Buffer& buffer) : DataWriter(buffer),_first(true),_layers(0) {
	writer.write("[]");
}

JSONWriter::~JSONWriter() {
	if (_layers)
		WARN(_layers, " JSON container not closed");
}

void JSONWriter::reset() {
	_first=true;
	_layers=0;
	DataWriter::reset();
	writer.write("[]");
}

UInt64 JSONWriter::beginObject(const char* type) {
	start(true);

	writer.write8('{');
	if(!type)
		return 0;
	writePropertyName("__type");
	writeString(type,strlen(type));
	return 0;
}

void JSONWriter::endObject() {

	writer.write8('}');

	end(true);
}

UInt64 JSONWriter::beginArray(UInt32 size) {
	start(true);

	writer.write8('[');
	return 0;
}

void JSONWriter::endArray() {

	writer.write8(']');

	end(true);
}


void JSONWriter::writePropertyName(const char* value) {
	writeString(value,strlen(value));
	writer.write8(':');
	_first=true;
}


void JSONWriter::writeString(const char* value, UInt32 size) {
	start();
	writer.write8('"');
	// Convert to UTF8 and replace \n by \\n
	char  buffer[2];
	const char* begin(value);
	while (size--) {
		switch (*value) {
			case '"':
			case '\\':
				buffer[0] = '\\';
				buffer[1] = *value;
				break;
			case '\n':
				memcpy(buffer, EXPAND("\\n"));
				break;
			case '\b':
				memcpy(buffer, EXPAND("\\b"));
				break;
			case '\f':
				memcpy(buffer, EXPAND("\\f"));
				break;
			case '\r':
				memcpy(buffer, EXPAND("\\r"));
				break;
			case '\t':
				memcpy(buffer, EXPAND("\\t"));
				break;
			default:
				if (!String::ToUTF8(*value, buffer))
					break;
				++value;
				continue;
		}
		if (value>begin)
			writer.write(begin, value - begin);
		writer.write(buffer, sizeof(buffer));
		begin = ++value;
	}
	if (value>begin)
		writer.write(begin, value - begin);

	writer.write8('"');
	end();
}

UInt64 JSONWriter::writeDate(const Date& date) {
	// Write directly because is necessary in UTF8 already!
	start();
	String::Append(writer, '"', String::Date(date, Date::FORMAT_ISO8601_FRAC), '"');
	end();
	return 0;
}



UInt64 JSONWriter::writeByte(const Packet& bytes) {
	start();

	writer.write(EXPAND("{\"__bin\":\""));
	Util::ToBase64(bytes.data(), bytes.size(), writer,true);
	writer.write("\"}");

	end();
	return 0;
}

void JSONWriter::start(bool isContainer) {

	// remove the last ']'
	if (_layers == 0)
		writer.clear(writer.size() - 1);


	if(!_first)
		writer.write8(',');

	if (isContainer) {
		_first=true;
		++_layers;
	}
}

void JSONWriter::end(bool isContainer) {

	if (isContainer) {
		if (_layers==0) {
			ERROR("JSON container already closed")
			return;
		}
		--_layers;
	}
	
	if(_first)
		_first=false;

	if (_layers==0)
		writer.write8(']');
}


} // namespace Mona
