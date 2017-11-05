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
#include "Mona/MediaReader.h"

namespace Mona {

/*!
[UInt32=>size][Media::Pack][...data...] */
struct MonaReader : virtual Object, MediaReader {
	MonaReader() : _size(0)  {}

private:
	UInt32  parse(Packet& buffer, Media::Source& source);
	void	onFlush(Packet& buffer, Media::Source& source);

	UInt32 _size;
};


} // namespace Mona
