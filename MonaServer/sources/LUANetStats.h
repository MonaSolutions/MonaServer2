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

#include "Mona/NetStats.h"
#include "Script.h"

template<typename LUAType>
struct LUANetStats {
	int	Bandwidth(lua_State* pState) {
		SCRIPT_CALLBACK(LUAType, object)
			SCRIPT_WRITE_BOOL(object.bandwidth());
		SCRIPT_CALLBACK_RETURN
	}
	int	PBandwidth(lua_State* pState) {
		SCRIPT_CALLBACK(LUAType, object)
			SCRIPT_WRITE_BOOL(object->bandwidth());
		SCRIPT_CALLBACK_RETURN
	}
	int	RecvTime(lua_State* pState) {
		SCRIPT_CALLBACK(LUAType, object)
			SCRIPT_WRITE_BOOL(object.recvTime());
		SCRIPT_CALLBACK_RETURN
	}
	int	PRecvTime(lua_State* pState) {
		SCRIPT_CALLBACK(LUAType, object)
			SCRIPT_WRITE_BOOL(object->recvTime());
		SCRIPT_CALLBACK_RETURN
	}
	int	RecvByteRate(lua_State* pState) {
		SCRIPT_CALLBACK(LUAType, object)
			SCRIPT_WRITE_BOOL(object.recvByteRate());
		SCRIPT_CALLBACK_RETURN
	}
	int	PRecvByteRate(lua_State* pState) {
		SCRIPT_CALLBACK(LUAType, object)
			SCRIPT_WRITE_BOOL(object->recvByteRate());
		SCRIPT_CALLBACK_RETURN
	}
	int	RecvLostRate(lua_State* pState) {
		SCRIPT_CALLBACK(LUAType, object)
			SCRIPT_WRITE_BOOL(object.recvLostRate());
		SCRIPT_CALLBACK_RETURN
	}
	int	PRecvLostRate(lua_State* pState) {
		SCRIPT_CALLBACK(LUAType, object)
			SCRIPT_WRITE_BOOL(object->recvLostRate());
		SCRIPT_CALLBACK_RETURN
	}
	int	SendTime(lua_State* pState) {
		SCRIPT_CALLBACK(LUAType, object)
			SCRIPT_WRITE_BOOL(object.sendTime());
		SCRIPT_CALLBACK_RETURN
	}
	int	PSendTime(lua_State* pState) {
		SCRIPT_CALLBACK(LUAType, object)
			SCRIPT_WRITE_BOOL(object->sendTime());
		SCRIPT_CALLBACK_RETURN
	}
	int	SendByteRate(lua_State* pState) {
		SCRIPT_CALLBACK(LUAType, object)
			SCRIPT_WRITE_BOOL(object.sendByteRate());
		SCRIPT_CALLBACK_RETURN
	}
	int	PSendByteRate(lua_State* pState) {
		SCRIPT_CALLBACK(LUAType, object)
			SCRIPT_WRITE_BOOL(object->sendByteRate());
		SCRIPT_CALLBACK_RETURN
	}
	int	SendLostRate(lua_State* pState) {
		SCRIPT_CALLBACK(LUAType, object)
			SCRIPT_WRITE_BOOL(object.sendLostRate());
		SCRIPT_CALLBACK_RETURN
	}
	int	PSendLostRate(lua_State* pState) {
		SCRIPT_CALLBACK(LUAType, object)
			SCRIPT_WRITE_BOOL(object->sendLostRate());
		SCRIPT_CALLBACK_RETURN
	}
};
