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
#include "Mona/Session.h"
#include "Mona/Socket.h"

namespace Mona {

struct SocketSession : Session, virtual Object {
	const shared<Socket>&	socket() { return _pSocket; }
	Socket*					operator->() { return _pSocket.get(); }
	Socket&					operator*() { return *_pSocket; }

protected:
	SocketSession(Protocol& protocol, const shared<Socket>& pSocket);
	SocketSession(Protocol& protocol, const shared<Socket>& pSocket, shared<Peer>& pPeer);

	virtual	void kill(Int32 error = 0, const char* reason = NULL);

	virtual void onParameters(const Parameters& parameters);

private:
	shared<Socket>	_pSocket;
};



} // namespace Mona
