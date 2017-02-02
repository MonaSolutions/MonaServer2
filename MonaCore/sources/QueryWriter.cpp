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

BinaryWriter& QueryWriter::writer() {
	_query = NULL;
	if (_isProperty) {
		DataWriter::writer.write("=");
		_isProperty = false;
	} else if (!_first)
		DataWriter::writer.write("&");
	else
		_first = false;
	return DataWriter::writer;
}

void QueryWriter::writePropertyName(const char* value) {
	if (!_first)
		DataWriter::writer.write("&"); 
	else
		_first = false;
	Util::EncodeURI(value, DataWriter::writer);
	_isProperty = true;
}


} // namespace Mona
