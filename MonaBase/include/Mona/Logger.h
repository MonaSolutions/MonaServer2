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

enum : UInt8 {
	LOG_FATAL = 1,
	LOG_CRITIC = 2,
	LOG_ERROR = 3,
	LOG_WARN = 4,
	LOG_NOTE = 5,
	LOG_INFO = 6,
	LOG_DEBUG = 7,
	LOG_TRACE = 8,
#if defined(_DEBUG)
	LOG_DEFAULT = LOG_DEBUG
#else
	LOG_DEFAULT = LOG_INFO
#endif
};

struct Logger : virtual Object {
	NULLABLE(!enabled())

	Logger() : fatal(NULL) {}
	/*!
	Test if always valid */
	virtual bool enabled() const { return true; }

	const char* name;
	const char* fatal;

	virtual bool log(LOG_LEVEL level, const Path& file, long line, const std::string& message) = 0;
	virtual bool dump(const std::string& header, const UInt8* data, UInt32 size) = 0;
};

} // namespace Mona
