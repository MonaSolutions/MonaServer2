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

#include "Service.h"
#include "LUAWriter.h"
#include "LUAQualityOfService.h"
#include "ScriptReader.h"

using namespace std;
using namespace Mona;



LUAWriter::LUAWriter(lua_State* pState,Writer& parentWriter):writer(parentWriter),_pState(pState) {
	onClose = [this](Int32 code) {
		this->writer.unsubscribe(onClose);
		Clear(_pState, this->writer);
		// Destruct the 2nd LUAWriter & detach the garbage collector
		if(Script::FromObject(_pState,this->writer)) {
			Script::DetachDestructor(_pState,-1);
			lua_pop(_pState, 1);
		}
		Script::ClearObject<LUAWriter>(_pState, this->writer);
		delete this; // delete LUAWriter instance!
	};
	writer.subscribe(onClose);
}

void LUAWriter::Clear(lua_State* pState,Writer& writer){
	// Can be called by garbage for newWriter or onDisconnection for mainWriter
	Script::ClearObject<LUAQualityOfService>(pState, writer.qos());
}

int LUAWriter::Close(lua_State* pState) {
	SCRIPT_CALLBACK(Writer,writer)
		writer.close(SCRIPT_READ_UINT(0));
	SCRIPT_CALLBACK_RETURN
}

int LUAWriter::SetReliable(lua_State* pState) {
	SCRIPT_CALLBACK(Writer, writer)
		writer.reliable = SCRIPT_READ_BOOL(writer.reliable);
	SCRIPT_CALLBACK_RETURN
}

int LUAWriter::Index(lua_State *pState) {
	SCRIPT_CALLBACK(Writer,writer)
		const char* name = SCRIPT_READ_STRING(NULL);
		if(name) {
			if(strcmp(name,"reliable")==0) {
				SCRIPT_WRITE_BOOL(writer.reliable)  // change
			}
		}
	SCRIPT_CALLBACK_RETURN
}

int LUAWriter::IndexConst(lua_State *pState) {
	SCRIPT_CALLBACK(Writer,writer)
		const char* name = SCRIPT_READ_STRING(NULL);
		if(name) {
			if (strcmp(name, "flush") == 0) {
				SCRIPT_WRITE_FUNCTION(LUAWriter::Flush)
			} else if (strcmp(name, "writeMessage") == 0) {
				SCRIPT_WRITE_FUNCTION(LUAWriter::WriteMessage)
			} else if (strcmp(name, "writeInvocation") == 0) {
				SCRIPT_WRITE_FUNCTION(LUAWriter::WriteInvocation)
			} else if (strcmp(name, "writeRaw") == 0) {
				SCRIPT_WRITE_FUNCTION(LUAWriter::WriteRaw)
			} else if (strcmp(name, "newWriter") == 0) {
				SCRIPT_WRITE_FUNCTION(LUAWriter::NewWriter)
			} else if (strcmp(name, "qos") == 0) {
				Script::AddObject<LUAQualityOfService>(pState,writer.qos());
			} else if (strcmp(name, "close") == 0) {
				SCRIPT_WRITE_FUNCTION(LUAWriter::Close)
			} else if (strcmp(name, "setReliable") == 0) {
				SCRIPT_WRITE_FUNCTION(LUAWriter::SetReliable)
			}
		}
	SCRIPT_CALLBACK_RETURN
}

int LUAWriter::Flush(lua_State* pState) {
	SCRIPT_CALLBACK(Writer,writer)
		if(writer.state()==Writer::STATE_OPENING)
			SCRIPT_WARN("Violation policy, impossible to flush data on a closed ",typeof(writer))
		else if (writer.state()==Writer::STATE_CLOSED)
			SCRIPT_ERROR("Impossible to flush data on a closed ", typeof(writer))
		else
			SCRIPT_WRITE_BOOL(writer.flush())
	SCRIPT_CALLBACK_RETURN
}

int LUAWriter::WriteMessage(lua_State* pState) {
	SCRIPT_CALLBACK(Writer,writer)
		SCRIPT_READ_NEXT(ScriptReader(pState, SCRIPT_READ_AVAILABLE).read(writer.writeMessage()));
	SCRIPT_CALLBACK_RETURN
}

int LUAWriter::WriteInvocation(lua_State* pState) {
	SCRIPT_CALLBACK(Writer, writer)
		const char* name(SCRIPT_READ_STRING("onMessage"));
		SCRIPT_READ_NEXT(ScriptReader(pState, SCRIPT_READ_AVAILABLE).read(writer.writeInvocation(name)));
	SCRIPT_CALLBACK_RETURN
}

int LUAWriter::WriteRaw(lua_State* pState) {
	SCRIPT_CALLBACK(Writer,writer)
		while(SCRIPT_READ_AVAILABLE) {
			SCRIPT_READ_BINARY(data, size);
			if (data)
				writer.writeRaw(data, size);
		}
	SCRIPT_CALLBACK_RETURN
}

int LUAWriter::NewWriter(lua_State* pState) {
	SCRIPT_CALLBACK(Writer, writer)
		Writer& newWriter(writer.newWriter());
		if (&newWriter == &writer)
			lua_pushvalue(pState, 1); // return the same writer
		else
			Script::NewObject<LUAWriter>(pState, (new LUAWriter(pState, newWriter))->writer); // manage with garbage collector a new writer
	SCRIPT_CALLBACK_RETURN
}
