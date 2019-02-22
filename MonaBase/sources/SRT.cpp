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

unique<SRT::Singleton> SRT::SRT_Singleton;

void SRT::LogCallback(void* opaque, int level, const char* file, int line, const char* area, const char* message) {
	//if (level != 7)
		WARN("L:", level, "|", file, "|", line, "|", area, "|", message)
}

SRT::Socket::Socket() : Mona::Socket(TYPE_DATAGRAM), _srt(::SRT_INVALID_SOCK) {

	if (!SRT_Singleton)
		SRT_Singleton.reset(new SRT::Singleton());
	/*_srt = ::srt_socket(AF_INET6, SOCK_DGRAM, 0);

	bool block = false;
	if (::srt_setsockopt(_srt, 0, SRTO_RCVSYN, &block, sizeof(block)) != 0) {
		disconnect();
		ERROR("SRT SRTO_RCVSYN: ", ::srt_getlasterror_str());
	}*/
}

SRT::Socket::Socket(const sockaddr& addr, SRTSOCKET id) : Mona::Socket(addr), _srt(id) {}

SRT::Socket::~Socket() {
	// gracefull disconnection => flush + shutdown + close
	/// shutdown + flush
	Exception ignore;
	flush(ignore, true);

	disconnect();
}

void SRT::Socket::disconnect() {
	if (_srt != ::SRT_INVALID_SOCK) {
		::srt_close(_srt);
		_srt = ::SRT_INVALID_SOCK;
	}
}

UInt32 SRT::Socket::available() const {
	return 1316; // SRT buffer size
}

bool SRT::Socket::bind(Exception& ex, const SocketAddress& address) {
	_srt = ::srt_socket(AF_INET6, SOCK_DGRAM, 0);

	bool block = false;
	if (::srt_setsockopt(_srt, 0, SRTO_RCVSYN, &block, sizeof(block)) != 0) {
		disconnect();
		ex.set<Ex::Net::Socket>("SRT SRTO_RCVSYN: ", ::srt_getlasterror_str());
		return false;
	}

	if (!Mona::Socket::bind(ex, address))
		return false;

	int result = srt_bind_peerof(_srt, id()); // /!\ Allow to bind and subscribe from IOSocket events
	if (result == SRT_ERROR) {
		ex.set<Ex::Net::Socket>("Unable to call bind_peerof : ", ::srt_getlasterror_str(), " on socket ", id());
		disconnect();
		return false;
	}

	return true;
}

bool SRT::Socket::listen(Exception& ex, int backlog) {

	if (::srt_listen(_srt, backlog)) {
		ex.set<Ex::Net::Socket>("SRT Listen: ", ::srt_getlasterror_str());
		disconnect();
		return false;
	}
	_listening = true;
	return true;
}

bool SRT::Socket::accept(Exception& ex, shared<Mona::Socket>& pSocket) {
	
	sockaddr_in6 scl;
	int sclen = sizeof scl;
	::SRTSOCKET newSocket = ::srt_accept(_srt, (sockaddr*)&scl, &sclen);
	if (newSocket == SRT_INVALID_SOCK) {
		int error = ::srt_getlasterror(NULL);
		if (error == SRT_EASYNCRCV) // not an error
			return false;
		ex.set<Ex::Net::Socket>("SRT accept: ", ::srt_getlasterror_str(), " ; ", error);
		return false;
	}

	// set non blocking
	bool blocking = false;
	if (::srt_setsockopt(newSocket, 0, SRTO_RCVSYN, &blocking, sizeof blocking) == -1) {
		ex.set<Ex::Net::Socket>("SRT SRTO_RCVSYN: ", ::srt_getlasterror_str());
		::srt_close(newSocket);
		return false;
	}

	pSocket.reset(new SRT::Socket((sockaddr&)scl, newSocket));
	return true;
}

bool SRT::Socket::connect(Exception& ex, const SocketAddress& address, UInt16 timeout) {
	// TODO
	_srt = ::srt_socket(AF_INET6, SOCK_DGRAM, 0);

	bool block = false;
	if (::srt_setsockopt(_srt, 0, SRTO_RCVSYN, &block, sizeof(block)) != 0) {
		disconnect();
		ex.set<Ex::Net::Socket>("SRT SRTO_RCVSYN: ", ::srt_getlasterror_str());
		return false;
	}

	if (::srt_setsockopt(_srt, 0, SRTO_SNDSYN, &block, sizeof(block)) != 0) {
		disconnect();
		ex.set<Ex::Net::Socket>("SRT SRTO_SNDSYN: ", ::srt_getlasterror_str());
		return false;
	}

	if (::srt_connect(_srt, address.data(), address.size()) != 0) {
		disconnect();
		ex.set<Ex::Net::Socket>("SRT connect: ", ::srt_getlasterror_str());
		return false;
	}

	return true;
}

int SRT::Socket::receive(Exception& ex, void* buffer, UInt32 size, int flags, SocketAddress* pAddress) {
	if (_srt == ::SRT_INVALID_SOCK)
		return -1;

	// assign pAddress (no other way possible here)
	if(pAddress)
		pAddress->set(peerAddress());

	// Read the message
	int result = ::srt_recvmsg(_srt, STR buffer, size); // /!\ SRT expect at least a buffer of 1316 bytes
	if (result == SRT_ERROR) {
		int error = ::srt_getlasterror(NULL);
		if (error == SRT_EASYNCRCV)
			return -1; // EAGAIN, wait for reception to be ready
		disconnect();
		if (error != ::SRT_ECONNLOST) // connection closed, not an error
			ex.set<Ex::Net::Socket>("Unable to receive data from SRT : ", ::srt_getlasterror_str(), " on socket ", id(), " ; ", error);
		return -1;
	}
	Mona::Socket::receive(result);
	return result;
}

int SRT::Socket::sendTo(Exception& ex, const void* data, UInt32 size, const SocketAddress& address, int flags) {
	// TODO
	int result = 0;
	if (result > 0)
		Mona::Socket::send(result);
	return result;
}

UInt64 SRT::Socket::queueing() const {
	// TODO
	return Mona::Socket::queueing();
}

bool SRT::Socket::flush(Exception& ex, bool deleting) {
	// Call when Writable!
	// TODO
	return true;
}

bool SRT::Socket::close(Socket::ShutdownType type) {
	// TODO
	return Mona::Socket::close(type);
}

}  // namespace Mona

#endif
