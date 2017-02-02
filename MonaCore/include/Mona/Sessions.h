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
#include "Mona/Entities.h"
#include "Mona/Logs.h"
#include <set>

namespace Mona {

struct Session;
struct Sessions : virtual Object {
	enum {
		BYPEER = 1,
		BYADDRESS = 2,
	};

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

	template<typename SessionType, UInt8 options = 0, typename ...Args>
	SessionType& create(Args&&... args) {
		SessionType* pSession = new SessionType(std::forward<Args>(args)...);
		UInt32& id(pSession->_id);
		const auto& it = _freeIds.begin();
		if (it != _freeIds.end()) {
			id = *it;
			_freeIds.erase(it);
		} else {
			if (_sessions.empty())
				id = 1;
			else
				id = _sessions.rbegin()->first + 1;
		}

		while (!_sessions.emplace(id, pSession).second) {
			CRITIC("Bad computing session id, id ", id, " already exists");
			do {
				++id;
			} while (find(id));
		}

		pSession->_sessionsOptions = options;
		addByPeer(*pSession);
		addByAddress(*pSession);
		DEBUG(pSession->name(), " created");
		return *pSession;
	}

	void	 manage();
	
private:

	void    remove(const std::map<UInt32, Session*>::iterator& it, UInt8 options);

	void	addByPeer(Session& session);
	void	removeByPeer(Session& session);

	void	addByAddress(Session& session);
	void	removeByAddress(Session& session);
	void	removeByAddress(const SocketAddress& address, Session& session);

	std::map<UInt32, Session*>							_sessions;
	std::set<UInt32>									_freeIds;
	std::map<const UInt8*, Session*, EntityComparator>	_sessionsByPeerId;
	std::map<SocketAddress, Session*>					_sessionsByAddress[2]; // 0 - UDP, 1 - TCP
};



} // namespace Mona
