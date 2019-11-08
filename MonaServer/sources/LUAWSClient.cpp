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

#include "Script.h"
#include "Mona/WS/WSClient.h"
#include "LUASocketAddress.h"
#include "LUAIPAddress.h"
#include "ScriptWriter.h"



using namespace std;

namespace Mona {

static int binaryData(lua_State *pState) {
	SCRIPT_CALLBACK(WSClient, client)
		if (SCRIPT_NEXT_READABLE)
			client.binaryData = SCRIPT_READ_BOOLEAN(client.binaryData);
		SCRIPT_WRITE_BOOLEAN(client.binaryData)
	SCRIPT_CALLBACK_RETURN
}
static int url(lua_State *pState) {
	SCRIPT_CALLBACK(WSClient, client)
		SCRIPT_WRITE_STRING(client.url())
	SCRIPT_CALLBACK_RETURN
}
static int ping(lua_State *pState) {
	SCRIPT_CALLBACK(WSClient, client)
		SCRIPT_WRITE_INT(client.ping())
	SCRIPT_CALLBACK_RETURN
}
static int connect(lua_State *pState) {
	SCRIPT_CALLBACK_TRY(WSClient, client)
		Exception ex;
		SocketAddress address(IPAddress::Loopback(), 80);
		if (SCRIPT_READ_ADDRESS(address) && client.connect(ex, address, SCRIPT_NEXT_READABLE ? SCRIPT_READ_STRING("") : "")) {
			if (ex)
				SCRIPT_CALLBACK_THROW(ex);
			SCRIPT_WRITE_BOOLEAN(true);
		} else
			SCRIPT_CALLBACK_THROW(ex);
	SCRIPT_CALLBACK_RETURN
}


template<> void Script::ObjInit(lua_State *pState, WSClient& client) {
	AddType<TCPClient>(pState, client);
	AddType<Mona::Client>(pState, client);
	client.onMessage = [pState, &client](DataReader& message) {
		static const char* const OnMessage("onMessage");
		SCRIPT_BEGIN(pState)
			string name;
			message.readString(name);
			SCRIPT_MEMBER_FUNCTION_BEGIN(client, name.empty() ? OnMessage : name.c_str(), OnMessage)
				if (!name.empty() && SCRIPT_FUNCTION_NAME == OnMessage)
					SCRIPT_WRITE_STRING(name);
				name.clear();
				ScriptWriter writer(pState);
				message.read(writer);
				SCRIPT_FUNCTION_CALL
			SCRIPT_FUNCTION_END
			if(!name.empty()) // rpc fails just if explicit name given not found!
				ERROR("Method ", name, " client ", client.address, " not found");
		SCRIPT_END
	};
	SCRIPT_BEGIN(pState);
		SCRIPT_DEFINE_FUNCTION("binaryData", &binaryData);
		SCRIPT_DEFINE_FUNCTION("url", &url);
		SCRIPT_DEFINE_FUNCTION("connect", &connect);
		SCRIPT_DEFINE_FUNCTION("ping", &ping);
	SCRIPT_END;
}
template<> void Script::ObjClear(lua_State *pState, WSClient& client) {
	RemoveType<TCPClient>(pState, client);
	RemoveType<Mona::Client>(pState, client);
}

}
