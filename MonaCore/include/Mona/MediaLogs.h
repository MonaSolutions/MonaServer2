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
#include "Mona/ServerAPI.h"
#include "Mona/Publish.h"

namespace Mona {


struct MediaLogs : MediaStream, virtual Object {

	MediaLogs(std::string&& name, Media::Source& source, ServerAPI& api) : _api(api), name(std::move(name)),
		MediaStream(MediaStream::TYPE_LOGS, source, "Stream logger ", name) {}
	virtual ~MediaLogs() { stop(); }

	const std::string name;

private:
	bool starting(const Parameters& parameters);
	void stopping();


	ServerAPI&		_api;
	unique<Publish> _pPublish;

};

} // namespace Mona
