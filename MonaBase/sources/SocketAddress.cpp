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

bool SocketAddress::set(Exception& ex, const IPAddress& host, const char* service) {
	UInt16 port = resolveService(ex, service);
	if (!port)
		return false;
	set(host, port);
	return true;
}

bool SocketAddress::setIntern(Exception& ex, const char* host, const char* service, bool resolveHost) {
	UInt16 port = resolveService(ex, service);
	if (!port)
		return false;
	return setIntern(ex, host, port, resolveHost);
}

bool SocketAddress::setIntern(Exception& ex, const char* hostAndPort, bool resolveHost) {
	const char* colon(strrchr(hostAndPort,':'));
	if (!colon) {
		ex.set<Ex::Net::Address::Port>("Missing port number in ", hostAndPort);
		return false;
	}
	
	UInt16 port = resolveService(ex, colon+1);
	return port ? setIntern(ex, string(hostAndPort, colon - hostAndPort).c_str(), port, resolveHost) : false;
}

UInt16 SocketAddress::resolveService(Exception& ex,const char* service) {
	UInt16 port=0;
	if (String::ToNumber<UInt16>(service,port))
		return port;
	struct servent* se = getservbyname(service, NULL);
	if (se)
		return ntohs(se->s_port);
	ex.set<Ex::Net::Address::Port>("Service ", service, " unknown");
	return 0;
}

bool SocketAddress::operator < (const SocketAddress& address) const {
	if (family() != address.family())
		return family() < address.family();
	if (host() != address.host())
		return host() < address.host();
	return (port() < address.port());
}


const SocketAddress& SocketAddress::Wildcard(IPAddress::Family family) {
	if (family == IPv6) {
		static SocketAddress IPv6Wildcard(IPAddress::IPv6);
		return IPv6Wildcard;
	}
	static SocketAddress IPv4Wildcard(IPAddress::IPv4);
	return IPv4Wildcard;
}

UInt16 SocketAddress::SplitLiteral(const char* value,string& host) {
	UInt16 port(0);
	host.assign(value);
	const char* colon(strchr(value,':'));
	if (colon && String::ToNumber(colon+1, port)) // check if it's really a marker port colon (could be a IPv6 colon)
		host.resize(colon-value);
	return port;
}

}	// namespace Mona
