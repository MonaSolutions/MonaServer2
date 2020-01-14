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

unique<MediaServer::Reader> MediaServer::Reader::New(Exception& ex, MediaStream::Type type, const char* request, Media::Source& source, const SocketAddress& address, IOSocket& io, const shared<TLS>& pTLS, string&& format) {
	if (type < TYPE_TCP || type >= TYPE_UDP) {
		ex.set<Ex::Format>(TypeToString(type), " stream has no server ability");
		return nullptr;
	}
	Path path(move(format));
	const char* subMime = MediaStream::Format(ex, type, request, path);
	if (!subMime) {
		if (type != MediaStream::TYPE_HTTP || !ex.cast<Ex::Format>())
			return nullptr;
		// Format can be empty with HTTP source
		ex = nullptr;
		return make_unique<MediaServer::Reader>(MediaServer::Type(type), request, unique<MediaReader>(), source, address, io, pTLS);
	}
	unique<MediaReader> pReader = MediaReader::New(subMime);
	if (!pReader)
		ex.set<Ex::Unsupported>(TypeToString(type), " stream with unsupported format ", subMime);
	return make_unique<MediaServer::Reader>(MediaServer::Type(type), request, move(pReader), source, address, io, pTLS);
}

MediaServer::Reader::Reader(MediaServer::Type type, const char* request, unique<MediaReader>&& pReader, Media::Source& source, const SocketAddress& address, IOSocket& io, const shared<TLS>& pTLS) :
	request(request), io(io), _pTLS(pTLS), address(address), _subMime(pReader ? pReader->subMime() : NULL),
	MediaStream(MediaStream::Type(type), source, "Stream server source ", TypeToString(MediaStream::Type(type)), "://", address, *request ? "/" : "", request, '|', String::Upper(pReader ? pReader->format() : "AUTO")) {
	_onConnnection = [this](const shared<Socket>& pSocket) {
		if (!_pTarget) {
			_pTarget.set<MediaSocket::Reader>(this->type, this->request.c_str(), MediaReader::New(_subMime), this->source, pSocket, this->io);
			_pTarget->onStop = [this]() { _pTarget.reset(); };
			_pTarget->start(params()); // give same parameters than parent!
			return;
		}
		WARN(description, " connection from ", pSocket->peerAddress(), " rejected");
	};
	pReader.reset(); // pReader to match with MediaSocket::Reader signature and guarantee a correct subMime
}

bool MediaServer::Reader::starting(const Parameters& parameters) {
	// can subscribe after bind + listen for server, no risk to miss an event
	if (initSocket(_pSocket, parameters, _pTLS)) {
		bool success;
		Exception ex;
		AUTO_ERROR(success = _pSocket->bind(ex, address) && _pSocket->listen(ex) &&
			io.subscribe(ex = nullptr, _pSocket, _onConnnection, _onError), description);
		if (success)
			return run();
	}
	stop();
	return false;
}

void MediaServer::Reader::stopping() {
	io.unsubscribe(_pSocket);
}




unique<MediaServer::Writer> MediaServer::Writer::New(Exception& ex, MediaStream::Type type, const char* request, const SocketAddress& address, IOSocket& io, const shared<TLS>& pTLS, string&& format) {
	if (type < TYPE_TCP || type >= TYPE_UDP) {
		ex.set<Ex::Format>(TypeToString(type), " stream has no server ability");
		return nullptr;
	}
	Path path(move(format));
	const char* subMime = MediaStream::Format(ex, type, request, path);
	if (!subMime)
		return nullptr;
	unique<MediaWriter> pWriter = MediaWriter::New(subMime);
	if (pWriter)
		return make_unique<MediaServer::Writer>(MediaServer::Type(type), request, move(pWriter), address, io, pTLS);
	ex.set<Ex::Unsupported>(TypeToString(type), " stream with unsupported format ", subMime);
	return nullptr;
}

MediaServer::Writer::Writer(MediaServer::Type type, const char* request, unique<MediaWriter>&& pWriter, const SocketAddress& address, IOSocket& io, const shared<TLS>& pTLS) :
	request(request), io(io), _pTLS(pTLS), address(address), _subMime(pWriter->subMime()),
	MediaStream(MediaStream::Type(type), "Stream server target ", TypeToString(MediaStream::Type(type)), "://", address, *request ? "/" : "", request, '|', String::Upper(pWriter->format())) {
	_onError = [this](const Exception& ex) { stop(LOG_ERROR, ex); };
	_onConnnection = [this](const shared<Socket>& pSocket) {
		MediaSocket::Writer* pTarget = addTarget<MediaSocket::Writer>(this->type, this->request.c_str(), MediaWriter::New(_subMime), pSocket, this->io);
		if (!pTarget)
			stop<Ex::Intern>(LOG_ERROR, "impossibe to add target ", pSocket->peerAddress());
		// Don't overrides onStop => close socket => remove target!
	};
	pWriter.reset(); // pWriter to match with MediaSocket::Writer signature and guarantee a correct subMime
}

bool MediaServer::Writer::starting(const Parameters& parameters) {
	// can subscribe after bind + listen for server, no risk to miss an event
	if(initSocket(_pSocket, parameters, _pTLS)) {
		bool success;
		AUTO_ERROR(success = (_pSocket->bind(ex, address) && _pSocket->listen(ex) && io.subscribe(ex, _pSocket, _onConnnection, _onError)), description);
		if (success)
			return run();
	}
	stop();
	return false;
}

void MediaServer::Writer::stopping() {
	io.unsubscribe(_pSocket);
}






} // namespace Mona
