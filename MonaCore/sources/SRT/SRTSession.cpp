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
#include "Mona/TSReader.h"
#include "Mona/TSWriter.h"

using namespace std;

#if defined(SRT_API)

namespace Mona {

SRTSession::SRTSession(SRTProtocol& protocol, const shared<Socket>& pSocket) :
	Session(protocol, pSocket->peerAddress()), _pSocket(pSocket), _pSubscription(NULL), _pPublication(NULL), _writer(self) {
	
}

void SRTSession::init(SRTProtocol::Params& params) {
	peer.setPath(params.path());
	// Connect the Peer
	Exception ex;
	peer.onConnection(ex, _writer, *_pSocket, params());
	if (ex)
		return kill(TO_ERROR(ex));

	// Start Play / Publish
	if (params.subscribe()) {
		_pWriter.set(MediaStream::TYPE_SRT, params.path(), new TSWriter(), _pSocket, api.ioSocket);
		if (!api.subscribe(ex, peer, params.stream(), *(_pSubscription = new Subscription(*_pWriter)))) {
			delete _pSubscription;
			_pSubscription = NULL;
			return kill(TO_ERROR(ex));
		}
		_pWriter->onStop = [&]() {
			// Unsubscribe before killing the session
			if (_pSubscription)
				api.unsubscribe(peer, *_pSubscription); // /!\ do not delete the subscription until the writer exists
			kill(TO_ERROR(_pWriter->ex));
		};
		_pWriter->start();
	}
	if (params.publish()) {
		if ((_pPublication = api.publish(ex, peer, params.stream())) == NULL)
			return kill(TO_ERROR(ex));
		_pReader.set(MediaStream::TYPE_SRT, params.path(), *_pPublication, new TSReader(), _pSocket, api.ioSocket).onStop = [&]() {
			// Unpublish before killing the session
			if (_pPublication) {
				api.unpublish(*_pPublication, peer);
				_pPublication = NULL;
			}
			kill(TO_ERROR(_pReader->ex));
		};
		_pReader->start();
	}
}

void SRTSession::kill(Int32 error, const char* reason) {
	// Stop the current  job
	if (_pWriter) {
		_pWriter->onStop = nullptr;
		_pWriter.reset(); // call stop
	}
	if (_pReader) {
		_pReader->onStop = nullptr;
		_pReader.reset(); // call stop
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
	Session::kill(error, reason);
}

} // namespace Mona

#endif
