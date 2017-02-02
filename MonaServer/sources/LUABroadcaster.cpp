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

#include "LUABroadcaster.h"
#include "LUAServer.h"
#include "Servers.h"
#include "ScriptReader.h"

using namespace std;
using namespace Mona;

void LUABroadcaster::Init(lua_State *pState, Broadcaster& broadcaster) {
	lua_getmetatable(pState, -1);
	lua_pushcfunction(pState,&LUABroadcaster::Len);
	lua_setfield(pState, -2, "__len");
	lua_newtable(pState);
	lua_setfield(pState,-2, "|items");
	lua_pop(pState, 1);
}


int	LUABroadcaster::Len(lua_State* pState) {
	SCRIPT_CALLBACK(Broadcaster, broadcaster)
		SCRIPT_WRITE_NUMBER(broadcaster.count())
	SCRIPT_CALLBACK_RETURN
}

void LUABroadcaster::AddServer(lua_State* pState, Broadcaster& broadcaster, const string& address) {
	// -1 must be the server table!
	if (Script::FromObject(pState, broadcaster)) {
		lua_getmetatable(pState, -1);
		lua_getfield(pState, -1, "|items");
		lua_pushvalue(pState, -4);
		lua_setfield(pState, -2, address.c_str());
		lua_pop(pState, 2);
		// methods comapraison to avoid an overload of servers methods
		if (address != "count" && address != "broadcast" && address != "initiators" && address != "targets") {
			lua_pushvalue(pState, -2);
			lua_setfield(pState, -2, address.c_str());
		}
		lua_pop(pState, 1);
	}
}

void LUABroadcaster::RemoveServer(lua_State* pState, Broadcaster& broadcaster, const string& address) {
	if (Script::FromObject(pState, broadcaster)) {
		lua_getmetatable(pState, -1);
		lua_getfield(pState, -1, "|items");
		lua_pushnil(pState);
		lua_setfield(pState, -2, address.c_str());
		lua_pop(pState, 2);
		lua_pushnil(pState);
		lua_setfield(pState, -2, address.c_str());
		lua_pop(pState, 1);
	}
}

int LUABroadcaster::Broadcast(lua_State* pState) {
	SCRIPT_CALLBACK(Broadcaster,broadcaster)
		const char* handler(SCRIPT_READ_STRING(""));
		if(strlen(handler)==0 || strcmp(handler,".")==0) {
			ERROR("handler of one sending server message can't be null or equal to '.'")
		} else {
			AMFWriter writer(broadcaster.poolBuffers);
			SCRIPT_READ_NEXT(ScriptReader(pState, SCRIPT_READ_AVAILABLE).read(writer));
			broadcaster.broadcast(handler,writer.packet);
		}
	SCRIPT_CALLBACK_RETURN
}

int LUABroadcaster::Index(lua_State *pState) {
	SCRIPT_CALLBACK(Broadcaster,broadcaster)
		const char* name = SCRIPT_READ_STRING(NULL);
		if (name) {
			// no methods (no methods in INDEXCONST) to be able to iterate on mona.servers with pairs
			if (strcmp(name, "broadcast") == 0) {
				SCRIPT_WRITE_FUNCTION(LUABroadcaster::Broadcast)
			} else if (strcmp(name, "initiators")==0) {
				lua_getmetatable(pState, 1);
				lua_getfield(pState, -1, "|initiators");
				lua_replace(pState, -2);
			} else if (strcmp(name, "targets") == 0) {
				lua_getmetatable(pState, 1);
				lua_getfield(pState, -1, "|targets");
				lua_replace(pState, -2);
			} else if (strcmp(name, "count") == 0) {
				SCRIPT_WRITE_NUMBER(broadcaster.count())
			}
		}
	SCRIPT_CALLBACK_RETURN
}

int LUABroadcaster::IndexConst(lua_State *pState) {
	SCRIPT_CALLBACK(Broadcaster,broadcaster)
		const char* name = SCRIPT_READ_STRING(NULL);
		if (name) {
			ServerConnection* pServer(broadcaster[name]);
			if (pServer)
				Script::AddObject<LUAServer>(pState, *pServer);
		}
	SCRIPT_CALLBACK_RETURN
}

