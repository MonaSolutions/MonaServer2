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
#include "Mona/Socket.h"
#include "LUAIPAddress.h"
#include "LUASocketAddress.h"

using namespace std;

namespace Mona {

static int address(lua_State *pState) {
	SCRIPT_CALLBACK(Socket, socket);
		SCRIPT_WRITE_ADDRESS(socket.address());
	SCRIPT_CALLBACK_RETURN;
}
static int peerAddress(lua_State *pState) {
	SCRIPT_CALLBACK(Socket, socket);
		SCRIPT_WRITE_ADDRESS(socket.peerAddress());
	SCRIPT_CALLBACK_RETURN;
}
static int isSecure(lua_State *pState) {
	SCRIPT_CALLBACK(Socket, socket);
		SCRIPT_WRITE_BOOLEAN(socket.isSecure());
	SCRIPT_CALLBACK_RETURN;
}
static int available(lua_State *pState) {
	SCRIPT_CALLBACK(Socket, socket);
		SCRIPT_WRITE_INT(socket.available());
	SCRIPT_CALLBACK_RETURN;
}
static int listening(lua_State *pState) {
	SCRIPT_CALLBACK(Socket, socket);
		SCRIPT_WRITE_BOOLEAN(socket.listening());
	SCRIPT_CALLBACK_RETURN;
}
static int recvBufferSize(lua_State *pState) {
	SCRIPT_CALLBACK_TRY(Socket, socket);
		if (SCRIPT_NEXT_READABLE) {
			Exception ex;
			if (!socket.setRecvBufferSize(ex, SCRIPT_READ_UINT32(socket.recvBufferSize())) || ex)
				SCRIPT_CALLBACK_THROW(ex);
		}
		SCRIPT_WRITE_INT(socket.recvBufferSize());
	SCRIPT_CALLBACK_RETURN;
}
static int sendBufferSize(lua_State *pState) {
	SCRIPT_CALLBACK_TRY(Socket, socket);
		if (SCRIPT_NEXT_READABLE) {
			Exception ex;
			if (!socket.setSendBufferSize(ex, SCRIPT_READ_UINT32(socket.sendBufferSize())) || ex)
				SCRIPT_CALLBACK_THROW(ex);
		}
		SCRIPT_WRITE_INT(socket.sendBufferSize());
	SCRIPT_CALLBACK_RETURN;
}
static int joinGroup(lua_State *pState) {
	SCRIPT_CALLBACK_TRY(Socket, socket);
		IPAddress ip;
		Exception ex;
		if (SCRIPT_READ_IP(ip)) {
			if (socket.joinGroup(ex, ip, SCRIPT_NEXT_READABLE ? SCRIPT_READ_UINT32(0) : 0)) {
				if(ex)
					SCRIPT_CALLBACK_THROW(ex);
				SCRIPT_WRITE_BOOLEAN(true);
			} else
				SCRIPT_CALLBACK_THROW(ex);
		} else
			SCRIPT_CALLBACK_THROW(ex);
	SCRIPT_CALLBACK_RETURN;
}
static int leaveGroup(lua_State *pState) {
	SCRIPT_CALLBACK_TRY(Socket, socket);
		IPAddress ip;
		Exception ex;
		if (SCRIPT_READ_IP(ip)) {
			socket.leaveGroup(ip, SCRIPT_NEXT_READABLE ? SCRIPT_READ_UINT32(0) : 0);
			if(ex)
				SCRIPT_CALLBACK_THROW(ex);
			SCRIPT_WRITE_BOOLEAN(true);
		} else
			SCRIPT_CALLBACK_THROW(ex);
	SCRIPT_CALLBACK_RETURN;
}

template<> void Script::ObjInit(lua_State *pState, Socket& socket) {
	AddType<Net::Stats>(pState, socket);

	SCRIPT_BEGIN(pState);
		SCRIPT_DEFINE_INT("id", socket.id());
		switch(socket.type) {
			case Socket::TYPE_DATAGRAM:
				SCRIPT_DEFINE_STRING("type", "datagram");
				break;
			case Socket::TYPE_STREAM:
				SCRIPT_DEFINE_STRING("type", "stream");
				break;
			case Socket::TYPE_OTHER:
				SCRIPT_DEFINE_STRING("type", "other");
				break;
		}
		SCRIPT_DEFINE_FUNCTION("isSecure", &isSecure); // virtual method (used by TLS::Socket for example), keep it in function and not in pure boolean! 
		SCRIPT_DEFINE_FUNCTION("address", &address);
		SCRIPT_DEFINE_FUNCTION("peerAddress", &peerAddress);
		SCRIPT_DEFINE_FUNCTION("available", &available);
		SCRIPT_DEFINE_FUNCTION("listening", &listening);
		SCRIPT_DEFINE_FUNCTION("recvBufferSize", &recvBufferSize);
		SCRIPT_DEFINE_FUNCTION("sendBufferSize", &sendBufferSize);
		SCRIPT_DEFINE_FUNCTION("joinGroup", &joinGroup);
		SCRIPT_DEFINE_FUNCTION("leaveGroup", &leaveGroup);
	SCRIPT_END;
}
template<> void Script::ObjClear(lua_State *pState, Socket& socket) {
	RemoveType<Net::Stats>(pState, socket);
}

}
