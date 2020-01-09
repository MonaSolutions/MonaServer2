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

SRTSession::SRTSession(SRTProtocol& protocol, const shared<Socket>& pSocket, shared<Peer>& pPeer) :
	SocketSession(protocol, pSocket, pPeer), _pSubscription(NULL), _pPublication(NULL), _writer(*pSocket) {
	
}

SRTSession::~SRTSession() {
	if (_pSubscription)
		delete _pSubscription;
}

void SRTSession::init(SRTProtocol::Params& params) {
	// Connect the Peer
	Exception ex;
	peer.onConnection(ex, _writer, *socket(), params());
	if (ex)
		return kill(TO_ERROR(ex));

	// Start Play / Publish
	if (params.publish()) {
		if (params.subscribe()) // Note: Bidirectional is not supported for now as a socket cannot be subcribed twice
			WARN(name(), " unsupport Bidirectional mode, just publication is processing");
		if ((_pPublication = api.publish(ex, peer, params.stream())) == NULL)
			return kill(TO_ERROR(ex));
		_pReader.set(MediaStream::TYPE_SRT, params.stream().c_str(), new TSReader(), *_pPublication, socket(), api.ioSocket);
		_pReader->onStop = [this]() { kill(TO_ERROR(_pReader->ex)); };
		_pReader->start(); // no pulse required, socket already ready
	} else { // by default "subscribe"!
		_pWriter.set(MediaStream::TYPE_SRT, params.stream().c_str(), new TSWriter(), socket(), api.ioSocket);
		if (!api.subscribe(ex, peer, params.stream(), *(_pSubscription = new Subscription(*_pWriter))))
			return kill(TO_ERROR(ex));
		_pWriter->onStop = [this]() { kill(TO_ERROR(_pWriter->ex)); };
		_pWriter->start(); // no pulse required, socket already ready
	}	
}

void SRTSession::kill(Int32 error, const char* reason) {
	if (died)
		return;
	// Stop the current  job
	if (_pWriter) {
		_pWriter->onStop = nullptr;
		_pWriter.reset(); // release resource + call stop
	}
	if (_pReader) {
		_pReader->onStop = nullptr;
		_pReader.reset(); // release resource + call stop
	}
	if (_pPublication)
		api.unpublish(*_pPublication, peer);
	if (_pSubscription) // delete subscription on SRTSession destruction to avoid a crash if this kill is called by subscription itself!
		api.unsubscribe(peer, *_pSubscription);
	Session::kill(error, reason);
}

bool SRTSession::manage() {
	if (!Session::manage())
		return false;
	// check subscription
	if (_pSubscription) {
		switch (_pSubscription->ejected()) {
			case Subscription::EJECTED_NONE:
				break;
			case Subscription::EJECTED_BANDWITDH:
				kill(ERROR_CONGESTED, String("Insufficient bandwidth to play ", _pSubscription->name()).c_str());
				return false;
			case Subscription::EJECTED_ERROR: //  HTTPWriter error, error already written!
				kill(ERROR_SOCKET);
				return false;
		}
	}
	return true;
}


} // namespace Mona

#endif
