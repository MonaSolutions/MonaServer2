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

size_t SocketAddress::ParsePort(string& address, UInt16& port) {
	size_t size = ParsePort(address.data(), address.size(), port);
	if (size == string::npos)
		return size;
	address.resize(size);
	return size;
}
size_t SocketAddress::ParsePort(const char* address, std::size_t size, UInt16& port) {
	if (size == string::npos)
		size = strlen(address);
	const char* colon = NULL;
	const char* begin = address;
	address += size;
	while (size--) {
		char c = *--address;
		switch (c) {
			case '.':
			case ']':
				// if no colon => no port
				// if colon => can be port, stop here
				size = 0;
				break;
			case ':':
				if (colon) // we are on a case with two :: without brakets => IPv6 without port!
					return string::npos;
				colon = address;
			default:;
		}
	}
	if (colon && String::ToNumber(colon + 1, port))
		return colon - begin;
	return string::npos;
}


SocketAddress& SocketAddress::set(BinaryReader& reader, Family family) {
	IPAddress::set(reader, family);
	setPort(reader.read16());
	return self;
}


bool SocketAddress::setIntern(Exception& ex, const char* hostAndPort, std::size_t size, bool resolveHost) {
	UInt16 port;
	size = ParsePort(hostAndPort, size, port);
	if (size != string::npos)
		return setIntern(ex, string(hostAndPort, size).c_str(), port, resolveHost);
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
