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
	typedef Event<void(RTMFP::EdgeMember&)>			ON(EdgeMember);
	typedef Event<void(shared<RTMFP::Session>&)>	ON(Session);

	RTMFPDecoder(const shared<RendezVous>& pRendezVous, const Handler& handler, const ThreadPool& threadPool);

private:
	void decode(shared<Buffer>& pBuffer, const SocketAddress& address, const shared<Socket>& pSocket);

	struct Handshake;
	bool finalizeHandshake(UInt32 id, const SocketAddress& address, shared<RTMFPReceiver>& pReceiver);

	template<typename ReceiverType>
	void receive(const shared<ReceiverType>& pReceiver, shared<Buffer>& pBuffer, const SocketAddress& address, const shared<Socket>& pSocket) {
		struct Receive : Runner, virtual Object {
			Receive(const shared<ReceiverType>& pReceiver, shared<Buffer>& pBuffer, const SocketAddress& address, const shared<Socket>& pSocket, const shared<std::atomic<UInt32>>& pReceiving) : Runner(typeof<ReceiverType>().c_str()), _pReceiving(pReceiving), _weakReceiver(pReceiver), _pBuffer(std::move(pBuffer)), _address(address), _pSocket(pSocket) {
				*_pReceiving += (_receiving = _pBuffer->size());
			}
			~Receive() {
				*_pReceiving -= _receiving;
			}
		private:
			bool run(Exception&) {
				shared<ReceiverType> pReceiver(_weakReceiver.lock());
				if (pReceiver)
					pReceiver->receive(*_pSocket, _pBuffer, _address);
				return true;
			}
			shared<Buffer>				_pBuffer;
			weak<ReceiverType>			_weakReceiver;
			shared<Socket>				_pSocket;
			SocketAddress				_address;
			shared<std::atomic<UInt32>>	_pReceiving;
			UInt32						_receiving;
		};
		_threadPool.queue<Receive>(pReceiver->track, pReceiver, pBuffer, address, pSocket, _pReceiving);
	}


	const ThreadPool&		_threadPool;
	const Handler&			_handler;

	std::map<UInt32, shared<RTMFPReceiver>>																	  _receivers;
	std::map<SocketAddress, shared<RTMFPReceiver>>															  _peers;
	std::map<SocketAddress, shared<Handshake>>																  _handshakes;
	shared<RendezVous>																						  _pRendezVous;
	shared<std::atomic<UInt32>>																				  _pReceiving;
};



} // namespace Mona
