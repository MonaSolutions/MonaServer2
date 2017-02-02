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

struct UDPSocket : virtual Object {
	typedef Socket::OnReceived	ON(Packet);
	typedef Socket::OnFlush		ON(Flush);
	typedef Socket::OnError		ON(Error);

	UDPSocket(IOSocket& io);
	virtual ~UDPSocket();

	IOSocket&				io;
	const shared<Socket>&	socket() { return _pSocket; }
	Socket*					operator->() { return _pSocket.get(); }

	bool connect(Exception& ex, const SocketAddress& address);
	bool connected() const { return _connected; }

	bool bind(Exception& ex, const SocketAddress& address);
	bool bind(Exception& ex, const IPAddress& ip = IPAddress::Wildcard()) { return bind(ex, SocketAddress(ip, 0)); }
	bool bound() { return _bound;  }	

	void disconnect();
	void close();

	/*!
	Returns threadId on success or 0 otherwise, if queueing>0 wait onFlush before to continue to send large data */
	bool	send(Exception& ex, const Packet& packet) { return send(ex, packet, SocketAddress::Wildcard()); }
	bool	send(Exception& ex, const Packet& packet, UInt16& thread) { return send(ex, packet, SocketAddress::Wildcard(), thread); }
	bool	send(Exception& ex, const Packet& packet, const SocketAddress& address) { return send(ex, packet, address, _sendingTrack) ? true : false; }
	bool	send(Exception& ex, const Packet& packet, const SocketAddress& address, UInt16& thread) { return send(ex, std::make_shared<Sender>(_pSocket, packet, address), thread); }
	template<typename SenderType>
	bool	send(Exception& ex, const shared<SenderType>& pSender) { return (_sendingTrack=send<SenderType>(ex, pSender, _sendingTrack)) ? true : false; }
	template<typename SenderType>
	bool	send(Exception& ex, const shared<SenderType>& pSender, UInt16& thread) {
		subscribe(ex); // warn (engine subscription not mandatory for udp sending)
		return io.threadPool.queue(ex, pSender, thread);
	}

	struct Sender : Runner, Packet {
		Sender(const shared<Socket>& pSocket, const Packet& packet, const SocketAddress& address) : Runner("UDPSender"), _address(address), _pSocket(pSocket), Packet(std::move(packet)) {}
		bool run(Exception& ex) { return _pSocket->write(ex, *this, _address) != -1; }
	private:
		shared<Socket>	_pSocket;
		SocketAddress			_address;
	};
private:
	virtual shared<Socket::Decoder> newDecoder() { return nullptr; }

	bool subscribe(Exception& ex);

	shared<Socket>		_pSocket;
	bool						_connected;
	bool						_bound;
	bool						_subscribed;
	UInt16						_sendingTrack;
};



} // namespace Mona
