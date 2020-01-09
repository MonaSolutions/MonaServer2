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
#include "Mona/Logger.h"
#include "Mona/File.h"

namespace Mona {

struct FileLogger : Logger, virtual Object {
	enum {
		DEFAULT_SIZE_BY_FILE = 1000000,
		DEFAULT_ROTATION = 10
	};
	FileLogger(std::string&& dir, UInt32 sizeByFile = DEFAULT_SIZE_BY_FILE, UInt16 rotation = DEFAULT_ROTATION);

	bool enabled() const { return _pFile ? true : false; }

	bool log(LOG_LEVEL level, const Path& file, long line, const std::string& message);
	bool dump(const std::string& header, const UInt8* data, UInt32 size);
private:
	void manage(UInt32 written);

	unique<File>	_pFile;
	UInt32			_written;
	UInt16			_rotation;
	UInt32			_sizeByFile;
};

} // namespace Mona
