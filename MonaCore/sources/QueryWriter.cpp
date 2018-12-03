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

#include "Mona/QueryWriter.h"


using namespace std;


namespace Mona {

const char* QueryWriter::query() const {
	if (_query)
		return _query;
	BinaryWriter& writer((BinaryWriter&)DataWriter::writer);
	writer.write8(0);
	writer.clear(writer.size() - 1);
	return _query = STR writer.data();
}

BinaryWriter& QueryWriter::write() {
	_query = NULL;
	if (_isProperty) {
		writer.write('=');
		_isProperty = false;
	} else if (!_first)
		writer.write(separator); // &
	else
		_first = false;
	return writer;
}

void QueryWriter::writePropertyName(const char* value) {
	writeString(value, strlen(value));
	_isProperty = true;
}

void QueryWriter::writeString(const char* value, UInt32 size) {
	if (uriChars)
		Util::EncodeURI(value, size, write());
	else
		write().write(value, size);
}


} // namespace Mona
