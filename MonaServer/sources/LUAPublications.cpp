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
#include "Mona/Publication.h"

using namespace std;

namespace Mona {

template<> void Script::ObjInit(lua_State *pState, const map<string, Publication>& publications) {
	struct Mapper : LUAMap<const map<string, Publication>>::Mapper<Mapper>, virtual Object {
		using LUAMap<const std::map<string, Publication>>::Mapper<Mapper>::Mapper;
		static void PushValue(lua_State *pState, const Publication& publication) { Script::FromObject(pState, publication); }
	};
	LUAMap<const map<string, Publication>>::Define<Mapper>(pState, -1);
}
template<> void Script::ObjClear(lua_State *pState, const map<string, Publication>& parameters) {
}

}
