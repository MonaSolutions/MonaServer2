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

#include "Mona/RTMP/RTMPSender.h"
#include "Mona/Session.h"

using namespace std;

namespace Mona {

RTMPSender::RTMPSender(AMF::Type type, UInt32 time, UInt32	streamId,
					   const shared<RTMP::Channel>& pChannel,
					   const shared<Socket>& pSocket,
					   const shared<RC4_KEY>& pEncryptKey,
					   Media::Data::Type packetType,
					   const Packet& packet) : Runner("RTMPSender"),  _type(type), _time(time), _streamId(streamId), _packetType(packetType), _packet(move(packet)),
							_pChannel(pChannel), _pSocket(pSocket), _pEncryptKey(pEncryptKey),
							writer(_pBuffer.set(18)) {  // 18 => maximum header size!
}

bool RTMPSender::run(Exception&) {
	_packetType = writer.convert(_packetType, _packet);
	
	UInt32 absoluteTime(_time);
	UInt8 headerFlag(0);
	RTMP::Channel& channel(*_pChannel);
	UInt32 bodySize(writer->size() + _packet.size());

	if (channel.id == 2)
		_streamId = 0; // channel 2 sends always message to streamId 0 (see specification)

	// Default = Chunk Message Type 0 : full header
	if (channel.streamId == _streamId) {
		if (_time >= channel.absoluteTime) {
			++headerFlag; // Chunk Message Type 1 : don't repeat the stream id
			_time -= channel.absoluteTime; // relative time!
			if (channel.type == _type && channel.bodySize == bodySize) {
				++headerFlag; // Chunk Message Type 2 : just timestamp delta
				if (channel.time == _time)
					++headerFlag; // Chunk Message Type 3 : no header, but can have extended time!
			}
		}  // else time<_channel.absoluteTime => header must be full, because can't be relative!
	}

	channel.streamId = _streamId;
	channel.absoluteTime = absoluteTime;
	channel.time = _time;
	channel.type = _type;
	channel.bodySize = bodySize;

	UInt8 headerSize(12 - 4 * headerFlag);
	if (!headerSize)
		headerSize = 1;
	if (_time >= 0xFFFFFF)
		headerSize += 4;
	if (channel.id>319)
		headerSize += 2;
	else if (channel.id > 63)
		++headerSize;

	_pBuffer->clip(18 - headerSize); // += offset!
	BinaryWriter writer(_pBuffer->data(), headerSize);

	if (channel.id>319) {
		writer.write8((headerFlag << 6) | 1);
		writer.write8((channel.id - 64) & 0x00FF);
		writer.write8((channel.id - 64) & 0xFF00);
	} else if (channel.id > 63) {
		writer.write8((headerFlag << 6) | 0);
		writer.write8(channel.id - 64);
	} else
		writer.write8((headerFlag << 6) | channel.id);

	if (headerFlag<3) {
		writer.write24(_time<0xFFFFFF ? _time : 0xFFFFFF);
		if (headerFlag<2) {
			writer.write24(bodySize);
			writer.write8(_type);
			if (!headerFlag) {
				writer.write8(_streamId);
				writer.write8(_streamId >> 8);
				writer.write8(_streamId >> 16);
				writer.write8(_streamId >> 24);
			}
		}
	}
	if (_time >= 0xFFFFFF)
		writer.write32(absoluteTime); // must be absolute here

	if (_pEncryptKey) {
		DUMP_RESPONSE("RTMPE", _pBuffer->data(), _pBuffer->size(), _pSocket->peerAddress());
		RC4(_pEncryptKey.get(), _pBuffer->size(), _pBuffer->data(), _pBuffer->data());
	} else
		DUMP_RESPONSE(_pSocket->isSecure() ? "RTMPS" : "RTMP", _pBuffer->data(), _pBuffer->size(), _pSocket->peerAddress());

	Exception ex;
	int result = _pSocket->write(ex, Packet(_pBuffer));
	if (ex || result<0)
		DEBUG(ex);
	if (result < 0)
		return true;

	if (_packet.size()) {
		if (_pEncryptKey) {
			DUMP_RESPONSE("RTMPE", _packet.data(), _packet.size(), _pSocket->peerAddress());
			_pBuffer.set(_packet.size());
			RC4(_pEncryptKey.get(), _packet.size(), _packet.data(), _pBuffer->data());
			_packet.set(_pBuffer);
		} else
			DUMP_RESPONSE(_pSocket->isSecure() ? "RTMPS" : "RTMP", _packet.data(), _packet.size(), _pSocket->peerAddress());
		if(_pSocket->write(ex, _packet)<0 && !ex)
			DEBUG(ex);
	}
	
	if (ex)
		DEBUG(ex);

	return true;
}


} // namespace Mona
