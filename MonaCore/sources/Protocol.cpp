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
	name(name), api(api), sessions(sessions) {
}

Protocol::Protocol(const char* name, Protocol& tunnel) :
	name(name), api(tunnel.api), sessions(tunnel.sessions), _pSocket(tunnel.socket()) {
	// copy parameters from tunnel (publicHost, publicPort,  etc...)
	for (auto& it : tunnel)
		setString(it.first, it.second);
	// if tunnel is disabled, disabled this protocol too
}


const char* Protocol::onParamUnfound(const string& key) const {
	return api.getString(key);
}

bool Protocol::load(Exception& ex) {
	if (_pSocket) {
		Exception ex;
		AUTO_ERROR(_pSocket->processParams(ex, self), name, " socket configuration");
		DEBUG(name, " socket buffers set to ", _pSocket->recvBufferSize(), "B in reception and ", _pSocket->sendBufferSize(),"B in sends");
	}
	return true;
}


} // namespace Mona
