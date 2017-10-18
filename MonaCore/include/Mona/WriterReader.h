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


struct WriterReader : DataReader, virtual Object {
	WriterReader() : _rest(true) {}

	void reset() { _rest = true; }
private:
	virtual bool write(DataWriter& writer) = 0;

	UInt8 followingType() { return _rest ? OTHER : END; }
	bool readOne(UInt8 type, DataWriter& writer) { return _rest = write(writer); }

	bool _rest;
};



} // namespace Mona
