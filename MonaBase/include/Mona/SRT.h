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
#if defined(_MSC_VER) || defined(ENABLE_SRT)
#include "srt/srt.h" // will define SRT_API
// undefine the conflicting srt logs macro
#undef LOG_INFO
#undef LOG_DEBUG
#endif


#include "Mona/TCPClient.h"
#include "Mona/TCPServer.h"


namespace Mona {

struct SRT : virtual Object {
	struct Stats : virtual Object {
		virtual UInt32 bandwidthEstimated() const = 0;
		virtual UInt32 bandwidthMaxUsed() const = 0;
		virtual UInt32 bufferTime() const = 0;
		virtual UInt16 negotiatedLatency() const = 0;
		virtual UInt32 recvLostCount() const = 0;
		virtual UInt32 sendLostCount() const = 0;
		virtual double retransmitRate() const = 0;
		virtual UInt16 rtt() const = 0;
		static Stats& Null();
	};
#if !defined(SRT_API)
	enum {
		RELIABLE_SIZE = Net::MTU_RELIABLE_SIZE
	};
#else
	enum {
		RELIABLE_SIZE = ::SRT_LIVE_DEF_PLSIZE
	};
	struct Client : TCPClient {
		Client(IOSocket& io) : Mona::TCPClient(io) { }

	private:
		shared<Mona::Socket> newSocket() { return std::make_shared<SRT::Socket>(); }
	};

	struct Server : TCPServer {
		Server(IOSocket& io) : Mona::TCPServer(io) {}

	private:
		shared<Mona::Socket> newSocket() { return std::make_shared<SRT::Socket>(); }
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

		virtual bool setNonBlockingMode(Exception& ex, bool value);

		virtual const SocketAddress& address() const;

		virtual bool setSendBufferSize(Exception& ex, int size);
		virtual bool getSendBufferSize(Exception& ex, int& size) const { return getOption(ex, SOL_SOCKET, ::SRTO_UDP_SNDBUF, size); }

		virtual bool setRecvBufferSize(Exception& ex, int size);
		virtual bool getRecvBufferSize(Exception& ex, int& size) const { return getOption(ex, SOL_SOCKET, ::SRTO_UDP_RCVBUF, size); }

	private:

		//virtual Mona::Socket* newSocket(Exception& ex, NET_SOCKET sockfd, const sockaddr& addr);
		virtual int	 receive(Exception& ex, void* buffer, UInt32 size, int flags, SocketAddress* pAddress);
		virtual bool flush(Exception& ex, bool deleting);
		virtual bool close(Socket::ShutdownType type = SHUTDOWN_BOTH);

		bool getOption(Exception& ex, int level, SRT_SOCKOPT option, int& value) const;
		bool setOption(Exception& ex, int level, SRT_SOCKOPT option, int value);


		std::atomic<bool> _shutdownRecv; 
		mutable std::atomic<UInt32>		_available;
	};

	static void Log(void* opaque, int level, const char* file, int line, const char* area, const char* message);

	SRT();
	~SRT();
	static SRT _SRT;
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
#endif
};



} // namespace Mona





