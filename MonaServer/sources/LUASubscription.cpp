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

#include "LUASubscription.h"
#include "LUAMap.h"
#include "LUAMedia.h"
#include "Mona/MapReader.h"
#include "Mona/ServerAPI.h"


using namespace std;

namespace Mona {

template<typename Type>
static int count(lua_State *pState) {
	SCRIPT_CALLBACK(Type, tracks)
		SCRIPT_WRITE_DOUBLE(tracks.size())
	SCRIPT_CALLBACK_RETURN
}
template<typename Type>
static int reliable(lua_State *pState) {
	SCRIPT_CALLBACK(Type, tracks)
		SCRIPT_WRITE_BOOLEAN(tracks.reliable);
	SCRIPT_CALLBACK_RETURN
}
template<typename Type>
static int dropped(lua_State *pState) {
	SCRIPT_CALLBACK(Type, tracks)
		SCRIPT_WRITE_INT(tracks.dropped);
	SCRIPT_CALLBACK_RETURN
}
template<typename Type>
static int multiTracks(lua_State *pState) {
	SCRIPT_CALLBACK(Type, tracks)
		SCRIPT_WRITE_BOOLEAN(tracks.multiTracks);
	SCRIPT_CALLBACK_RETURN
}
template<typename Type>
static int selected(lua_State *pState) {
	SCRIPT_CALLBACK(Type, tracks)
		SCRIPT_WRITE_BOOLEAN(tracks.selected(SCRIPT_READ_UINT8(0)));
	SCRIPT_CALLBACK_RETURN
}
template<typename Type>
void TracksInit(lua_State *pState, Type& tracks) {
	SCRIPT_BEGIN(pState)
		SCRIPT_DEFINE_FUNCTION("count", &count<Type>);
		SCRIPT_DEFINE_FUNCTION("reliable", &reliable<Type>);
		SCRIPT_DEFINE_FUNCTION("dropped", &dropped<Type>);
		SCRIPT_DEFINE_FUNCTION("multiTracks", &multiTracks<Type>);
		SCRIPT_DEFINE_FUNCTION("selected", &selected<Type>);
	SCRIPT_END
}
template<> void Script::ObjInit(lua_State *pState, const Subscription::MediaTracks<Subscription::Track>& tracks) { TracksInit(pState, tracks); }
template<> void Script::ObjInit(lua_State *pState, const Subscription::MediaTracks<Subscription::VideoTrack>& tracks) { TracksInit(pState, tracks); }
template<> void Script::ObjInit(lua_State *pState, const Subscription::Tracks<Subscription::Track>& tracks) { TracksInit(pState, tracks); }
template<> void Script::ObjClear(lua_State *pState, const Subscription::MediaTracks<Subscription::Track>& tracks) {}
template<> void Script::ObjClear(lua_State *pState, const Subscription::MediaTracks<Subscription::VideoTrack>& tracks) {}
template<> void Script::ObjClear(lua_State *pState, const Subscription::Tracks<Subscription::Track>& tracks) {}


static int ejected(lua_State *pState) {
	SCRIPT_CALLBACK(Subscription, subscription)
		switch (subscription.ejected()) {
			case Subscription::EJECTED_BANDWITDH:
				SCRIPT_WRITE_STRING("bandwidth");
				break;
			case Subscription::EJECTED_ERROR:
				SCRIPT_WRITE_STRING("error");
				break;
			case Subscription::EJECTED_NONE:
				break;
		}
	SCRIPT_CALLBACK_RETURN
}
static int streaming(lua_State *pState) {
	SCRIPT_CALLBACK(Subscription, subscription)
		if(subscription.streaming())
			SCRIPT_WRITE_DOUBLE(subscription.streaming())
	SCRIPT_CALLBACK_RETURN
}
static int target(lua_State *pState) {
	SCRIPT_CALLBACK(Subscription, subscription)
		SCRIPT_WRITE_STRING(typeof(subscription.target())); // When "Subscription" = subscription intern, else will be the ProtocolWriter!
	SCRIPT_CALLBACK_RETURN
}


template<> void Script::ObjInit(lua_State *pState, Subscription& subscription) {
	// parameters
	lua_pushliteral(pState, "parameters");
	AddObject(pState, (Parameters&)subscription);
	lua_getmetatable(pState, -1);
	lua_setmetatable(pState, -6); // metatable of parameters becomes metatable of __index of subscription object!

	SCRIPT_BEGIN(pState);
		SCRIPT_DEFINE_FUNCTION("__call", &LUAMap<const Parameters>::Call<LUAMap<const Parameters>::Mapper<Subscription>>);
		SCRIPT_DEFINE_FUNCTION("__pairs", &LUAMap<const Parameters>::Pairs<LUAMap<const Parameters>::Mapper<Subscription>>);
		SCRIPT_DEFINE_STRING("name", subscription.name());
		SCRIPT_DEFINE_FUNCTION("target", &target);
		SCRIPT_DEFINE_FUNCTION("ejected", &ejected);
		SCRIPT_DEFINE_FUNCTION("streaming", &streaming);
		
		SCRIPT_DEFINE("audios", AddObject(pState, subscription.audios));
		SCRIPT_DEFINE("videos", AddObject(pState, subscription.videos));
		SCRIPT_DEFINE("datas", AddObject(pState, subscription.datas));
	SCRIPT_END;
}
template<> void Script::ObjClear(lua_State *pState, Subscription& subscription) {
	RemoveObject(pState, (Parameters&)subscription);
	RemoveObject(pState, subscription.audios);
	RemoveObject(pState, subscription.videos);
	RemoveObject(pState, subscription.datas);
	lua_getmetatable(pState, -1);
	lua_pushliteral(pState, "|api");
	lua_rawget(pState, -2);
	if (lua_islightuserdata(pState, -1)) {
		ServerAPI* pAPI = (ServerAPI*)lua_touserdata(pState, -1);
		if (pAPI)
			pAPI->unsubscribe(subscription);
	}
	lua_pop(pState, 2);
}


UInt64 LUASubscription::queueing() const {
	UInt64 queueing = 0;
	SCRIPT_BEGIN(_pState)
		SCRIPT_MEMBER_FUNCTION_BEGIN((Subscription&)self, "queueing")
			SCRIPT_FUNCTION_CALL
			queueing = SCRIPT_READ_UINT32(0);
		SCRIPT_FUNCTION_END
	SCRIPT_END
	return queueing;
}
bool LUASubscription::beginMedia(const string& name) {
	bool result = true;
	SCRIPT_BEGIN(_pState)
		SCRIPT_MEMBER_FUNCTION_BEGIN(self(), "onBegin")
			SCRIPT_WRITE_STRING(name);
			SCRIPT_FUNCTION_CALL
			if(SCRIPT_FUNCTION_ERROR)
				result = false;
			else if (SCRIPT_NEXT_READABLE)
				result = SCRIPT_READ_BOOLEAN(true);
		SCRIPT_FUNCTION_END
	SCRIPT_END
	return result;
}
bool LUASubscription::writeProperties(const Media::Properties& properties) {
	bool result = true;
	SCRIPT_BEGIN(_pState)
		SCRIPT_MEMBER_FUNCTION_BEGIN(self(), "onProperties", "onMedia")
			if (strcmp(SCRIPT_FUNCTION_NAME, "onMedia") == 0)
				SCRIPT_WRITE_STRING("properties");
			ScriptWriter writer(_pState);
			MapReader<Parameters>(properties).read(writer);
			SCRIPT_FUNCTION_CALL
			if(SCRIPT_FUNCTION_ERROR)
				result = false;
			else if (SCRIPT_NEXT_READABLE)
				result = SCRIPT_READ_BOOLEAN(true);
		SCRIPT_FUNCTION_END
	SCRIPT_END
	return result;
}
bool LUASubscription::writeAudio(UInt8 track, const Media::Audio::Tag& tag, const Packet& packet, bool reliable) {
	bool result = true;
	SCRIPT_BEGIN(_pState)
		SCRIPT_MEMBER_FUNCTION_BEGIN(self(), "onAudio", "onMedia")
			SCRIPT_WRITE_INT(track);
			ScriptWriter writer(_pState);
			LUAMedia::Tag::Reader(tag).read(writer);
			Script::NewObject(_pState, new Packet(packet));
			SCRIPT_WRITE_BOOLEAN(reliable);
			SCRIPT_FUNCTION_CALL
			if(SCRIPT_FUNCTION_ERROR)
				result = false;
			else if (SCRIPT_NEXT_READABLE)
				result = SCRIPT_READ_BOOLEAN(true);
		SCRIPT_FUNCTION_END
	SCRIPT_END
	return result;
}

bool LUASubscription::writeVideo(UInt8 track, const Media::Video::Tag& tag, const Packet& packet, bool reliable) {
	bool result = true;
	SCRIPT_BEGIN(_pState)
		SCRIPT_MEMBER_FUNCTION_BEGIN(self(), "onVideo", "onMedia")
			SCRIPT_WRITE_INT(track);
			ScriptWriter writer(_pState);
			LUAMedia::Tag::Reader(tag).read(writer);
			Script::NewObject(_pState, new Packet(packet));
			SCRIPT_WRITE_BOOLEAN(reliable);
			SCRIPT_FUNCTION_CALL
			if(SCRIPT_FUNCTION_ERROR)
				result = false;
			else if (SCRIPT_NEXT_READABLE)
				result = SCRIPT_READ_BOOLEAN(true);
		SCRIPT_FUNCTION_END
	SCRIPT_END
	return result;
}
bool LUASubscription::writeData(UInt8 track, Media::Data::Type type, const Packet& packet, bool reliable) {
	bool result = true;
	SCRIPT_BEGIN(_pState)
		SCRIPT_MEMBER_FUNCTION_BEGIN(self(), "onData", "onMedia")
			SCRIPT_WRITE_INT(track);
			SCRIPT_WRITE_STRING(Media::Data::TypeToString(type))
			Script::NewObject(_pState, new Packet(packet));
			SCRIPT_WRITE_BOOLEAN(reliable);
			SCRIPT_FUNCTION_CALL
			if(SCRIPT_FUNCTION_ERROR)
				result = false;
			else if (SCRIPT_NEXT_READABLE)
				result = SCRIPT_READ_BOOLEAN(true);
		SCRIPT_FUNCTION_END
	SCRIPT_END
	return result;
}
bool LUASubscription::endMedia() {
	bool result = false; // no flush if no return, to avoid a crash if LUASubscription is deleted!
	SCRIPT_BEGIN(_pState)
		SCRIPT_MEMBER_FUNCTION_BEGIN(self(), "onEnd", "onMedia")
			if(strcmp(SCRIPT_FUNCTION_NAME, "onMedia")==0)
				SCRIPT_WRITE_NIL;
			SCRIPT_FUNCTION_CALL
			if (SCRIPT_NEXT_READABLE) // if return something, considerate the object always alive => true by default to execute the flush!
				result = SCRIPT_READ_BOOLEAN(true);
		SCRIPT_FUNCTION_END
	SCRIPT_END
}
void LUASubscription::flush() {
	SCRIPT_BEGIN(_pState)
		SCRIPT_MEMBER_FUNCTION_BEGIN(self(), "onFlush", "onMedia")
			SCRIPT_FUNCTION_CALL
		SCRIPT_FUNCTION_END
	SCRIPT_END
}

}
