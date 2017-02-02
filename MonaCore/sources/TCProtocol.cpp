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
	onError(_server.onError) {
	onError = [this](const Exception& ex) { WARN("Protocol ", this->name, ", ", ex); }; // onError by default!

	_server.onConnection = [this](const shared<Socket>& pSocket) {
		if (auth(pSocket->peerAddress()))
			return onConnection(pSocket);
	};

	setNumber("bufferSize", 0xFFFF);
	setNumber("recvBufferSize", 0xFFFF);
	setNumber("sendBufferSize", 0xFFFF);
}


TCProtocol::~TCProtocol() {
	_server.onError = nullptr;
	_server.onConnection = nullptr;
}

bool TCProtocol::load(Exception& ex) {
	if (!Protocol::load(ex))
		return false;
	if (!hasKey("timeout"))
		WARN("Protocol ", name, " has no TCP connection timeout");
	return _server.start(ex, address);
}


} // namespace Mona
