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

#include "Script.h"
#include "Mona/Net.h"

namespace Mona {

#define SCRIPT_DEFINE_NETSTATS(TYPE)	SCRIPT_DEFINE_FUNCTION("recvTime", &LUANetStats<TYPE>::RecvTime);\
										SCRIPT_DEFINE_FUNCTION("recvByteRate", &LUANetStats<TYPE>::RecvByteRate);\
										SCRIPT_DEFINE_FUNCTION("recvLostRate", &LUANetStats<TYPE>::RecvLostRate);\
										SCRIPT_DEFINE_FUNCTION("sendTime", &LUANetStats<TYPE>::SendTime);\
										SCRIPT_DEFINE_FUNCTION("sendByteRate", &LUANetStats<TYPE>::SendByteRate);\
										SCRIPT_DEFINE_FUNCTION("sendLostRate", &LUANetStats<TYPE>::SendLostRate);\
										SCRIPT_DEFINE_FUNCTION("queueing", &LUANetStats<TYPE>::Queueing);

template<typename Type>
struct LUANetStats : virtual Static {
	static int RecvTime(lua_State *pState) {
		SCRIPT_CALLBACK(Type, object);
			SCRIPT_WRITE_DOUBLE(NetStats(object).recvTime());
		SCRIPT_CALLBACK_RETURN;
	}
	static int RecvByteRate(lua_State *pState) {
		SCRIPT_CALLBACK(Type, object);
			SCRIPT_WRITE_DOUBLE(NetStats(object).recvByteRate());
		SCRIPT_CALLBACK_RETURN;
	}
	static int RecvLostRate(lua_State *pState) {
		SCRIPT_CALLBACK(Type, object);
			SCRIPT_WRITE_DOUBLE(NetStats(object).recvLostRate());
		SCRIPT_CALLBACK_RETURN;
	}
	static int SendTime(lua_State *pState) {
		SCRIPT_CALLBACK(Type, object);
			SCRIPT_WRITE_DOUBLE(NetStats(object).sendTime());
		SCRIPT_CALLBACK_RETURN;
	}
	static int SendByteRate(lua_State *pState) {
		SCRIPT_CALLBACK(Type, object);
			SCRIPT_WRITE_DOUBLE(NetStats(object).sendByteRate());
		SCRIPT_CALLBACK_RETURN;
	}
	static int SendLostRate(lua_State *pState) {
		SCRIPT_CALLBACK(Type, object);
			SCRIPT_WRITE_DOUBLE(NetStats(object).sendLostRate());
		SCRIPT_CALLBACK_RETURN;
	}
	static int Queueing(lua_State *pState) {
		SCRIPT_CALLBACK(Type, object);
			SCRIPT_WRITE_DOUBLE(NetStats(object).queueing());
		SCRIPT_CALLBACK_RETURN;
	}
private:
	static Net::Stats& NetStats(Type& object);
};

}
