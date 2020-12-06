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
#include "Mona/HTTP/HTTPSender.h"
#include "Mona/Playlist.h"


namespace Mona {

/*!
Playlist send */
struct HTTPPlaylistSender : HTTPSender, virtual Object {
	HTTPPlaylistSender(const shared<const HTTP::Header>& pRequest, const shared<Socket>& pSocket,
		const Path& path, const Segments& segments, std::string&& format = "ts") :
		_playlist(path), _format(std::move(format)), HTTPSender("HTTPPlaylistSender", pRequest, pSocket) {
		segments.to(_playlist);
	}

private:
	void  run() override;

	Playlist	_playlist;
	std::string	_format;
};

/*!
Master Playlist send */
struct HTTPMPlaylistSender : HTTPSender, virtual Object {
	HTTPMPlaylistSender(const shared<const HTTP::Header>& pRequest, const shared<Socket>& pSocket,
		Playlist::Master&& playlist) : _playlist(std::move(playlist)), HTTPSender("HTTPMPlaylistSender", pRequest, pSocket) {
	}

private:
	void  run() override;

	Playlist::Master _playlist;
};


} // namespace Mona
