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
	static Buffer&	Write(Buffer& buffer, Type type, const char* subType=NULL);
};




} // namespace Mona
