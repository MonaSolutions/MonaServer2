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
#include "Mona/TCPServer.h"
#include "Mona/TCPClient.h"
#include "LUASocketAddress.h"


using namespace std;

namespace Mona {

static bool OnError(lua_State *pState, TCPServer& server, const string& error) {
	bool gotten = false;
	SCRIPT_BEGIN(pState)
		SCRIPT_MEMBER_FUNCTION_BEGIN(server, "onError")
			gotten = true;
			SCRIPT_WRITE_STRING(error)
			SCRIPT_FUNCTION_CALL
		SCRIPT_FUNCTION_END
	SCRIPT_END
	return gotten;
}

static int running(lua_State *pState) {
	SCRIPT_CALLBACK(TCPServer, server)
		SCRIPT_WRITE_BOOLEAN(server.running())
	SCRIPT_CALLBACK_RETURN
}
static int start(lua_State *pState) {
	SCRIPT_CALLBACK_TRY(TCPServer, server)
		Exception ex;
		SocketAddress address;
		if(SCRIPT_READ_ADDRESS(address) && server.start(ex, address)) {
			if (ex)
				SCRIPT_CALLBACK_THROW(ex);
			SCRIPT_WRITE_BOOLEAN(true)
		} else
			SCRIPT_CALLBACK_THROW(ex);
	SCRIPT_CALLBACK_RETURN
}
static int stop(lua_State *pState) {
	SCRIPT_CALLBACK(TCPServer, server)
		server.stop();
	SCRIPT_CALLBACK_RETURN
}

template<> void Script::ObjInit(lua_State *pState, TCPServer& server) {
	server.onError = [pState, &server](const Exception& ex) {
		if(!OnError(pState, server, ex))
			WARN(ex);
	};
	server.onConnection = [pState, &server](const shared<Socket>& pSocket) {
		unique<TCPClient> pClient(SET, server.io);
		Exception ex;
		if (pClient->connect(ex, pSocket)) {
			if (ex)
				WARN("LUATCPServer ", pSocket->peerAddress(), " connection, ", ex);
			SCRIPT_BEGIN(pState)
				SCRIPT_MEMBER_FUNCTION_BEGIN(server, "onConnection")
					Script::NewObject(pState, pClient.release());
					SCRIPT_FUNCTION_CALL
				SCRIPT_FUNCTION_END
			SCRIPT_END
		} else {
			String error(pSocket->peerAddress(), " connection, ", ex);
			if (!OnError(pState, server, error))
				ERROR(error);
		}

	};
	if (server->type < Socket::TYPE_OTHER)
		AddType<Socket>(pState, *server);
	SCRIPT_BEGIN(pState);
		SCRIPT_DEFINE_FUNCTION("start", &start);
		SCRIPT_DEFINE_FUNCTION("stop", &stop);
		SCRIPT_DEFINE_FUNCTION("running", &running);
	SCRIPT_END;
}
template<> void Script::ObjClear(lua_State *pState, TCPServer& server) {
	if (server->type < Socket::TYPE_OTHER)
		RemoveType<Socket>(pState, *server);
	server.onConnection = nullptr;
	server.onError = nullptr;
}

}
