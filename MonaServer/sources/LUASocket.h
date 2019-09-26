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

#pragma once

#include "Script.h"
#include "Mona/Socket.h"
#include "LUANetStats.h"

namespace Mona {

#define SCRIPT_DEFINE_SOCKET(TYPE)	SCRIPT_DEFINE_FUNCTION("address", &LUASocket<TYPE>::Address);\
									SCRIPT_DEFINE_FUNCTION("peerAddress", &LUASocket<TYPE>::PeerAddress);\
									SCRIPT_DEFINE_FUNCTION("isSecure", &LUASocket<TYPE>::IsSecure);\
									SCRIPT_DEFINE_NETSTATS(TYPE)

template<typename Type>
struct LUASocket : virtual Static {
	static int Address(lua_State *pState) {
		SCRIPT_CALLBACK(Type, object);
			Script::NewObject(pState, new SocketAddress(Socket(object).address()));
		SCRIPT_CALLBACK_RETURN;
	}
	static int PeerAddress(lua_State *pState) {
		SCRIPT_CALLBACK(Type, object);
			Script::NewObject(pState, new SocketAddress(Socket(object).peerAddress()));
		SCRIPT_CALLBACK_RETURN;
	}
	static int IsSecure(lua_State *pState) {
		SCRIPT_CALLBACK(Type, object);
			SCRIPT_WRITE_BOOLEAN(Socket(object).isSecure());
		SCRIPT_CALLBACK_RETURN;
	}
private:
	static Mona::Socket& Socket(Type& object);
};

}
