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
#include "Mona/MediaWriter.h"
#include "LUAMedia.h"


using namespace std;

namespace Mona {

static const MediaWriter::OnWrite& OnWrite(lua_State *pState) {
	lua_getmetatable(pState, 1);
	lua_rawgeti(pState, -1, 0);
	MediaWriter::OnWrite* pOnWrite = (MediaWriter::OnWrite*)lua_touserdata(pState, -1);
	lua_pop(pState, 2);
	return *pOnWrite;
}

static int beginMedia(lua_State *pState) {
	SCRIPT_CALLBACK(MediaWriter, writer)
		writer.beginMedia(OnWrite(pState));
	SCRIPT_CALLBACK_RETURN
}
static int writeProperties(lua_State *pState) {
	SCRIPT_CALLBACK(MediaWriter, writer)
		Media::Properties properties;
		MapWriter<Media::Properties> mapWriter(properties);
		SCRIPT_READ_NEXT(ScriptReader(pState, SCRIPT_NEXT_READABLE).read(mapWriter));
		writer.writeProperties(properties, OnWrite(pState));
	SCRIPT_CALLBACK_RETURN
}
static int writeAudio(lua_State *pState) {
	SCRIPT_CALLBACK(MediaWriter, writer)
		LUAMedia::Tag::Writer tagWriter(LUAMedia::Tag::Writer::TYPE_AUDIO);
		SCRIPT_READ_NEXT(ScriptReader(pState, SCRIPT_NEXT_READABLE).read(tagWriter, 1));
		SCRIPT_READ_PACKET(packet);
		writer.writeAudio(SCRIPT_NEXT_READABLE ? SCRIPT_READ_UINT8(tagWriter.track) : tagWriter.track, tagWriter.audio(), packet, OnWrite(pState));
	SCRIPT_CALLBACK_RETURN
}
static int writeVideo(lua_State *pState) {
	SCRIPT_CALLBACK(MediaWriter, writer)
		LUAMedia::Tag::Writer tagWriter(LUAMedia::Tag::Writer::TYPE_VIDEO);
		SCRIPT_READ_NEXT(ScriptReader(pState, SCRIPT_NEXT_READABLE).read(tagWriter, 1));
		SCRIPT_READ_PACKET(packet);
		writer.writeVideo(SCRIPT_NEXT_READABLE ? SCRIPT_READ_UINT8(tagWriter.track) : tagWriter.track, tagWriter.video(), packet, OnWrite(pState));
	SCRIPT_CALLBACK_RETURN
}
static int writeData(lua_State *pState) {
	SCRIPT_CALLBACK(MediaWriter, writer)
		Media::Data::Type type = Media::Data::Type(SCRIPT_READ_UINT8(Media::Data::TYPE_UNKNOWN));
		SCRIPT_READ_PACKET(packet);
		writer.writeData(SCRIPT_NEXT_READABLE ? SCRIPT_READ_UINT8(0) : 0, type, packet, OnWrite(pState));
	SCRIPT_CALLBACK_RETURN
}
static int writeMedia(lua_State *pState) {
	SCRIPT_CALLBACK(MediaWriter, writer)
		if (SCRIPT_NEXT_TYPE == LUA_TSTRING) {
			// data
			Media::Data::Type type = Media::Data::ToType(SCRIPT_READ_STRING(""));
			SCRIPT_READ_PACKET(packet);
			writer.writeData(SCRIPT_NEXT_READABLE ? SCRIPT_READ_UINT8(0) : 0, type, packet, OnWrite(pState));
		} else {
			LUAMedia::Tag::Writer tagWriter(LUAMedia::Tag::Writer::TYPE_BOTH);
			SCRIPT_READ_NEXT(ScriptReader(pState, SCRIPT_NEXT_READABLE).read(tagWriter, 1));
			SCRIPT_READ_PACKET(packet);
			if (tagWriter.isAudio())
				writer.writeAudio(SCRIPT_NEXT_READABLE ? SCRIPT_READ_UINT8(tagWriter.track) : tagWriter.track, tagWriter.audio(), packet, OnWrite(pState));
			else
				writer.writeVideo(SCRIPT_NEXT_READABLE ? SCRIPT_READ_UINT8(tagWriter.track) : tagWriter.track, tagWriter.video(), packet, OnWrite(pState));
		}
	SCRIPT_CALLBACK_RETURN
}
static int endMedia(lua_State *pState) {
	SCRIPT_CALLBACK(MediaWriter, writer)
		writer.endMedia(OnWrite(pState));
	SCRIPT_CALLBACK_RETURN
}

template<> void Script::ObjInit(lua_State *pState, MediaWriter& writer) {
	SCRIPT_BEGIN(pState)
		lua_pushlightuserdata(pState, new MediaWriter::OnWrite([pState, &writer](const Packet& packet) {
			SCRIPT_BEGIN(pState)
				SCRIPT_MEMBER_FUNCTION_BEGIN(writer, "onWrite")
					SCRIPT_WRITE_PACKET(packet)
					SCRIPT_FUNCTION_CALL
				SCRIPT_FUNCTION_END
			SCRIPT_END
		}));
		lua_rawseti(pState, -3, 0);

		SCRIPT_DEFINE_STRING("format", writer.format());
		string buffer;
		SCRIPT_DEFINE_STRING("mime", MIME::Write(buffer, writer.mime(), writer.subMime()));
		SCRIPT_DEFINE_STRING("subMime", writer.subMime());

		SCRIPT_DEFINE_FUNCTION("beginMedia", &beginMedia);
		SCRIPT_DEFINE_FUNCTION("writeProperties", &writeProperties);
		SCRIPT_DEFINE_FUNCTION("writeAudio", &writeAudio);
		SCRIPT_DEFINE_FUNCTION("writeVideo", &writeVideo);
		SCRIPT_DEFINE_FUNCTION("writeData", &writeData);
		SCRIPT_DEFINE_FUNCTION("endMedia", &endMedia);
		SCRIPT_DEFINE_FUNCTION("writeMedia", &writeMedia);
	SCRIPT_END
}
template<> void Script::ObjClear(lua_State *pState, MediaWriter& writer) {
	lua_getmetatable(pState, -1);
	lua_rawgeti(pState, -1, 0);
	delete (MediaWriter::OnWrite*)lua_touserdata(pState, -1);
	lua_pop(pState, 2);
}

}
