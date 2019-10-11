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


#include "Mona/UDPSocket.h"


using namespace std;


namespace Mona {

const shared<Socket>& UDPSocket::socket() {
	if (!_pSocket) {
		_sendingTrack = 0;
		_pSocket.set(Socket::TYPE_DATAGRAM);
		Exception ex;
		_subscribed = io.subscribe(ex, _pSocket, newDecoder(), onPacket, onFlush, onError);
		if(!_subscribed || ex)
			onError(ex);
	}
	return _pSocket;
}

bool UDPSocket::connect(Exception& ex, const SocketAddress& address) {
	if (socket()->connect(ex, address))
		return true;
	close(); // release resources
	return false;
}


void UDPSocket::disconnect() {
	if (!_pSocket)
		return;
	Exception ex;
	if (!_pSocket->connect(ex, SocketAddress::Wildcard()) || ex)
		onError(ex);
}

bool UDPSocket::bind(Exception& ex, const SocketAddress& address) {
	if (socket()->bind(ex, address))
		return true;
	close(); // release resources
	return false;
}

void UDPSocket::close() {
	if (_subscribed) {
		_subscribed = false;
		io.unsubscribe(_pSocket);
	}
	_pSocket.reset();
}


} // namespace Mona
