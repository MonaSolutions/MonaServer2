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

#include "Mona.h"
#include "Mona/Net.h"
#include "Mona/Thread.h"
#include "Mona/ThreadPool.h"
#include "Mona/Socket.h"

namespace Mona {

struct IOSRTSocket;
struct IOSocket : protected Thread, virtual Object {
	IOSocket(const Handler& handler, const ThreadPool& threadPool, const char* name = "IOSocket");
	~IOSocket();

	const Handler&			handler;
	const ThreadPool&		threadPool;

	UInt32					subscribers() const { return _subscribers; }

	bool					subscribe(Exception& ex, const shared<Socket>& pSocket,
								const Socket::OnReceived& onReceived,
								const Socket::OnFlush& onFlush,
								const Socket::OnError& onError,
								const Socket::OnDisconnection& onDisconnection=nullptr) { return subscribe(ex, pSocket, onReceived, onFlush, onDisconnection, nullptr, onError); }
	bool					subscribe(Exception& ex, const shared<Socket>& pSocket,
								Socket::Decoder* pDecoder,
								const Socket::OnReceived& onReceived,
								const Socket::OnFlush& onFlush,
								const Socket::OnError& onError,
								const Socket::OnDisconnection& onDisconnection=nullptr) { return subscribe(ex, pSocket, pDecoder, onReceived, onFlush, onDisconnection, nullptr, onError); }
	bool					subscribe(Exception& ex, const shared<Socket>& pSocket,
								const Socket::OnAccept& onAccept,
								const Socket::OnError& onError) { return subscribe(ex, pSocket, nullptr, nullptr, nullptr, onAccept, onError); }
	
	virtual bool			subscribe(Exception& ex, const shared<Socket>& pSocket);

	/*!
	Unsubscribe pSocket and reset shared<Socket> to avoid to resubscribe the same socket which could crash decoder assignation */
	void					unsubscribe(shared<Socket>& pSocket);

	virtual void			stop();

protected:
	std::mutex				_mutex;
	Signal					_initSignal;
	std::atomic<UInt32>		_subscribers;

	void			read(const shared<Socket>& pSocket, int error);
	void			write(const shared<Socket>& pSocket, int error);
	void			close(const shared<Socket>& pSocket, int error);

	virtual void	unsubscribe(Socket* pSocket);

private:

	template<typename SocketType>
	bool subscribe(Exception& ex, const shared<SocketType>& pSocket,
			Socket::Decoder* pDecoder,
			const Socket::OnReceived& onReceived,
			const Socket::OnFlush& onFlush,
			const Socket::OnDisconnection& onDisconnection,
			const Socket::OnAccept& onAccept,
			const Socket::OnError& onError) {
		pSocket->_externDecoder = pDecoder && pSocket.get() != (SocketType*)pDecoder;
		pSocket->_pDecoder = pDecoder;
		if (subscribe(ex, pSocket, onReceived, onFlush, onDisconnection, onAccept, onError))
			return true;
		if (!pSocket->_externDecoder)
			return false;
		delete pDecoder;
		return pSocket->_externDecoder = false;
	}

	bool subscribe(Exception& ex, const shared<Socket>& pSocket,
			const Socket::OnReceived& onReceived,
			const Socket::OnFlush& onFlush,
			const Socket::OnDisconnection& onDisconnection,
			const Socket::OnAccept& onAccept,
			const Socket::OnError& onError);
	
	virtual bool run(Exception& ex, const volatile bool& requestStop);

#if defined(_WIN32)
	std::map<NET_SOCKET, weak<Socket>>	_sockets;
	std::mutex									_mutexSockets;
#else
	int											_eventFD;
#endif

	NET_SYSTEM									_system;
	shared<IOSRTSocket>							_pIOSRTSocket;

	struct Action;
};


} // namespace Mona
