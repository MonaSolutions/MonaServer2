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
	
	bool load(Exception& ex);

	Entity::Map<RTMFP::Group>	groups;

private:
	shared<Socket::Decoder>		newDecoder();

	Buffer& initBuffer(shared<Buffer>& pBuffer);
	void	send(UInt8 type, shared<Buffer>& pBuffer, const SocketAddress& address, shared<Packet>& pResponse);


	RTMFPDecoder::OnHandshake	_onHandshake;
	RTMFPDecoder::OnSession		_onSession;
	
	UInt8						_certificat[77];
};


} // namespace Mona
