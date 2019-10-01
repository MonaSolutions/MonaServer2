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

#include "LUAWriter.h"

using namespace std;

namespace Mona {

template<> Mona::Writer& LUAWriter<Mona::Writer>::Writer(Mona::Writer& writer) { return writer; }

static int reliable(lua_State *pState) {
	SCRIPT_CALLBACK(Writer, writer)
		if (SCRIPT_NEXT_READABLE)
			writer.reliable = SCRIPT_READ_BOOLEAN(writer.reliable);
		SCRIPT_WRITE_BOOLEAN(writer.reliable);
	SCRIPT_CALLBACK_RETURN
}
static int queueing(lua_State *pState) {
	SCRIPT_CALLBACK(Writer, writer)
		SCRIPT_WRITE_DOUBLE(writer.queueing());
	SCRIPT_CALLBACK_RETURN
}
static int closed(lua_State *pState) {
	SCRIPT_CALLBACK(Writer, writer)
		SCRIPT_WRITE_BOOLEAN(writer.closed());
	SCRIPT_CALLBACK_RETURN
}


template<> void Script::ObjInit(lua_State *pState, Writer& writer) {
	if(!writer.onClose)
		writer.onClose = [&](Int32 error, const char* reason) { RemoveObject(pState, writer); };
	SCRIPT_BEGIN(pState)
		SCRIPT_DEFINE_FUNCTION("queueing", &queueing);
		SCRIPT_DEFINE_FUNCTION("reliable", &reliable);
		SCRIPT_DEFINE_FUNCTION("closed", &closed);
		SCRIPT_DEFINE_WRITER(Writer);
	SCRIPT_END
}
template<> void Script::ObjClear(lua_State *pState, Writer& writer) {
}

}
