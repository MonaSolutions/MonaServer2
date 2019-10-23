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

#include "Mona/DataWriter.h"
#include "Mona/DataReader.h"

using namespace std;


namespace Mona {

DataWriter& DataWriter::Null() {
	static struct DataWriterNull : DataWriter, virtual Object {
		DataWriterNull() {}
		
		void writePropertyName(const char* value) {}

		void   writeNumber(double value) {}
		void   writeString(const char* value, UInt32 size) {}
		void   writeBoolean(bool value) {}
		void   writeNull() {}
		UInt64 writeDate(const Date& date) { return 0; }
		UInt64 writeByte(const Packet& bytes) { return 0; }
	} Null;
	return Null;
}

void DataWriter::writeValue(UInt8 type, const char* value, UInt32 size, double number) {
	switch (type) {
		case DataReader::STRING:
			writeString(value, size);
			break;
		case DataReader::NUMBER:
			writeNumber(number);
			break;
		case DataReader::BOOLEAN:
			writeBoolean(number != 0);
			break;
		case DataReader::DATE:
			writeDate((Int64)number);
			break;
		default:
			writeNull();
		break;
	}
}
void DataWriter::writeValue(const char* value, UInt32 size) {
	double number;
	UInt8 type = DataReader::ParseValue(value, size, number);
	writeValue(type, value, size, number);
}

} // namespace Mona
