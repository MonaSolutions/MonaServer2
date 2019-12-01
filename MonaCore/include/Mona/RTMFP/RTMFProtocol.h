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
#include "Mona/UDProtocol.h"
#include "Mona/RTMFP/RTMFPDecoder.h"

namespace Mona {

struct RTMFProtocol : UDProtocol, virtual Object  {
	RTMFProtocol(const char* name, ServerAPI& api, Sessions& sessions);
	~RTMFProtocol();
	
	SocketAddress load(Exception& ex);

	Entity::Map<RTMFP::Group>	groups;

	Buffer& initBuffer(shared<Buffer>& pBuffer);
	void	send(UInt8 type, shared<Buffer>& pBuffer, const SocketAddress& address) { shared<Packet> pResponse; send(type, pBuffer, address, pResponse); }
	void	send(UInt8 type, shared<Buffer>& pBuffer, const SocketAddress& address, shared<Packet>& pResponse) { std::set<SocketAddress> addresses({ address }); send(type, pBuffer, addresses, pResponse); }
	void	send(UInt8 type, shared<Buffer>& pBuffer, std::set<SocketAddress>& addresses) { shared<Packet> pResponse; send(type, pBuffer, addresses, pResponse); }
	void	send(UInt8 type, shared<Buffer>& pBuffer, std::set<SocketAddress>& addresses, shared<Packet>& pResponse);

	std::set<SocketAddress> addresses;
private:
	void manage();
	Socket::Decoder* newDecoder();


	UInt8						_manageTimes;
	RTMFPDecoder::OnHandshake	_onHandshake;
	RTMFPDecoder::OnEdgeMember	_onEdgeMember;
	RTMFPDecoder::OnSession		_onSession;
	shared<RendezVous>			_pRendezVous;
	
	UInt8						_certificat[77];
};


} // namespace Mona
