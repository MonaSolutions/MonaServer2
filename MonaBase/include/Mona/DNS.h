/*
This code is in part based on code from the POCO C++ Libraries,
licensed under the Boost software license :
https://www.boost.org/LICENSE_1_0.txt

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
#include "Mona/HostEntry.h"


namespace Mona {

/*!
This class provides an interface to the domain name service.
An internal DNS cache is used to speed up name lookups. */
struct DNS : virtual Static {
	// Returns a HostEntry object containing the DNS information for the host with the given name
	static bool HostByName(Exception& ex, const std::string& hostname, HostEntry& host) { return HostByName(ex, hostname.data(), host); }
	static bool HostByName(Exception& ex, const char* hostname, HostEntry& host);
		
	// Returns a HostEntry object containing the DNS information for the host with the given IP address
	// BEWARE blocking method!!
	static bool HostByAddress(Exception& ex, const IPAddress& address, HostEntry& host);

	// Returns a HostEntry object containing the DNS information for the host with the given IP address or host name
	// BEWARE blocking method!!
	static bool Resolve(Exception& ex, const std::string& address, HostEntry& host)  { return Resolve(ex, address.data(), host); }
	static bool Resolve(Exception& ex, const char* address, HostEntry& host);
		
	// Returns a HostEntry object containing the DNS information for this host
	// BEWARE blocking method!!
	static bool ThisHost(Exception& ex,HostEntry& host);

	// Returns the host name of this host
	static bool HostName(Exception& ex, std::string& host);

private:

	// Set the exception according to the getaddrinfo() error code
	template <typename ...Args>
	static void SetAIError(Exception& ex, int error, Args&&... args) {
		ex.set<Ex::Net::Address::Ip>(gai_strerror(error), std::forward<Args>(args)...);
	}

};


} // namespace Mona
