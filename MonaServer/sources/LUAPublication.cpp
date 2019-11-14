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

#include "Mona/ServerAPI.h"
#include "Script.h"
#include "LUAMap.h"


using namespace std;

namespace Mona {

template<typename Type>
static int count(lua_State *pState) {
	SCRIPT_CALLBACK(Type, tracks)
		SCRIPT_WRITE_DOUBLE(tracks.size())
	SCRIPT_CALLBACK_RETURN
}
template<typename Type>
static int byteRate(lua_State *pState) {
	SCRIPT_CALLBACK(Type, tracks)
		SCRIPT_WRITE_DOUBLE(tracks.byteRate())
	SCRIPT_CALLBACK_RETURN
}
template<typename Type>
static int lostRate(lua_State *pState) {
	SCRIPT_CALLBACK(Type, tracks)
		SCRIPT_WRITE_DOUBLE(tracks.lostRate())
	SCRIPT_CALLBACK_RETURN
}
template<typename Type>
static int lastTime(lua_State *pState) {
	SCRIPT_CALLBACK(Type, tracks)
		SCRIPT_WRITE_INT(tracks.lastTime)
	SCRIPT_CALLBACK_RETURN
}
template<typename Type>
void TracksInit(lua_State *pState, Type& tracks) {
	SCRIPT_BEGIN(pState)
		SCRIPT_DEFINE_FUNCTION("count", &count<Type>);
		SCRIPT_DEFINE_FUNCTION("byteRate", &byteRate<Type>);
		SCRIPT_DEFINE_FUNCTION("lostRate", &lostRate<Type>);
	SCRIPT_END
}
template<typename Type>
void MediaTracksInit(lua_State *pState, Type& tracks) {
	SCRIPT_BEGIN(pState)
		TracksInit<Type>(pState, tracks);
		SCRIPT_DEFINE_FUNCTION("lastTime", &lastTime<Type>);
	SCRIPT_END
}
template<> void Script::ObjInit(lua_State *pState, const Publication::Tracks<Publication::DataTrack>& tracks) { TracksInit(pState, tracks); }
template<> void Script::ObjInit(lua_State *pState, const Publication::MediaTracks<Publication::VideoTrack>& tracks) { MediaTracksInit(pState, tracks); }
template<> void Script::ObjInit(lua_State *pState, const Publication::MediaTracks<Publication::AudioTrack>& tracks) { MediaTracksInit(pState, tracks); }
template<> void Script::ObjClear(lua_State *pState, const Publication::Tracks<Publication::DataTrack>& tracks) {}
template<> void Script::ObjClear(lua_State *pState, const Publication::MediaTracks<Publication::VideoTrack>& tracks) {}
template<> void Script::ObjClear(lua_State *pState, const Publication::MediaTracks<Publication::AudioTrack>& tracks) {}



static int latency(lua_State *pState) {
	SCRIPT_CALLBACK(Publication, publication)
		SCRIPT_WRITE_INT(publication.latency())
	SCRIPT_CALLBACK_RETURN
}

template<> void Script::ObjInit(lua_State *pState, Publication& publication) {
	AddType<Media::Source>(pState, publication);

	SCRIPT_BEGIN(pState);
		SCRIPT_DEFINE("properties", AddObject<Parameters>(pState, publication));
		SCRIPT_DEFINE("__tab", lua_pushvalue(pState, -2));
		SCRIPT_DEFINE_FUNCTION("__call", &(LUAMap<Publication>::Call<>));

		SCRIPT_DEFINE_STRING("name", publication.name())
		SCRIPT_DEFINE("audios", AddObject(pState, publication.audios));
		SCRIPT_DEFINE("videos", AddObject(pState, publication.videos));
		SCRIPT_DEFINE("datas", AddObject(pState, publication.datas));
		SCRIPT_DEFINE_FUNCTION("latency", &latency);
		SCRIPT_DEFINE_FUNCTION("byteRate", &byteRate<const Publication>);
		SCRIPT_DEFINE_FUNCTION("lostRate", &lostRate<const Publication>);
	SCRIPT_END;
}
template<> void Script::ObjClear(lua_State *pState, Publication& publication) {
	RemoveType<Media::Source>(pState, publication);

	RemoveObject<Parameters>(pState, publication);
	RemoveObject(pState, publication.audios);
	RemoveObject(pState, publication.videos);
	RemoveObject(pState, publication.datas);
	lua_getmetatable(pState, -1);
	lua_pushliteral(pState, "|api");
	lua_rawget(pState, -2);
	if (lua_islightuserdata(pState, -1)) {
		ServerAPI* pAPI = (ServerAPI*)lua_touserdata(pState, -1);
		if (pAPI)
			pAPI->unpublish(publication);
	}
	lua_pop(pState, 2);
}

}

