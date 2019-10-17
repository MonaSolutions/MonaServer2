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
	HTTPHeader() : WriterReader(OTHER) {}
	void write(DataWriter& writer) {
		writer.beginObject();
		writer.writeStringProperty("content-type", EXPAND("html"));
		writer.endObject();
	}
} reader;
client.writer().writeRaw(reader); */
struct WriterReader : DataReader, virtual Object {
	WriterReader(UInt8 type) : _done(false), _type(type) {}

	void reset() { _done = false; }

protected:
	virtual UInt8 followingType() { return _done ? END : _type; }

private:
	virtual void write(DataWriter& writer) = 0;

	bool readOne(UInt8 type, DataWriter& writer) { _done = true; write(writer); return true; }

	bool  _done;
	UInt8 _type;
};



} // namespace Mona
