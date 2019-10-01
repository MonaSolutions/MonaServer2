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

#include "LUAMap.h"
#include "Mona/Parameters.h"


using namespace std;

namespace Mona {


template<> void Script::ObjInit(lua_State *pState, const Parameters& parameters) {
	SCRIPT_BEGIN(pState)
		SCRIPT_DEFINE_FUNCTION("__index", &LUAMap<const Parameters>::Index);
		SCRIPT_DEFINE_FUNCTION("__call", &LUAMap<const Parameters>::Call);
		SCRIPT_DEFINE_FUNCTION("__pairs", &LUAMap<const Parameters>::Pairs);
	SCRIPT_END
}
template<> void Script::ObjClear(lua_State *pState, const Parameters& parameters) {

}

template<> void Script::ObjInit(lua_State *pState, Parameters& parameters) {
	SCRIPT_BEGIN(pState)
		SCRIPT_DEFINE_FUNCTION("__index", &LUAMap<Parameters>::Index);
		SCRIPT_DEFINE_FUNCTION("__call", &LUAMap<Parameters>::Call);
		SCRIPT_DEFINE_FUNCTION("__pairs", &LUAMap<Parameters>::Pairs);
	SCRIPT_END
}
template<> void Script::ObjClear(lua_State *pState, Parameters& parameters) {

}

}

