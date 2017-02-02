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


#include "LUAPublication.h"
#include "Mona/JSONReader.h"
#include "Mona/AMFReader.h"
#include "Mona/StringReader.h"
#include "Mona/XMLRPCReader.h"
#include "ScriptReader.h"

using namespace std;
using namespace Mona;



void LUAPublication::AddListener(lua_State* pState, UInt8 indexPublication, UInt8 indexListener) {
	// -1 must be the client table!
	Script::Collection(pState, indexPublication, "listeners");
	lua_pushvalue(pState, indexListener);
	lua_pushvalue(pState, -4); // client table
	Script::FillCollection(pState, 1);
	lua_pop(pState, 1);
}

void LUAPublication::RemoveListener(lua_State* pState, const Publication& publication) {
	// -1 must be the listener table!
	if (Script::FromObject<Publication>(pState, publication)) {
		Script::Collection(pState, -1, "listeners");
		lua_pushvalue(pState, -3); // listener table
		lua_pushnil(pState);
		Script::FillCollection(pState, 1);
		lua_pop(pState, 2);
	}
}

void LUAPublication::Clear(lua_State* pState, Publication& publication) {
	Script::ClearObject<LUAQualityOfService>(pState, publication.dataQOS());
	Script::ClearObject<LUAQualityOfService>(pState, publication.audioQOS());
	Script::ClearObject<LUAQualityOfService>(pState, publication.videoQOS());
}

void LUAPublication::Delete(lua_State* pState, Publication& publication) {
	if (!publication.running())
		return;
	lua_getmetatable(pState, -1);
	lua_getfield(pState, -1, "|invoker");
	lua_replace(pState, -2);
	Invoker* pInvoker = (Invoker*)lua_touserdata(pState, -1);
	if (pInvoker)
		pInvoker->unpublish(publication.name());
	lua_pop(pState, 1);
}

int LUAPublication::Close(lua_State *pState) {
	SCRIPT_CALLBACK(Publication, publication)
		lua_getmetatable(pState, 1);
		lua_getfield(pState, -1, "|invoker");
		lua_replace(pState, -2);

		Script::DetachDestructor(pState,1);

		Invoker* pInvoker = (Invoker*)lua_touserdata(pState, -1);
		if (!pInvoker) {
			SCRIPT_BEGIN(pState)
				SCRIPT_ERROR("You have not the handle on publication ", publication.name(), ", you can't close it")
			SCRIPT_END
		} else if (publication.running())
			pInvoker->unpublish(publication.name()); // call LUAPublication::Clear (because no destructor)

		lua_pop(pState, 1);
	SCRIPT_CALLBACK_RETURN
}


int	LUAPublication::PushAudio(lua_State *pState) {
	SCRIPT_CALLBACK(Publication, publication)
		UInt32 time = SCRIPT_READ_UINT(0);
		SCRIPT_READ_BINARY(pData, size);
		if (pData) {
			PacketReader packet(pData, size);
			UInt16 ping(SCRIPT_READ_UINT(0));
			publication.pushAudio(time,packet, ping, UInt64(SCRIPT_READ_DOUBLE(0)));
		}
	SCRIPT_CALLBACK_RETURN
}

int	LUAPublication::PushVideo(lua_State *pState) {
	SCRIPT_CALLBACK(Publication, publication)
		UInt32 time = SCRIPT_READ_UINT(0);
		SCRIPT_READ_BINARY(pData, size);
		if (pData) {
			PacketReader packet(pData, size);
			UInt16 ping(SCRIPT_READ_UINT(0));
			publication.pushVideo(time,packet, ping,UInt64(SCRIPT_READ_DOUBLE(0)));
		}
	SCRIPT_CALLBACK_RETURN
}

int	LUAPublication::Flush(lua_State *pState) {
	SCRIPT_CALLBACK(Publication, publication)
		publication.flush();
	SCRIPT_CALLBACK_RETURN
}

int LUAPublication::WriteProperties(lua_State *pState) {
	SCRIPT_CALLBACK(Publication, publication)
		ScriptReader reader(pState, SCRIPT_READ_AVAILABLE);
		publication.writeProperties(reader);
		Script::Collection(pState, 1, "properties");
	SCRIPT_CALLBACK_RETURN
}

int LUAPublication::ClearProperties(lua_State *pState) {
	SCRIPT_CALLBACK(Publication, publication)
		publication.clearProperties();
	SCRIPT_CALLBACK_RETURN
}

int LUAPublication::Index(lua_State *pState) {
	SCRIPT_CALLBACK(Publication, publication)
		const char* name = SCRIPT_READ_STRING(NULL);
		if(name) {
			if(strcmp(name,"running")==0) {
				SCRIPT_WRITE_BOOL(publication.running())
			} else if(strcmp(name,"lastTime")==0) {
				SCRIPT_WRITE_NUMBER(publication.lastTime()) // can change
			} else if(strcmp(name,"byteRate")==0) {
				SCRIPT_WRITE_NUMBER(publication.byteRate()) // can change
			}
		}
	SCRIPT_CALLBACK_RETURN
}


int LUAPublication::IndexConst(lua_State *pState) {
	SCRIPT_CALLBACK(Publication, publication)
		const char* name = SCRIPT_READ_STRING(NULL);
		if(name) {
			if(strcmp(name,"name")==0) {
				SCRIPT_WRITE_STRING(publication.name().c_str())
			} else if(strcmp(name,"listeners")==0) {
				Script::Collection(pState, 1, "listeners");
			} else if(strcmp(name,"audioQOS")==0) {
				Script::AddObject<LUAQualityOfService>(pState, publication.audioQOS());
			} else if(strcmp(name,"videoQOS")==0) {
				Script::AddObject<LUAQualityOfService>(pState, publication.videoQOS());
			} else if(strcmp(name,"dataQOS")==0) {
				Script::AddObject<LUAQualityOfService>(pState, publication.dataQOS());
			} else if(strcmp(name,"close")==0) {
				SCRIPT_WRITE_FUNCTION(LUAPublication::Close)
			} else if(strcmp(name,"pushAudio")==0) {
				SCRIPT_WRITE_FUNCTION(LUAPublication::PushAudio)
			} else if(strcmp(name,"flush")==0) {
				SCRIPT_WRITE_FUNCTION(LUAPublication::Flush)
			} else if(strcmp(name,"pushVideo")==0) {
				SCRIPT_WRITE_FUNCTION(LUAPublication::PushVideo)
			} else if(strcmp(name,"pushAMFData")==0) {
				SCRIPT_WRITE_FUNCTION(LUAPublication::PushData<Mona::AMFReader>)
			} else if(strcmp(name,"pushXMLRPCData")==0) {
				SCRIPT_WRITE_FUNCTION(LUAPublication::PushDataWithBuffers<Mona::XMLRPCReader>)
			} else if(strcmp(name,"pushJSONData")==0) {
				SCRIPT_WRITE_FUNCTION(LUAPublication::PushDataWithBuffers<Mona::JSONReader>)
			} else if(strcmp(name,"pushData")==0) {
				SCRIPT_WRITE_FUNCTION(LUAPublication::PushData<Mona::StringReader>)
			} else if (strcmp(name,"writeProperties")==0) {
				SCRIPT_WRITE_FUNCTION(LUAPublication::WriteProperties)
			} else if (strcmp(name,"clearProperties")==0) {
				SCRIPT_WRITE_FUNCTION(LUAPublication::ClearProperties)
			} else if (strcmp(name,"properties")==0) {
				// if no properties, returns nothing
				Script::GetCollection(pState, 1, "properties");
			} else if(Script::GetCollection(pState, 1, "properties")) {
				lua_getfield(pState, -1, name);
				lua_replace(pState, -2);
			}
		}
	SCRIPT_CALLBACK_RETURN
}
