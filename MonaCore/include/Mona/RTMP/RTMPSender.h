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
#include "Mona/RTMP/RTMP.h"
#include "Mona/AMFWriter.h"
#include "Mona/Socket.h"
#include "Mona/Media.h"

namespace Mona {

class RTMPSender : public Runner, public virtual Object {
	shared<Buffer> _pBuffer; // build it in first to initialize AMFWriter!
public:
	RTMPSender(AMF::Type type, UInt32 time, UInt32 streamId,
			   const shared<RTMP::Channel>& pChannel,
			   const shared<Socket>& pSocket,
			   const shared<RC4_KEY>& pEncryptKey,
			   Media::Data::Type packetType,
			   const Packet& packet);

	AMFWriter	writer;

private:
	bool run(Exception&);

	AMF::Type				_type;
	UInt32					_time;
	UInt32					_streamId;
	Media::Data::Type		_packetType;
	Packet					_packet;

	shared<RC4_KEY>			_pEncryptKey;
	shared<Socket>			_pSocket;
	shared<RTMP::Channel>	_pChannel;
};


} // namespace Mona
