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

#include "LUAMedia.h"

using namespace std;

namespace Mona {

UInt64 LUAMedia::Tag::Writer::beginObject(const char* type) {
	if(!type)
		_type = String::ICompare(type, EXPAND("audio")) == 0 ? TYPE_AUDIO : TYPE_VIDEO; // doesn't start by audio => is video
	return MapWriter<Writer>::beginObject(type);
}
void LUAMedia::Tag::Writer::emplace(const string& key, string&& value) {
	if (String::ICompare(key, "track") == 0) {
		String::ToNumber(value, track);
		return;
	}
	// Video::Tag::type/codec are aligned with Audio::Tag::type/codec
	if (String::ICompare(key, "time") == 0) {
		String::ToNumber(value, _video.time);
		return;
	}
	if (String::ICompare(key, "codec") == 0) {
		String::ToNumber(value,(UInt8&)_video.codec);
		return;
	}

	if (!type || type == TYPE_VIDEO) {
		if (String::ICompare(key, "frame") == 0) {
			_type = TYPE_VIDEO;
			String::ToNumber(value,(UInt8&)_video.frame);
			return;
		}
		if (String::ICompare(key, "compositionOffset") == 0) {
			_type = TYPE_VIDEO;
			String::ToNumber(value, _video.compositionOffset);
			return;
		}
	}
	if (!type || type == TYPE_AUDIO) {
		if (String::ICompare(key, "isConfig") == 0) {
			_type = TYPE_AUDIO;
			_audio.isConfig = String::IsTrue(value);
			return;
		}
		if (String::ICompare(key, "channels") == 0) {
			_type = TYPE_AUDIO;
			String::ToNumber(value, _audio.channels);
			return;
		}
		if (String::ICompare(key, "rate") == 0) {
			_type = TYPE_AUDIO;
			String::ToNumber(value, _audio.rate);
			return;
		}
	}
}

void LUAMedia::Tag::Writer::clear() {
	_audio.reset();
	_video.reset();
	_type = type;
	track = 1;
}

bool LUAMedia::Tag::Reader::readOne(UInt8 type, DataWriter& writer) {
	if (isAudio) {
		const Media::Audio::Tag& tag(*(Media::Audio::Tag*)_pTag);
		writer.beginObject("audio");
		writer.writeNumberProperty("time", tag.time);
		writer.writeNumberProperty("codec", tag.codec);
		writer.writeBooleanProperty("isConfig", tag.isConfig);
		writer.writeNumberProperty("channels", tag.channels);
		writer.writeNumberProperty("rate", tag.rate);
		writer.endObject();
	} else {
		const Media::Video::Tag& tag(*(Media::Video::Tag*)_pTag);
		writer.beginObject("video");
		writer.writeNumberProperty("time", tag.time);
		writer.writeNumberProperty("codec", tag.codec);
		writer.writeNumberProperty("frame", tag.frame);
		writer.writeNumberProperty("compositionOffset", tag.compositionOffset);
		writer.endObject();
	}
	return true;
}

static int writeAudio(lua_State *pState) {
	SCRIPT_CALLBACK(Media::Source, source)
		LUAMedia::Tag::Writer writer(LUAMedia::Tag::Writer::TYPE_AUDIO);
		if (SCRIPT_NEXT_TYPE == LUA_TNUMBER) // to be compatible with a mapping Media::Target => Media::Source (ex: Subcription::on## => Publication)
			writer.track = SCRIPT_READ_UINT8(1);
		SCRIPT_READ_NEXT(ScriptReader(pState, SCRIPT_NEXT_READABLE).read(writer, 1));
		SCRIPT_READ_PACKET(packet);
		source.writeAudio(writer.audio(), packet, SCRIPT_NEXT_READABLE ? SCRIPT_READ_UINT8(writer.track) : writer.track);
	SCRIPT_CALLBACK_RETURN
}
static int writeVideo(lua_State *pState) {
	SCRIPT_CALLBACK(Media::Source, source)
		LUAMedia::Tag::Writer writer(LUAMedia::Tag::Writer::TYPE_VIDEO);
		if(SCRIPT_NEXT_TYPE==LUA_TNUMBER) // to be compatible with a mapping Media::Target => Media::Source (ex: Subcription::on## => Publication)
			writer.track = SCRIPT_READ_UINT8(1);
		SCRIPT_READ_NEXT(ScriptReader(pState, SCRIPT_NEXT_READABLE).read(writer, 1));
		SCRIPT_READ_PACKET(packet);
		source.writeVideo(writer.video(), packet, SCRIPT_NEXT_READABLE ? SCRIPT_READ_UINT8(writer.track) : writer.track);
	SCRIPT_CALLBACK_RETURN
}
static int writeData(lua_State *pState) {
	SCRIPT_CALLBACK(Media::Source, source)
		UInt8 track(SCRIPT_NEXT_TYPE == LUA_TNUMBER ? SCRIPT_READ_UINT8(0) : 0); // to be compatible with a mapping Media::Target => Media::Source (ex: Subcription::on## => Publication)
		Media::Data::Type type = Media::Data::ToType(SCRIPT_READ_STRING(""));
		SCRIPT_READ_PACKET(packet);
		source.writeData(type, packet, SCRIPT_NEXT_READABLE ? SCRIPT_READ_UINT8(track) : track);
	SCRIPT_CALLBACK_RETURN
}
static int writeMedia(lua_State *pState) {
	// Can replace writeAudio/writeVideo/writeData/writeProperties/reset/flush to allow a direct redirection from LUASubscription::onMedia to LUAMedia::Source::writeMedia!
	SCRIPT_CALLBACK(Media::Source, source)
		if(SCRIPT_NEXT_READABLE) {
			if (SCRIPT_NEXT_TYPE == LUA_TNIL) {
				SCRIPT_READ_NIL
				source.reset();
			}
			UInt8 track(SCRIPT_NEXT_TYPE == LUA_TNUMBER ? SCRIPT_READ_UINT8(0) : 0); // to be compatible with a mapping Media::Target => Media::Source (ex: Subcription::on## => Publication)
			if (SCRIPT_NEXT_TYPE == LUA_TSTRING) {
				// data
				const char* subMime = SCRIPT_READ_STRING("");
				Media::Data::Type type = Media::Data::ToType(subMime);
				if (!type && SCRIPT_NEXT_TYPE == LUA_TTABLE && String::ICompare(subMime, "properties")==0) {
					ScriptReader reader(pState, SCRIPT_NEXT_READABLE);
					Media::Properties properties;
					SCRIPT_READ_NEXT(properties.addProperties(0, reader));
					source.addProperties(properties);
				} else {
					SCRIPT_READ_PACKET(packet);
					source.writeData(type, packet, SCRIPT_NEXT_READABLE ? SCRIPT_READ_UINT8(track) : track);
				}
			} else {
				LUAMedia::Tag::Writer writer(LUAMedia::Tag::Writer::TYPE_BOTH);
				writer.track = track;
				SCRIPT_READ_NEXT(ScriptReader(pState, SCRIPT_NEXT_READABLE).read(writer, 1));
				SCRIPT_READ_PACKET(packet);
				if (writer.isAudio())
					source.writeAudio(writer.audio(), packet, SCRIPT_NEXT_READABLE ? SCRIPT_READ_UINT8(writer.track) : writer.track);
				else
					source.writeVideo(writer.video(), packet, SCRIPT_NEXT_READABLE ? SCRIPT_READ_UINT8(writer.track) : writer.track);
			}
		} else
			source.flush();
	SCRIPT_CALLBACK_RETURN
}
static int reportLost(lua_State *pState) {
	// ["audio"|"video"|"data"],  lost, [track]
	SCRIPT_CALLBACK(Media::Source, source)
		const char* type = SCRIPT_NEXT_TYPE == LUA_TSTRING ? SCRIPT_READ_STRING(NULL) : NULL;
		UInt32 lost = SCRIPT_READ_UINT32(0);
		UInt8 track = SCRIPT_NEXT_READABLE ? SCRIPT_READ_UINT8(0) : 0;
		if (type) {
			if (String::ICompare(type, EXPAND("audio")) == 0)
				source.reportLost(Media::TYPE_AUDIO, lost, track);
			else if (String::ICompare(type, EXPAND("video")) == 0)
				source.reportLost(Media::TYPE_VIDEO, lost, track);
			else
				source.reportLost(Media::TYPE_DATA, lost, track);
		} else
			source.reportLost(Media::TYPE_NONE, lost, track);
	SCRIPT_CALLBACK_RETURN
}
static int flush(lua_State *pState) {
	SCRIPT_CALLBACK(Media::Source, source)
		source.flush();
	SCRIPT_CALLBACK_RETURN
}
static int reset(lua_State *pState) {
	SCRIPT_CALLBACK(Media::Source, source)
		source.reset();
	SCRIPT_CALLBACK_RETURN
}

template<> void Script::ObjInit(lua_State *pState, Media::Source& source) {
	SCRIPT_BEGIN(pState);
		SCRIPT_DEFINE_FUNCTION("writeAudio", writeAudio);
		SCRIPT_DEFINE_FUNCTION("writeVideo", writeVideo);
		SCRIPT_DEFINE_FUNCTION("writeData", writeData);
		SCRIPT_DEFINE_FUNCTION("writeMedia", writeMedia);
		SCRIPT_DEFINE_FUNCTION("reportLost", reportLost);
		SCRIPT_DEFINE_FUNCTION("flush", flush);
		SCRIPT_DEFINE_FUNCTION("reset", reset);
	SCRIPT_END;
}
template<> void Script::ObjClear(lua_State *pState, Media::Source& source) {}

}
