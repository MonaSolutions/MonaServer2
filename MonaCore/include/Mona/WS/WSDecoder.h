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
#include "Mona/StreamData.h"
#include "Mona/WS/WS.h"

namespace Mona {


struct WSDecoder : Socket::Decoder, private StreamData<Socket&>, virtual Object {
	typedef Event<void(WS::Message&)> ON(Message);

	WSDecoder(const Handler& handler, const char* name = NULL);

	void decode(shared<Buffer>& pBuffer, const SocketAddress& address, const shared<Socket>& pSocket) { decode(Packet(pBuffer), address, pSocket); }
	void decode(const Packet& packet, const SocketAddress& address, const shared<Socket>& pSocket);
private:
	const char* name(const Socket& socket) const { return _name ? _name : (socket.isSecure() ? "WSS" : "WS"); }

	UInt32 onStreamData(Packet& buffer, Socket& socket);

	UInt32			_size;
	UInt8			_type;
	bool			_masked;
	const Handler&	_handler;
	UInt32			_limit;
	const char*		_name;

	struct Message : StreamData<UInt8, bool, const Socket&> {
		Message(WSDecoder& decoder) : _decoder(decoder), _type(0) {}

	private:
		UInt32 onStreamData(Packet& buffer, UInt8 type, bool flush, const Socket& socket);
		UInt8		_type;
		WSDecoder& _decoder;
	} _message;
};


} // namespace Mona
