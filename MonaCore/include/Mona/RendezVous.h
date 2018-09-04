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
#include "Mona/SocketAddress.h"
#include "Mona/Packet.h"
#include "Mona/Entity.h"
#include "Mona/Timer.h"

namespace Mona {

struct RendezVous : virtual Object {
	RendezVous(const Timer& timer);
	virtual ~RendezVous();

	/*!
	Create manually a redirection to a existing peer */
	void setRedirection(const UInt8* peerId, std::map<SocketAddress, bool>& addresses);
	/*!
	Remove manually a redirection to a removing peer */
	void eraseRedirection(const UInt8* peerId);
	/*!
	Create a volatile and temporary redirection */
	void setRedirection(const Packet& peerId, std::map<SocketAddress, bool>& addresses, UInt32 timeout);

	template<typename DataType=void>
	void set(const UInt8* peerId, const SocketAddress& address, const SocketAddress& serverAddress, DataType* pData = NULL) { std::set<SocketAddress> addresses; setIntern(peerId, address, serverAddress, addresses, pData); }
	template<typename DataType = void>
	void set(const UInt8* peerId, const SocketAddress& address, const SocketAddress& serverAddress, std::set<SocketAddress>& addresses, DataType* pData = NULL) { setIntern(peerId, address, serverAddress, addresses, pData); }

	void erase(const UInt8* peerId);

	/*!
	On meet successed it returns something!=NULL (pData or this) and aAddresses and bAddresses contains at less one address (the public always in first entry)
	On meet failed it returns NULL but aAddresses can be filled with redirections addresses */
	template<typename DataType = void>
	DataType* meet(const SocketAddress& aAddress, const UInt8* bPeerId, std::map<SocketAddress, bool>& aAddresses, SocketAddress& bAddress, std::map<SocketAddress, bool>& bAddresses) { return (DataType*)meetIntern(aAddress, bPeerId, aAddresses, bAddress, bAddresses); }

private:
	struct Redirection : std::map<SocketAddress, bool>, virtual Object {
		Redirection() : _timeout(0) {}
		bool obsolete(const Time& now) const { return _timeout && (now - _time) > _timeout; }
		void set(std::map<SocketAddress, bool>& addresses, const Packet& packet = Packet::Null(), UInt32 timeout = 0) { 
			std::map<SocketAddress, bool>::operator=(std::move(addresses)); _packet.set(std::move(packet)); _timeout = timeout; _time.update(); 
		}

	private:
		Time   _time;
		Packet _packet;
		UInt32	_timeout;

	};
	struct Peer : virtual Object {
		SocketAddress			address;
		SocketAddress			serverAddress;
		std::set<SocketAddress> addresses;
		void*					pData;
	};
	void  setIntern(const UInt8* peerId, const SocketAddress& address, const SocketAddress& serverAddress, std::set<SocketAddress>& addresses, void* pData);
	void* meetIntern(const SocketAddress& aAddress, const UInt8* bPeerId, std::map<SocketAddress, bool>& aAddresses, SocketAddress& bAddress, std::map<SocketAddress, bool>& bAddresses);

	std::mutex													_mutex;
	std::map<const UInt8*, Redirection, Entity::Comparator>		_redirections;
	std::map<const UInt8*, Peer, Entity::Comparator>			_peers;
	std::map<SocketAddress, Peer*>								_peersByAddress;
	const Timer&												_timer;
	Timer::OnTimer												_onTimer;
};

}  // namespace Mona
