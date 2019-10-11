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
#include "Mona/TCPClient.h"
#include "LUASocketAddress.h"


using namespace std;

namespace Mona {

static int connected(lua_State *pState) {
	SCRIPT_CALLBACK(TCPClient, client)
		SCRIPT_WRITE_BOOLEAN(client.connected())
	SCRIPT_CALLBACK_RETURN
}
static int connecting(lua_State *pState) {
	SCRIPT_CALLBACK(TCPClient, client)
		SCRIPT_WRITE_BOOLEAN(client.connecting())
	SCRIPT_CALLBACK_RETURN
}
static int connect(lua_State *pState) {
	SCRIPT_CALLBACK_TRY(TCPClient, client)
		Exception ex;
		SocketAddress address(IPAddress::Loopback(), 0);
		if (SCRIPT_READ_ADDRESS(address) && client.connect(ex, address)) {
			if (ex)
				SCRIPT_CALLBACK_THROW(ex);
			SCRIPT_WRITE_BOOLEAN(true);
		} else
			SCRIPT_CALLBACK_THROW(ex);
	SCRIPT_CALLBACK_RETURN
}
static int send(lua_State *pState) {
	SCRIPT_CALLBACK_TRY(TCPClient, client)
		SCRIPT_READ_PACKET(packet);
		Exception ex;
		if (client.send(ex, packet)) {
			if (ex)
				SCRIPT_WARN(ex);
			SCRIPT_WRITE_BOOLEAN(true);
		} else
			SCRIPT_CALLBACK_THROW(ex);
	SCRIPT_CALLBACK_RETURN
}
static int disconnect(lua_State *pState) {
	SCRIPT_CALLBACK(TCPClient, client)
		client.disconnect();
	SCRIPT_CALLBACK_RETURN
}

template<> void Script::ObjInit(lua_State *pState, TCPClient& client) {
	client.onError = [pState, &client](const Exception& ex) {
		bool gotten = false;
		SCRIPT_BEGIN(pState)
			SCRIPT_MEMBER_FUNCTION_BEGIN(client, "onError")
				gotten = true;
				SCRIPT_WRITE_STRING(ex)
				SCRIPT_FUNCTION_CALL
			SCRIPT_FUNCTION_END
		SCRIPT_END
		if (!gotten)
			WARN(ex);
	};
	client.onFlush = [pState, &client]() {
		SCRIPT_BEGIN(pState)
			SCRIPT_MEMBER_FUNCTION_BEGIN(client, "onFlush")
				SCRIPT_FUNCTION_CALL
			SCRIPT_FUNCTION_END
		SCRIPT_END
	};
	client.onDisconnection = [pState, &client](const SocketAddress& peerAddress) {
		SCRIPT_BEGIN(pState)
			SCRIPT_MEMBER_FUNCTION_BEGIN(client, "onDisconnection")
				SCRIPT_WRITE_ADDRESS(peerAddress);
				SCRIPT_FUNCTION_CALL
			SCRIPT_FUNCTION_END
		SCRIPT_END
	};
	client.onData = [pState, &client](Packet& buffer) {
		UInt32 rest = 0;
		SCRIPT_BEGIN(pState)
			SCRIPT_MEMBER_FUNCTION_BEGIN(client, "onData")
				Script::NewObject(pState, new Packet(buffer));
				SCRIPT_FUNCTION_CALL
				if(SCRIPT_NEXT_READABLE)
					rest = SCRIPT_READ_UINT32(0); // by default, consume all
			SCRIPT_FUNCTION_END
		SCRIPT_END
		return rest;
	};
	if (client->type < Socket::TYPE_OTHER)
		AddType<Socket>(pState, *client);
	SCRIPT_BEGIN(pState);
		SCRIPT_DEFINE_FUNCTION("connected", &connected);
		SCRIPT_DEFINE_FUNCTION("connecting", &connecting);
		SCRIPT_DEFINE_FUNCTION("connect", &connect);
		SCRIPT_DEFINE_FUNCTION("disconnect", &disconnect);
		SCRIPT_DEFINE_FUNCTION("send", &send);
	SCRIPT_END;
}
template<> void Script::ObjClear(lua_State *pState, TCPClient& client) {
	if (client->type < Socket::TYPE_OTHER)
		RemoveType<Socket>(pState, *client);
	client.onData = nullptr;
	client.onFlush = nullptr;
	client.onDisconnection = nullptr;
	client.onError = nullptr;
}

}
