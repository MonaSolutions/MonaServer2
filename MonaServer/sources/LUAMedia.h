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
#include "Mona/Media.h"
#include "Mona/MapWriter.h"
#include "ScriptReader.h"
#include "ScriptWriter.h"


namespace Mona {

struct LUAMedia : virtual Static {
	
	struct Tag : virtual Static {
		struct Writer : virtual Object, MapWriter<Writer> {
			const enum Type {
				TYPE_BOTH=0,
				TYPE_AUDIO,
				TYPE_VIDEO
			} type;
			Writer(Type type) : MapWriter<Writer>(self), _type(type), type(type), track(1) {}
			~Writer() {}

			UInt8 track;
			bool isAudio() const { return _type == TYPE_AUDIO; }
			const Media::Audio::Tag& audio() { DEBUG_ASSERT(!_type || _type == TYPE_AUDIO);  return _audio; }
			const Media::Video::Tag& video() { DEBUG_ASSERT(!_type || _type == TYPE_VIDEO);  return _video; }
		
			void clear();
			void emplace(const std::string& key, std::string&& value);
		private:
			UInt64 beginObject(const char* type = NULL);

			Type	_type;
			union {
				Media::Audio::Tag _audio;
				Media::Video::Tag _video;
			};
		};

		struct Reader : virtual Object, DataReader {
			Reader(const Media::Audio::Tag& audio) : isAudio(true), _pTag((void*)&audio), DataReader(Packet::Null(), OTHER) {}
			Reader(const Media::Video::Tag& video) : isAudio(false), _pTag((void*)&video), DataReader(Packet::Null(), OTHER) {}
			const bool isAudio;
		private:
			bool	readOne(UInt8 type, DataWriter& writer);

			void*  _pTag;
		};
	};


};

}
