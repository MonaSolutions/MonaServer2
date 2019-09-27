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
#include "Mona/Session.h"
#include "Mona/MediaSocket.h"

#if defined(SRT_API)
namespace Mona {

struct SRTSession : Session, virtual Object {
	SRTSession(Protocol& protocol, const shared<Socket>& pSocket);
	~SRTSession() { disengage(); }

	/*!
	Start the session by calling Peer.onConnection()
	\return true if the connection is accepted, false otherwise */
	bool	init();

	void	subscribe(Subscription* pSubscription, Publication* pPublication, unique<MediaSocket::Reader>&& pReader, unique<MediaSocket::Writer>&& pWriter);
	void	disengage();

	UInt64	queueing() const { return _pSocket->queueing(); }
	void	flush() { if (_pWriter) _pWriter->flush(); }

private:
	struct SRTWriter : Writer, virtual Object {
		SRTWriter(SRTSession& session) : _session(session) {}

		virtual UInt64			queueing() const { return _session.queueing(); }
	private:
		virtual void			closing(Int32 code, const char* reason = NULL) {}

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
