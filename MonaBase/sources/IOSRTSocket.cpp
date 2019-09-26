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

#include "Mona/IOSRTSocket.h"

#if defined(SRT_API)

using namespace std;

namespace Mona {

IOSRTSocket::IOSRTSocket(const Handler& handler, const ThreadPool& threadPool, const char* name) : IOSocket(handler, threadPool, name), _epoll(0) {
}

IOSRTSocket::~IOSRTSocket() {
	
}


bool IOSRTSocket::subscribe(Exception& ex, const shared<Socket>& pSocket) {
	lock_guard<mutex> lock(_mutex); // must protect "start" + _system (to avoid a write operation on restarting) + _subscribers increment
	if (!running()) {
		_initSignal.reset();
		start(); // only way to start IOSRTSocket::run
		_initSignal.wait(); // wait _system assignment
	}

	if (!_epoll) {
		ex.set<Ex::Net::System>(name(), " hasn't been able to start, impossible to manage sockets");
		return false;
	}

	int modes = SRT_EPOLL_IN | SRT_EPOLL_OUT | SRT_EPOLL_ERR | SRT_EPOLL_ET;
	lock_guard<mutex> lockSockets(_mutexSockets); // must protected _sockets
	if (::srt_epoll_add_usock(_epoll, pSocket->id(), &modes) < 0) {
		ex.set<Ex::Net::System>(::srt_getlasterror_str(), ", ", name(), " can't manage socket ", *pSocket);
		return false;
	}
	_sockets.emplace(*pSocket, pSocket);
	++_subscribers;
	return true;
}

void IOSRTSocket::unsubscribe(Socket* pSocket) {

	{
		// decrements _count before the PostMessage
		lock_guard<mutex> lock(_mutexSockets);
		if (!_sockets.erase(*pSocket))
			return;
	}

	lock_guard<mutex> lock(_mutex); // to avoid a restart during _system reading + protected _count decrement
	--_subscribers;
	::srt_epoll_remove_usock(_epoll, pSocket->id());
}


bool IOSRTSocket::run(Exception& ex, const volatile bool& requestStop) {

	_epoll=0;

	// max possible socket, must be superior to the maximum threads possible on the system, 
	// and not too high because could exceed stack limitation of 1Mo like on Android
#define MAXEVENTS  1024

	::SRT_EPOLL_EVENT events[MAXEVENTS];
	_epoll = ::srt_epoll_create();
	if (_epoll < 0)
		_epoll = 0;
	_initSignal.set();

	if (!_epoll) {
		const char* error = ::srt_getlasterror_str();
		ex.set<Ex::Net::System>(error ? error : ("Impossible to run IOSRTSocket"));
		return false;
	}

	int result(0);
	for (;;) {
		result = ::srt_epoll_uwait(_epoll, events, MAXEVENTS, -1);

		if (!_subscribers) {
			lock_guard<mutex> lock(_mutex);
			// no more socket to manage?
			if (!_subscribers) {
				::srt_epoll_release(_epoll);
				stop(); // to set stop=true before unlock!
				return true;
			}
		}

		int i;
		if (result < 0) {
			int error = ::srt_getlasterror(NULL);
			if (error == SRT_ETIMEOUT || error == SRT_EINVPARAM) // ETIMEOUT is not an error, EINVPARAM can be received when no subscribers have been subscribed yet
				continue;
			break;
		}

		if (result > MAXEVENTS) // result is the complete number of event waiting, the rest will be pickup on next epoll_wait call!
			result = MAXEVENTS;

		// for each reading socket
		for(i=0;i<result;++i) {

			SRT_EPOLL_EVENT& event(events[i]);

			shared<Socket> pSocket;
			{
				lock_guard<mutex> lock(_mutexSockets);
				auto it = _sockets.find(event.fd);
				if (it != _sockets.end())
					pSocket = it->second.lock();
			}
			if (!pSocket)
				continue; // socket error

			// EPOLLIN | EPOLLOUT | EPOLLERR
			//printf("%d => %u\n", pSocket->id(), event.events);

			if (event.events & SRT_EPOLL_IN)
				read(pSocket, 0);
			if (event.events & SRT_EPOLL_OUT)
				write(pSocket, 0);
			if (event.events & SRT_EPOLL_ERR)
				close(pSocket, 0); // useless to test state error with ::srt_getsockstate a peer close is always signaled as BROKEN
			if (!event.events)
				WARN("Event without value : ", event.events);
		}
		
	}
	::srt_epoll_release(_epoll);

	if (result < 0) { // error
		ex.set<Ex::Net::System>("impossible to manage sockets, ", ::srt_getlasterror_str());
		return false;
	}
	
	
	if (!_subscribers)
		return true; // IOSocket deletion
		
	ex.set<Ex::Net::System>("dies with remaining sockets managed");
	return false;
}

void IOSRTSocket::stop() {
	Thread::stop();
}

} // namespace Mona

#endif
