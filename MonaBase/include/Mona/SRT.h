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
#include "Mona/Logs.h" // Included before srt.h to avoid declaration conflicts
#if defined(_MSC_VER) || defined(ENABLE_SRT)
#include "srt/srt.h" // will define SRT_API
#endif

#if defined(SRT_API)
#include "Mona/TCPClient.h"
#include "Mona/TCPServer.h"


namespace Mona {

struct SRT : virtual Static {

	struct Client : TCPClient {
		Client(IOSocket& io) : TCPClient(io) { }

	private:
		shared<Mona::Socket> newSocket() { return std::make_shared<SRT::Socket>(); }
	};

	struct Server : TCPServer {
		Server(IOSocket& io) : TCPServer(io) {}
	};
	
	struct Socket : virtual Object, Mona::Socket {
		Socket();
		Socket(const sockaddr& addr, SRTSOCKET id);
		virtual ~Socket();

		bool  isSecure() const { return true; }

		UInt32  available() const;
		UInt64	queueing() const;

		virtual bool accept(Exception& ex, shared<Mona::Socket>& pSocket);

		virtual bool connect(Exception& ex, const SocketAddress& address, UInt16 timeout = 0);

		virtual int	 sendTo(Exception& ex, const void* data, UInt32 size, const SocketAddress& address, int flags = 0);

		virtual bool bind(Exception& ex, const SocketAddress& address);

		virtual bool listen(Exception& ex, int backlog = SOMAXCONN);
	private:

		void disconnect();

		//virtual Mona::Socket* newSocket(Exception& ex, NET_SOCKET sockfd, const sockaddr& addr);
		virtual int	 receive(Exception& ex, void* buffer, UInt32 size, int flags, SocketAddress* pAddress);
		virtual bool flush(Exception& ex, bool deleting);
		virtual bool close(Socket::ShutdownType type = SHUTDOWN_BOTH);


		::SRTSOCKET	_srt; // SRT context ID
		//::SRTSOCKET _currentID; // Current context ID, created after SRT accept
		//bool		_connected;
	};

	static void LogCallback(void* opaque, int level, const char* file, int line, const char* area, const char* message);

	struct Singleton : virtual Object {
		Singleton() {
			if (::srt_startup()) {
				FATAL_ERROR("SRT startup: Error starting SRT library");
				return;
			}
			::srt_setloghandler(nullptr, LogCallback);
			::srt_setloglevel(0xff);
		}
		
		virtual ~Singleton() {
			::srt_setloghandler(nullptr, nullptr);
			::srt_cleanup();
		}
	};
	static unique<Singleton> SRT_Singleton;
};

//
// Automatically link Base library with SRT
//
#if defined(_MSC_VER)
#if defined(_DEBUG)
#pragma comment(lib, "pthread_libd.lib")
#pragma comment(lib, "srt_staticd.lib")
#else
#pragma comment(lib, "pthread_lib.lib")
#pragma comment(lib, "srt_static.lib")
#endif
#endif

} // namespace Mona

#endif



