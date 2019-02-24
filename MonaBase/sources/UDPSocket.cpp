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


UDPSocket::UDPSocket(IOSocket& io) : io(io), _connected(false), _bound(false) {
}

UDPSocket::~UDPSocket() {
	if (_pSocket)
		io.unsubscribe(_pSocket);
}

bool UDPSocket::connect(Exception& ex, const SocketAddress& address) {
	if (!_pSocket) {
		_pSocket.set(Socket::TYPE_DATAGRAM);
		io.subscribe(ex, _pSocket, newDecoder(), onPacket, onFlush, onError);
	}
	if (_pSocket->connect(ex, address))
		return _connected = true;
	close(); // release resources
	return false;
}


void UDPSocket::disconnect() {
	if (!_pSocket)
		return;
	Exception ex;
	if (_pSocket->connect(ex, SocketAddress::Wildcard()))
		_connected = false;
	if (ex)
		onError(ex);
}

bool UDPSocket::bind(Exception& ex, const SocketAddress& address) {
	if (_bound) {
		if (address == _pSocket->address())
			return true;
		close();
	}
	_pSocket.set(Socket::TYPE_DATAGRAM);
	if (io.subscribe(ex, _pSocket, newDecoder(), onPacket, onFlush, onError) && _pSocket->bind(ex, address))
		return _bound = true;
	close(); // release resources
	return false;
}

void UDPSocket::close() {
	if (_pSocket)
		io.unsubscribe(_pSocket);
	_connected = _bound = false;
}

bool UDPSocket::send(Exception& ex, const Packet& packet, const SocketAddress& address, int flags) {
	if (!_pSocket && !bind(ex))  // explicit bind to create and subscribe socket
		return false;
	return _pSocket->write(ex, packet, address, flags) != -1;
}


} // namespace Mona
