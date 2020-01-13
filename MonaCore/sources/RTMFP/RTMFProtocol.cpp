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

#include "Mona/RTMFP/RTMFProtocol.h"
#include "Mona/URL.h"
#include "Mona/ServerAPI.h"
#include "Mona/RTMFP/RTMFPSession.h"

using namespace std;


namespace Mona {


RTMFProtocol::RTMFProtocol(const char* name, ServerAPI& api, Sessions& sessions) : UDProtocol(name, api, sessions), _pRendezVous(SET, api.timer), _manageTimes(0) {
	memcpy(_certificat, "\x01\x0A\x41\x0E", 4);
	Util::Random(&_certificat[4], 64);
	memcpy(&_certificat[68], "\x02\x15\x02\x02\x15\x05\x02\x15\x0E", 9);

	setNumber("port", 1935);
	setNumber("keepalivePeer",   10);
	setNumber("keepaliveServer", 15);

	_onHandshake = [this](RTMFP::Handshake& handshake) {
		BinaryReader reader(handshake.data(), handshake.size());

		// Fill peer infos
		shared<Peer> pPeer(SET, this->api, "RTMFP", handshake.address);
		const char* url = STR reader.current();
		size_t size = reader.next(reader.available() - 16);
		string serverAddress;
		url = URL::ParseRequest(URL::Parse(url, size, serverAddress), size, (Path&)pPeer->path, REQUEST_MAKE_FOLDER);
		pPeer->setServerAddress(serverAddress);
		pPeer->setQuery(string(url, size));
		URL::ParseQuery(pPeer->query, pPeer->properties());

		// prepare response
		
		shared<Buffer> pBuffer;
		BinaryWriter writer(initBuffer(pBuffer));
		writer.write8(16).write(reader.current(), 16); // tag

		SocketAddress redirection;
		if (pPeer->onHandshake(redirection)) {
			// redirection
			RTMFP::WriteAddress(writer, redirection, RTMFP::LOCATION_REDIRECTION);
			send(0x71, pBuffer, handshake.address, handshake.pResponse);
		}else {
			// Create session and write id in cookie response!
			writer.write8(RTMFP::SIZE_COOKIE);
			writer.write32(this->sessions.create<RTMFPSession>(*this, this->api, pPeer).id());
			writer.writeRandom(RTMFP::SIZE_COOKIE - 4);
			// instance id (certificat in the middle)
			writer.write(_certificat, sizeof(_certificat));
			send(0x70, pBuffer, handshake.address, handshake.pResponse);
		}
	};
	_onEdgeMember = [this](RTMFP::EdgeMember& edgeMember) {
		const auto& it = groups.find(edgeMember.groupId);
		if (it == groups.end())
			return;
		_pRendezVous->setRedirection(edgeMember, edgeMember.redirections, 400000); // timeout between 6 and 7 min => RTMFP session validity time (time max to dispatch edgeMember) + 95sec (time max of udp hole punching attempt) 
		it->second->exchange(edgeMember.id, edgeMember.peerId);
	};
	_onSession = [this](shared<RTMFP::Session>& pSession) {
		RTMFPSession* pClient = this->sessions.find<RTMFPSession>(pSession->id);
		if (pClient)
			pClient->init(pSession);
		else
			ERROR("Session ", pSession->id, " unfound");
	};
}

RTMFProtocol::~RTMFProtocol() {
	_onHandshake = nullptr;
	_onEdgeMember = nullptr;
	_onSession = nullptr;
	if (groups.empty())
		return;
	CRITIC("Few peers are always member of deleting groups");
	for (auto& it : groups)
		delete it.second;
}


void RTMFProtocol::manage() {
	// Every 10 seconds manage RTMFPDecoder!
	if (++_manageTimes < 5)
		return;
	_manageTimes = 0;
	// Trick: sends a empty loopback packet to RTMFPDecoder, which on reception manage its ressource
	SocketAddress address(this->address);
	if (!address.host())
		address.host().set(IPAddress::Loopback(address.family()));
	Exception ex;
	AUTO_ERROR(socket()->sendTo(ex, NULL, 0, address) >= 0, "RTMFP manage");
}

SocketAddress RTMFProtocol::load(Exception& ex) {

	SocketAddress address = UDProtocol::load(ex);
	if (!address)
		return address;

	if (getNumber<UInt16, 10>("keepalivePeer") < 5) {
		WARN("Value of RTMFP.keepalivePeer can't be less than 5 sec")
		setNumber("keepalivePeer", 5);
	}
	if (getNumber<UInt16, 15>("keepaliveServer") < 5) {
		WARN("Value of RTMFP.keepaliveServer can't be less than 5 sec")
		setNumber("keepaliveServer", 5);
	}

	if (const char* addresses = getString("addresses")) {
		String::ForEach forEach([this](UInt32 index, const char* value) {
			SocketAddress address;
			Exception ex;
			if (!address.set(ex, value) && !address.set(ex, value, 1935))
				return true;
			this->addresses.emplace(address);
			return true;
		});
		String::Split(addresses, ";", forEach, SPLIT_IGNORE_EMPTY | SPLIT_TRIM);
	}
	return address;
}


Socket::Decoder* RTMFProtocol::newDecoder() {
	RTMFPDecoder* pDecoder = new RTMFPDecoder(_pRendezVous, api.handler, api.threadPool);
	pDecoder->onSession = _onSession;
	pDecoder->onEdgeMember = _onEdgeMember;
	pDecoder->onHandshake = _onHandshake;
	return pDecoder;
}

Buffer& RTMFProtocol::initBuffer(shared<Buffer>& pBuffer) {
	pBuffer.set(6);
	return BinaryWriter(*pBuffer).write8(0x0B).write16(RTMFP::TimeNow()).next(3).buffer();
}

void RTMFProtocol::send(UInt8 type, shared<Buffer>& pBuffer, set<SocketAddress>& addresses, shared<Packet>& pResponse) {
	struct Sender : Runner, virtual Object {
		Sender(const shared<Socket>& pSocket, shared<Buffer>& pBuffer, set<SocketAddress>& addresses, shared<Packet>& pResponse) : _addresses(move(addresses)), _pResponse(move(pResponse)), _pSocket(pSocket), _pBuffer(move(pBuffer)), Runner("RTMFPProtocolSender") {}
	private:
		bool run(Exception& ex) {
			Packet packet(RTMFP::Engine::Encode(_pBuffer, 0, _addresses));
			if(_pResponse) {
				_pResponse->set(move(packet));
				_pResponse.reset(); // free response before sending to avoid "38 before 30 handshake" error
			}
			for(const SocketAddress& address : _addresses)
				RTMFP::Send(*_pSocket, packet, address);
			return true;
		}
		shared<Buffer>		_pBuffer;
		shared<Socket>		_pSocket;
		set<SocketAddress>	_addresses;
		shared<Packet>		_pResponse;
	};
	Exception ex;
	BinaryWriter(pBuffer->data() + 9, 3).write8(type).write16(pBuffer->size() - 12);
	api.threadPool.queue<Sender>(0, socket(), pBuffer, addresses, pResponse);
}



} // namespace Mona
