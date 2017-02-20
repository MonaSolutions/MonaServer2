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
	const auto& it = _peers.find(peerId);
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
	const auto& it = _peers.find(peerId);
	if (it == _peers.end())
		return;
	_peersByAddress.erase(it->second.address);
	_peers.erase(it);
}

void* RendezVous::meetIntern(const SocketAddress& aAddress, const UInt8* bPeerId, map<SocketAddress, bool>& aAddresses, SocketAddress& bAddress, map<SocketAddress, bool>& bAddresses) {
	lock_guard<mutex> lock(_mutex);
	const auto& bIt = _peers.find(bPeerId);
	if (bIt == _peers.end())
		return NULL;
	Peer& b(bIt->second);
	bAddress = b.address;
	Peer* pA;
	const auto& aIt = _peersByAddress.find(aAddress);
	if (aIt == _peersByAddress.end())
		pA = NULL;
	else
		pA = aIt->second;

	// If the both peer see the server in a different way (and serverAddress.host()!= public address host written above),
	// Means an exterior peer, but we can't know which one is the exterior peer
	// so add an interiorAddress build with how see eachone the server on the both side
	// Usefull in the case where Caller is an exterior peer, and SessionWanted an interior peer,
	// we give as host to Caller how it sees the server (= SessionWanted host), and port of SessionWanted
	bool hasExteriorPeer = pA && pA->serverAddress.host() != b.serverAddress.host();

//// A get B
	bAddresses[bAddress] = true;
	if (hasExteriorPeer)
		bAddresses[SocketAddress(pA->serverAddress.host(), bAddress.port())] = true;
	for (const SocketAddress& address : b.addresses)
		bAddresses[address] = false;
	
//// B get A
	aAddresses[aAddress] = true;
	if (hasExteriorPeer)
		aAddresses[SocketAddress(b.serverAddress.host(), aAddress.port())] = true;
	if(pA) {
		for (const SocketAddress& address : pA->addresses)
			aAddresses[address] = false;
	}
	return b.pData ? b.pData : this; // must return something != NULL (success)
}




}  // namespace Mona
