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


struct QueryReader : DataReader, virtual Object {
	QueryReader(const Packet& packet) :  DataReader(packet), _type(END) {}
	QueryReader(const char* data, std::size_t size=std::string::npos) : DataReader(Packet(data, size==std::string::npos ? strlen(data) : size)), _type(END) {}

	void		reset() { DataReader::reset(); _type = END; }

private:
	enum {
		OBJECT =	OTHER
	};
	
	UInt8	followingType();
	bool	readOne(UInt8 type, DataWriter& writer);

	std::string _property;
	std::string _value;
	double		_number;
	UInt8		_type;
};


} // namespace Mona
