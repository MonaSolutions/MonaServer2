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
#include "Mona/Sessions.h"
#include "Mona/Parameters.h"

namespace Mona {

struct ServerAPI;
struct Protocol : virtual Object, Parameters {
	const char*			name;

	const SocketAddress	address; // protocol address

	ServerAPI&		api;
	Sessions&		sessions;

	bool load(Exception& ex);

	virtual void  manage() {}
	virtual const shared<Socket>&	socket() { return _pSocket; }

protected:
	Protocol(const char* name, ServerAPI& api, Sessions& sessions);
	Protocol(const char* name, Protocol& tunnel);
	
	bool auth(const SocketAddress& address);

private:
	const char* onParamUnfound(const std::string& key) const;

	shared<Socket>	_pSocket;
};


} // namespace Mona
