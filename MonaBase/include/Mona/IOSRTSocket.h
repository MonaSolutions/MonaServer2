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

#include "Mona/IOSocket.h"
#include "Mona/SRT.h"

#if defined(SRT_API)

namespace Mona {

struct IOSRTSocket : virtual IOSocket, virtual Object {
	IOSRTSocket(const Handler& handler, const ThreadPool& threadPool, const char* name = "IOSRTSocket");
	~IOSRTSocket();

	virtual bool			subscribe(Exception& ex, const shared<Socket>& pSocket);
	
	virtual void			unsubscribe(Socket* pSocket);

	virtual void			stop();
private:
	virtual bool			run(Exception& ex, const volatile bool& requestStop);

	int						_epoll;
	std::map<SRTSOCKET, weak<Socket>>	_sockets;
	std::mutex				_mutexSockets;
};


} // namespace Mona

#endif
