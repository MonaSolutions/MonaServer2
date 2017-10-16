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

namespace Mona {

struct RendezVous : virtual Object {
	RendezVous();

	void setRedirection(const UInt8* peerId, std::map<SocketAddress, bool>& addresses);
	void setRedirection(const Packet& peerId, std::map<SocketAddress, bool>& addresses, UInt32 timeout);
	void eraseRedirection(const UInt8* peerId);

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
		NULLABLE
		Redirection() : timeout(0) {}
		operator bool() const { return !timeout || !_time.isElapsed(timeout); }
		UInt32					timeout;
		void set(std::map<SocketAddress, bool>& addresses, const Packet& packet = Packet::Null()) { std::map<SocketAddress, bool>::operator=(std::move(addresses)); _packet.set(std::move(packet)); }
	private:
		Time   _time;
		Packet _packet;
	};
	struct Peer : virtual Object {
		SocketAddress			address;
		SocketAddress			serverAddress;
		std::set<SocketAddress> addresses;
		void*					pData;
	};
	void  setIntern(const UInt8* peerId, const SocketAddress& address, const SocketAddress& serverAddress, std::set<SocketAddress>& addresses, void* pData);
	void* meetIntern(const SocketAddress& aAddress, const UInt8* bPeerId, std::map<SocketAddress, bool>& aAddresses, SocketAddress& bAddress, std::map<SocketAddress, bool>& bAddresses);

	
	std::function<bool(const UInt8*, std::map<const UInt8*, Redirection, Entity::Comparator>::iterator&)> _checkRedirection;
	std::mutex _mutex;
	std::map<const UInt8*, Redirection, Entity::Comparator>		_redirections;
	std::map<const UInt8*, Peer, Entity::Comparator>			_peers;
	std::map<SocketAddress, Peer*>								_peersByAddress;
		
};

}  // namespace Mona
