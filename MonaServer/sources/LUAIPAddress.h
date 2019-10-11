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
#include "Mona/SocketAddress.h"

namespace Mona {

#define SCRIPT_READ_IP(IP)		LUAIPAddress::From(ex, __pState, SCRIPT_READ_NEXT(1), IP)
#define SCRIPT_WRITE_IP(...)	Script::NewObject(__pState, new IPAddress(__VA_ARGS__));

struct LUAIPAddress : virtual Static {
	/*!
	Convert variable LUA to IPAddress, if first field is string with as first char '@' it uses DNS to resolve address */
	static bool From(Exception& ex, lua_State *pState, int index, IPAddress& address);
};

}
