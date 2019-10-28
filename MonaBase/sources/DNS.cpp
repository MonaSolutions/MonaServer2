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


#include "Mona/DNS.h"
#include "Mona/SocketAddress.h"

using namespace std;


namespace Mona {


bool DNS::HostByName(Exception& ex, const char* hostname, HostEntry& host) {
	struct addrinfo* pAI;
	struct addrinfo hints;
	memset(&hints, 0, sizeof(hints));
	hints.ai_flags = AI_CANONNAME | AI_ADDRCONFIG;
	int rc = getaddrinfo(hostname, NULL, &hints, &pAI); 
	if (rc == 0) {
		host.set(ex,pAI);
		freeaddrinfo(pAI);
		return true;
	}
	SetAIError(ex,rc, " (hostname=",hostname,")");
	return false;
}

// BEWARE blocking method!!
bool DNS::HostByAddress(Exception& ex,const IPAddress& address, HostEntry& host) {
	SocketAddress sa;
	sa.set(address, 0);
	static char fqname[1024];
	int rc = getnameinfo(sa.data(), sa.size(), fqname, sizeof(fqname), NULL, 0, NI_NAMEREQD);
	if (rc == 0) {
		struct addrinfo* pAI;
		struct addrinfo hints;
		memset(&hints, 0, sizeof(hints));
		hints.ai_flags = AI_CANONNAME | AI_ADDRCONFIG;
		rc = getaddrinfo(fqname, NULL, &hints, &pAI);
		if (rc == 0) {
			host.set(ex, pAI);
			freeaddrinfo(pAI);
			return true;
		}
	}
	SetAIError(ex, rc, " (address=", address,")");
	return false;
}


bool DNS::Resolve(Exception& ex, const char* address, HostEntry& host) {
	IPAddress ip;
	Exception ignore;
	if (ip.set(ignore, address))
		return HostByAddress(ex,ip,host);
	return HostByName(ex, address, host);
}

bool DNS::ThisHost(Exception& ex, HostEntry& host) {
	string name;
	if (!HostName(ex, name))
		return false;
	return HostByName(ex,name, host);
}


bool DNS::HostName(Exception& ex,string& host) {
	char buffer[256];
	int rc = gethostname(buffer, sizeof(buffer));
	if (rc == 0) {
		host.assign(buffer);
		return true;
	}
	ex.set<Ex::Net::System>("Cannot get host name, ",Net::LastErrorMessage());
	return false;
}


} // namespace Mona
