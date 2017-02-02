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


UDPSocket::UDPSocket(IOSocket& io) : _sendingTrack(0), io(io), _connected(false), _bound(false), _subscribed(false), _pSocket(new Socket(Socket::TYPE_DATAGRAM)) {
}

UDPSocket::~UDPSocket() {
	if (_subscribed)
		io.unsubscribe(_pSocket);
}

bool UDPSocket::subscribe(Exception& ex) {
	if (_subscribed)
		return true;
	return _subscribed= io.subscribe(ex, _pSocket, newDecoder(), onPacket, onFlush, onError);
}


bool UDPSocket::connect(Exception& ex, const SocketAddress& address) {
	// connect
	if (!_pSocket->connect(ex, address))
		return false;
	// subscribe after connect for not getting two WRITE events (useless and impact socket performance)
	if(subscribe(ex))
		return _connected = true;
	// to reset the not working socket which has been able to connect but can't receive (connect = send + receive ability)
	close();
	return false;
}

void UDPSocket::disconnect() {
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
	if (!_pSocket->bind(ex, address))
		return false;
	if (subscribe(ex))
		return _bound = true;
	// to reset the not working socket which has been able to bind!
	close();
	return false; 
}

void UDPSocket::close() {
	if (_subscribed)
		io.unsubscribe(_pSocket);
	_sendingTrack = 0;
	_connected = _bound = _subscribed = false;
	_pSocket.reset(new Socket(Socket::TYPE_DATAGRAM));
}

} // namespace Mona
