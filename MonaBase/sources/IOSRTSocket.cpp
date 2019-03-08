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

IOSRTSocket::IOSRTSocket(const Handler& handler, const ThreadPool& threadPool, const char* name) : IOSocket(handler, threadPool, name) {
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

	int modes = SRT_EPOLL_IN | SRT_EPOLL_OUT | SRT_EPOLL_ERR;
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

	::SRTSOCKET revents[MAXEVENTS];
	::SRTSOCKET wevents[MAXEVENTS];
	_epoll = ::srt_epoll_create();

	if(_epoll <= 0)
		_epoll = 0;

	_initSignal.set();

	if (!_epoll) {
		ex.set<Ex::Net::System>("impossible to start IOSRTSocket");
		return false;
	}

	int result(0);
	for (;;) {

		// TODO: fix 250, normally we should wait for events without end
		int rnum(MAXEVENTS), wnum(MAXEVENTS);
		result = ::srt_epoll_wait(_epoll, &revents[0], &rnum, &wevents[0], &wnum, 50, nullptr, nullptr, nullptr, nullptr);

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
			if (::srt_getlasterror(NULL) == SRT_ETIMEOUT) // ETIMEOUT is not an error
				continue;
			break;
		}

		// for each reading socket
		for(i=0;i<rnum;++i) {

			SRTSOCKET& event(revents[i]);

			shared<Socket> pSocket;
			{
				lock_guard<mutex> lock(_mutexSockets);
				auto it = _sockets.find(event);
				if (it != _sockets.end())
					pSocket = it->second.lock();
			}
			if (!pSocket)
				continue; // socket error

			::SRT_SOCKSTATUS state = ::srt_getsockstate(event);
			switch (state) {
				case ::SRTS_LISTENING:
				case ::SRTS_CONNECTED: {
					read(pSocket, 0); 
					break;
				}						
				case ::SRTS_BROKEN:
				case ::SRTS_NONEXIST:
				case ::SRTS_CLOSING:
				case ::SRTS_CLOSED: {
					int error = ::srt_getlasterror(NULL);
					//DEBUG("Remove socket from poll (on poll event); ", event, " ; ", state, " ; error ", error);
					if (error == SRT_ETIMEOUT)
						close(pSocket, NET_ECONNREFUSED);
					else
						close(pSocket, 0); // Do we need to close the socket? (can be called multiple times)
				}
				break;
				default: {
					WARN("Unexpected event on ", event, "state ", state); 
					break;
				}					 
			}
		}

		// for each writting socket
		for (i = 0; i<wnum; ++i) {

			SRTSOCKET& event(wevents[i]);

			shared<Socket> pSocket;
			{
				lock_guard<mutex> lock(_mutexSockets);
				auto it = _sockets.find(event);
				if (it != _sockets.end())
					pSocket = it->second.lock();
			}
			if (!pSocket)
				continue; // socket error

			write(pSocket, 0);
		}
	}
	::srt_epoll_release(_epoll);

	if (result < 0) { // error
		ex.set<Ex::Net::System>("impossible to manage sockets (error ", errno, ")");
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
