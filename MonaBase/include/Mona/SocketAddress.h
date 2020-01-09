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

#include "Mona.h"
#include "Mona/IPAddress.h"


namespace Mona {

/// This class represents an internet (IP) endpoint/socket
/// address. The address can belong either to the
/// IPv4 or the IPv6 address family and consists of a
/// host address and a port number.
struct SocketAddress : private IPAddress, virtual Object {
	CONST_STRING(addrToString());
	NULLABLE(isWildcard() && !port())

	/*!
	Creates a wildcard (all zero) IPv4/IPv6 SocketAddress */
	SocketAddress(IPAddress::Family family = IPAddress::IPv4) : IPAddress(family) {}
	/*!
	Create SocketAddress from a native socket address */
	SocketAddress(const sockaddr& addr) : IPAddress(addr) {}
	/*!
	Create a SocketAddress copy from other */
	SocketAddress(const SocketAddress& other) : IPAddress(other, other.port()) {}
	/*!
	Create a SocketAddress from host and port number */
	SocketAddress(const IPAddress& host, UInt16 port) : IPAddress(host, port) {}
	/*!
	Create a SocketAddress from binary data */
	SocketAddress(BinaryReader& reader, Family family = IPv4) : IPAddress(reader, family) { setPort(reader.read16()); }
	/*!
	Set SocketAddress from a native socket address */
	SocketAddress& set(const sockaddr& addr) { IPAddress::set(addr); return *this;}
	/*!
	Set a SocketAddress from other */
	SocketAddress& set(const SocketAddress& other) { return set(other, other.port()); }
	/*!
	Set a SocketAddress from other */
	SocketAddress& operator=(const SocketAddress& other) { return set(other); }
	SocketAddress& operator=(std::nullptr_t) { return reset(); }
	/*!
	set SocketAddress from a given IP and a given port */
	SocketAddress& set(const IPAddress& host, UInt16 port) { IPAddress::set(host, port); return *this; }
	/*!
	set SocketAddress from a given IP and a parsed port */
	bool set(Exception& ex, const IPAddress& host, const std::string& port) { return set(ex, host, port.c_str()); }
	bool set(Exception& ex, const IPAddress& host, const char* port);
	/*!
	Set SocketAddress from binary data */
	SocketAddress& set(BinaryReader& reader, Family family = IPv4);
	/*!
	set SocketAddress from a parsed IP and a given port */
	bool set(Exception& ex, const std::string& host, UInt16 port) { return setIntern(ex, host.c_str(), port, false); }
	bool set(Exception& ex, const char* host, UInt16 port) { return setIntern(ex, host, port, false); }
	bool setWithDNS(Exception& ex, const std::string& host, UInt16 port) { return setIntern(ex, host.c_str(), port, true); }
	bool setWithDNS(Exception& ex, const char* host, UInt16 port) { return setIntern(ex, host, port, true); }
	/*!
	set SocketAddress from a parsed IP and a parsed port */
	bool set(Exception& ex, const std::string& host, const std::string& port) { return setIntern(ex, host.c_str(), port.c_str(), false); }
	bool set(Exception& ex, const char* host,		 const std::string& port) { return setIntern(ex, host, port.c_str(), false); }
	bool set(Exception& ex, const std::string& host, const char* port) { return setIntern(ex, host.c_str(), port, false); }
	bool set(Exception& ex, const char* host,		 const char* port) { return setIntern(ex, host, port, false); }
	bool setWithDNS(Exception& ex, const std::string& host, const std::string& port) { return setIntern(ex, host.c_str(), port.c_str(), true); }
	bool setWithDNS(Exception& ex, const char* host,		const std::string& port) { return setIntern(ex, host, port.c_str(), true); }
	bool setWithDNS(Exception& ex, const std::string& host, const char* port) { return setIntern(ex, host.c_str(), port, true); }
	bool setWithDNS(Exception& ex, const char* host,		const char* port) { return setIntern(ex, host, port, true); }
	/*!
	set SocketAddress from a parsed IP:port */
	bool set(Exception& ex, const std::string& hostAndPort) { return setIntern(ex, hostAndPort.c_str(), false); }
	bool set(Exception& ex, const char* hostAndPort) { return setIntern(ex, hostAndPort, false); }
	bool setWithDNS(Exception& ex, const std::string& hostAndPort) { return setIntern(ex, hostAndPort.c_str(), true); }
	bool setWithDNS(Exception& ex, const char* hostAndPort) { return setIntern(ex, hostAndPort, true); }
	
	SocketAddress&  setPort(UInt16 port) { IPAddress::setPort(port); return self; }
	bool			setPort(Exception& ex, const char* port);
	bool			setPort(Exception& ex, const std::string& port) { return setPort(ex, port.c_str()); }

	SocketAddress& reset() { IPAddress::reset(); return self; }

	IPAddress::Family		family() const { return IPAddress::family(); }
	IPAddress&				host() { return *this; }
	const IPAddress&		host() const { return *this; }
	UInt16					port() const { return IPAddress::port(); }

	// Native socket address
	const sockaddr*	data() const { return reinterpret_cast<const sockaddr*>(&IPAddress::addr()); }
	UInt8			size() const { return sizeof(IPAddress::addr()); }
	
	bool operator == (const SocketAddress& address) const { return port() == address.port() && host() == address.host(); }
	bool operator != (const SocketAddress& address) const { return !operator==(address); }
	bool operator < (const SocketAddress& address) const;
	bool operator <= (const SocketAddress& address) const { return operator==(address) || operator<(address); }
	bool operator >  (const SocketAddress& address) const { return !operator<=(address); }
	bool operator >= (const SocketAddress& address) const { return operator==(address) || operator>(address); }

	// Returns a wildcard IPv4 or IPv6 address (0.0.0.0) with port to 0
	static const SocketAddress& Wildcard(IPAddress::Family family = IPAddress::IPv4);

	static UInt16 SplitLiteral(const char* value, std::string& host);
	static UInt16 SplitLiteral(const std::string& value, std::string& host) { return SplitLiteral(value.data(),host); }

private:
	bool setIntern(Exception& ex, const char* hostAndPort, bool resolveHost);
	bool setIntern(Exception& ex, const char* host, const char* port, bool resolveHost);
	bool setIntern(Exception& ex, const char* host, UInt16 port, bool resolveHost) { return resolveHost ? IPAddress::setWithDNS(ex, host, port) : IPAddress::set(ex, host, port); }

	

};



} // namespace Mona
