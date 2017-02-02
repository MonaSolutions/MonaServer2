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

#include "LUAListener.h"
#include "LUAQualityOfService.h"
#include "LUAPublication.h"
#include "LUAClient.h"

using namespace std;
using namespace Mona;

void LUAListener::Init(lua_State* pState, Listener& listener) {
	Script::InitCollectionParameters(pState,listener,"parameters",listener,true);
}

void LUAListener::Clear(lua_State* pState,Listener& listener){
	Script::ClearCollectionParameters(pState,"parameters",listener);

	Script::ClearObject<LUAQualityOfService>(pState, listener.dataQOS());
	Script::ClearObject<LUAQualityOfService>(pState, listener.audioQOS());
	Script::ClearObject<LUAQualityOfService>(pState, listener.videoQOS());
}


int	LUAListener::SetReceiveAudio(lua_State *pState) {
	SCRIPT_CALLBACK(Listener, listener)
		listener.receiveAudio = SCRIPT_READ_BOOL(listener.receiveAudio);
	SCRIPT_CALLBACK_RETURN
}

int	LUAListener::SetReceiveVideo(lua_State *pState) {
	SCRIPT_CALLBACK(Listener, listener)
		listener.receiveVideo = SCRIPT_READ_BOOL(listener.receiveVideo);
	SCRIPT_CALLBACK_RETURN
}

int	LUAListener::SetReceiveData(lua_State *pState) {
	SCRIPT_CALLBACK(Listener, listener)
		listener.receiveData = SCRIPT_READ_BOOL(listener.receiveData);
	SCRIPT_CALLBACK_RETURN
}

int LUAListener::Index(lua_State *pState) {
	SCRIPT_CALLBACK(Listener,listener)
		const char* name = SCRIPT_READ_STRING(NULL);
		if (name) {
			if(strcmp(name,"receiveAudio")==0) {
				SCRIPT_WRITE_BOOL(listener.receiveAudio);
			} else if(strcmp(name,"receiveVideo")==0) {
				SCRIPT_WRITE_BOOL(listener.receiveVideo);
			} else if(strcmp(name,"receiveData")==0) {
				SCRIPT_WRITE_BOOL(listener.receiveVideo);
			}
		}
	SCRIPT_CALLBACK_RETURN
}


int LUAListener::IndexConst(lua_State *pState) {
	SCRIPT_CALLBACK(Listener,listener)
		const char* name = SCRIPT_READ_STRING(NULL);
		if (name) {
			if(strcmp(name,"audioQOS")==0) {
				Script::AddObject<LUAQualityOfService>(pState, listener.audioQOS());
			} else if(strcmp(name,"videoQOS")==0) {
				Script::AddObject<LUAQualityOfService>(pState, listener.videoQOS());
			} else if (strcmp(name, "dataQOS") == 0) {
				Script::AddObject<LUAQualityOfService>(pState, listener.dataQOS());
			} else if(strcmp(name,"setReceiveAudio")==0) {
				SCRIPT_WRITE_FUNCTION(LUAListener::SetReceiveAudio)
			} else if(strcmp(name,"setReceiveVideo")==0) {
				SCRIPT_WRITE_FUNCTION(LUAListener::SetReceiveVideo)
			} else if(strcmp(name,"setReceiveData")==0) {
				SCRIPT_WRITE_FUNCTION(LUAListener::SetReceiveData)
			} else if(strcmp(name,"client")==0) {
				Script::AddObject<LUAClient>(pState, listener.client);
			} else if (strcmp(name,"parameters")==0) {
				Script::Collection(pState, 1, "parameters");
			} else {
				Script::Collection(pState,1, "parameters");
				lua_getfield(pState, -1, name);
				lua_replace(pState, -2);
			}
		}
	SCRIPT_CALLBACK_RETURN
}

