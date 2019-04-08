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


unique<MediaServer::Writer> MediaServer::Writer::New(MediaServer::Type type, const Path& path, const char* subMime, const SocketAddress& address, IOSocket& io, const shared<TLS>& pTLS) {
	unique<MediaWriter> pWriter(MediaWriter::New(subMime));
	return pWriter ? make_unique<MediaServer::Writer>(type, path, move(pWriter), address, io, pTLS) : nullptr;
}

MediaServer::Writer::Writer(MediaServer::Type type, const Path& path, unique<MediaWriter>&& pWriter, const SocketAddress& address, IOSocket& io, const shared<TLS>& pTLS) :
	Media::Stream(Media::Stream::Type(type), path), io(io), _pTLS(pTLS), address(address), _format(pWriter->format()), _subMime(pWriter->subMime()) {
	_onError = [this](const Exception& ex) { stop(LOG_ERROR, ex); };
	_onConnnection = [this](const shared<Socket>& pSocket) {
		// let's call beginMedia to start the stream!
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




unique<MediaServer::Reader> MediaServer::Reader::New(MediaServer::Type type, const Path& path, Media::Source& source, const char* subMime, const SocketAddress& address, IOSocket& io, const shared<TLS>& pTLS) {
	if (!*subMime && type == MediaServer::Type::TYPE_HTTP) // Format can be empty with HTTP source
		return make_unique<MediaServer::Reader>(type, path, source, unique<MediaReader>(), address, io, pTLS);
	unique<MediaReader> pReader(MediaReader::New(subMime));
	return pReader ? make_unique<MediaServer::Reader>(type, path, source, move(pReader), address, io, pTLS) : nullptr;

}

MediaServer::Reader::Reader(MediaServer::Type type, const Path& path, Media::Source& source, unique<MediaReader>&& pReader, const SocketAddress& address, IOSocket& io, const shared<TLS>& pTLS) :
	Media::Stream(Media::Stream::Type(type), path, source), _streaming(false), io(io), _pTLS(pTLS), address(address), _pReader(move(pReader)) {
	_onSocketDisconnection = [&]() { stop<Ex::Net::Socket>(LOG_DEBUG, "disconnection"); };
	_onSocketFlush = [this]() { finalizeStart(); };
	_onSocketError = [this](const Exception& ex) { stop(!_streaming ? LOG_DEBUG : LOG_WARN, ex); };
	_onConnnection = [this](const shared<Socket>& pSocket) {
		if (!_pSocketClient) {
			_pSocketClient = pSocket;
			Decoder* pDecoder(new Decoder(this->io.handler, _pReader, this->source.name(), this->type));
			pDecoder->onResponse = _onResponse = [this](HTTP::Response& response)->void { 
				if (this->type == Media::Stream::TYPE_HTTP)
					return stop<Ex::Protocol>(LOG_ERROR, "HTTP response on a server stream source (only HTTP request is expected)");
				writeMedia(response); 
			};
			pDecoder->onRequest = _onRequest = [this](HTTP::Request& request) {
				if (this->type == Media::Stream::TYPE_HTTP && request)
					return; // ignore POST header
				writeMedia(request);
			};
			bool success;
			Exception ex;
			AUTO_ERROR(success = this->io.subscribe(ex = nullptr, pSocket, pDecoder, nullptr, _onSocketFlush, _onSocketError, _onSocketDisconnection), description());
			if (!success) {
				_pSocketClient.reset();
				stop();
			}
			return;
		}
		WARN("Connection from ", pSocket->peerAddress(), " refused on MediaServer Reader ", this->path, ", there is already a publisher");
	};
}

void MediaServer::Reader::Decoder::decode(shared<Buffer>& pBuffer, const SocketAddress& address, const shared<Socket>& pSocket) {
	if (_pReader.unique()) { // Work too for HTTP (if _pReader == NULL then _pReader.unique() == false)
		pBuffer.reset();
		return;
	}
	if (_address != address) {
		if (_address)
			_pReader->flush(*this); // address has changed, means that we are in UDP, it's a new stream (come from someone else)
		_address = address;
	}
	HTTPDecoder::decode(pBuffer, address, pSocket);
}

UInt32 MediaServer::Reader::Decoder::onStreamData(Packet& buffer, const shared<Socket>& pSocket) {
	if (_type == TYPE_HTTP)
		return HTTPDecoder::onStreamData(buffer, pSocket);
	_pReader->read(buffer, *this);
	return 0;
}

void MediaServer::Reader::writeMedia(const HTTP::Message& message) {
	if (message.ex)
		return stop<Ex::Protocol>(LOG_ERROR, message.ex);
	if (!message.pMedia) {
		if (_streaming)
			return stop<Ex::Net::Socket>(LOG_DEBUG, "end of stream");
		return stop<Ex::Protocol>(LOG_ERROR, "HTTP response is not a media");
	}
	_streaming = true;
	if (message.lost)
		source.reportLost(message.pMedia->type, message.lost, message.pMedia->track);
	else if (message.pMedia->type)
		source.writeMedia(*message.pMedia);
	else if (!message.flush && !message) // !response => if header it's the first "publish" command
		return source.reset();
	if (message.flush)
		source.flush();
}

bool MediaServer::Reader::starting(const Parameters& parameters) {
	// can subscribe after bind + listen for server, no risk to miss an event
	_pSocket = newSocket(parameters, _pTLS);

	bool success;
	Exception ex;
	AUTO_ERROR(success = _pSocket->bind(ex, address) && _pSocket->listen(ex) && 
		io.subscribe(ex = nullptr, _pSocket, _onConnnection, _onSocketError), description());
	if (!success) {
		_pSocket.reset();
		stop();
		return true;
	}
	return true;
}

void MediaServer::Reader::stopping() {
	// unsubscribe client
	if (_pSocketClient) {
		io.unsubscribe(_pSocketClient);
		_onRequest = nullptr;
		_onResponse = nullptr;
		_pSocketClient.reset();
	}

	// unsubscribe server
	io.unsubscribe(_pSocket);

	if (_streaming) { // else source.reset useless was always connected!
		if (_pReader.unique()) // works also when _pReader is null (unique()==false)
			_pReader->flush(source);
		else
			source.reset(); // because the source.reset of _pReader->flush() can't be called (parallel!)
	}
	_streaming = false;
	// reset _pReader because could be used by different thread by new Socket and its decoding thread
	if (_pReader)
		_pReader = MediaReader::New(_pReader->subMime());

}



} // namespace Mona
