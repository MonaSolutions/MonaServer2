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

#include "Mona/RendezVous.h"
#include "Mona/Logs.h"


using namespace std;


namespace Mona {

void RendezVous::setIntern(const UInt8* peerId, const SocketAddress& address, const SocketAddress& serverAddress, std::set<SocketAddress>& addresses, void* pData) {
	lock_guard<mutex> lock(_mutex);
	auto& it = _peers.find(peerId);
	Peer* pPeer;
	if (it != _peers.end()) {
		pPeer = &it->second;
		_peersByAddress.erase(it->second.address); // change address!
	} else
		pPeer = &_peers[peerId];
	pPeer->address = address;
	pPeer->serverAddress = serverAddress;
	pPeer->pData = pData;
	if (!addresses.empty())
		pPeer->addresses = move(addresses);
	_peersByAddress.emplace(address, pPeer);
}

void RendezVous::erase(const UInt8* peerId) {
	lock_guard<mutex> lock(_mutex);
	auto& it = _peers.find(peerId);
	_peersByAddress.erase(it->second.address);
	_peers.erase(it);
}

void* RendezVous::meetIntern(const SocketAddress& addressA, const UInt8* peerIdB, BinaryWriter& writerA, BinaryWriter& writerB, SocketAddress& addressB, UInt8 attempts) {
	lock_guard<mutex> lock(_mutex);
//// A get B
	auto& it = _peers.find(peerIdB);
	if (it == _peers.end())
		return NULL;
	Peer* pPeerA(NULL);
	Peer& peerB(it->second);
	addressB = peerB.address;
	if (attempts > 0 || addressA.host() == addressB.host()) {
		// try in first just with public address (excepting if the both peer are on the same machine)
		auto& it = _peersByAddress.find(addressA);
		if (it != _peersByAddress.end())
			pPeerA = it->second;
	}

	WriteAddress(writerA, addressB, TYPE_PUBLIC);
	DEBUG(addressA," get public ", addressB);

	if (pPeerA && pPeerA->serverAddress.host() != peerB.serverAddress.host() && pPeerA->serverAddress.host() != addressB.host()) {
		// the both peer see the server in a different way (and serverAddress.host()!= public address host written above),
		// Means an exterior peer, but we can't know which one is the exterior peer
		// so add an interiorAddress build with how see eachone the server on the both side

		// Usefull in the case where Caller is an exterior peer, and SessionWanted an interior peer,
		// we give as host to Caller how it sees the server (= SessionWanted host), and port of SessionWanted
		SocketAddress address(pPeerA->serverAddress.host(), addressB.port());
		WriteAddress(writerA, address, TYPE_PUBLIC);
		DEBUG(addressA," get public ", address);
	}
	for (const SocketAddress& address : peerB.addresses) {
		WriteAddress(writerA, address, TYPE_LOCAL);
		DEBUG(addressA," get local ", address);
	}
	
//// B get A
	// the both peer see the server in a different way (and serverAddress.host()!= public address host written above),
	// Means an exterior peer, but we can't know which one is the exterior peer
	// so add an interiorAddress build with how see eachone the server on the both side
	bool hasAnExteriorPeer(pPeerA && pPeerA->serverAddress.host() != peerB.serverAddress.host() && peerB.serverAddress.host() != addressA.host());

	// times starts to 0
	UInt8 index = 0;
	const SocketAddress* pAddress(&addressA);
	if (pPeerA && !pPeerA->addresses.empty()) {
		// If two clients are on the same lan, starts with private address
		if (pPeerA->address.host() == addressA.host())
			++attempts;

		index = attempts % (pPeerA->addresses.size() + (hasAnExteriorPeer ? 2 : 1));
		if (index > 0) {
			if (hasAnExteriorPeer && --index == 0)
				pAddress = NULL;
			else {
				auto it = pPeerA->addresses.begin();
				advance(it, --index);
				pAddress = &(*it);
			}
		}
	}

	if (!pAddress) {
		// Usefull in the case where B is an exterior peer, and A an interior peer,
		// we give as host to B how it sees the server (= A host), and port of A
		SocketAddress address(peerB.serverAddress.host(), addressA.port());
		DEBUG(addressB, " get public ", address);
		WriteAddress(writerB, address, TYPE_PUBLIC);
	} else if(index) {
		DEBUG(addressB, " get public ", *pAddress);
		WriteAddress(writerB, *pAddress, TYPE_PUBLIC);
	} else {
		DEBUG(addressB, " get local ", *pAddress);
		WriteAddress(writerB, *pAddress, TYPE_LOCAL);
	}
	return peerB.pData ? peerB.pData : this; // must return something != NULL (success)
}


BinaryWriter& RendezVous::WriteAddress(BinaryWriter& writer, const SocketAddress& address, Type type) {
	const IPAddress& host = address.host();
	if (host.family() == IPAddress::IPv6)
		writer.write8(type | 0x80);
	else
		writer.write8(type);
	NET_SOCKLEN size(host.size());
	const UInt8* bytes = BIN host.data();
	for (NET_SOCKLEN i = 0; i<size; ++i)
		writer.write8(bytes[i]);
	return writer.write16(address.port());
}



}  // namespace Mona
