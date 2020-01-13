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

#include "LUASocketAddress.h"
#include "LUAIPAddress.h"

using namespace std;

namespace Mona {

int __tostring(lua_State *pState) {
	SCRIPT_CALLBACK(SocketAddress, address)
		SCRIPT_WRITE_STRING(address);
	SCRIPT_CALLBACK_RETURN
}

template<> void Script::ObjInit(lua_State *pState, SocketAddress& address) {
	Script::AddComparator<SocketAddress>(pState);

	SCRIPT_BEGIN(pState)
		SCRIPT_DEFINE_FUNCTION("__tostring", &__tostring)

		SCRIPT_DEFINE_INT("port", address.port());
		SCRIPT_DEFINE_BOOLEAN("isIPv6", address.family() == IPAddress::IPv6);
		SCRIPT_DEFINE("host", SCRIPT_WRITE_IP(address.host()));
		
	SCRIPT_END
}
template<> void Script::ObjClear(lua_State *pState, SocketAddress& address) {

}

bool LUASocketAddress::From(Exception& ex, lua_State *pState, int& index, SocketAddress& address) {

	int isNum; 
	UInt16 port = range<UInt16>(lua_tointegerx(pState, index, &isNum));
	if (isNum) {
		// just port?
		address.setPort(port);
		return true;
	}

	if (lua_type(pState, index)==LUA_TSTRING) { // lua_type because can be encapsulated in a lua_next
		const char* value = lua_tostring(pState,index);
		if (lua_type(pState, ++index) == LUA_TSTRING)
			return *value == '@' ? address.setWithDNS(ex, value + 1, lua_tostring(pState, index)) : address.set(ex, value, lua_tostring(pState, index));
		port = range<UInt16>(lua_tointegerx(pState, index, &isNum));
		for(;;) {
			if (isNum)
				return *value == '@' ? address.setWithDNS(ex, value + 1, port) : address.set(ex, value, port);
			--index;
			// try with default port (if already set on address?)
			bool result = *value == '@' ? address.setWithDNS(ex, value + 1) : address.set(ex, value);
			if (result || !(port = address.port()) || !ex.cast<Ex::Net::Address::Port>())
				return result;
			isNum = true;
		}
	}
	
	if(lua_istable(pState,index)) {
		// In first SocketAddress because can be also a IPAddress!
		SocketAddress* pOther = Script::ToObject<SocketAddress>(pState, index);
		if (pOther) {
			address.set(*pOther);
			return true;
		}
		IPAddress* pHost = Script::ToObject<IPAddress>(pState, index);
		if (pHost) {
			if (lua_type(pState, ++index)==LUA_TSTRING)
				return address.set(ex, *pHost, lua_tostring(pState, index));
			port = range<UInt16>(lua_tointegerx(pState, index, &isNum));
			if (isNum || (port = address.port())) {
				address.set(*pHost, port);
				return true;
			}
			--index;
			ex.set<Ex::Net::Address::Port>("Missing port number in ", *pHost);
			return 0;
		}
	}

	ex.set<Ex::Net::Address>("No valid SocketAddress arguments");
	return 0;
}

}
