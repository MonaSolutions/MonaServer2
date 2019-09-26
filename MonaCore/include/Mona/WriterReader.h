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
#include "Mona/DataReader.h"


namespace Mona {

/*!
Help to custom writing operation for method which require a DataReader like Writer::writeRaw =>
struct HTTPHeader : WriterReader {
	void writeOne(DataWriter& writer, bool& again) {
		writer.beginObject();
		writer.writeStringProperty("content-type", EXPAND("html"));
		writer.endObject();
		writer.writeString(EXPAND("hello"));
	}
} reader;
client.writer().writeRaw(reader); */
struct WriterReader : DataReader, virtual Object {
	WriterReader() : _again(true), _written(0) {}

	void reset() { _again = true; _written = 0; }

protected:
	virtual UInt8 followingType() { return _again ? OTHER : END; }
	UInt8 written() const { return _written; }
private:
	virtual void writeOne(DataWriter& writer, bool& again) = 0;

	bool readOne(UInt8 type, DataWriter& writer) { _again = false; writeOne(writer, _again);  ++_written; return true; }

	bool  _again;
	UInt8 _written;
};



} // namespace Mona
