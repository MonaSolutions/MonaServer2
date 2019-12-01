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
#include "Mona/TCProtocol.h"
#include "Mona/HTTP/HTTPSession.h"

namespace Mona {

struct HTTProtocol : TCProtocol, virtual Object {
	HTTProtocol(const char* name, ServerAPI& api, Sessions& sessions, const shared<TLS>& pTLS = nullptr) :
		TCProtocol(name, api, sessions, pTLS) {

		setNumber("port", pTLS ? 443 : 80);
		setNumber("timeout", 10); // ideal value between 7 and 10, but take 10 to be equal to max keyframe interval configurable for a video (10sec), to allow a live streaming not interrupted by session timeout
		setBoolean("index", true); // index directory, if false => forbid directory index, otherwise redirection to index
		setBoolean("rendezVous", false);

		onConnection = [this](const shared<Socket>& pSocket) {
			// Create session
			this->sessions.create<HTTPSession>(self, pSocket).connect();
		};
	}
	~HTTProtocol() { onConnection = nullptr; }

	shared<HTTP::RendezVous> pRendezVous;

	SocketAddress load(Exception& ex) {
		if (getBoolean<false>("rendezVous"))
			pRendezVous.set(api.timer);
		return TCProtocol::load(ex);
	}

};


} // namespace Mona
