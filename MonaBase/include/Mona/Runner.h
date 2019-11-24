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
#include "Mona/Thread.h"
#include "Mona/Logs.h"

namespace Mona {


struct Runner : virtual Object {
	Runner(const char* name) : name(name), noLog(Logs::Logging()), noDump(Logs::Dumping())  {}

	const char* name;
	bool noLog;
	bool noDump;

	template <typename ...Args>
	void run(Args&&... args) {
		Thread::ChangeName newName(std::forward<Args>(args)...);
		Exception ex;
		Logs::Disable logs(!noLog, !noDump);
		AUTO_ERROR(run(ex), newName);
	}

private:
	// If ex is raised, an error is displayed if the operation has returned false
	// otherwise a warning is displayed
	virtual bool run(Exception& ex) = 0;
};


} // namespace Mona
