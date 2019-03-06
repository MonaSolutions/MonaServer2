/*
This file is a part of MonaSolutions Copyright 2017
mathieu.poux[a]gmail.com
jammetthomas[a]gmail.com

This program is free software: you can redistribute it and/or
modify it under the terms of the the Mozilla Public License v2.0.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
Mozilla Public License v. 2.0 received along this program for more
details (or else see http://mozilla.org/MPL/2.0/).

*/

#pragma once

#include "Mona/Mona.h"
#include "Mona/Playlist.h"

namespace Mona {

/*!
Tool to compute queue congestion */
struct M3U8 : virtual Static {

	struct Writer : Playlist::Writer, virtual Object {

		Writer(IOFile& io) : Playlist::Writer(io) {}

		Writer& open(const Path& path, bool append);
		void    write(UInt32 sequence, UInt32 duration);

	private:
		std::string _ext;
	};
};

} // namespace Mona

