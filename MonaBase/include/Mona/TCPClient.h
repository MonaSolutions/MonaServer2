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
#include "Mona/StreamData.h"

namespace Mona {

struct TCPClient : private StreamData<>, virtual Object {
	typedef Event<UInt32(Packet& buffer)>				ON(Data);
	typedef Event<void()>								ON(Flush);
	typedef Event<void(const SocketAddress& address)>	ON(Disconnection);
	typedef Socket::OnError								ON(Error);

/*!
	Create a new TCPClient */
	TCPClient(IOSocket& io, const shared<TLS>& pTLS=nullptr);
	virtual ~TCPClient();

	IOSocket&				io;
	const shared<Socket>&	socket();
	Socket*					operator->() { return socket().get(); }

	bool	connect(Exception& ex, const SocketAddress& address);
	bool	connect(Exception& ex, const shared<Socket>& pSocket);

	bool	connecting() const { return _pSocket ? (!_connected && _pSocket->peerAddress()) : false; }
	bool	connected() const { return _connected; }
	void	disconnect();

	/*!
	Returns true on success, if queueing>0 wait onFlush before to continue to send large data */
	bool	send(Exception& ex, const Packet& packet) { return send(ex, std::make_shared<Sender>(_pSocket, packet)); }
	template<typename SenderType>
	bool	send(Exception& ex, const shared<SenderType>& pSender) {
		if (!_pSocket || !_pSocket->peerAddress()) {
			Socket::SetException(ex, NET_ENOTCONN);
			return false;
		}
		return io.threadPool.queue(ex, pSender, _sendingTrack);
	}

	struct Sender : Runner, Packet {
		Sender(const shared<Socket>& pSocket, const Packet& packet) : Runner("TCPSender"), _pSocket(pSocket), Packet(std::move(packet)) {}
		bool run(Exception& ex) { return _pSocket->write(ex, *this) != -1; }
	private:
		shared<Socket>	_pSocket;
	};

private:
	virtual shared<Socket::Decoder> newDecoder() { return nullptr; }

	UInt32 onStreamData(Packet& buffer) { return onData(buffer); }

	Socket::OnReceived		_onReceived;
	Socket::OnFlush			_onFlush;
	Socket::OnDisconnection	_onDisconnection;

	shared<Socket>		_pSocket;
	bool				_connected;
	bool				_subscribed;
	shared<TLS>			_pTLS;
	UInt16				_sendingTrack;
};


} // namespace Mona
