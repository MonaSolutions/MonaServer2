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

RendezVous::RendezVous(const Timer& timer) : _timer(timer),
	_onTimer([this](UInt32)->UInt32 {
		lock_guard<mutex> lock(_mutex);

		Time now; // get time just once for performance
		// remove obsolete redirections
		auto it = _redirections.begin();
		while (it != _redirections.end()) {
			if (it->second.obsolete(now))
				it = _redirections.erase(it);
			else
				++it;
		}
		return 15000;
	}) {
	_timer.set(_onTimer, 15000); // 15s is sufficient
}

RendezVous::~RendezVous() {
	_timer.set(_onTimer, 0);
}

void RendezVous::setRedirection(const UInt8* peerId, map<SocketAddress, bool>& addresses) {
	lock_guard<mutex> lock(_mutex);
	_redirections[peerId].set(addresses);
}

void RendezVous::setRedirection(const Packet& peerId, map<SocketAddress, bool>& addresses, UInt32 timeout) {
	lock_guard<mutex> lock(_mutex);
	Redirection& redirection = _redirections[peerId.data()];
	redirection.set(addresses, peerId, timeout);
}
void RendezVous::eraseRedirection(const UInt8* peerId) {
	lock_guard<mutex> lock(_mutex);
	_redirections.erase(peerId);
}

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
	const auto& itRedirection = _redirections.find(bPeerId);
	if (itRedirection != _redirections.end()) {
		for (auto& it : itRedirection->second)
			aAddresses.emplace(it.first, it.second);
		return NULL;
	}
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
	// fill in first local addresses to avoid an overloading of one public address (at less one public address is required)
	for (const SocketAddress& address : b.addresses)
		bAddresses[address] = false;
	bAddresses[bAddress] = true;
	if (hasExteriorPeer)
		bAddresses[SocketAddress(pA->serverAddress.host(), bAddress.port())] = true;
	
//// B get A
	if (pA) {
		// fill in first local addresses to avoid an overloading of one public address (at less one public address is required)
		for (const SocketAddress& address : pA->addresses)
			aAddresses[address] = false;
	}
	aAddresses[aAddress] = true;
	if (hasExteriorPeer)
		aAddresses[SocketAddress(b.serverAddress.host(), aAddress.port())] = true;
	return b.pData ? b.pData : this; // must return something != NULL (success)
}




}  // namespace Mona
