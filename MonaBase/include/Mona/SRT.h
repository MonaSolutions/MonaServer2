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
		virtual double bandwidthEstimated() const = 0;
		virtual double bandwidthMaxUsed() const = 0;
		virtual UInt32 bufferTime() const = 0;
		virtual double negotiatedLatency() const = 0;
		virtual UInt32 recvLostCount() const = 0;
		virtual UInt32 sendLostCount() const = 0;
		virtual double retransmitRate() const = 0;
		virtual double rtt() const = 0;
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
	
	struct Socket : virtual Object, Stats, Mona::Socket {
		Socket();
		Socket(const sockaddr& addr, SRTSOCKET id);
		virtual ~Socket();

		bool  isSecure() const { return true; }

		UInt32  available() const;

		virtual bool accept(Exception& ex, shared<Mona::Socket>& pSocket);

		virtual bool connect(Exception& ex, const SocketAddress& address, UInt16 timeout = 0);

		virtual int	 sendTo(Exception& ex, const void* data, UInt32 size, const SocketAddress& address, int flags = 0);

		virtual bool bind(Exception& ex, const SocketAddress& address);

		virtual bool listen(Exception& ex, int backlog = SOMAXCONN);

		virtual bool setNonBlockingMode(Exception& ex, bool value);

		virtual const SocketAddress& address() const;

		virtual bool setSendBufferSize(Exception& ex, int size);
		virtual bool getSendBufferSize(Exception& ex, int& size) const { return getOption(ex, ::SRTO_UDP_SNDBUF, size); }
		virtual bool setRecvBufferSize(Exception& ex, int size);
		virtual bool getRecvBufferSize(Exception& ex, int& size) const { return getOption(ex, ::SRTO_UDP_RCVBUF, size); }

		virtual bool setReuseAddress(Exception& ex, bool value) { return setOption(ex, ::SRTO_REUSEADDR, value ? 1 : 0); }
		virtual bool getReuseAddress(Exception& ex, bool& value) const { int val; return getOption(ex, ::SRTO_REUSEADDR, val); }

		virtual bool setLinger(Exception& ex, bool on, int seconds);
		virtual bool getLinger(Exception& ex, bool& on, int& seconds) const;

		bool getEncryptionState(Exception& ex, int& state) const { return getOption(ex, ::SRTO_RCVKMSTATE, state); }
		bool getPeerDecryptionState(Exception& ex, int& state) const { return getOption(ex, ::SRTO_SNDKMSTATE, state); }
		bool getEncryptionType(Exception& ex, int& type) const { return getOption(ex, ::SRTO_PBKEYLEN, type); }
		bool setPassphrase(Exception& ex, const char* data, UInt32 size);

		bool setLatency(Exception& ex, int value) { return setOption(ex, ::SRTO_RCVLATENCY, value); }
		bool getLatency(Exception& ex, int& value) const { return getOption(ex, ::SRTO_RCVLATENCY, value); }
		bool setPeerLatency(Exception& ex, int value) { return setOption(ex, ::SRTO_PEERLATENCY, value); }
		bool getPeerLatency(Exception& ex, int& value) const { return getOption(ex, ::SRTO_PEERLATENCY, value); }		

		bool setMSS(Exception& ex, int value) { return setOption(ex, ::SRTO_MSS, value); }
		bool getMSS(Exception& ex, int& value) const { return getOption(ex, ::SRTO_MSS, value); }
		bool setOverheadBW(Exception& ex, int value) { return setOption(ex, ::SRTO_OHEADBW, value); }
		// bool getOverheadBW(Exception& ex, int& value) const { return getOption(ex, ::SRTO_OHEADBW, value); }  Not supported for now in SRT
		bool setMaxBW(Exception& ex, Int64 value) { return setOption(ex, ::SRTO_MAXBW, value); }
		bool getMaxBW(Exception& ex, Int64& value) const { return getOption(ex, ::SRTO_MAXBW, value); }		

		bool getStats(Exception& ex, SRT_TRACEBSTATS* perf) const;

		// Stats
		virtual double bandwidthEstimated() const { Exception ex; return !getStats(ex, &_stats) ? 0 : _stats.mbpsBandwidth; }
		virtual double bandwidthMaxUsed() const { Exception ex; return !getStats(ex, &_stats) ? 0 : _stats.mbpsMaxBW; }
		virtual UInt32 bufferTime() const { Exception ex; return !getStats(ex, &_stats) ? 0 : _stats.msRcvBuf; } // TODO: not sure it is just rcv
		virtual double negotiatedLatency() const { Exception ex; int val1, val2; return (!getLatency(ex, val1) || !getLatency(ex, val2)) ? 0 : val1 + val2; }
		virtual UInt32 recvLostCount() const { Exception ex; return !getStats(ex, &_stats) ? 0 : _stats.pktRcvLoss; }
		virtual UInt32 sendLostCount() const { Exception ex; return !getStats(ex, &_stats) ? 0 : _stats.pktSndLoss; }
		virtual double retransmitRate() const { Exception ex; return !getStats(ex, &_stats) ? 0 : (double)_stats.byteRetrans; }
		virtual double rtt() const { Exception ex; return !getStats(ex, &_stats) ? 0 : _stats.msRTT; }

	private:

		//virtual Mona::Socket* newSocket(Exception& ex, NET_SOCKET sockfd, const sockaddr& addr);
		virtual int	 receive(Exception& ex, void* buffer, UInt32 size, int flags, SocketAddress* pAddress);
		virtual bool close(Socket::ShutdownType type = SHUTDOWN_BOTH);

		template<typename Type>
		bool getOption(Exception& ex, SRT_SOCKOPT option, Type& value) const {
			if (_id == ::SRT_INVALID_SOCK) {
				SetException(ex, NET_ESHUTDOWN);
				return false;
			}
			int length(sizeof(value));
			if (::srt_getsockflag(_id, option, reinterpret_cast<char*>(&value), &length) != -1)
				return true;
			SetException(ex, GetError(::srt_getlasterror(NULL)), " ", ::srt_getlasterror_str());
			return false;
		}

		template<typename Type>
		bool setOption(Exception& ex, SRT_SOCKOPT option, Type value) {
			if (_id == ::SRT_INVALID_SOCK) {
				SetException(ex, NET_ESHUTDOWN);
				return false;
			}
			int length(sizeof(value));
			if (::srt_setsockflag(_id, option, reinterpret_cast<const char*>(&value), length) != -1)
				return true;
			SetException(ex, GetError(::srt_getlasterror(NULL)), " ", ::srt_getlasterror_str());
			return false;
		}

		std::atomic<bool>				_shutdownRecv; 
		mutable std::atomic<UInt32>		_available; // available bytes (1316 or 0 if eagain has been received)
		mutable SRT_TRACEBSTATS			_stats; // permanent stats object
	};

	static int GetError(int error=::srt_getlasterror(NULL));
	/// \brief map the SRT error to the Mona Error code

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





