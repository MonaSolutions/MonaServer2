/*
This file is a part of MonaSolutions Copyright 2017
mathieu.poux[a]gmail.com
jammetthomas[a]gmail.com

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License received along this program for more
details (or else see http://www.gnu.org/licenses/).

*/

#include "LUAIPAddress.h"

using namespace std;

namespace Mona {

static int __tostring(lua_State *pState) {
	SCRIPT_CALLBACK(IPAddress, host)
		SCRIPT_WRITE_STRING(host);
	SCRIPT_CALLBACK_RETURN
}

template<> void Script::ObjInit(lua_State *pState, IPAddress& host) {
	Script::AddComparator<IPAddress>(pState);

	SCRIPT_BEGIN(pState);
		SCRIPT_DEFINE_FUNCTION("__tostring", &__tostring);

		SCRIPT_DEFINE_BOOLEAN("isWildcard", host.isWildcard());
		SCRIPT_DEFINE_BOOLEAN("isBroadcast", host.isBroadcast());
		SCRIPT_DEFINE_BOOLEAN("isAnyBroadcast", host.isAnyBroadcast());
		SCRIPT_DEFINE_BOOLEAN("isLoopback", host.isLoopback());
		SCRIPT_DEFINE_BOOLEAN("isMulticast", host.isMulticast());
		SCRIPT_DEFINE_BOOLEAN("isUnicast", host.isUnicast());
		SCRIPT_DEFINE_BOOLEAN("isLinkLocal", host.isLinkLocal());
		SCRIPT_DEFINE_BOOLEAN("isSiteLocal", host.isSiteLocal());
		SCRIPT_DEFINE_BOOLEAN("isIPv4Compatible", host.isIPv4Compatible());
		SCRIPT_DEFINE_BOOLEAN("isIPv4Mapped", host.isIPv4Mapped());
		SCRIPT_DEFINE_BOOLEAN("isWellKnownMC", host.isWellKnownMC());
		SCRIPT_DEFINE_BOOLEAN("isNodeLocalMC", host.isNodeLocalMC());
		SCRIPT_DEFINE_BOOLEAN("isLinkLocalMC", host.isLinkLocalMC());
		SCRIPT_DEFINE_BOOLEAN("isSiteLocalMC", host.isSiteLocalMC());
		SCRIPT_DEFINE_BOOLEAN("isOrgLocalMC", host.isOrgLocalMC());
		SCRIPT_DEFINE_BOOLEAN("isGlobalMC", host.isGlobalMC());
		SCRIPT_DEFINE_BOOLEAN("isLocal", host.isLocal());
		SCRIPT_DEFINE_BOOLEAN("isIPv6", host.family() == IPAddress::IPv6);

	SCRIPT_END;
}
template<> void Script::ObjClear(lua_State *pState, IPAddress& host) {

}


bool LUAIPAddress::From(Exception& ex, lua_State *pState, int index, IPAddress& address) {
	if (lua_type(pState, index) == LUA_TSTRING) { // lua_type because can be encapsulated in a lua_next + a host can't be a simple number!
		const char* value = lua_tostring(pState, index);
		return *value == '@' ? address.setWithDNS(ex, value+1) : address.set(ex, value);
	}

	if(lua_istable(pState,index)) {
		// In first SocketAddress because can be also a IPAddress!
		SocketAddress* pAddress = Script::ToObject<SocketAddress>(pState, index);
		if (pAddress) {
			address.set(pAddress->host());
			return true;
		}
		IPAddress* pOther = Script::ToObject<IPAddress>(pState, index);
		if (pOther) {
			address.set(*pOther);
			return true;
		}
	}
	ex.set<Ex::Net::Address::Ip>("No valid IPAddress arguments");
	return false;
}

}
