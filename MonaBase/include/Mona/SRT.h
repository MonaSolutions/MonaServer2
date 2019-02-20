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

#if defined(_MSC_VER) || defined(ENABLE_SRT)
#include "srt/srt.h"
#endif

#if defined(SRT_API)
#include "Mona/Mona.h"
#include "Mona/TCPClient.h"

namespace Mona {

struct SRT : virtual Static {

	struct Client : TCPClient {
	private:
		Mona::Socket* newSocket() { return new SRT::Socket(); }
	};
	
	struct Socket : virtual Object, Mona::Socket {
		Socket();
		~Socket();

		bool  isSecure() const { return true; }

		UInt32  available() const;
		UInt64	queueing() const;

		bool connect(Exception& ex, const SocketAddress& address, UInt16 timeout = 0);

		int	 sendTo(Exception& ex, const void* data, UInt32 size, const SocketAddress& address, int flags = 0);

		bool flush(Exception& ex) { return Mona::Socket::flush(ex); }
	private:
		int	 receive(Exception& ex, void* buffer, UInt32 size, int flags, SocketAddress* pAddress);
		bool flush(Exception& ex, bool deleting);
		bool close(Socket::ShutdownType type = SHUTDOWN_BOTH);

	};

};

//
// Automatically link Base library with SRT
//
#if defined(_MSC_VER)
#if defined(_DEBUG)
	#pragma comment(lib, "srt_staticd.lib")
#else
	#pragma comment(lib, "srt_static.lib")
#endif
#endif

} // namespace Mona

#endif
