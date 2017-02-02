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

UInt32 WSDecoder::onStreamData(Packet& buffer, Socket& socket) {
	do {

		BinaryReader reader(buffer.data(), buffer.size());

		if(!_size) {
			if (reader.available()<2)
				return reader.size();

			_type = reader.read8() & 0x0F;
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
			
			DUMP_REQUEST(socket.isSecure() ? "WSS" : "WS", reader.data(), reader.current()-reader.data(), socket.peerAddress());
		}

		if (reader.shrink(_size)<_size)
			return reader.available();

		_size = 0;

		if (_masked)
			WS::Unmask(reader);

		Packet packet(buffer, reader.current(), reader.available());
		DUMP_REQUEST(socket.isSecure() ? "WSS" : "WS", packet.data(), packet.size(), socket.peerAddress());
		buffer += reader.size();
		_handler.queue(onRequest, _type, packet, !buffer);

	} while (buffer);

	return 0;
}

} // namespace Mona
