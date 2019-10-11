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
#include "Mona/SRT.h"

using namespace std;

namespace Mona {

#if defined(SRT_API)
template<> void Script::ObjInit(lua_State *pState, SRT::Client& client) {
	AddType<SRT::Socket>(pState, *client);
	AddType<TCPClient>(pState, client);
}
template<> void Script::ObjClear(lua_State *pState, SRT::Client& client) {
	RemoveType<SRT::Socket>(pState, *client);
	RemoveType<TCPClient>(pState, client);
}
#endif

}
