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

#include "Mona/RTMP/RTMPWriter.h"
#include "Mona/RTMP/RTMPSession.h"
#include "Mona/Logs.h"

using namespace std;


namespace Mona {

RTMPWriter::RTMPWriter(UInt32 channelId, TCPClient& client, const shared<RC4_KEY>& pEncryptKey) : streamId(0),
	_pChannel(SET, channelId), _pEncryptKey(pEncryptKey), _client(client) {
}

void RTMPWriter::writeProtocolSettings() {
	// to eliminate chunks of packet in the server->client direction
	write(AMF::TYPE_CHUNKSIZE)->write32(0x7FFFFFFF);
	// to increase the window ack size in the server->client direction
	writeWinAckSize(2500000);
	// to increase the window ack size in the client->server direction
	write(AMF::TYPE_BANDWIDTH)->write32(2500000).write8(0); // 0 = hard setting
	// Stream Begin - ID 0 (see specification)
	write(AMF::TYPE_RAW)->write16(0).write32(0);
}

void RTMPWriter::flushing() {
	for (auto& pSender : _senders)
		_client.send(pSender);
	_senders.clear();
}

AMFWriter& RTMPWriter::write(AMF::Type type, UInt32 time, Media::Data::Type packetType, const Packet& packet, bool reliable) {
	if(closed())
        return AMFWriter::Null();
	_senders.emplace_back(SET, type, time, streamId, _pChannel, _client.socket(), _pEncryptKey, packetType, packet);
	return _senders.back()->writer;
}



} // namespace Mona
