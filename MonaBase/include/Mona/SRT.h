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
	
#if defined(SRT_API)
	static int			LastError();
	static const char*	LastErrorMessage() { return ::srt_getlasterror_str(); }

	struct Stats : Net::Stats {
		NULLABLE(!_stats.msTimeStamp)

		Stats() { _stats.msTimeStamp = 0; }
	
		double bandwidthEstimated() const { return self ? _stats.mbpsBandwidth : 0; }
		double bandwidthMaxUsed() const { return self ? _stats.mbpsMaxBW : 0; }
		UInt32 recvBufferTime() const { return self ? _stats.msRcvBuf : 0; }
		UInt32 recvNegotiatedDelay() const { return self ? _stats.msRcvTsbPdDelay : 0; }
		UInt64 recvLostCount() const { return self ? _stats.pktRcvLossTotal : 0; }
		double recvLostRate() const { return self ? _stats.pktRcvLoss : 0; }
		UInt32 recvDropped() const { return self ? _stats.pktRcvDropTotal : 0; }
		UInt32 sendBufferTime() const { return self ? _stats.msSndBuf : 0; }
		UInt32 sendNegotiatedDelay() const { return self ? _stats.msSndTsbPdDelay : 0; }
		UInt64 sendLostCount() const { return self ? _stats.pktSndLossTotal : 0; }
		double sendLostRate() const { return self ? _stats.pktSndLoss : 0; }
		UInt32 sendDropped() const { return self ? _stats.pktSndDropTotal : 0; }
		UInt64 retransmitRate() const { return self ? _stats.byteRetrans : 0; }
		double rtt() const { return self ? _stats.msRTT : 0; }

		Time	recvTime() const { return self ? (Time::Now() - _stats.msRcvTsbPdDelay) : 0; }
		UInt64	recvByteRate() const { return UInt64(self ? _stats.mbpsRecvRate : 0); }
		Time	sendTime() const { return self ? (Time::Now() - _stats.msSndTsbPdDelay) : 0; }
		UInt64	sendByteRate() const { return UInt64(self ? _stats.mbpsSendRate : 0); }
		UInt64	queueing() const { return self ? _stats.pktCongestionWindow : 0; }

		const SRT_TRACEBSTATS*	operator->() const { return &_stats; }
		SRT_TRACEBSTATS*		operator&() { return &_stats; }

	private:
		SRT_TRACEBSTATS _stats;
	};

	struct Socket : virtual Object, Mona::Socket {
		Socket();
		virtual ~Socket();

		enum Type {
			TYPE_SRT = Mona::Socket::TYPE_OTHER,
		};

		bool  isSecure() const { return true; }

		UInt32  available() const;

		const std::string& streamId() const { return _streamId; }

		virtual bool accept(Exception& ex, shared<Mona::Socket>& pSocket);

		virtual bool connect(Exception& ex, const SocketAddress& address, UInt16 timeout = 0);

		virtual int	 sendTo(Exception& ex, const void* data, UInt32 size, const SocketAddress& address, int flags = 0);

		virtual bool bind(Exception& ex, const SocketAddress& address);

		virtual bool listen(Exception& ex, int backlog = SOMAXCONN);

		virtual bool setNonBlockingMode(Exception& ex, bool value);

		virtual bool processParams(Exception& ex, const Parameters& parameter, const char* prefix = "srt.");

		virtual bool setSendBufferSize(Exception& ex, UInt32 size);
		virtual bool getSendBufferSize(Exception& ex, UInt32& size) const { return getOption(ex, ::SRTO_UDP_SNDBUF, size); }
		virtual bool setRecvBufferSize(Exception& ex, UInt32 size);
		virtual bool getRecvBufferSize(Exception& ex, UInt32& size) const { return getOption(ex, ::SRTO_UDP_RCVBUF, size); }

		virtual bool setReuseAddress(Exception& ex, bool value) { return setOption(ex, ::SRTO_REUSEADDR, value ? 1 : 0); }
		virtual bool getReuseAddress(Exception& ex, bool& value) const { int val; return getOption(ex, ::SRTO_REUSEADDR, val); }

		virtual bool setLinger(Exception& ex, bool on, int seconds);
		virtual bool getLinger(Exception& ex, bool& on, int& seconds) const;

		bool setConnectionTimeout(Exception& ex, UInt32 value) { return setOption(ex, ::SRTO_CONNTIMEO, value); }
		bool getConnectionTimeout(Exception& ex, UInt32& value) const { return getOption(ex, ::SRTO_CONNTIMEO, value); }

		bool getRecvEncryptionState(Exception& ex, SRT_KM_STATE& state) const { return getOption(ex, ::SRTO_RCVKMSTATE, state); }
		bool getSendEncryptionState(Exception& ex, SRT_KM_STATE& state) const { return getOption(ex, ::SRTO_SNDKMSTATE, state); }
		bool setEncryptionSize(Exception& ex, UInt32 type) { return setOption(ex, ::SRTO_PBKEYLEN, type); }
		bool getEncryptionSize(Exception& ex, UInt32& type) const { return getOption(ex, ::SRTO_PBKEYLEN, type); }
		bool setPassphrase(Exception& ex, const char* data, UInt32 size);
		bool setStreamId(Exception& ex, const char* data, UInt32 size);

		bool setLatency(Exception& ex, UInt32 value) { return setOption(ex, ::SRTO_RCVLATENCY, value); }
		bool getLatency(Exception& ex, UInt32& value) const { return getOption(ex, ::SRTO_RCVLATENCY, value); }
		bool setPeerLatency(Exception& ex, UInt32 value) { return setOption(ex, ::SRTO_PEERLATENCY, value); }
		bool getPeerLatency(Exception& ex, UInt32& value) const { return getOption(ex, ::SRTO_PEERLATENCY, value); }

		bool setMSS(Exception& ex, UInt32 value) { return setOption(ex, ::SRTO_MSS, value); }
		bool getMSS(Exception& ex, UInt32& value) const { return getOption(ex, ::SRTO_MSS, value); }
		bool setOverheadBW(Exception& ex, UInt32 value) { return setOption(ex, ::SRTO_OHEADBW, value); }
		bool getOverheadBW(Exception& ex, UInt32& value) const { return getOption(ex, ::SRTO_OHEADBW, value); } // Not supported for now in SRT
		bool setMaxBW(Exception& ex, Int64 value) { return setOption(ex, ::SRTO_MAXBW, value); }
		bool getMaxBW(Exception& ex, Int64& value) const { return getOption(ex, ::SRTO_MAXBW, value); }	

		bool setPktDrop(Exception& ex, bool value) { return setOption(ex, ::SRTO_TLPKTDROP, value); }
		bool getPktDrop(Exception& ex, bool& value) const { int val; bool res = getOption(ex, ::SRTO_TLPKTDROP, val); value = val > 0; return res; }

		bool getStats(Exception& ex, SRT::Stats& stats) const;


	private:
		Socket(SRTSOCKET id, const sockaddr& addr);
		bool setIPV6Only(Exception& ex, bool enable) { return setOption(ex, SRTO_IPV6ONLY, enable ? 1 : 0); }
		void computeAddress();

		template <typename ...Args>
		static Exception& SetException(Exception& ex, Args&&... args) {
			ex.set<Ex::Net::Socket>(LastErrorMessage(), std::forward<Args>(args)...).code = LastError();
			return ex;
		}
		template <typename ...Args>
		static Exception& SetException(int error, Exception& ex, Args&&... args) { return Mona::Socket::SetException(error, ex, std::forward<Args>(args)...);  }

		virtual int	 receive(Exception& ex, void* buffer, UInt32 size, int flags, SocketAddress* pAddress);
		virtual bool close(Socket::ShutdownType type = SHUTDOWN_BOTH);

		template<typename Type>
		bool getOption(Exception& ex, SRT_SOCKOPT option, Type& value) const {
			if (_ex) {
				ex = _ex;
				return false;
			}
			int length(sizeof(value));
			if (::srt_getsockflag(_id, option, reinterpret_cast<char*>(&value), &length) != -1)
				return true;
			SetException(ex);
			return false;
		}

		template<typename Type>
		bool setOption(Exception& ex, SRT_SOCKOPT option, Type value) {
			if (_ex) {
				ex = _ex;
				return false;
			}
			int length(sizeof(value));
			if (::srt_setsockflag(_id, option, reinterpret_cast<const char*>(&value), length) != -1)
				return true;
			SetException(ex);
			return false;
		}

		static int ListenCallback(void* opaq, SRTSOCKET ns, int hsversion, const struct sockaddr* peeraddr, const char* streamid);

		Exception				_ex;
		volatile bool			_shutdownRecv;
		std::string				_streamId;
	};

	struct Client : TCPClient {
		Client(IOSocket& io) : Mona::TCPClient(io) {}

		SRT::Socket&	operator*() { return (SRT::Socket&)TCPClient::operator*(); }
		SRT::Socket*	operator->() { return (SRT::Socket*)TCPClient::operator->(); }
	private:
		shared<Mona::Socket> newSocket() { return std::make_shared<SRT::Socket>(); }
	};

	struct Server : TCPServer {
		Server(IOSocket& io) : Mona::TCPServer(io) {}

		SRT::Socket&	operator*() { return (SRT::Socket&)TCPServer::operator*(); }
		SRT::Socket*	operator->() { return (SRT::Socket*)TCPServer::operator->(); }
	private:
		shared<Mona::Socket> newSocket() { return std::make_shared<SRT::Socket>(); }
	};


private:
	SRT();
	~SRT();
	static void Log(void* opaque, int level, const char* file, int line, const char* area, const char* message);
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





