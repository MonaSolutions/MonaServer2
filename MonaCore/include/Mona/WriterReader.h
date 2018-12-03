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
	bool writeOne(DataWriter& writer) {
		writer.beginObject();
		writer.writeStringProperty("content-type", EXPAND("html"));
		writer.endObject();
		writer.writeString(EXPAND("hello"));
		return false;
	}
} reader;
client.writer().writeRaw(reader); */
struct WriterReader : DataReader, virtual Object {
	WriterReader() : _rest(true), _written(0) {}

	void reset() { _rest = true; _written = 0; }

protected:
	virtual UInt8 followingType() { return _rest ? OTHER : END; }
	UInt8 written() const { return _written; }
private:
	virtual bool writeOne(DataWriter& writer) = 0;

	bool readOne(UInt8 type, DataWriter& writer) { _rest = writeOne(writer);  ++_written; return _rest; }

	bool  _rest;
	UInt8 _written;
};



} // namespace Mona
