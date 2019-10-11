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
#include "Mona/SocketSession.h"
#include "Mona/TCPClient.h"

namespace Mona {

struct TCPSession : SocketSession, TCPClient, virtual Object {
	const shared<Socket>&	socket() { return SocketSession::socket(); }
	Socket&		operator*() { return SocketSession::operator*(); }
	Socket*		operator->() { return SocketSession::operator->(); }

	void connect(); // to override the connect of TCPClient!

protected:
	TCPSession(Protocol& protocol, const shared<Socket>& pSocket);
	TCPSession(Protocol& protocol, const shared<Socket>& pSocket, shared<Peer>& pPeer);

	virtual	void kill(Int32 error = 0, const char* reason = NULL);

	/*!
	Subscribe to this event to receive data in child session, or overloads newDecoder() function */
	TCPClient::OnData&		onData;
};



} // namespace Mona
