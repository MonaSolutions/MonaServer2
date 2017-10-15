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

namespace Mona {

struct UDPSocket : virtual NullableObject {
	typedef Socket::OnReceived	ON(Packet);
	typedef Socket::OnFlush		ON(Flush);
	typedef Socket::OnError		ON(Error);

	UDPSocket(IOSocket& io);
	virtual ~UDPSocket();

	IOSocket&	io;

	operator	bool() const { return _pSocket.operator bool(); }
	operator	const shared<Socket>&() { return _pSocket; }
	Socket&		operator*() { return *_pSocket; }
	Socket*		operator->() { return _pSocket.get(); }

	bool		connect(Exception& ex, const SocketAddress& address);
	bool		connected() const { return _connected; }

	bool		bind(Exception& ex, const SocketAddress& address);
	bool		bind(Exception& ex, const IPAddress& ip = IPAddress::Wildcard()) { return bind(ex, SocketAddress(ip, 0)); }
	bool		bound() { return _bound;  }

	void		disconnect();
	void		close();

	bool		send(Exception& ex, const Packet& packet, int flags = 0) { return send(ex, packet, SocketAddress::Wildcard(), flags); }
	bool		send(Exception& ex, const Packet& packet, const SocketAddress& address, int flags = 0);

	/*!
	Runner example to send data with custom process in parallel thread */
	struct Sender : Runner {
		Sender(const shared<Socket>& pSocket, const Packet& packet, int flags = 0) :
			Runner("UDPSender"), _pSocket(pSocket), _flags(flags), _packet(std::move(packet)) { FATAL_CHECK(pSocket); }
		Sender(const shared<Socket>& pSocket, const Packet& packet, const SocketAddress& address, int flags = 0) :
			Runner("UDPSender"), _pSocket(pSocket), _flags(flags), _packet(std::move(packet)), _address(address) { FATAL_CHECK(pSocket); }
	private:
		bool run(Exception& ex) { _pSocket->write(ex, _packet, _address, _flags);  return true; } // UDP socket error is a WARN
		shared<Socket>	_pSocket;
		SocketAddress	_address;
		Packet			_packet;
		int				_flags;
	};
private:
	virtual shared<Socket::Decoder> newDecoder() { return nullptr; }

	shared<Socket>		_pSocket;
	bool				_connected;
	bool				_bound;
};



} // namespace Mona
