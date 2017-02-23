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

#pragma once


#include "Mona/Mona.h"
#include "Mona/IOSocket.h"
#include "Mona/TLS.h"

namespace Mona {

struct Proxy : virtual Object {
	typedef Socket::OnFlush			ON(Flush);
	typedef Socket::OnError			ON(Error);
	typedef Socket::OnDisconnection	ON(Disconnection);

	Proxy(IOSocket& io, const shared<TLS>& pTLS = nullptr);
	virtual ~Proxy();

	IOSocket&				io;
	const shared<Socket>&	socket();
	Socket*					operator->() { return socket().get(); }
	
	bool relay(Exception& ex, const shared<Socket>& pSocket, const Packet& packet, const SocketAddress& addressFrom, const SocketAddress& addressTo);
	void close();

private:
	struct Decoder : Socket::Decoder {
		typedef Socket::OnError			ON(Error);

		Decoder(const Handler& handler, const shared<Socket>& pSocket, const SocketAddress& address) : _pSocket(pSocket), _handler(handler), _address(address) {}
	private:
		UInt32 decode(shared<Buffer>& pBuffer, const SocketAddress& address, const shared<Socket>& pSocket);
		shared<Socket> _pSocket;
		const Handler& _handler;
		SocketAddress  _address;
	};


	shared<Socket>			_pSocket;
	Socket::OnFlush			_onFlush;
	shared<TLS>				_pTLS;
	bool					_connected;
	bool					_subscribed;
};



} // namespace Mona
