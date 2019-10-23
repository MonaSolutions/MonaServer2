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

#include "Mona/DataReader.h"
#include "Mona/Logs.h"

using namespace std;


namespace Mona {

DataReader& DataReader::Null() {
	static struct DataReaderNull : DataReader, virtual Object {
		DataReaderNull() : DataReader(Packet::Null()) {}
		bool	readOne(UInt8 type, DataWriter& writer) { return false; }
	} Null;
	return Null;
}

UInt8 DataReader::ParseValue(const char* value, UInt32 size, double& number) {
	if (String::ICompare(value, size, "null") == 0)
		return NIL;
	if (String::ICompare(value, size, "false") == 0) {
		number = 0;
		return BOOLEAN;
	}
	if (String::ICompare(value, size, "true") == 0) {
		number = 1;
		return BOOLEAN;
	}
	Exception ex;
	thread_local Date date;
	if (date.update(ex, value, size)) {
		number = (double)date;
		if (ex)
			WARN("Parse date, ", ex);
		return DATE;
	}
	if (String::ToNumber(value, number))
		return NUMBER;
	return STRING;
}

bool DataReader::readNext(DataWriter& writer) {
	UInt8 type(nextType());
	_nextType = END; // to prevent recursive readNext call (and refresh followingType call)
	if(type!=END)
		return readOne(type, writer);
	return false;
}


UInt32 DataReader::read(DataWriter& writer, UInt32 count) {
	bool all(count == END);
	UInt32 results(0);
	while ((all || count-- > 0) && readNext(writer))
		++results;
	return results;
}

bool DataReader::read(UInt8 type, DataWriter& writer) {
	if (nextType() != type)
		return false;
	UInt32 count(read(writer, 1));
	if (count>1) {
		WARN(typeof(*this), " has written many object for just one reading of type ",type);
		return true;
	}
	return count==1;
}

} // namespace Mona
