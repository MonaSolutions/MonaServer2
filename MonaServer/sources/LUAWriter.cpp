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

#include "ScriptReader.h"
#include "Mona/Writer.h"

using namespace std;

namespace Mona {

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
static int writeMessage(lua_State *pState) {
	SCRIPT_CALLBACK(Writer, writer)
		SCRIPT_READ_NEXT(ScriptReader(pState, SCRIPT_NEXT_READABLE).read(writer.writeMessage()));
	SCRIPT_CALLBACK_RETURN
}
static int writeInvocation(lua_State *pState) {
	SCRIPT_CALLBACK(Writer, writer)
		const char* name = SCRIPT_NEXT_TYPE == LUA_TSTRING ? SCRIPT_READ_STRING(NULL) : NULL;
		SCRIPT_READ_NEXT(ScriptReader(pState, SCRIPT_NEXT_READABLE).read(writer.writeInvocation(name ? name : "onMessage")));
	SCRIPT_CALLBACK_RETURN
}
static int writeRaw(lua_State* pState) {
	SCRIPT_CALLBACK(Writer, writer)
		const Packet* pPacket = Script::ToObject<Packet>(pState, lua_gettop(pState));
		if (pPacket)
			SCRIPT_NEXT_SHRINK(SCRIPT_NEXT_READABLE - 1)
		else
			pPacket = &Packet::Null();
		ScriptReader reader(pState, SCRIPT_NEXT_READABLE);
		writer.writeRaw(reader, *pPacket);
	SCRIPT_CALLBACK_RETURN
}
static int newWriter(lua_State* pState) {
	SCRIPT_CALLBACK(Writer, writer)
		Writer& newWriter = writer.newWriter();
		if (&newWriter == &writer)
			lua_pushvalue(pState, 1); // return the same writer
		else // manage with garbage collector a new writer, if client died RemoveObject is done by writer.onClose (see ObjInit)
			Script::AddObject(pState, newWriter, 1);
	SCRIPT_CALLBACK_RETURN
}
static int flush(lua_State* pState) {
	SCRIPT_CALLBACK(Writer, writer)
		SCRIPT_WRITE_BOOLEAN(writer.flush())
	SCRIPT_CALLBACK_RETURN
}
static int close(lua_State* pState) {
	SCRIPT_CALLBACK(Writer, writer)
		UInt32 code = SCRIPT_NEXT_READABLE ? SCRIPT_READ_UINT32(0) : 0;
		writer.close(code, SCRIPT_NEXT_READABLE ? SCRIPT_READ_STRING(NULL) : NULL);
	SCRIPT_CALLBACK_RETURN
}


template<> void Script::ObjInit(lua_State *pState, Writer& writer) {
	if(!writer.onClose)
		writer.onClose = [&](Int32 error, const char* reason) { RemoveObject(pState, writer); };
	SCRIPT_BEGIN(pState)
		SCRIPT_DEFINE_FUNCTION("queueing", &queueing);
		SCRIPT_DEFINE_FUNCTION("reliable", &reliable);
		SCRIPT_DEFINE_FUNCTION("writeMessage", &writeMessage);
		SCRIPT_DEFINE_FUNCTION("writeInvocation", &writeInvocation);
		SCRIPT_DEFINE_FUNCTION("writeRaw", &writeRaw);
		SCRIPT_DEFINE_FUNCTION("newWriter", &newWriter);
		SCRIPT_DEFINE_FUNCTION("flush", &flush);
		SCRIPT_DEFINE_FUNCTION("close", &close);
	SCRIPT_END
}
template<> void Script::ObjClear(lua_State *pState, Writer& writer) {
}

}
