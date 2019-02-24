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
#include"Mona/Socket.h"
#include"Mona/Handler.h"
#include "Mona/StreamData.h"

namespace Mona {

struct RTMPDecoder : Socket::Decoder, private StreamData<Socket&>, virtual Object {
	typedef Event<void(unique<RC4_KEY>&)> ON(EncryptKey);
	typedef Event<void(RTMP::Request&)> ON(Request);

	RTMPDecoder(const Handler& handler);

private:
	struct Channel : RTMP::Channel, virtual Object {
		Channel(UInt32 id) : RTMP::Channel(id) {}
		void clear() {
			RTMP::Channel::reset();
			_pBuffer.reset();
		}
		void reset() { _pBuffer.set(); }

		Buffer*	operator->() { return _pBuffer.get(); }
		Buffer&	operator*() { return *_pBuffer; }
		shared<Buffer>& operator()() { return _pBuffer; }
	private:
		shared<Buffer> _pBuffer;
	};

	enum Handshake {
		HANDSHAKE_FIRST,
		HANDSHAKE_SECOND,
		HANDSHAKE_DONE
	};
	void	decode(shared<Buffer>& pBuffer, const SocketAddress& address, const shared<Socket>& pSocket);
	UInt32	onStreamData(Packet& buffer, Socket& socket);

	const Handler&				_handler;
	UInt32						_decoded;
	Handshake					_handshake;
	std::map<UInt32, Channel>	_channels;
	UInt32						_chunkSize;
	UInt32						_winAckSize;
	UInt32						_unackBytes;
	unique<RC4_KEY>				_pDecryptKey;
#if defined(_DEBUG)
	UInt32						_readBytes;
#endif
};

} // namespace Mona

