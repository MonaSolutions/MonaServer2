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


#include "Mona/Proxy.h"


using namespace std;


namespace Mona {


void Proxy::Decoder::decode(shared<Buffer>& pBuffer, const SocketAddress& address, const shared<Socket>& pSocket) {
	Exception ex;
	if (_pSocket->write(ex, Packet(pBuffer), _address)<0 || ex)
		_handler.queue(onError, ex);
}

Proxy::Proxy(IOSocket& io) : io(io), _connected(false),
		_onFlush([this](){
			_connected = true;
			onFlush();
		}) {
}

Proxy::~Proxy() {
	if (_pSocket)
		io.unsubscribe(_pSocket);
}

const shared<Socket>& Proxy::relay(Exception& ex, const shared<Socket>& pSocket, const Packet& packet, const SocketAddress& addressTo, const SocketAddress& addressFrom) {
	if (_pSocket && _pSocket->peerAddress() != addressTo)
		close(); // destinator has changed!
	if (!_pSocket) {
		if (pSocket->isSecure())
			_pSocket.set<TLS::Socket>(pSocket->type, ((TLS::Socket*)pSocket.get())->pTLS);
		else
			_pSocket.set(pSocket->type);
		Decoder* pDecoder = new Decoder(io.handler, pSocket, addressFrom);
		pDecoder->onError = onError;
		if (!io.subscribe(ex, _pSocket, pDecoder, nullptr, _onFlush, onError, onDisconnection)) {
			_pSocket.reset();
			return _pSocket;
		}
	}
	if (!_pSocket->peerAddress() || !_connected) {
		// Pulse connect if always not writable
		if (!_pSocket->connect(ex = nullptr, addressTo)) {
			close();
			return _pSocket;
		}
		if (ex && ex.cast<Ex::Net::Socket>().code == NET_EWOULDBLOCK)
			ex = nullptr;
	}
	Exception exWrite;
	if (_pSocket->write(exWrite, packet)<0 || exWrite)
		onError(exWrite);
	return _pSocket;
}

void Proxy::close() {
	if (!_pSocket)
		return;
	io.unsubscribe(_pSocket);
	_connected = false;
}

} // namespace Mona
