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

#if defined(SRT_API)
namespace Mona {

struct SRTSession : Session, virtual Object {
	SRTSession(SRTProtocol& protocol, const shared<Socket>& pSocket, shared<Peer>& pPeer);
	~SRTSession();

	/*!
	Start the session by calling Peer.onConnection()
	\return true if the connection is accepted, false otherwise */
	void	init(SRTProtocol::Params& params);

	void	kill(Int32 error = 0, const char* reason = NULL);

	UInt64	queueing() const { return _pSocket->queueing(); }
	void	flush() { if (_pWriter) _pWriter->flush(); }

private:
	struct SRTWriter : Writer, virtual Object {
		SRTWriter(SRTSession& session) : _session(session) {}
		UInt64			queueing() const { return _session.queueing(); }
	private:
		SRTSession& _session;
	};

	shared<Socket>				_pSocket;
	SRTWriter					_writer;
	Subscription*				_pSubscription;
	Publication*				_pPublication;
	unique<MediaSocket::Reader>	_pReader;
	unique<MediaSocket::Writer>	_pWriter;
};

} // namespace Mona
#endif
