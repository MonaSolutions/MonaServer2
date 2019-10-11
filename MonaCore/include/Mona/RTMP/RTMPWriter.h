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
#include "Mona/FlashWriter.h"
#include "Mona/TCPClient.h"
#include "Mona/RTMP/RTMPSender.h"

namespace Mona {


struct RTMPWriter : FlashWriter, virtual Object {
	RTMPWriter(UInt32 channelId, TCPClient& client, const shared<RC4_KEY>& pEncryptKey);

	UInt64			queueing() const { return _client->queueing(); }

	UInt32			streamId;

	void			clear() { _senders.clear(); FlashWriter::clear(); }

	void			writePing(const Time& connectionTime) { write(AMF::TYPE_RAW)->write16(0x0006).write32(range<UInt32>(connectionTime.elapsed())); }

	void			writeAck(UInt32 count) { write(AMF::TYPE_ACK)->write32(count); }
	void			writeWinAckSize(UInt32 value) { write(AMF::TYPE_WIN_ACKSIZE)->write32(value); }
	void			writeProtocolSettings();

private:
	AMFWriter&		write(AMF::Type type, UInt32 time=0, Media::Data::Type packetType = Media::Data::TYPE_AMF, const Packet& packet=Packet::Null(), bool reliable = true);
	void			flushing();
	
	shared<RTMP::Channel>			_pChannel; // Output channel
	TCPClient&						_client;
	const shared<RC4_KEY>&			_pEncryptKey;

	std::deque<shared<RTMPSender>>	_senders;
};



} // namespace Mona
