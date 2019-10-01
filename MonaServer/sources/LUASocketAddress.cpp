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

using namespace std;

namespace Mona {

int __tostring(lua_State *pState) {
	SCRIPT_CALLBACK(SocketAddress, address)
		SCRIPT_WRITE_DATA(address.c_str(), address.length());
	SCRIPT_CALLBACK_RETURN
}

template<> void Script::ObjInit(lua_State *pState, SocketAddress& address) {
	Script::AddComparator<SocketAddress>(pState);

	SCRIPT_BEGIN(pState)
		SCRIPT_DEFINE_FUNCTION("__tostring", &__tostring)

		SCRIPT_DEFINE_INT("port", address.port());
		SCRIPT_DEFINE_BOOLEAN("isIPv6", address.family() == IPAddress::IPv6);
		SCRIPT_DEFINE("host", NewObject(pState, new IPAddress(address.host())));
		
	SCRIPT_END
}
template<> void Script::ObjClear(lua_State *pState, SocketAddress& address) {

}

bool LUASocketAddress::From(Exception& ex, lua_State *pState, int index, SocketAddress& address, bool withDNS) {

	if (lua_isnumber(pState, index)) {
		// just port?
		address.setPort(UInt16(lua_tonumber(pState, index)));
		return true;
	}

	if (lua_type(pState, index)==LUA_TSTRING) { // lua_type because can be encapsulated in a lua_next
		const char* value = lua_tostring(pState,index);
		if (lua_type(pState, ++index)==LUA_TSTRING)
			return withDNS ? address.setWithDNS(ex, value, lua_tostring(pState,index)) : address.set(ex, value, lua_tostring(pState,index));
		if (lua_isnumber(pState,index))
			return withDNS ? address.setWithDNS(ex, value, (UInt16)lua_tonumber(pState,index)) : address.set(ex, value, (UInt16)lua_tonumber(pState,index));
		return withDNS ? address.setWithDNS(ex, value) : address.set(ex, value);
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
			if (lua_isnumber(pState, index)) {
				address.set(*pHost, (UInt16)lua_tonumber(pState, index));
				return true;
			}
			return address.set(ex, *pHost, 0);
		}
	}

	ex.set<Ex::Net::Address>("No valid SocketAddress arguments");
	return false;
}

}
