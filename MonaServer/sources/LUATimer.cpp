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

#include "LUATimer.h"

using namespace std;

namespace Mona {

LUATimer::LUATimer(lua_State* pState, const Timer& timer, int index) : _pState(pState), _ref(LUA_REFNIL), timer(timer),
	Timer::OnTimer([this](UInt32 delay) -> UInt32 {
		UInt32 timeout(0);
		SCRIPT_BEGIN(_pState)
			SCRIPT_FUNCTION_BEGIN("onTimer", _refFunc)
				SCRIPT_WRITE_INT(delay)
				SCRIPT_FUNCTION_CALL
				if(SCRIPT_NEXT_READABLE)
					timeout = SCRIPT_READ_UINT32(timeout);
			SCRIPT_FUNCTION_END
		SCRIPT_END
		if(timeout)
			return timeout;
		unpin();
		return 0;
	}) {

	// record function in registry!
	lua_pushvalue(_pState, index);
	_refFunc = luaL_ref(pState, LUA_REGISTRYINDEX);
}

LUATimer::~LUATimer() {
	timer.set(self, 0);
	luaL_unref(_pState, LUA_REGISTRYINDEX, _refFunc);
}

void LUATimer::pin(int index) {
	if (!self)
		return unpin();
	if (_ref != LUA_REFNIL)
		return;
	// link timer with environment table to stay valid until end of service application
	lua_rawgeti(_pState, LUA_REGISTRYINDEX, _refFunc);
	lua_getfenv(_pState, -1);
	lua_getmetatable(_pState, -1);
	lua_pushvalue(_pState, index-3); // index!
	_ref = luaL_ref(_pState, -2); // record object in metatable of environment
	lua_pop(_pState, 3); // remove metatable, environment table and function 
}
void LUATimer::unpin() {
	if (_ref == LUA_REFNIL)
		return;
	// remove timer of environment table!
	lua_rawgeti(_pState, LUA_REGISTRYINDEX, _refFunc);
	lua_getfenv(_pState, -1);
	lua_getmetatable(_pState, -1);
	luaL_unref(_pState, -1, _ref);
	_ref = LUA_REFNIL;
	lua_pop(_pState, 3); // remove metatable, environment table and function
}


static int nextRaising(lua_State *pState) {
	SCRIPT_CALLBACK(LUATimer, timer)
		SCRIPT_WRITE_DOUBLE(timer.nextRaising())
	SCRIPT_CALLBACK_RETURN
}
static int __call(lua_State *pState) {
	SCRIPT_CALLBACK(LUATimer, timer)
		timer();
	SCRIPT_CALLBACK_RETURN
}


template<> void Script::ObjInit(lua_State *pState, LUATimer& timer) {
	SCRIPT_BEGIN(pState);
		SCRIPT_DEFINE_FUNCTION("__call", __call);
		SCRIPT_DEFINE_FUNCTION("nextRaising", nextRaising);
	SCRIPT_END;
}
template<> void Script::ObjClear(lua_State *pState, LUATimer& timer) {
}


}
