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
#include "Mona/Writer.h"
#include "ScriptReader.h"

namespace Mona {


#define SCRIPT_DEFINE_WRITER(TYPE)	SCRIPT_DEFINE_FUNCTION("reliable", &LUAWriter<TYPE>::Reliable);\
									SCRIPT_DEFINE_FUNCTION("queueing", &LUAWriter<TYPE>::Queueing);\
									SCRIPT_DEFINE_FUNCTION("writeMessage", &LUAWriter<TYPE>::WriteMessage);\
									SCRIPT_DEFINE_FUNCTION("writeInvocation", &LUAWriter<TYPE>::WriteInvocation);\
									SCRIPT_DEFINE_FUNCTION("writeRaw", &LUAWriter<TYPE>::WriteRaw);\
									SCRIPT_DEFINE_FUNCTION("newWriter", &LUAWriter<TYPE>::NewWriter);\
									SCRIPT_DEFINE_FUNCTION("flush", &LUAWriter<TYPE>::Flush);\
									SCRIPT_DEFINE_FUNCTION("close", &LUAWriter<TYPE>::Close);

template<typename Type>
struct LUAWriter : virtual Static {
	static int Reliable(lua_State *pState) {
		SCRIPT_CALLBACK(Type, obj)
			Mona::Writer& writer = Writer(obj);
			if (SCRIPT_NEXT_READABLE)
				writer.reliable = SCRIPT_READ_BOOLEAN(writer.reliable);
			SCRIPT_WRITE_BOOLEAN(writer.reliable);
		SCRIPT_CALLBACK_RETURN
	}
	static int Queueing(lua_State *pState) {
		SCRIPT_CALLBACK(Type, obj)
			SCRIPT_WRITE_DOUBLE(Writer(obj).queueing());
		SCRIPT_CALLBACK_RETURN
	}
	static int WriteMessage(lua_State *pState) {
		SCRIPT_CALLBACK(Type, obj)
			SCRIPT_READ_NEXT(ScriptReader(pState, SCRIPT_NEXT_READABLE).read(Writer(obj).writeMessage()));
		SCRIPT_CALLBACK_RETURN
	}
	static int WriteInvocation(lua_State *pState) {
		SCRIPT_CALLBACK(Type, obj)
			const char* name = SCRIPT_NEXT_TYPE == LUA_TSTRING ? SCRIPT_READ_STRING(NULL) : NULL;
			SCRIPT_READ_NEXT(ScriptReader(pState, SCRIPT_NEXT_READABLE).read(Writer(obj).writeInvocation(name ? name : "onMessage")));
		SCRIPT_CALLBACK_RETURN
	}
	static int WriteRaw(lua_State* pState) {
		SCRIPT_CALLBACK(Type, obj)
			const Packet* pPacket = Script::ToObject<Packet>(pState, lua_gettop(pState));
			if(pPacket)
				SCRIPT_NEXT_SHRINK(SCRIPT_NEXT_READABLE-1)
			else
				pPacket = &Packet::Null();
			ScriptReader reader(pState, SCRIPT_NEXT_READABLE);
			Writer(obj).writeRaw(reader, *pPacket);
		SCRIPT_CALLBACK_RETURN
	}
	static int NewWriter(lua_State* pState) {
		SCRIPT_CALLBACK(Type, obj)
			Mona::Writer& writer = Writer(obj);
			Mona::Writer& newWriter(writer.newWriter());
			if (&newWriter == &writer)
				lua_pushvalue(pState, 1); // return the same writer
			else // manage with garbage collector a new writer, if client died RemoveObject is done by writer.onClose (see ObjInit)
				Script::AddObject(pState, newWriter, 1); // manage with garbage collector a new writer
		SCRIPT_CALLBACK_RETURN
	}
	static int Flush(lua_State* pState) {
		SCRIPT_CALLBACK(Type, obj)
			SCRIPT_WRITE_BOOLEAN(Writer(obj).flush())
		SCRIPT_CALLBACK_RETURN
	}
	static int Close(lua_State* pState) {
		SCRIPT_CALLBACK(Type, obj)
			UInt32 code = SCRIPT_NEXT_READABLE ? SCRIPT_READ_UINT32(0) : 0;
			Writer(obj).close(code, SCRIPT_NEXT_READABLE ? SCRIPT_READ_STRING(NULL) : NULL);
		SCRIPT_CALLBACK_RETURN
	}
private:
	static Mona::Writer& Writer(Type& object);
};

}

