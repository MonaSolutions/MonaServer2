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

#include "Mona/Mona.h"
#include "Mona/Buffer.h"
#include "Mona/Path.h"

namespace Mona {


struct MIME : virtual Static {

	enum Type {
		TYPE_UNKNOWN = 0,
		TYPE_APPLICATION,
		TYPE_TEXT,
		TYPE_EXAMPLE,
		TYPE_AUDIO,
		TYPE_VIDEO,
		TYPE_IMAGE,
		TYPE_MESSAGE,
		TYPE_MODEL,
		TYPE_MULTIPART
	};

	static Type		Read(const Path& file, const char*& subType);
	static Type		Read(const char* value, const char*& subType);
	template<typename Buffer>
	static Buffer&  Write(Buffer& buffer, Type type, const char* subType=NULL) {
		switch (type) {
			case TYPE_TEXT:
				buffer.append(EXPAND("text/"));
				if (!subType)
					return buffer.append(EXPAND("html; charset=utf-8"));
				break;
			case TYPE_IMAGE:
				buffer.append(EXPAND("image/"));
				if (!subType)
					return buffer.append(EXPAND("jpeg"));
				break;
			case TYPE_APPLICATION:
				buffer.append(EXPAND("application/"));
				if (!subType)
					return buffer.append(EXPAND("octet-stream"));
				break;
			case TYPE_MULTIPART:
				buffer.append(EXPAND("multipart/"));
				if (!subType)
					return buffer.append(EXPAND("mixed"));
				break;
			case TYPE_AUDIO:
				buffer.append(EXPAND("audio/"));
				if (!subType)
					return buffer.append(EXPAND("mp2t"));
				break;
			case TYPE_VIDEO:
				buffer.append(EXPAND("video/"));
				if (!subType)
					return buffer.append(EXPAND("mp2t"));
				break;
			case TYPE_MESSAGE:
				buffer.append(EXPAND("message/"));
				if (!subType)
					return buffer.append(EXPAND("example"));
				break;
			case TYPE_MODEL:
				buffer.append(EXPAND("model/"));
				if (!subType)
					return buffer.append(EXPAND("example"));
				break;
			case TYPE_EXAMPLE:
				return buffer.append(EXPAND("example"));
			default: // TYPE_NONE
				return buffer.append(EXPAND("application/octet-stream"));
		}
		return buffer.append(subType, strlen(subType));
	}
};




} // namespace Mona
