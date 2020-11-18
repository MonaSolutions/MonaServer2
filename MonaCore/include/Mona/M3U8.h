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

struct M3U8 : virtual Static {
	/*!
	Write a M3U8 type live (memory)
	https://developer.apple.com/documentation/http_live_streaming/example_playlists_for_http_live_streaming/live_playlist_sliding_window_construction */
	static Buffer& Write(const Playlist& playlist, Buffer& buffer, bool isEvent = false);

	/*!
	Write a M3U8 type event (file)
	https://developer.apple.com/documentation/http_live_streaming/example_playlists_for_http_live_streaming/event_playlist_construction */
	struct Writer : Playlist::Writer, virtual Object {
		Writer(IOFile& io) : Playlist::Writer(io) {}
		~Writer();

	private:
		void    open(const Playlist& playlist);
		void    write(UInt32 sequence, UInt16 duration);

		std::string _ext;
	};

};

} // namespace Mona

