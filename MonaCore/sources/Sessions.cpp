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

#include "Mona/Sessions.h"
#include "Mona/UDProtocol.h"
#include "Mona/Session.h"

using namespace std;

namespace Mona {

Sessions::~Sessions() {
	// delete sessions
	if (!_sessions.empty())
		WARN("sessions are deleting");
	for (auto& it : _sessions) {
		it.second->kill(Session::ERROR_SERVER);
		delete it.second;
	}
}

void Sessions::addByAddress(Session& session) {
	if (session._sessionsOptions&SESSION_BYADDRESS) {

		session.peer.onAddressChanged = [this, &session](const SocketAddress& oldAddress) {
			if(oldAddress)
				INFO(session.name(), " has changed its address, ", oldAddress, " -> ", session.peer.address);
			removeByAddress(oldAddress, session);
			addByAddress(session);
		};
		if (!session.peer.address) // no address yet, wait next onAddressChanged event (usefull for TCPSession)
			return;

		auto& map(dynamic_cast<UDProtocol*>(&session.protocol()) ? _sessionsByAddress[0] : _sessionsByAddress[1]);
		const auto& it = map.emplace(session.peer.address, &session);
		if (it.second)
			return;
		INFO(it.first->second->name(), " overloaded by ", session.name(), " (by ", session.peer.address, ")");
		const auto& itSession = _sessions.find(it.first->second->_id);
		if (itSession == _sessions.end())
			CRITIC("Overloaded ", it.first->second->name(), " impossible to find in sessions collection")
		else
			remove(itSession, SESSION_BYPEER);
		it.first->second = &session;
	}
}

void Sessions::removeByAddress(Session& session) {
	removeByAddress(session.peer.address, session);
}

void Sessions::removeByAddress(const SocketAddress& address, Session& session) {
	if (session._sessionsOptions&SESSION_BYADDRESS) {

		session.peer.onAddressChanged = nullptr;

		if (!address)
			return; // if no address, was not registered!

		auto& map(dynamic_cast<UDProtocol*>(&session.protocol()) ? _sessionsByAddress[0] : _sessionsByAddress[1]);
		if (map.erase(address) == 0) {
			ERROR(session.name(), " unfound in address sessions collection with key ", address);
			for (auto it = map.begin(); it != map.end(); ++it) {
				if (it->second == &session) {
					INFO("The correct key was ", it->first);
					map.erase(it);
					break;
				}
			}
		}
	}
}

void Sessions::addByPeer(Session& session) {
	if (session._sessionsOptions&SESSION_BYPEER) {
		const auto& it = _sessionsByPeerId.emplace(session);
		if (it.second)
			return;
		INFO(it.first->second->name(), " overloaded by ", session.name(), " (by peer id)");
		const auto& itSession = _sessions.find(it.first->second->_id);
		if (itSession == _sessions.end())
			CRITIC("Overloaded ", it.first->second->name(), " impossible to find in sessions collection")
		else
			remove(itSession, SESSION_BYADDRESS);
		it.first->second = &session;
	}
}

void Sessions::removeByPeer(Session& session) {
	if (session._sessionsOptions&SESSION_BYPEER) {
		if (_sessionsByPeerId.erase(session.peer.id) == 0) {
			ERROR(session.name(), " unfound in peer sessions collection with key ", String::Hex(session.peer.id, Entity::SIZE));
			for (auto it = _sessionsByPeerId.begin(); it != _sessionsByPeerId.end(); ++it) {
				if (it->second == &session) {
					INFO("The correct key was ", String::Hex(it->first, Entity::SIZE));
					_sessionsByPeerId.erase(it);
					break;
				}
			}
		}
	}
}

void Sessions::remove(const map<UInt32, Session*>::iterator& it, SESSION_OPTIONS options) {
	Session& session(*it->second);
	DEBUG(session.name(), " deleted (client.id=", String::Hex(session.peer.id, Entity::SIZE),")");

	if (options&SESSION_BYPEER)
		removeByPeer(session);
	if (options&SESSION_BYADDRESS)
		removeByAddress(session);

	// If remove is called, session can be always not closed if the call comes from addByAddress or addByPeer
	// Here it means an obsolete session, we can kill it
	session.kill(Session::ERROR_ZOMBIE);

	_freeIds.emplace_back(session._id);
	delete &session;
	_sessions.erase(it);
}


void Sessions::manage() {
	auto it = _sessions.begin();
	while (it != _sessions.end()) {
		Session& session(*it->second);
		if (!session.died && session.manage())
			session.flush();
		if (session.died) {
			auto itRemove(it++);
			remove(itRemove, SESSION_BYPEER | SESSION_BYADDRESS);
			continue;
		}
		++it;
	}
}


} // namespace Mona
