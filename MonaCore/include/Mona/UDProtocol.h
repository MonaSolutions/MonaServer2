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
#include "Mona/Protocol.h"
#include "Mona/UDPSocket.h"

namespace Mona {

struct UDProtocol : Protocol, private UDPSocket, virtual Object {
	UDPSocket::OnPacket& onPacket;
	UDPSocket::OnError   onError;

	SocketAddress load(Exception& ex);

	const shared<Socket>& socket() { return UDPSocket::socket(); }
protected:
	UDProtocol(const char* name, ServerAPI& api, Sessions& sessions);
};


} // namespace Mona
