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
#include "Mona/Socket.h"
#include "Mona/Logs.h"
#include "Mona/Entity.h"

namespace Mona {

typedef UInt8 SESSION_OPTIONS;
enum {
	SESSION_BYPEER = 1,
	SESSION_BYADDRESS = 2,
};

/*!
Allow to manage sessions + override obsolete session on address duplication */
class Session;
struct Sessions : virtual Object {

	Sessions() {}
	virtual ~Sessions();

	template<typename SessionType = Session>
	SessionType* findByAddress(const SocketAddress& address, Socket::Type type) {
		auto& map(type == Socket::TYPE_DATAGRAM ? _sessionsByAddress[0] : _sessionsByAddress[1]);
		const auto& it = map.find(address);
		if (it == map.end())
			return NULL;
		return dynamic_cast<SessionType*>(it->second);
	}


	template<typename SessionType = Session>
	SessionType* findByPeer(const UInt8* peerId) {
		const auto& it = _sessionsByPeerId.find(peerId);
		if (it == _sessionsByPeerId.end())
			return NULL;
		return dynamic_cast<SessionType*>(it->second);
	}


	template<typename SessionType = Session>
	SessionType* find(UInt32 id) {
		const auto& it = _sessions.find(id);
		if (it == _sessions.end())
			return NULL;
		return dynamic_cast<SessionType*>(it->second);
	}

	template<typename SessionType, SESSION_OPTIONS options = SESSION_BYADDRESS, typename ...Args>
	SessionType& create(Args&&... args) {
		SessionType* pSession = new SessionType(std::forward<Args>(args)...);
		if (!_freeIds.empty()) {
			pSession->_id = _freeIds.front()-1;
			_freeIds.pop_front();
		} else if (!_sessions.empty())
			pSession->_id = _sessions.rbegin()->first;
		
		while (!_sessions.emplace(++pSession->_id, pSession).second)
			CRITIC("Bad computing session id, id ", pSession->_id, " already exists");

		pSession->_sessionsOptions = options;
		addByPeer(*pSession);
		addByAddress(*pSession);
		DEBUG(pSession->name(), " created (client.id=", String::Hex(pSession->peer.id, Entity::SIZE),")");
		return *pSession;
	}

	void	 manage();
	
private:

	void    remove(const std::map<UInt32, Session*>::iterator& it, SESSION_OPTIONS options);

	void	addByPeer(Session& session);
	void	removeByPeer(Session& session);

	void	addByAddress(Session& session);
	void	removeByAddress(Session& session);
	void	removeByAddress(const SocketAddress& address, Session& session);

	std::map<UInt32, Session*>			_sessions;
	std::deque<UInt32>					_freeIds;
	Entity::Map<Session>				_sessionsByPeerId;
	std::map<SocketAddress, Session*>	_sessionsByAddress[2]; // 0 - UDP, 1 - TCP
};



} // namespace Mona
