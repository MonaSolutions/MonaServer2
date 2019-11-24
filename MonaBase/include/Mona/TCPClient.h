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
	virtual ~TCPClient() { disconnect(); }

	IOSocket&	io;

	const shared<Socket>& socket();
	Socket&			      operator*() { return *socket(); }
	Socket*				  operator->() { return socket().get(); }
	

	bool		connect(Exception& ex, const SocketAddress& address);
	bool		connect(Exception& ex, const shared<Socket>& pSocket);

	bool			connecting() const { return _pSocket ? (!_connected && _pSocket->peerAddress()) : false; }
	bool			connected() const { return _connected; }
	virtual void	disconnect();

	virtual bool	send(Exception& ex, const Packet& packet, int flags = 0);

	template<typename RunnerType>
	void		send(const shared<RunnerType>& pRunner) { io.threadPool.queue(_sendingTrack, pRunner); }
	template <typename RunnerType, typename ...Args>
	void		send(Args&&... args) { io.threadPool.queue<RunnerType>(_sendingTrack, std::forward<Args>(args)...); }
	/*!
	Runner example to send data with custom process in parallel thread */
	struct Sender : Runner {
		Sender(const shared<Socket>& pSocket, const Packet& packet, int flags = 0) :
			Runner("TCPSender"), _pSocket(pSocket), _flags(flags), _packet(std::move(packet)) { DEBUG_ASSERT(pSocket); }
	private:
		bool run(Exception& ex) { return _pSocket->write(ex, _packet, _flags) != -1; }
		shared<Socket>	_pSocket;
		Packet			_packet;
		int				_flags;
	};

private:
	virtual Socket::Decoder* newDecoder() { return NULL; }
	virtual shared<Socket> newSocket();

	UInt32 onStreamData(Packet& buffer) { return onData(buffer); }

	Socket::OnReceived		_onReceived;
	Socket::OnFlush			_onFlush;
	Socket::OnDisconnection	_onDisconnection;

	shared<Socket>		_pSocket;
	bool				_connected;
	shared<TLS>			_pTLS;
	bool				_subscribed;
	UInt16				_sendingTrack;
};


} // namespace Mona
