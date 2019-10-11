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

#include "Mona/TCPClient.h"
#include "Mona/Exceptions.h"

using namespace std;


namespace Mona {

TCPClient::TCPClient(IOSocket& io, const shared<TLS>& pTLS) : _pTLS(pTLS), io(io), _connected(false), _subscribed(false),
	_onReceived([this](shared<Buffer>& pBuffer, const SocketAddress& address) {
		_connected = true;
		// Check that it exceeds not socket buffer
		if (!addStreamData(Packet(pBuffer), _pSocket->recvBufferSize())) {
			_pSocket->shutdown(Socket::SHUTDOWN_RECV);
			Exception ex;
			ex.set<Ex::Protocol>("Socket ", *_pSocket, " message exceeds buffer maximum ", _pSocket->recvBufferSize(), " size");
			onError(ex);
		}
	}),
	_onFlush([this]() {
		_connected = true;
		onFlush();
	}),
	_onDisconnection([this]() { disconnect(); }) {
}

shared<Socket> TCPClient::newSocket() {
	if (_pTLS)
		return make_shared<TLS::Socket>(Socket::TYPE_STREAM, _pTLS);
	return make_shared<Socket>(Socket::TYPE_STREAM);
}
const shared<Socket>& TCPClient::socket() {
	if (!_pSocket) {
		_sendingTrack = 0;
		_pSocket = newSocket();
		Exception ex;
		_subscribed = io.subscribe(ex, _pSocket, newDecoder(), _onReceived, _onFlush, onError, _onDisconnection);
		if (!_subscribed || ex)
			onError(ex);
	}
	return _pSocket;
}

bool TCPClient::connect(Exception& ex,const SocketAddress& address) {
	// connect, allow multiple call to pulse connect!
	if (!socket()->connect(ex, address)) {
		disconnect();
		return false;
	}
	if (ex && ex.cast<Ex::Net::Socket>().code == NET_EWOULDBLOCK)
		ex = nullptr;
	return true;
}

bool TCPClient::connect(Exception& ex, const shared<Socket>& pSocket) {
	if (!pSocket->peerAddress()) {
		Socket::SetException(NET_ENOTCONN, ex);
		return false;
	}
	// here pSocket is necessary connected
	if (_pSocket) {
		// Was already connected to this socket?
		if (_pSocket == pSocket)
			return connect(ex, pSocket->peerAddress());
		
		if (_pSocket->peerAddress()) {
			Socket::SetException(NET_EISCONN, ex, " to ", _pSocket->peerAddress());
			return false;
		}
	}

	if (!io.subscribe(ex, pSocket, newDecoder(), _onReceived, _onFlush, onError, _onDisconnection))
		return false; // cancel connect operation!
	if (_subscribed)
		io.unsubscribe(_pSocket);
	else
		_subscribed = true;
	_sendingTrack = 0;
	_pSocket = pSocket;
	return true;
}

void TCPClient::disconnect() {
	if (!_pSocket)
		return;
	_connected = false;
	SocketAddress peerAddress(_pSocket->peerAddress());
	if (_subscribed) {
		_subscribed = false;
		io.unsubscribe(_pSocket);
	}
	_pSocket.reset();
	if (!peerAddress)
		return; //  was not connected, no onDisconnection need
	// No shutdown here because otherwise it can reset the connection before end of sending (on a RECV shutdown TCP reset the connection if data are available, and so prevent sending too)
	// don't reset _sendingTrack here, because onDisconnection can send last messages
	clearStreamData();
	onDisconnection(peerAddress); // On properly disconnection last messages can be sent!
}

bool TCPClient::send(Exception& ex, const Packet& packet, int flags) {
	if (!_pSocket || !_pSocket->peerAddress()) {
		Socket::SetException(NET_ENOTCONN, ex);
		return false;
	}
	return _pSocket->write(ex, packet, flags) != -1;
}


} // namespace Mona
