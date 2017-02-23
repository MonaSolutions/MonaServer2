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
#include "Mona/Logs.h"


using namespace std;


namespace Mona {


UInt32 Proxy::Decoder::decode(shared<Buffer>& pBuffer, const SocketAddress& address, const shared<Socket>& pSocket) {
	Exception ex;
	if (!_pSocket->write(ex, Packet(pBuffer), _address) || ex)
		_handler.queue(onError, ex);
	return 0;
}

Proxy::Proxy(IOSocket& io, const shared<TLS>& pTLS) : _pTLS(pTLS), io(io), _connected(false), _subscribed(false),
		_onFlush([this](){
			_connected = true;
			onFlush();
		}) {
}

Proxy::~Proxy() {
	if (_subscribed)
		io.unsubscribe(_pSocket);
}

const shared<Socket>& Proxy::socket() {
	if (!_pSocket)
		_pSocket.reset(_pTLS ? new TLS::Socket(Socket::TYPE_STREAM, _pTLS) : new Socket(Socket::TYPE_STREAM));
	return _pSocket;
}

bool Proxy::relay(Exception& ex, const shared<Socket>& pSocket, const Packet& packet, const SocketAddress& addressFrom, const SocketAddress& addressTo) {
	if (_pSocket && _pSocket->peerAddress() != addressTo)
		close(); // destinator has changed!
	if (!_subscribed) {
		shared<Decoder> pDecoder(new Decoder(io.handler, pSocket, addressFrom));
		pDecoder->onError = onError;
		if (!(_subscribed = io.subscribe(ex, socket(), pDecoder, nullptr, _onFlush, onError, onDisconnection))) {
			_pSocket.reset();
			return false;
		}
	}
	if (!_pSocket->peerAddress() || !_connected) {
		// Pulse connect if always not writable
		if (!_pSocket->connect(ex = nullptr, addressTo)) {
			if (ex.cast<Ex::Net::Socket>().code != NET_EWOULDBLOCK)
				return false;
			ex = nullptr;
		}
	}
	Exception exWrite;
	if (!_pSocket->write(exWrite, packet) || ex)
		onError(exWrite);
	return true;
}

void Proxy::close() {
	if (!_subscribed)
		return;
	io.unsubscribe(_pSocket);
	_subscribed = _connected = false;
}

} // namespace Mona
