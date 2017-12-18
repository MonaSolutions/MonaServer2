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

struct FileWatcher : virtual Object {
	template <typename ...Args>
	FileWatcher(Args&&... args) : file(std::forward<Args>(args)...), _lastChange(0) {}
	

	/// look if the file has changed, call clearFile if doesn't exist anymore, or call clearFile and loadFile if file has change
	/// return true if file exists
	bool		watchFile();

	const Path	file;

private:
	virtual void loadFile() = 0;
	virtual void clearFile() = 0;

	Time		_lastChange;
};

} // namespace Mona
