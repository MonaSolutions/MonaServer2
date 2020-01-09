/*
This file is a part of MonaSolutions Copyright 2017
mathieu.poux[a]gmail.com
jammetthomas[a]gmail.com

This program is free software: you can redistribute it and/or
modify it under the terms of the the Mozilla Public License v2.0.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
Mozilla Public License v. 2.0 received along this program for more
details (or else see http://mozilla.org/MPL/2.0/).

*/


#include "Mona/STUN/STUNProtocol.h"
#include "Mona/BinaryWriter.h"
#include "Mona/Logs.h"


using namespace std;


namespace Mona {

Socket::Decoder* STUNProtocol::newDecoder() {
	struct Decoder : Socket::Decoder, virtual Object {
	private:
		void decode(shared<Buffer>& pBuffer, const SocketAddress& address, const shared<Socket>& pSocket) {
			Packet packet(pBuffer); // capture pBuffer to not call onReception event (see Socket::decode comments)
			DUMP_REQUEST("STUN", packet.data(), packet.size(), address);

			BinaryReader reader(packet.data(), packet.size());
			UInt16 type = reader.read16();
			reader.next(2); // size
			switch (type) {
				case 0x01: { // Binding Request
					if (reader.available() != 16) {
						ERROR("Unexpected size ", reader.size(), " received in Binding Request from ", address);
						return;
					}
					string transactionId;
					reader.read(16, transactionId); // cookie & transaction ID
					DEBUG("STUN ", address, " address resolution ", transactionId);

					shared<Buffer> pBufferOut(SET);
					BinaryWriter writer(*pBufferOut);
					writer.write16(0x0101); // Binding Success response
					writer.write16(address.host().size() + 8); // size of the message
					writer.write(transactionId);

					writer.write16(0x0020); // XOR Mapped Address
					writer.write16(address.host().size() + 4); // size
					writer.write8(0); // reserved
					writer.write8(address.family() == IPAddress::IPv4 ? 1 : 2); // @ family
					writer.write16(address.port() ^ ((transactionId[0] << 8) + transactionId[1])); // port XOR 1st bytes of cookie

					const void* pAdd = address.host().data();
					for (UInt8 i = 0; i < address.host().size(); ++i)
						writer.write8((*((UInt8*)pAdd + i)) ^ transactionId[i]);

					Exception ex;
					DUMP_RESPONSE("STUN", pBufferOut->data(), pBufferOut->size(), address);
					AUTO_WARN(pSocket->write(ex, Packet(pBufferOut), address), "STUN response");
					break;
				}
				default:
					WARN("Unknown message type ", type, " from ", address, " (size=", packet.size(), ")");
					break;
			}
		}
	};
	return new Decoder();
}

} // namespace Mona
