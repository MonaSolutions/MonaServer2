/*
This file is a part of MonaSolutions Copyright 2017
mathieu.poux[a]gmail.com
jammetthomas[a]gmail.com

This program is free software: you can redistribute it and/or
modify it under the terms of the the Mozilla Public License v2.0.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
Mozilla Public License v. 2.0 received along this program for more
details (or else see http://mozilla.org/MPL/2.0/).

*/

#pragma once

#include "Mona/Mona.h"
#include "Mona/Path.h"

namespace Mona {

typedef UInt8 LOG_LEVEL;

enum {
	LOG_FATAL = 1,
	LOG_CRITIC = 2,
	LOG_ERROR = 3,
	LOG_WARN = 4,
	LOG_NOTE = 5,
	LOG_INFO = 6,
	LOG_DEBUG = 7,
	LOG_TRACE = 8
};

struct Logger : virtual Object {
    virtual void log(LOG_LEVEL level, const Path& file, long line, const std::string& message);
	virtual void dump(const std::string& header, const UInt8* data, UInt32 size);
};

} // namespace Mona
