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
	LUAMap<const Parameters>::Define(pState, -1);
}
template<> void Script::ObjClear(lua_State *pState, const Parameters& parameters) {

}

template<> void Script::ObjInit(lua_State *pState, Parameters& parameters) {
	LUAMap<Parameters>::Define(pState, -1);
}
template<> void Script::ObjClear(lua_State *pState, Parameters& parameters) {

}

}

