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
		// Check that it exceeds not socket buffer
		if (!addStreamData(move(pBuffer), _pSocket->recvBufferSize())) {
			_pSocket->shutdown(Socket::SHUTDOWN_RECV);
			Exception ex;
			ex.set<Ex::Protocol>("Socket ", *_pSocket, " message exceeds buffer maximum ", _pSocket->recvBufferSize(), " size");
			onError(ex);
		}
	}),
	_onFlush([this]() {
		_connected = true; // writable!
		onFlush();
	}),
	_onDisconnection([this]() { disconnect(); }) {
}


TCPClient::~TCPClient() {
	if (_subscribed)
		io.unsubscribe(_pSocket);
	// No disconnection required here, objet is deleting so can't have any event subscribers (TCPClient is not thread-safe!)
}

const shared<Socket>& TCPClient::socket() {
	if (!_pSocket) {
		_sendingTrack = 0;
		_pSocket.reset(_pTLS ? new TLS::Socket(Socket::TYPE_STREAM, _pTLS) : new Socket(Socket::TYPE_STREAM));
	}
	return _pSocket;
}

bool TCPClient::connect(Exception& ex,const SocketAddress& address) {
	// engine subscription BEFORE connect to be able to detect connection success/failure
	if(!_subscribed && !(_subscribed= io.subscribe(ex, socket(), newDecoder(), _onReceived, _onFlush, onError, _onDisconnection)))
		return false;
	if (_connected || _pSocket->connect(ex, address))
		return true;
	if (ex.cast<Ex::Net::Socket>().code == NET_EWOULDBLOCK) {
		ex = nullptr;
		return true;
	}
	return false;
}

bool TCPClient::connect(Exception& ex, const shared<Socket>& pSocket) {
	if (!pSocket->peerAddress()) {
		Socket::SetException(ex, NET_ENOTCONN);
		return false;
	}
	// here pSocket is necessary connected

	if (_pSocket) {
		// Was already connected?
		if (_pSocket == pSocket)
			return connect(ex, pSocket->peerAddress());
		
		if (_pSocket->peerAddress()) {
			Socket::SetException(ex, NET_EISCONN, " to ", _pSocket->peerAddress());
			return false;
		}
	}

	// Here no need to move onReceiving subscription from old _pSocket to pSocket,
	// because onReceiving subscription should be done before TCPClient::connect call directly on new pSocket to get decoding reception immediatly on SocketEngine subscription

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
	if (!_subscribed) // if !_subscribed, it have been connected
		return;
	// No shutdown here because otherwise it can reset the connection before end of sending (on a RECV shutdown TCP reset the connection if data are available, and so prevent sending too)
	_subscribed = _connected = false;
	// don't reset _sendingTrack here, because onDisconnection can send last messages
	clearStreamData();
	shared<Socket> pSocket(_pSocket);
	_pSocket.reset();
	IOSocket& io = this->io;
	if (pSocket->peerAddress()) // else peer was not connected, no onDisconnection need
		onDisconnection(pSocket->peerAddress()); // On properly disconnection last messages can be sent!
	// use a local copy of io and pSocket because onDisconnection can delete this!
	io.unsubscribe(pSocket);
}


} // namespace Mona
