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
#include "Mona/MediaReader.h"
#include "LUAMedia.h"


using namespace std;

namespace Mona {

static int flush(lua_State *pState) {
	SCRIPT_CALLBACK(MediaReader, reader)
		Media::Source* pSource = Script::ToObject<Media::Source>(pState, SCRIPT_READ_NEXT(1));
		reader.flush(pSource ? *pSource : Media::Source::Null());
	SCRIPT_CALLBACK_RETURN
}
static int read(lua_State *pState) {
	SCRIPT_CALLBACK(MediaReader, reader)
		SCRIPT_READ_PACKET(packet);
		if(packet.data()) {
			Media::Source* pSource = Script::ToObject<Media::Source>(pState, SCRIPT_READ_NEXT(1));
			reader.read(packet, pSource ? *pSource : Media::Source::Null());
		} else
			SCRIPT_ERROR("Takes in first argument string or packet data");
	SCRIPT_CALLBACK_RETURN
}

template<> void Script::ObjInit(lua_State *pState, MediaReader& reader) {
	SCRIPT_BEGIN(pState)
		SCRIPT_DEFINE_STRING("format", reader.format());
		string buffer;
		SCRIPT_DEFINE_STRING("mime", MIME::Write(buffer, reader.mime(), reader.subMime()));
		SCRIPT_DEFINE_STRING("subMime", reader.subMime());

		SCRIPT_DEFINE_FUNCTION("read", &read);
		SCRIPT_DEFINE_FUNCTION("flush", &flush);
	SCRIPT_END
}
template<> void Script::ObjClear(lua_State *pState, MediaReader& reader) {}

}
