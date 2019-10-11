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
#include "Mona/Net.h"

using namespace std;

namespace Mona {

static int recvTime(lua_State *pState) {
	SCRIPT_CALLBACK(Net::Stats, stats);
		SCRIPT_WRITE_DOUBLE(stats.recvTime());
	SCRIPT_CALLBACK_RETURN;
}
static int recvByteRate(lua_State *pState) {
	SCRIPT_CALLBACK(Net::Stats, stats);
		SCRIPT_WRITE_DOUBLE(stats.recvByteRate());
	SCRIPT_CALLBACK_RETURN;
}
static int recvLostRate(lua_State *pState) {
	SCRIPT_CALLBACK(Net::Stats, stats);
		SCRIPT_WRITE_DOUBLE(stats.recvLostRate());
	SCRIPT_CALLBACK_RETURN;
}
static int sendTime(lua_State *pState) {
	SCRIPT_CALLBACK(Net::Stats, stats);
		SCRIPT_WRITE_DOUBLE(stats.sendTime());
	SCRIPT_CALLBACK_RETURN;
}
static int sendByteRate(lua_State *pState) {
	SCRIPT_CALLBACK(Net::Stats, stats);
		SCRIPT_WRITE_DOUBLE(stats.sendByteRate());
	SCRIPT_CALLBACK_RETURN;
}
static int sendLostRate(lua_State *pState) {
	SCRIPT_CALLBACK(Net::Stats, stats);
		SCRIPT_WRITE_DOUBLE(stats.sendLostRate());
	SCRIPT_CALLBACK_RETURN;
}
static int queueing(lua_State *pState) {
	SCRIPT_CALLBACK(Net::Stats, stats);
		SCRIPT_WRITE_DOUBLE(stats.queueing());
	SCRIPT_CALLBACK_RETURN;
}

template<> void Script::ObjInit(lua_State *pState, Net::Stats& stats) {
	SCRIPT_BEGIN(pState);
		SCRIPT_DEFINE_FUNCTION("recvTime", recvTime);
		SCRIPT_DEFINE_FUNCTION("recvByteRate", recvByteRate);
		SCRIPT_DEFINE_FUNCTION("recvLostRate", recvLostRate);
		SCRIPT_DEFINE_FUNCTION("sendTime", sendTime);
		SCRIPT_DEFINE_FUNCTION("sendByteRate", sendByteRate);
		SCRIPT_DEFINE_FUNCTION("sendLostRate", sendLostRate);
		SCRIPT_DEFINE_FUNCTION("queueing", queueing);
	SCRIPT_END;
}
template<> void Script::ObjClear(lua_State *pState, Net::Stats& stats) {
	
}

}
