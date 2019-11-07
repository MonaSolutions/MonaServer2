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
#include "Mona/Packet.h"

using namespace std;

namespace Mona {

static int __tostring(lua_State *pState) {
	SCRIPT_CALLBACK(Packet, packet)
		SCRIPT_WRITE_DATA(packet.data(), packet.size());
	SCRIPT_CALLBACK_RETURN
}
static int sub(lua_State *pState) {
	SCRIPT_CALLBACK(Packet, packet)
		double offset = SCRIPT_READ_DOUBLE(-1);
		if(offset>=0) {
			double size = SCRIPT_READ_DOUBLE(-1);
			if (size < 0) { // just one argument, size!
				size = offset;
				offset = 0;
			}
			if (offset > packet.size()) {
				SCRIPT_WARN("Packet:sub offset ", offset," truncated to packet size ", packet.size());
				offset = packet.size();
			}
			if ((offset + size) > packet.size()) {
				SCRIPT_WARN("Packet:sub size ", size, " truncated to not exceed packet size ", packet.size());
				size = packet.size() - offset;
			}
			Script::NewObject(pState, new Packet(packet, packet.data() + UInt32(offset), UInt32(size)));
		} else
			lua_pushvalue(pState, 1); // push itself!
	SCRIPT_CALLBACK_RETURN
}
static int byte(lua_State *pState) {
	SCRIPT_CALLBACK(Packet, packet)
		UInt32 index = SCRIPT_READ_UINT32(0);
		UInt32 size = SCRIPT_NEXT_READABLE ? SCRIPT_READ_UINT32(packet.size()) : packet.size();
		if (!index || index > packet.size())
			SCRIPT_ERROR(index, " out of string range 1-", packet.size())
		else if(--index<size)
			SCRIPT_WRITE_INT(packet.data()[index])
	SCRIPT_CALLBACK_RETURN
}
static int len(lua_State *pState) {
	SCRIPT_CALLBACK(Packet, packet)
		SCRIPT_WRITE_INT(packet.size());
	SCRIPT_CALLBACK_RETURN
}
static int shrink(lua_State *pState) {
	SCRIPT_CALLBACK(Packet, packet)
		packet.shrink(SCRIPT_READ_UINT32(0));
		lua_pushvalue(pState, 1); // myself!
	SCRIPT_CALLBACK_RETURN
}
static int clip(lua_State *pState) {
	SCRIPT_CALLBACK(Packet, packet)
		packet.clip(SCRIPT_READ_UINT32(0));
		lua_pushvalue(pState, 1); // myself!
	SCRIPT_CALLBACK_RETURN
}

template<> void Script::ObjInit(lua_State *pState, Packet& packet) {
	AddComparator<Packet>(pState);
	SCRIPT_BEGIN(pState);
		SCRIPT_DEFINE_FUNCTION("len", &len); // to be compatible with a string
		SCRIPT_DEFINE_FUNCTION("__tostring", &__tostring);
		SCRIPT_DEFINE_FUNCTION("sub", &sub); // to be compatible with a string
		SCRIPT_DEFINE_FUNCTION("byte", &byte); // to be compatible with a string
		SCRIPT_DEFINE_FUNCTION("shrink", &shrink);
		SCRIPT_DEFINE_FUNCTION("clip", &clip);
	SCRIPT_END;
}
template<> void Script::ObjClear(lua_State *pState, Packet& packet) {
	
}

}
