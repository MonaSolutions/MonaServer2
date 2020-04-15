/*
This file is a part of MonaSolutions Copyright 2017
mathieu.poux[a]gmail.com
jammetthomas[a]gmail.com

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License received along this program for more
details (or else see http://www.gnu.org/licenses/).

*/

#include "Mona/SocketSession.h"

using namespace std;


namespace Mona {

SocketSession::SocketSession(Protocol& protocol, const shared<Socket>& pSocket) : _pSocket(pSocket), Session(protocol, pSocket->peerAddress()) {
	if (!peer.serverAddress.host()) // use TCP client bind to determine server address if need (can have been assigned already by a protocol publicHost, not override in this case)
		peer.setServerAddress(SocketAddress(pSocket->address().host(), peer.serverAddress.port()));
}
SocketSession::SocketSession(Protocol& protocol, const shared<Socket>& pSocket, shared<Peer>& pPeer) : _pSocket(pSocket), Session(protocol, pPeer) {
	if (!peer.serverAddress.host()) // use TCP client bind to determine server address if need (can have been assigned already by a protocol publicHost, not override in this case)
		peer.setServerAddress(SocketAddress(pSocket->address().host(), peer.serverAddress.port()));
}

void SocketSession::onParameters(const Parameters& parameters) {
	Session::onParameters(parameters);
	protocol().initSocket(*_pSocket);
}

void SocketSession::kill(Int32 error, const char* reason) {
	if (died)
		return;
	Session::kill(error, reason); // onPeerDisconnection, before socket disconnection to allow possible last message
	_pSocket->shutdown(Socket::SHUTDOWN_RECV); // shutdown just recveing part to send current "sending" packet (+ help websocket on chrome to avoid uncatchable error)
}

} // namespace Mona
