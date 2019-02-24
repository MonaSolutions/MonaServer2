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

#include "Mona/TCPServer.h"


using namespace std;


namespace Mona {

TCPServer::TCPServer(IOSocket& io, const shared<TLS>& pTLS) : io(io), _pTLS(pTLS) {
}

TCPServer::~TCPServer() {
	if (_pSocket)
		io.unsubscribe(_pSocket);
}

bool TCPServer::start(Exception& ex,const SocketAddress& address) {
	if (_pSocket) {
		if (address == _pSocket->address())
			return true;
		stop();
	}

	_pSocket.set<TLS::Socket>(Socket::TYPE_STREAM, _pTLS);
	// can subscribe after bind + listen for server, no risk to miss an event
	if (_pSocket->bind(ex, address) && _pSocket->listen(ex) && io.subscribe(ex, _pSocket, onConnection, onError))
		return true;

	stop();
	return false;
}

void TCPServer::stop() {
	if (_pSocket)
		io.unsubscribe(_pSocket);
}




} // namespace Mona
