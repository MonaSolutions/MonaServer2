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


using namespace std;

namespace Mona {


unique<MediaServer> MediaServer::New(Type type, const Path& path, const char* subMime, const SocketAddress& address, IOSocket& io, const shared<TLS>& pTLS) {
	unique<MediaWriter> pWriter(MediaWriter::New(subMime));
	return pWriter ? make_unique<MediaServer>(type, path, move(pWriter), address, io, pTLS) : nullptr;
}

MediaServer::MediaServer(Type type, const Path& path, unique<MediaWriter>&& pWriter, const SocketAddress& address, IOSocket& io, const shared<TLS>& pTLS) :
	Media::Stream(Media::Stream::Type(type), path), io(io), _pTLS(pTLS), address(address), _format(pWriter->format()), _subMime(pWriter->subMime()) {
	_onError = [this](const Exception& ex) { Stream::stop(LOG_ERROR, ex); };
	_onConnnection = [this](const shared<Socket>& pSocket) {
		newStreamTarget<MediaSocket::Writer>(this->type, this->path, MediaWriter::New(_subMime), pSocket, this->io);
	};
}

void MediaServer::starting(const Parameters& parameters) {
	if (_pSocket)
		return;
	Exception ex;
	// can subscribe after bind + listen for server, no risk to miss an event
	_pSocket = newSocket(parameters, _pTLS);
	INFO(description(), " starts");
	bool success;
	AUTO_ERROR(success = (_pSocket->bind(ex, address) && _pSocket->listen(ex) && io.subscribe(ex, _pSocket, _onConnnection, _onError)), description());
	if (!success)
		Stream::stop(ex);
}

void MediaServer::stopping() {
	io.unsubscribe(_pSocket);
	INFO(description(), " stops");
}



} // namespace Mona
