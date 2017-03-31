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
#include "Mona/Handler.h"

namespace Mona {

struct IOSocket : private Thread, virtual Object {
	IOSocket(const Handler& handler, const ThreadPool& threadPool, const char* name = "IOSocket");
	~IOSocket();

	const Handler&			handler;
	const ThreadPool&		threadPool;

	UInt32					subscribers() const { return _subscribers; }

	bool					subscribe(Exception& ex, const shared<Socket>& pSocket,
								const Socket::OnReceived& onReceived,
								const Socket::OnFlush& onFlush,
								const Socket::OnError& onError,
								const Socket::OnDisconnection& onDisconnection=nullptr) { shared<Socket::Decoder> pDecoder; return subscribe(ex, pSocket, std::move(pDecoder), onReceived, onFlush, onDisconnection, nullptr, onError); }
	bool					subscribe(Exception& ex, const shared<Socket>& pSocket,
								shared<Socket::Decoder>&& pDecoder,
								const Socket::OnReceived& onReceived,
								const Socket::OnFlush& onFlush,
								const Socket::OnError& onError,
								const Socket::OnDisconnection& onDisconnection=nullptr) { return subscribe(ex, pSocket, std::move(pDecoder), onReceived, onFlush, onDisconnection, nullptr, onError); }
	bool					subscribe(Exception& ex, const shared<Socket>& pSocket,
								const Socket::OnAccept& onAccept,
								const Socket::OnError& onError) { shared<Socket::Decoder> pDecoder; return subscribe(ex, pSocket, std::move(pDecoder), nullptr, nullptr, nullptr, onAccept, onError); }
	
	/*!
	Unsubscribe pSocket and reset shared<Socket> to avoid to resubscribe the same socket which could crash decoder assignation */
	void					unsubscribe(shared<Socket>& pSocket);

private:

	enum { MAXEVENTS = 0xFFFF}; // max possible linux sockets = port available

	bool					subscribe(Exception& ex, const shared<Socket>& pSocket,
								shared<Socket::Decoder>&& pDecoder,
								const Socket::OnReceived& onReceived,
								const Socket::OnFlush& onFlush,
								const Socket::OnDisconnection& onDisconnection,
								const Socket::OnAccept& onAccept,
								const Socket::OnError& onError);

	void			read(const shared<Socket>& pSocket, int error);
	void			write(const shared<Socket>& pSocket, int error);
	void			close(const shared<Socket>& pSocket, int error);
	
	bool run(Exception& ex, const volatile bool& stopping);

#if defined(_WIN32)
	std::map<NET_SOCKET, weak<Socket>>	_sockets;
	std::mutex									_mutexSockets;
#else
	int											_eventFD;
#endif

	NET_SYSTEM									_system;
	std::atomic<UInt32>							_subscribers;
	std::mutex									_mutex;
	Signal										_initSignal;

	struct Action;
	struct Send;
};


} // namespace Mona
