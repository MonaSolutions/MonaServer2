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
#include "Mona/UDPSocket.h"
#include "LUASocketAddress.h"


using namespace std;

namespace Mona {

static int connected(lua_State *pState) {
	SCRIPT_CALLBACK(UDPSocket, socket)
		SCRIPT_WRITE_BOOLEAN(socket.connected())
	SCRIPT_CALLBACK_RETURN
}
static int bound(lua_State *pState) {
	SCRIPT_CALLBACK(UDPSocket, socket)
		SCRIPT_WRITE_BOOLEAN(socket.bound())
	SCRIPT_CALLBACK_RETURN
}
static int connect(lua_State *pState) {
	SCRIPT_CALLBACK_TRY(UDPSocket, socket)
		Exception ex;
		SocketAddress address(IPAddress::Loopback(), 0);
		if (SCRIPT_READ_ADDRESS(address) && socket.connect(ex, address)) {
			if (ex)
				SCRIPT_CALLBACK_THROW(ex);
			SCRIPT_WRITE_BOOLEAN(true);
		} else
			SCRIPT_CALLBACK_THROW(ex);
	SCRIPT_CALLBACK_RETURN
}
static int bind(lua_State *pState) {
	SCRIPT_CALLBACK_TRY(UDPSocket, socket)
		Exception ex;
		SocketAddress address;
		if (SCRIPT_READ_ADDRESS(address) && socket.bind(ex, address)) {
			if (ex)
				SCRIPT_CALLBACK_THROW(ex);
			SCRIPT_WRITE_BOOLEAN(true);
		} else
			SCRIPT_CALLBACK_THROW(ex);
	SCRIPT_CALLBACK_RETURN
}
static int send(lua_State *pState) {
	SCRIPT_CALLBACK_TRY(UDPSocket, socket)
		SCRIPT_READ_PACKET(packet);
		Exception ex;
		SocketAddress address(IPAddress::Loopback(), 0);
		if (SCRIPT_NEXT_READABLE) {
			if (SCRIPT_READ_ADDRESS(address)) {
				if(ex)
					SCRIPT_CALLBACK_THROW(ex);
			} else {
				SCRIPT_CALLBACK_THROW(ex);
				address.reset(); // Wildcard!
			}
		}
		if (socket.send(ex, packet, address)) {
			if (ex)
				SCRIPT_CALLBACK_THROW(ex);
			SCRIPT_WRITE_BOOLEAN(true)
		} else
			SCRIPT_CALLBACK_THROW(ex);
	SCRIPT_CALLBACK_RETURN
}
static int disconnect(lua_State *pState) {
	SCRIPT_CALLBACK(UDPSocket, socket)
		socket.disconnect();
	SCRIPT_CALLBACK_RETURN
}
static int close(lua_State *pState) {
	SCRIPT_CALLBACK(UDPSocket, socket)
		socket.close();
	SCRIPT_CALLBACK_RETURN
}

template<> void Script::ObjInit(lua_State *pState, UDPSocket& socket) {
	socket.onError = [pState, &socket](const Exception& ex) {
		bool gotten = false;
		SCRIPT_BEGIN(pState)
			SCRIPT_MEMBER_FUNCTION_BEGIN(socket, "onError")
				gotten = true;
				SCRIPT_WRITE_STRING(ex)
				SCRIPT_FUNCTION_CALL
			SCRIPT_FUNCTION_END
		SCRIPT_END
		if (!gotten)
			WARN(ex);
	};
	socket.onFlush = [pState, &socket]() {
		SCRIPT_BEGIN(pState)
			SCRIPT_MEMBER_FUNCTION_BEGIN(socket, "onFlush")
				SCRIPT_FUNCTION_CALL
			SCRIPT_FUNCTION_END
		SCRIPT_END
	};
	socket.onPacket = [pState, &socket](shared<Buffer>& pBuffer, const SocketAddress& address) {
		SCRIPT_BEGIN(pState)
			SCRIPT_MEMBER_FUNCTION_BEGIN(socket, "onPacket")
				Script::NewObject(pState, new Packet(pBuffer));
				SCRIPT_FUNCTION_CALL
			SCRIPT_FUNCTION_END
		SCRIPT_END
	};
	AddType<Socket>(pState, *socket);
	SCRIPT_BEGIN(pState);
		SCRIPT_DEFINE_FUNCTION("connected", &connected);
		SCRIPT_DEFINE_FUNCTION("connect", &connect);
		SCRIPT_DEFINE_FUNCTION("disconnect", &disconnect);
		SCRIPT_DEFINE_FUNCTION("bound", &bound);
		SCRIPT_DEFINE_FUNCTION("bind", &bind);
		SCRIPT_DEFINE_FUNCTION("send", &send);
		SCRIPT_DEFINE_FUNCTION("close", &close);
	SCRIPT_END;
}
template<> void Script::ObjClear(lua_State *pState, UDPSocket& socket) {
	RemoveType<Socket>(pState, *socket);
	socket.onPacket = nullptr;
	socket.onFlush = nullptr;
	socket.onError = nullptr;
}

}
