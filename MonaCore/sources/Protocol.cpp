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

#include "Mona/Protocol.h"
#include "Mona/ServerAPI.h"

using namespace std;

namespace Mona {

Protocol::Protocol(const char* name, ServerAPI& api, Sessions& sessions) :
	name(name), api(api), sessions(sessions), _pGateway(NULL) {
}

Protocol::Protocol(const char* name, Protocol& gateway) :
	name(name), api(gateway.api), sessions(gateway.sessions), _pGateway(&gateway) {
	// copy parameters from gateway (publicHost, publicPort,  etc...)
	for (const auto& it : gateway)
		setParameter(it.first, it.second);
}

const string* Protocol::onParamUnfound(const string& key) const {
	return api.getParameter(key);
}


SocketAddress Protocol::load(Exception& ex) {
	if (_pGateway)
		return _pGateway->address;
	ERROR(typeof(self)," must overload Protocol::load method");
	return SocketAddress::Wildcard();
}

Socket& Protocol::initSocket(Socket& socket) {
	Exception ex;
	AUTO_WARN(socket.processParams(ex, self), name, " socket");
	DEBUG(name, " set ", socket, " socket buffers set to ", socket.recvBufferSize(), "B in reception and ", socket.sendBufferSize(),"B in sends");
	return socket;
}


} // namespace Mona
