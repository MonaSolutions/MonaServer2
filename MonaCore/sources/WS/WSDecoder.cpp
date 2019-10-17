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

#include "Mona/WS/WSDecoder.h"
#include "Mona/WS/WS.h"

using namespace std;

namespace Mona {

WSDecoder::WSDecoder(const Handler& handler, const char* name) :
	_name(name), _masked(false), _type(0), _size(0), _handler(handler), _message(self) {
}

void WSDecoder::decode(const Packet& packet, const SocketAddress& address, const shared<Socket>& pSocket) {
	_limit = pSocket->recvBufferSize();
	if (!addStreamData(packet, _limit, *pSocket)) {
		ERROR("WS message exceeds buffer maximum ", pSocket->recvBufferSize(), " size");
		pSocket->shutdown(Socket::SHUTDOWN_RECV); // no more reception
	}
}

UInt32 WSDecoder::onStreamData(Packet& buffer, Socket& socket) {
	do {

		BinaryReader reader(buffer.data(), buffer.size());

		if(!_size) {
			if (reader.available()<2)
				return reader.size();

			_type = reader.read8();
			UInt8 lengthByte = reader.read8();
			if ((lengthByte & 0x7f) == 127) {
				if (reader.available()<8)
					return reader.size();
				_size = UInt32(reader.read64()); // if size is truncated, the following request will be bad-formed and the client should be rejected (TODO tested!)
			} else if ((lengthByte & 0x7f) ==126) {
				if (reader.available()<2)
					return reader.size();
				_size = reader.read16();
			} else
				_size = (lengthByte & 0x7f);

			if (lengthByte & 0x80) {
				_size += 4;
				_masked = true;
			}
		}

		if (reader.shrink(_size)<_size)
			return reader.available();

		_size = 0;

		if (_masked)
			WS::Unmask(reader);

		Packet packet(buffer, reader.current(), reader.available());
		buffer += reader.size();
		if (_type & 0x08) { // => control frame (most signifiant bit of opcode = 1)
			// https://tools.ietf.org/html/rfc6455#section-5.4
			// Control frames(see Section 5.5) MAY be injected in the middle of
			// a fragmented message.Control frames themselves MUST NOT be
			// fragmented.
			DUMP_REQUEST(name(socket), packet.data(), packet.size(), socket.peerAddress());
			_handler.queue(onMessage, _type&0x0F, packet, !buffer);
		} else
			_message.addStreamData(packet, _limit, _type, !buffer, socket);
	} while (buffer);

	return 0;
}

UInt32 WSDecoder::Message::onStreamData(Packet& buffer, UInt8 type, bool flush, const Socket& socket) {
	UInt8 newType = (type & 0x0F);
	if (newType)
		_type = newType;

	if (!(type & 0x80))
		return buffer.size(); // wait next frame!
	DUMP_REQUEST(_decoder.name(socket), buffer.data(), buffer.size(), socket.peerAddress());
	_decoder._handler.queue(_decoder.onMessage, _type, buffer, flush);
	_type = 0;
	return 0;
}

} // namespace Mona
