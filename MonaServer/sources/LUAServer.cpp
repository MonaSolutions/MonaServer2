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

#include "LUAServer.h"
#include "ScriptReader.h"
#include "LUASocketAddress.h"

using namespace std;
using namespace Mona;


int LUAServer::Send(lua_State* pState) {
	SCRIPT_CALLBACK(ServerConnection,server)
		const char* handler(SCRIPT_READ_STRING(NULL));
		if(!handler || strcmp(handler,".")==0) {
			ERROR("Invalid '",handler,"' handler for the sending server message")
		} else {
			shared<ServerMessage> pMessage(new ServerMessage(handler,server.poolBuffers()));
			SCRIPT_READ_NEXT(ScriptReader(pState, SCRIPT_READ_AVAILABLE).read(*pMessage));
			server.send(pMessage);
		}
	SCRIPT_CALLBACK_RETURN
}

int LUAServer::Reject(lua_State* pState) {
	SCRIPT_CALLBACK(ServerConnection,server)
		server.reject(SCRIPT_READ_STRING("unknown error"));
	SCRIPT_CALLBACK_RETURN
}


int LUAServer::IndexConst(lua_State* pState) {
	SCRIPT_CALLBACK(ServerConnection,server)
		const char* name = SCRIPT_READ_STRING(NULL);
		if(name) {
			if(strcmp(name,"send")==0) {
				SCRIPT_WRITE_FUNCTION(LUAServer::Send)
			} else if(strcmp(name,"isTarget")==0) {
				SCRIPT_WRITE_BOOL(server.isTarget)
			} else if(strcmp(name,"address")==0) {
				Script::AddObject<LUASocketAddress>(pState, server.address);
			} else if (strcmp(name,"reject")==0) {
				SCRIPT_WRITE_FUNCTION(LUAServer::Reject)
			} else if (strcmp(name,"configs")==0) {
				if(Script::Collection(pState, 1, "configs")) {
					for (auto& it : server)
						Script::PushKeyValue(pState, it.first, it.second);
					Script::FillCollection(pState, server.count());
				}
			} else {
				Script::Collection(pState,1, "configs");
				lua_getfield(pState, -1, name);
				lua_replace(pState, -2);
			}
		}
	SCRIPT_CALLBACK_RETURN
}
