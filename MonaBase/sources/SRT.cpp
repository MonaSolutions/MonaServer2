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

SRT::Socket::Socket() : Mona::Socket(TYPE_DATAGRAM) {}

SRT::Socket::~Socket() {
	// gracefull disconnection => flush + shutdown + close
	/// shutdown + flush
	Exception ignore;
	flush(ignore, true);
	//TODO
}

UInt32 SRT::Socket::available() const {
	UInt32 available = Mona::Socket::available();
	// TODO
	return available;
}

bool SRT::Socket::connect(Exception& ex, const SocketAddress& address, UInt16 timeout) {
	// TODO
	return true;
}

int SRT::Socket::receive(Exception& ex, void* buffer, UInt32 size, int flags, SocketAddress* pAddress) {
	// TODO
	int result = 0;
	// assign pAddress (no other way possible here)
	if(pAddress)
		pAddress->set(peerAddress());
	if (result > 0)
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

