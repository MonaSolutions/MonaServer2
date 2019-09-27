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
#include "Mona/SRT/SRTSession.h"

using namespace std;

#if defined(SRT_API)

namespace Mona {

SRTSession::SRTSession(Protocol& protocol, const shared<Socket>& pSocket) :
	Session(protocol, pSocket->peerAddress()), _pSocket(pSocket), _pSubscription(NULL), _pPublication(NULL), _writer(*this) {
	
}

bool SRTSession::init() {
	// Initialize the Peer
	Exception ex;
	peer.onConnection(ex, _writer, *_pSocket, DataReader::Null());
	if (ex) 
		kill(ToError(ex), ex.c_str());
	return !ex;
}

void SRTSession::subscribe(Subscription* pSubscription, Publication* pPublication, unique<MediaSocket::Reader>&& pReader, unique<MediaSocket::Writer>&& pWriter) {
	_pSubscription = pSubscription;
	_pPublication = pPublication;
	if (pReader) {
		_pReader = move(pReader);
		_pReader->onStop = [&]() {
			// Unpublish before killing the session
			if (_pPublication) {
				api.unpublish(*_pPublication, peer);
				_pPublication = NULL;
			}
			kill(_pReader->ex ? ToError(_pReader->ex) : 0, _pReader->ex.c_str());
		};
		_pReader->start();
	}
	if (pWriter) {
		_pWriter = move(pWriter);
		_pWriter->onStop = [&]() {
			// Unsubscribe before killing the session
			if (_pSubscription)
				api.unsubscribe(peer, *_pSubscription); // /!\ do not delete the subscription until the writer exists
			kill(_pWriter->ex ? ToError(_pWriter->ex) : 0, _pWriter->ex.c_str());
		};
		_pWriter->start();
	}
}

void SRTSession::disengage() {
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

} // namespace Mona

#endif
