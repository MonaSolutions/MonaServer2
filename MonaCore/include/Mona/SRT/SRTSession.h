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

#pragma once

#include "Mona/SRT.h"
#include "Mona/MediaSocket.h"
#include "Mona/SRT/SRTProtocol.h"
#include "Mona/SocketSession.h"

#if defined(SRT_API)
namespace Mona {

struct SRTSession : SocketSession, virtual Object {
	SRTSession(SRTProtocol& protocol, const shared<Socket>& pSocket, shared<Peer>& pPeer);
	~SRTSession();
	void	init(SRTProtocol::Params& params);

private:
	bool	manage();
	void	kill(Int32 error = 0, const char* reason = NULL);

	void	flush() { if (_pWriter) _pWriter->flush(); }

	/*!
	Do a SRT Writer to allow a client:close() + get a right queueing */
	struct SRTWriter : Writer, virtual Object {
		SRTWriter(const Socket& socket) : _socket(socket) {}
		UInt64			queueing() const { return _socket.queueing(); }
	private:
		const Socket& _socket;
	};

	SRTWriter					_writer;
	Subscription*				_pSubscription;
	Publication*				_pPublication;
	unique<MediaSocket::Reader>	_pReader;
	unique<MediaSocket::Writer>	_pWriter;
};

} // namespace Mona
#endif
