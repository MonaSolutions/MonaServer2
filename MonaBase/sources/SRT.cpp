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


#include "Mona/SRT.h"

#if defined(SRT_API)

using namespace std;

namespace Mona {

SRT::Socket::Socket() : Mona::Socket(TYPE_SRT), _shutdownRecv(false) {

	_id = ::srt_socket(AF_INET, SOCK_DGRAM, 0); // TODO: This is ipv4 for now, we must find a way to enable ipv6 without knowing the address (IPV6ONLY?)

	/*int opt = 1;
	::srt_setsockflag(_id, ::SRTO_SENDER, &opt, sizeof opt);*/
}

SRT::Socket::Socket(const sockaddr& addr, SRTSOCKET id) : Mona::Socket(id, addr, Socket::TYPE_SRT), _shutdownRecv(false) { }

SRT::Socket::~Socket() {
	if (_id == ::SRT_INVALID_SOCK)
		return;
	// gracefull disconnection => flush + shutdown + close
	/// shutdown + flush
	Exception ignore;
	flush(ignore, true);

	close();
}

UInt32 SRT::Socket::available() const {
	return SRT_LIVE_DEF_PLSIZE; // SRT buffer size
}

bool SRT::Socket::bind(Exception& ex, const SocketAddress& address) {
	//_id = ::srt_socket(address.family() == IPAddress::IPv6 ? AF_INET6 : AF_INET, SOCK_DGRAM, 0);

	if (_id == ::SRT_INVALID_SOCK)
		return false;

	union {
		struct sockaddr_in  sa_in;
		struct sockaddr_in6 sa_in6;
	} addr;
	int addrSize = address.family() == IPAddress::IPv6 ? sizeof(sockaddr_in6) : sizeof(sockaddr_in);
	memcpy(&addr, address.data(), addrSize);
	sockaddr* pAdd = reinterpret_cast<sockaddr*>(&addr);
	pAdd->sa_family = address.family() == IPAddress::IPv6 ? AF_INET6 : AF_INET;
	if (::srt_bind(_id, pAdd, addrSize)) {
		ex.set<Ex::Net::Socket>("SRT Bind: ", ::srt_getlasterror_str(), " ; net error : ", Net::LastError());
		close();
		return false;
	}

	if (address)
		_address = address; // if port = 0, will be computed!
	else
		_address.set(IPAddress::Loopback(), 0); // to advise that address must be computed

	return true;
}

bool SRT::Socket::listen(Exception& ex, int backlog) {
	if (_id == ::SRT_INVALID_SOCK) {
		ex.set<Ex::Net::Socket>("SRT invalid socket");
		return false;
	}

	if (::srt_listen(_id, backlog)) {
		ex.set<Ex::Net::Socket>("SRT Listen: ", ::srt_getlasterror_str());
		close();
		return false;
	}
	_listening = true;
	return true;
}

bool SRT::Socket::setNonBlockingMode(Exception& ex, bool value) {
	if (_id == ::SRT_INVALID_SOCK) {
		ex.set<Ex::Net::Socket>("SRT type is invalid");
		return false;
	}

	bool block = !value;
	if (::srt_setsockopt(_id, 0, SRTO_SNDSYN, &block, sizeof(block)) != 0) {
		ex.set<Ex::Net::Socket>("SRT unable to set SRTO_SNDSYN option : ", ::srt_getlasterror_str());
		return false;
	}
	if (::srt_setsockopt(_id, 0, SRTO_RCVSYN, &block, sizeof(block)) != 0) {
		ex.set<Ex::Net::Socket>("SRT unable to set SRTO_RCVSYN option : ", ::srt_getlasterror_str());
		return false;
	}
	
	return _nonBlockingMode = value;
}

const SocketAddress& SRT::Socket::address() const {
	if (_address && !_address.port()) {
		// computable!
		union {
			struct sockaddr_in  sa_in;
			struct sockaddr_in6 sa_in6;
		} addr;
		int addrSize = sizeof(addr);
		if (::srt_getsockname(_id, (sockaddr*)&addr, &addrSize) == 0)
			_address.set((sockaddr&)addr);
	}
	return _address;
}

bool SRT::Socket::accept(Exception& ex, shared<Mona::Socket>& pSocket) {
	if (_id == ::SRT_INVALID_SOCK) {
		ex.set<Ex::Net::Socket>("SRT invalid socket");
		return false;
	}

	sockaddr_in6 scl;
	int sclen = sizeof scl;
	::SRTSOCKET newSocket = ::srt_accept(_id, (sockaddr*)&scl, &sclen);
	if (newSocket == SRT_INVALID_SOCK) {
		if (::srt_getlasterror(NULL) == SRT_EASYNCRCV) { // not an error
			SetException(ex, NET_EWOULDBLOCK);
			return false;
		}
		ex.set<Ex::Net::Socket>("SRT accept: ", ::srt_getlasterror_str());
		return false;
	}

	pSocket.set<SRT::Socket>((sockaddr&)scl, newSocket);
	return true;
}

bool SRT::Socket::connect(Exception& ex, const SocketAddress& address, UInt16 timeout) {
	//_id = ::srt_socket(address.family() == IPAddress::IPv6 ? AF_INET6 : AF_INET, SOCK_DGRAM, 0);

	int state;
	if ((state = ::srt_getsockstate(_id)) == ::SRTS_CONNECTED || state == ::SRTS_CONNECTING)
		return true; // Note: added because can be called multiple times with Media

	union {
		struct sockaddr_in  sa_in;
		struct sockaddr_in6 sa_in6;
	} addr;
	int addrSize = address.family() == IPAddress::IPv6 ? sizeof(sockaddr_in6) : sizeof(sockaddr_in);
	memcpy(&addr, address.data(), addrSize);
	sockaddr* pAdd = reinterpret_cast<sockaddr*>(&addr);
	pAdd->sa_family = address.family() == IPAddress::IPv6 ? AF_INET6 : AF_INET;
	if (::srt_connect(_id, pAdd, addrSize) != 0) {
		close();
		ex.set<Ex::Net::Socket>("SRT connect: ", ::srt_getlasterror_str());
		return false;
	}

	_address.set(IPAddress::Loopback(), 0); // to advise that address must be computed
	_peerAddress = address;

	return true;
}

int SRT::Socket::receive(Exception& ex, void* buffer, UInt32 size, int flags, SocketAddress* pAddress) {
	if (_id == ::SRT_INVALID_SOCK) {
		ex.set<Ex::Net::Socket>("SRT invalid socket");
		return -1;
	}
	if (_shutdownRecv) {
		ex.set<Ex::Net::Socket>("SRT receive shutdown, cannot receive packets anymore");
		return -1;
	}

	// assign pAddress (no other way possible here)
	if(pAddress)
		pAddress->set(peerAddress());

	// Read the message
	int result = ::srt_recvmsg(_id, STR buffer, size); // /!\ SRT expect at least a buffer of 1316 bytes
	if (result == SRT_ERROR) {
		int error = ::srt_getlasterror(NULL);
		if (error == SRT_EASYNCRCV) // EAGAIN, wait for reception to be ready
			SetException(ex, NET_EWOULDBLOCK, " (address=", pAddress ? *pAddress : _peerAddress, ", size=", size, ", flags=", flags, ")");
		else if (::srt_getlasterror(NULL) == SRT_ENOCONN) // Connecting (TODO: not sure it is well handled)
			SetException(ex, NET_ENOTCONN, " (address=", pAddress ? *pAddress : _peerAddress, ", size=", size, ", flags=", flags, ")");
		else if (error == ::SRT_ECONNLOST) // Connection closed, not an error
			SetException(ex, NET_ESHUTDOWN, " (address=", pAddress ? *pAddress : _peerAddress, ", size=", size, ", flags=", flags, ")");
		else { 
			ex.set<Ex::Net::Socket>("Unable to receive data from SRT : ", ::srt_getlasterror_str(), " on socket ", id(), " ; ", error);
			close();
		}
		return -1;
	}
	Mona::Socket::receive(result);

	if (!_address)
		_address.set(IPAddress::Loopback(), 0); // to advise that address is computable

	return result;
}

int SRT::Socket::sendTo(Exception& ex, const void* data, UInt32 size, const SocketAddress& address, int flags) {
	if (!size || size > ::SRT_LIVE_DEF_PLSIZE) {
		ex.set<Ex::Net::Socket>("SRT: data sent must be in the range [1,1316]");
		return -1;
	}

	/*size_t chunk = 0;
	for (size_t i = 0; i < size; i += chunk) {
		chunk = min<size_t>(size - i, (size_t)SRT_LIVE_DEF_PLSIZE);
		if (::srt_send(_id, ((STR data) + i), chunk) < 0) {
			ex.set<Ex::Net::Socket>("SRT: send error; ", ::srt_getlasterror_str());
			return -1;
		}
	}*/
	if (::srt_send(_id, STR data, size) < 0) {
		if (::srt_getlasterror(NULL) == SRT_ENOCONN)
			SetException(ex, NET_ENOTCONN, " (address=", address ? address : _peerAddress, ", size=", size, ", flags=", flags, ")");
		else
			ex.set<Ex::Net::Socket>("SRT: send error; ", ::srt_getlasterror_str());
		return -1;
	}

	Mona::Socket::send(size);

	if (!_address)
		_address.set(IPAddress::Loopback(), 0); // to advise that address is computable

	return size;
}

UInt64 SRT::Socket::queueing() const {
	// TODO
	return Mona::Socket::queueing();
}

bool SRT::Socket::flush(Exception& ex, bool deleting) {
	// Call when Writable!
	// TODO
	return Mona::Socket::flush(ex, deleting);
}

bool SRT::Socket::close(Socket::ShutdownType type) {
	if (type == Socket::SHUTDOWN_RECV) {
		_shutdownRecv = true;
		return true;
	}
	if (::srt_close(_id) < 0)
		return false;
	return true;
}

bool SRT::Socket::setRecvBufferSize(Exception& ex, int size) {
	int state;
	if ((state = ::srt_getsockstate(_id)) == ::SRTS_CONNECTED || state == ::SRTS_CONNECTING)
		return true; // Note: added because can be called multiple times with Media

	if (!setOption(ex, SOL_SOCKET, ::SRTO_UDP_RCVBUF, size))
		return false;
	_recvBufferSize = size;
	return true;
}
bool SRT::Socket::setSendBufferSize(Exception& ex, int size) {
	int state;
	if ((state = ::srt_getsockstate(_id)) == ::SRTS_CONNECTED || state == ::SRTS_CONNECTING)
		return true; // Note: added because can be called multiple times with Media

	if (!setOption(ex, SOL_SOCKET, ::SRTO_UDP_SNDBUF, size))
		return false;
	_sendBufferSize = size;
	return true;
}

bool SRT::Socket::getOption(Exception& ex, int level, SRT_SOCKOPT option, int& value) const {
	if (_ex) {
		ex = _ex;
		return false;
	}
	int length(sizeof(value));
	if (::srt_getsockopt(_id, level, option, reinterpret_cast<char*>(&value), &length) != -1)
		return true;
	ex.set<Ex::Net::Socket>("Unable to get option ", option, " value : ", ::srt_getlasterror_str());
	return false;
}

bool SRT::Socket::setOption(Exception& ex, int level, SRT_SOCKOPT option, int value) {
	if (_ex) {
		ex = _ex;
		return false;
	}
	int length(sizeof(value));
	if (::srt_setsockopt(_id, level, option, reinterpret_cast<const char*>(&value), length) != -1)
		return true;
	ex.set<Ex::Net::Socket>("Unable to set option ", option, " to ", value, " : ", ::srt_getlasterror_str());
	return false;
}

}  // namespace Mona

#endif
