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

#include "Mona/MediaServer.h"
#include "Mona/MediaSocket.h"


using namespace std;

namespace Mona {


unique<MediaServer> MediaServer::New(Type type, const Path& path, const char* subMime, const SocketAddress& address, IOSocket& io, const shared<TLS>& pTLS) {
	unique<MediaWriter> pWriter(MediaWriter::New(subMime));
	return pWriter ? make_unique<MediaServer>(type, path, move(pWriter), address, io, pTLS) : nullptr;
}

const shared<Socket>& MediaServer::socket() {
	if (!_pSocket) {
#if defined(SRT_API)
		if (type==Media::Stream::TYPE_SRT)
			_pSocket.set<SRT::Socket>();
		else
#endif
		if (_pTLS)
			_pSocket.set<TLS::Socket>(Socket::TYPE_STREAM, _pTLS);
		else
			_pSocket.set(Socket::TYPE_STREAM);
	}
	return _pSocket;
}

MediaServer::MediaServer(Type type, const Path& path, unique<MediaWriter>&& pWriter, const SocketAddress& address, IOSocket& io, const shared<TLS>& pTLS) :
	Media::Stream(Media::Stream::Type(type), path), io(io), _pTLS(pTLS), address(address), _subMime(pWriter->subMime()), _running(false) {
	_onError = [this](const Exception& ex) { Stream::stop(LOG_ERROR, ex); };
	_onConnnection = [this](const shared<Socket>& pSocket) {
		shared<MediaSocket::Writer> pStream(SET, this->type, this->path, MediaWriter::New(_subMime), pSocket, this->io);
		pStream->onStop = [this, pStream](const Exception& ex) {
			// MediaSocket::Writer stop = pSocket->reset = disconnection!
			removeTarget(*pStream);
			_streams.erase(pStream);
		};
		if(addTarget(*pStream)) // before start to allow a call to start with parameters
			pStream->start(); // necessary MediaServer is running here!
	};
}

void MediaServer::starting(const Parameters& parameters) {
	if (_running)
		return;
	Exception ex;
	// can subscribe after bind + listen for server, no risk to miss an event
	AUTO_ERROR(_running = (socket()->bind(ex, address) && _pSocket->listen(ex) && io.subscribe(ex, _pSocket, _onConnnection, _onError)), description());
	if (!_running) {
		_pSocket.reset();
		return Stream::stop(ex);
	}
	INFO(description(), " starts");
}

void MediaServer::stopping() {
	_running = false;
	io.unsubscribe(_pSocket);
	_streams.clear(); // stop all children stream!
	INFO(description(), " stops");
}



} // namespace Mona
