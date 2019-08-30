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
#include "Mona/MapReader.h"

#if defined(SRT_API)
namespace Mona {

struct SRTSession : Session, virtual Object {
	SRTSession(Protocol& protocol, const shared<Socket>& pSocket) : Session(protocol, pSocket->peerAddress()), _pSocket(pSocket), _pSubscription(NULL), _pPublication(NULL), _writer(*this) {
		// Initialize the Peer
		std::map<std::string, std::string> parameters;
		MapReader<std::map<std::string, std::string>> reader(parameters);
		Exception ex;
		peer.onConnection(ex, _writer, *pSocket, reader);
	}

	virtual ~SRTSession() { disengage(); }

	void init(Subscription* pSubscription, Publication* pPublication, shared<MediaSocket::Reader>& pReader, shared<MediaSocket::Writer>& pWriter) {
		_pSubscription = pSubscription;
		_pPublication = pPublication; 
		if (_pReader = pReader) {
			_pReader->onStop = [&]() {
				// Unpublish before killing the session
				if (_pPublication) {
					api.unpublish(*_pPublication, peer);
					_pPublication = NULL;
				}
				kill(_pReader->ex? ToError(_pReader->ex) : 0, _pReader->ex.c_str());
			};
			_pReader->start();
		}
		if (_pWriter = pWriter) {
			_pWriter->onStop = [&]() {
				// Unsubscribe before killing the session
				if (_pSubscription)
					api.unsubscribe(peer, *_pSubscription); // /!\ do not delete the subscription until the writer exists
				kill(_pWriter->ex ? ToError(_pWriter->ex) : 0, _pWriter->ex.c_str());
			};
			_pWriter->start();
		}
	}

	void disengage() {
		// Stop the current  job
		if (_pWriter) {
			_pWriter->onStop = nullptr;
			_pWriter->stop();
			_pWriter.reset();
		}
		if (_pReader) {
			_pReader->onStop = nullptr;
			_pReader->stop();
			_pReader.reset();
		}

		if (_pPublication) {
			api.unpublish(*_pPublication, peer);
			_pPublication = NULL;
		}
		if (_pSubscription) {
			api.unsubscribe(peer, *_pSubscription);
			delete _pSubscription;
			_pSubscription = NULL;
		}
	}

	UInt64	queueing() const { return _pSocket->queueing(); }
	
	virtual void flush() {
		if (_pWriter)
			_pWriter->flush();
	}

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
	shared<MediaSocket::Reader>	_pReader;
	shared<MediaSocket::Writer>	_pWriter;
};

} // namespace Mona
#endif
