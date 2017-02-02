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

TCPServer::TCPServer(IOSocket& io, const shared<TLS>& pTLS) : io(io), _pSocket(new TLS::Socket(Socket::TYPE_STREAM, pTLS)), _running(false) {
}

TCPServer::~TCPServer() {
	if (_running)
		io.unsubscribe(_pSocket);
}

bool TCPServer::start(Exception& ex,const SocketAddress& address) {
	if (_running) {
		if (address == _pSocket->address())
			return true;
		stop();
	}

	if (!_pSocket->bind(ex, address))
		return false;
	
	if (!_pSocket->listen(ex))
		return false;
	_running = true; // _running before subscription to do working the possible "stop" on subscribption fails

	// subscribe after bind + listen in TCP!
	if (io.subscribe(ex, _pSocket, onConnection, onError))
		return true;

	stop();
	return false;
}

void TCPServer::stop() {
	if (!_running)
		return;
	shared<TLS> pTLS(((TLS::Socket*)_pSocket.get())->pTLS);
	io.unsubscribe(_pSocket);
	_pSocket.reset(new TLS::Socket(Socket::TYPE_STREAM, pTLS));
	_running = false;
}




} // namespace Mona
