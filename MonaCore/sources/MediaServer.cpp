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

unique<MediaServer::Reader> MediaServer::Reader::New(MediaServer::Type type, const Path& path, Media::Source& source, const char* subMime, const SocketAddress& address, IOSocket& io, const shared<TLS>& pTLS) {
	if (!*subMime && type == MediaServer::Type::TYPE_HTTP) // Format can be empty with HTTP source
		return make_unique<MediaServer::Reader>(type, path, source, unique<MediaReader>(), address, io, pTLS);
	unique<MediaReader> pReader(MediaReader::New(subMime));
	return pReader ? make_unique<MediaServer::Reader>(type, path, source, move(pReader), address, io, pTLS) : nullptr;

}

MediaServer::Reader::Reader(MediaServer::Type type, const Path& path, Media::Source& source, unique<MediaReader>&& pReader, const SocketAddress& address, IOSocket& io, const shared<TLS>& pTLS) :
	MediaStream(MediaStream::Type(type), path, source), _streaming(false), io(io), _pTLS(pTLS), address(address), _pReader(move(pReader)) {
	_onConnnection = [this](const shared<Socket>& pSocket) {
		if (!_pTarget) {
			_pTarget.set<MediaSocket::Reader>(this->type, this->path, this->source, MediaReader::New(_pReader->subMime()), pSocket, this->io);
			_pTarget->onStop = [this]() { _pTarget.reset(); };
			_pTarget->start();
			return;
		}
		WARN("Connection from ", pSocket->peerAddress(), " refused on MediaServer Reader ", this->path, ", there is already a publisher");
	};
}

bool MediaServer::Reader::starting(const Parameters& parameters) {
	// can subscribe after bind + listen for server, no risk to miss an event
	_pSocket = newSocket(parameters, _pTLS);

	bool success;
	Exception ex;
	AUTO_ERROR(success = _pSocket->bind(ex, address) && _pSocket->listen(ex) && 
		io.subscribe(ex = nullptr, _pSocket, _onConnnection, _onError), description());
	if (!success) {
		_pSocket.reset();
		stop();
		return true;
	}
	return true;
}

void MediaServer::Reader::stopping() {
	io.unsubscribe(_pSocket);
}




unique<MediaServer::Writer> MediaServer::Writer::New(MediaServer::Type type, const Path& path, const char* subMime, const SocketAddress& address, IOSocket& io, const shared<TLS>& pTLS) {
	unique<MediaWriter> pWriter(MediaWriter::New(subMime));
	return pWriter ? make_unique<MediaServer::Writer>(type, path, move(pWriter), address, io, pTLS) : nullptr;
}

MediaServer::Writer::Writer(MediaServer::Type type, const Path& path, unique<MediaWriter>&& pWriter, const SocketAddress& address, IOSocket& io, const shared<TLS>& pTLS) :
	MediaStream(MediaStream::Type(type), path), io(io), _pTLS(pTLS), address(address), _format(pWriter->format()), _subMime(pWriter->subMime()) {
	_onError = [this](const Exception& ex) { stop(LOG_ERROR, ex); };
	_onConnnection = [this](const shared<Socket>& pSocket) {
		MediaSocket::Writer* pTarget = addTarget<MediaSocket::Writer>(this->type, this->path, MediaWriter::New(_subMime), pSocket, this->io);
		if (!pTarget)
			return stop<Ex::Intern>(LOG_ERROR, "Impossibe to add target ", pTarget->description());
		// Don't overrides onStop => close socket => remove target!
	};
}

bool MediaServer::Writer::starting(const Parameters& parameters) {
	// can subscribe after bind + listen for server, no risk to miss an event
	_pSocket = newSocket(parameters, _pTLS);
	bool success;
	AUTO_ERROR(success = (_pSocket->bind(ex, address) && _pSocket->listen(ex) && io.subscribe(ex, _pSocket, _onConnnection, _onError)), description());
	if (!success)
		stop();
	return true;
}

void MediaServer::Writer::stopping() {
	io.unsubscribe(_pSocket);
}






} // namespace Mona
