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

#pragma once

#include "Mona/Mona.h"
#include "Mona/RTMFP/RTMFPReceiver.h"

namespace Mona {

struct RTMFPDecoder : Socket::Decoder, virtual Object {
	typedef Event<void(RTMFP::Handshake&)>			ON(Handshake);
	typedef Event<void(shared<RTMFP::Session>&)>	ON(Session);

	RTMFPDecoder(const Handler& handler, const ThreadPool& threadPool);

private:
	UInt32 decode(shared<Buffer>& pBuffer, const SocketAddress& address, const shared<Socket>& pSocket);

	struct Handshake;
	bool finalizeHandshake(UInt32 id, const SocketAddress& address, shared<RTMFPReceiver>& pReceiver);

	const ThreadPool&		_threadPool;
	const Handler&			_handler;

	std::function<bool(UInt32, std::map<UInt32, shared<RTMFPReceiver>>::iterator&)>							  _validateReceiver;
	std::map<UInt32, shared<RTMFPReceiver>>																	  _receivers;
	std::map<SocketAddress, shared<RTMFPReceiver>>															  _peers;
	std::function<bool(const SocketAddress& address, std::map<SocketAddress, shared<Handshake>>::iterator&)>  _validateHandshake;
	std::map<SocketAddress, shared<Handshake>>																  _handshakes;
	shared<RendezVous>																						  _pRendezVous;
};



} // namespace Mona
