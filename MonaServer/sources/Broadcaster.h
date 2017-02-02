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

#include "ServerConnection.h"
#include <set>

class Broadcaster : public virtual Mona::Object {
public:
	Broadcaster() {}
	
	typedef std::set<ServerConnection*>::const_iterator Iterator;
	
	Iterator	 begin() const { return _servers.begin(); }
	Iterator	 end() const { return _servers.end(); }
	Mona::UInt32 count() const { return _servers.size(); }

	ServerConnection* operator[](const std::string& address) {
		for (ServerConnection* pServer : _servers) {
			if (pServer->address == address || (pServer->getString("publicHost", _buffer) && _buffer == address))
				return pServer;
		}
		return NULL;
	}
	
	void broadcast(ServerMessage& message) {
		for (ServerConnection* pServer : _servers)
			pServer->send(message);
	}

private:
	bool add(ServerConnection* pServer) { return _servers.emplace(pServer).second; }
	void remove(ServerConnection* pServer) { _servers.erase(pServer); }
	void clear() { _servers.clear(); }
	
	std::set<ServerConnection*>	_servers;
	std::string					_buffer;

	friend class Servers;
};
