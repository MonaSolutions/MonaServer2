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


#include "Mona/UDPSocket.h"
#include "Script.h"

class LUAUDPSocket : private Mona::UDPSocket {
public:
	LUAUDPSocket(Mona::SocketEngine& engine, bool allowBroadcast, lua_State* pState);

	static std::map<std::string, std::pair<bool, lua_CFunction>> Methods;

	static void Init(lua_State *pState, LUAUDPSocket& udp) {}
	static void	Clear(lua_State* pState, LUAUDPSocket& udp);
	static void	Delete(lua_State* pState, LUAUDPSocket& udp) { delete &udp; }

private:
	virtual ~LUAUDPSocket();

	static int Connected(lua_State* pState);
	static int Bound(lua_State* pState);
	static int Address(lua_State* pState);
	static int PeerAddress(lua_State* pState);

	static int Connect(lua_State* pState);
	static int Disconnect(lua_State* pState);
	static int Bind(lua_State* pState);
	static int Send(lua_State* pState);
	static int Close(lua_State* pState);
	

	lua_State*			_pState;
};
