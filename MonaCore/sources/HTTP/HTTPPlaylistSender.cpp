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

#include "Mona/HTTP/HTTPPlaylistSender.h"

using namespace std;


namespace Mona {

template<typename SenderType, typename PlaylistType>
static void Send(SenderType& sender, const PlaylistType& playlist, const Path& type, const HTTP::Header& request) {
	Exception ex;
	bool success;
	AUTO_ERROR(success = Playlist::Write(ex, type.extension(), playlist, sender.buffer()), typeof<PlaylistType>());
	if (success) {
		const char* subMime = request.subMime;
		MIME::Type mime = request.mime;
		if (!mime)
			mime = MIME::Read(type, subMime);
		sender.send(HTTP_CODE_200, mime, subMime);
	} else
		sender.sendError(HTTP_CODE_406, ex);
}


void HTTPPlaylistSender::run() {
	Path type = _playlist;
	_playlist.setExtension(_format);
	Send(self, _playlist, type, *pRequest);
}

void HTTPMPlaylistSender::run() {
	Path type = _playlist;
	_playlist.set(_playlist.parent());
	Send(self, _playlist, type, *pRequest);
}


} // namespace Mona
