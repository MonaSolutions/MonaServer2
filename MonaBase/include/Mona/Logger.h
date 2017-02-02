/*
Copyright 2014 Mona
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

This file is a part of Mona.
*/

#pragma once

#include "Mona/Mona.h"
#include "Mona/Path.h"

namespace Mona {

#define LOG_LEVEL	UInt8

#define LOG_FATAL	1
#define LOG_CRITIC	2
#define LOG_ERROR	3
#define LOG_WARN	4
#define LOG_NOTE	5
#define LOG_INFO	6
#define LOG_DEBUG	7
#define LOG_TRACE	8

struct Logger : virtual Object {
    virtual void log(LOG_LEVEL level, const Path& file, long line, const std::string& message);
	virtual void dump(const std::string& header, const UInt8* data, UInt32 size);
};

} // namespace Mona
