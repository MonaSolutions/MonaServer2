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

shared<Socket> TCPServer::newSocket() {
	if (_pTLS)
		return make_shared<TLS::Socket>(Socket::TYPE_STREAM, _pTLS);
	return make_shared<Socket>(Socket::TYPE_STREAM);
}
const shared<Socket>& TCPServer::socket() {
	if (!_pSocket)
		_pSocket = newSocket();
	return _pSocket;
}

bool TCPServer::start(Exception& ex,const SocketAddress& address) {
	// listen has to be called BEFORE io.sibscribe (can subscribe after bind + listen for server, no risk to miss an event)
	if (socket()->bind(ex, address) && _pSocket->listen(ex) && (_subscribed=io.subscribe(ex, _pSocket, onConnection, onError)))
		return true;
	stop();
	return false;
}

void TCPServer::stop() {
	if (_subscribed) {
		_subscribed = false;
		io.unsubscribe(_pSocket);
	}
	_pSocket.reset();
}




} // namespace Mona
