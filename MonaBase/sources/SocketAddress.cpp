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


#include "Mona/SocketAddress.h"
#include "Mona/String.h"


using namespace std;

namespace Mona {


const SocketAddress& SocketAddress::Wildcard(IPAddress::Family family) {
	if (family == IPv6) {
		static SocketAddress IPv6Wildcard(IPAddress::IPv6);
		return IPv6Wildcard;
	}
	static SocketAddress IPv4Wildcard(IPAddress::IPv4);
	return IPv4Wildcard;
}
UInt16 SocketAddress::SplitLiteral(const char* value, string& host) {
	UInt16 port(0);
	host.assign(value);
	const char* colon(strchr(value, ':'));
	if (colon && String::ToNumber(colon + 1, port)) // check if it's really a marker port colon (could be a IPv6 colon)
		host.resize(colon - value);
	return port;
}

bool SocketAddress::setPort(Exception& ex, const char* port) {
	UInt16 number;
	if (!String::ToNumber(ex, port, number))
		return false;
	IPAddress::setPort(number);
	return true;
}

bool SocketAddress::set(Exception& ex, const IPAddress& host, const char* port) {
	if (!setPort(ex, port))
		return false;
	IPAddress::set(host);
	return true;
}

SocketAddress& SocketAddress::set(BinaryReader& reader, Family family) {
	IPAddress::set(reader, family);
	setPort(reader.read16());
	return self;
}

bool SocketAddress::setIntern(Exception& ex, const char* host, const char* port, bool resolveHost) {
	if(!port) // to solve the ambiguitis call between set(..., const char* port) and set(..., UInt16 port) when port = 0
		return setIntern(ex, host, UInt16(0), resolveHost);
	UInt16 number;
	if (!String::ToNumber(port, number)) {
		ex.set<Ex::Net::Address::Port>("Invalid port number ", port);
		return false;
	}
	return setIntern(ex, host, number, resolveHost);
}

bool SocketAddress::setIntern(Exception& ex, const char* hostAndPort, bool resolveHost) {
	const char* colon(strrchr(hostAndPort,':'));
	if (colon)
		return setIntern(ex, string(hostAndPort, colon - hostAndPort).c_str(), colon+1, resolveHost);
	ex.set<Ex::Net::Address::Port>("Missing port number in ", hostAndPort);
	return false;
}

bool SocketAddress::operator < (const SocketAddress& address) const {
	if (family() != address.family())
		return family() < address.family();
	if (host() != address.host())
		return host() < address.host();
	return (port() < address.port());
}


}	// namespace Mona
