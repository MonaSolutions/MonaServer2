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

#include "Mona/TCProtocol.h"
#include "Mona/ServerAPI.h"

using namespace std;

namespace Mona {

TCProtocol::TCProtocol(const char* name, ServerAPI& api, Sessions& sessions, const shared<TLS>& pTLS) : _server(api.ioSocket, pTLS), Protocol(name, api, sessions),
	onConnection(_server.onConnection) {
	_server.onError = [this](const Exception& ex) {
		if (onError)
			return onError(ex);
		WARN("Protocol ", this->name, ", ", ex); // onError by default!
	};
}

SocketAddress TCProtocol::load(Exception& ex) {
	initSocket(*_server);
	if (!hasKey("timeout"))
		ex.set<Ex::Intern>("no TCP connection timeout");
	return _server.start(ex, address) ? _server->address() : SocketAddress::Wildcard();
}


} // namespace Mona
