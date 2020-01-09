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

bool MediaLogs::starting(const Parameters& parameters) {
	_pPublish.set(_api, source);
	if (Logs::AddLogger<Publish::Logger>(name, *_pPublish))
		return run();
	_pPublish.reset();
	stop<Ex::Intern>(LOG_ERROR, "Duplicated logger ", name);
	return false;
}

void MediaLogs::stopping() {
	if (!_pPublish)
		return;
	Logs::RemoveLogger(name);
	_pPublish.reset();
}



} // namespace Mona
