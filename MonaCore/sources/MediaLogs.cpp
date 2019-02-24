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

#include "Mona/MediaLogs.h"

using namespace std;

namespace Mona {

void MediaLogs::starting(const Parameters& parameters) {
	if (_pPublish)
		return;
	_pPublish.set(_api, source);
	INFO(description(), " starts");
	if (!Logs::AddLogger<Publish::Logger>(path.c_str(), *_pPublish))
		stop<Ex::Intern>(LOG_ERROR, "duplicated logger");
}

void MediaLogs::stopping() {
	if(!_pPublish)
		return;
	Logs::RemoveLogger(path.c_str());
	_pPublish.reset();
	INFO(description(), " stops");
}



} // namespace Mona
