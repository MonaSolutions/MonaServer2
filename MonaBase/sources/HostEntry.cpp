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


#include "Mona/HostEntry.h"

using namespace std;

namespace Mona {

	
void HostEntry::set(Exception& ex,const hostent& entry) {

	_name = entry.h_name;
	char** alias = entry.h_aliases;
	if (alias) {
		while (*alias) {
			_aliases.emplace_back(*alias);
			++alias;
		}
	}

	char** address = entry.h_addr_list;
	if (address) {
		while (*address) {
			if (entry.h_addrtype == AF_INET6) {
				_addresses.emplace(*reinterpret_cast<in_addr*>(*address));
			} else if (entry.h_addrtype == AF_INET) {
				_addresses.emplace(*reinterpret_cast<in6_addr*>(*address));
			} else
				ex.set<Ex::Net::Address::Ip>("Unsupported host entry");
			++address;
		}
	}
}


void HostEntry::set(Exception& ex, const addrinfo* ainfo) {
	for (; ainfo; ainfo = ainfo->ai_next) {
		if (ainfo->ai_canonname)
			_name.assign(ainfo->ai_canonname);
		if (ainfo->ai_addrlen && ainfo->ai_addr) {
			if (ainfo->ai_addr->sa_family == AF_INET6)
				_addresses.emplace(reinterpret_cast<sockaddr_in6*>(ainfo->ai_addr)->sin6_addr, (UInt32)reinterpret_cast<struct sockaddr_in6*>(ainfo->ai_addr)->sin6_scope_id);
			else if (ainfo->ai_addr->sa_family == AF_INET)
				_addresses.emplace(reinterpret_cast<sockaddr_in*>(ainfo->ai_addr)->sin_addr);
			else
				ex.set<Ex::Net::Address::Ip>("Unknown ip family ", ainfo->ai_addr->sa_family);
		}
	}
}



} // namespace Mona
