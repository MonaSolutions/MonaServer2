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

#include "Mona/MediaSocket.h"
#include "Mona/Logs.h"
#include "Mona/SRT.h"

using namespace std;

namespace Mona {

bool MediaSocket::SendHTTPHeader(HTTP::Type type, const shared<Socket>& pSocket, const string& request, MIME::Type mime, const char* subMime, const char* name, const string& description) {
	shared<Buffer> pBuffer(SET);
	switch (type) {
		case HTTP::TYPE_GET:
			String::Append(*pBuffer, "GET ", request.empty() ? "/" : request, " HTTP/1.1"); break;
		case HTTP::TYPE_POST:
			String::Append(*pBuffer, "POST ", request.empty() ? "/" : request, " HTTP/1.1"); break;
		default:
			String::Append(*pBuffer, "HTTP/1.1 200 OK"); break; // by default send a response
	}
	String::Append(*pBuffer, "\r\nCache-Control: no-cache, no-store\r\nPragma: no-cache\r\nConnection: close\r\nUser-Agent: MonaServer\r\nHost: ", pSocket->peerAddress());
	if (subMime)
		MIME::Write(String::Append(*pBuffer, "\r\nContent-Type: "), mime, subMime);
	String::Append(*pBuffer, "\r\n\r\n");
	DUMP_RESPONSE(name, pBuffer->data(), pBuffer->size(), pSocket->peerAddress());
	int sent;
	Exception ex;
	AUTO_ERROR((sent = pSocket->write(ex, Packet(pBuffer))) >= 0, description);
	return (sent >= 0);
}

void MediaSocket::Reader::Decoder::decode(shared<Buffer>& pBuffer, const SocketAddress& address, const shared<Socket>& pSocket) {
	if (_pReader.unique()) { // Work too for HTTP (if _pReader == NULL then _pReader.unique() == false)
		pBuffer.reset();
		return;
	}
	if (_address != address) {
		if (_address && _pReader)
			_pReader->flush(self); // address has changed, means that we are in UDP, it's a new stream (come from someone else)
		_address = address;
	}
	HTTPDecoder::decode(pBuffer, address, pSocket);
}

UInt32 MediaSocket::Reader::Decoder::onStreamData(Packet& buffer, const shared<Socket>& pSocket) {
	if (_type == TYPE_HTTP)
		return HTTPDecoder::onStreamData(buffer, pSocket);
	_pReader->read(buffer, self);
	return 0;
}

MediaSocket::Reader::Reader(Type type, string&& request, unique<MediaReader>&& pReader, Media::Source& source, const SocketAddress& address, IOSocket& io, const shared<TLS>& pTLS) :
	request(move(request)), _streaming(false), io(io), _pTLS(pTLS), _pReader(move(pReader)), _httpAnswer(false), address((!address.host() && type != TYPE_UDP) ? IPAddress::Loopback() : address.host(), address.port()),
	MediaStream(type, source, "Stream source ", TypeToString(type), "://", address, request.empty() ? "" : "/", request, '|', String::Upper(pReader ? pReader->format() : "AUTO")) {
	_onSocketDisconnection = [this]() { stop<Ex::Net::Socket>(LOG_DEBUG, "disconnection"); };
	_onSocketFlush = [this]() { run(); };
	_onSocketError = [this](const Exception& ex) { stop(state()==STATE_STARTING ? LOG_DEBUG : LOG_WARN, ex); };
}

MediaSocket::Reader::Reader(Type type, string&& request, unique<MediaReader>&& pReader, Media::Source& source, const shared<Socket>& pSocket, IOSocket& io) :
	_pSocket(pSocket), request(move(request)), _streaming(false), io(io), _pReader(move(pReader)), _httpAnswer(true), address(pSocket->peerAddress()),
	MediaStream(type, source, "Stream source ", TypeToString(type), "://", pSocket->peerAddress(), '/', request.empty() ? "" : "/", request, '|', String::Upper(pReader ? pReader->format() : "AUTO")) {
	_onSocketDisconnection = [this]() { stop<Ex::Net::Socket>(LOG_DEBUG, "disconnection"); };
	_onSocketFlush = [this]() { run(); };
	_onSocketError = [this](const Exception& ex) { stop(state() == STATE_STARTING ? LOG_DEBUG : LOG_WARN, ex); };
	initSocket();
}

bool MediaSocket::Reader::initSocket(const Parameters& parameters) {
	MediaStream::initSocket(_pSocket, parameters, _pTLS);
	bool success;
	if (type == TYPE_UDP) { // Bind if UDP
		AUTO_ERROR(success = _pSocket->bind(ex, address), description);
		if (!success) {
			_pSocket.reset();
			stop();
			return true;
		}
	}
	Decoder* pDecoder(new Decoder(io.handler, _pReader, source.name(), type));
	pDecoder->onResponse = _onResponse = [this](HTTP::Response& response)->void {
		if(response.code != 200)
			return stop<Ex::Protocol>(LOG_ERROR, response.getString("code", "HTTP request error"));
		if (this->type == MediaStream::TYPE_HTTP && _httpAnswer)
			return stop<Ex::Protocol>(LOG_ERROR, "HTTP response on a server stream source (only HTTP request is expected)");
		writeMedia(response);
	};
	pDecoder->onRequest = _onRequest = [this](HTTP::Request& request) {
		if (type == MediaStream::TYPE_HTTP) {
			if (!_httpAnswer)
				return stop<Ex::Protocol>(LOG_ERROR, "HTTP request on a stream source (only HTTP response is expected)");
				// send HTTP Header answer!
			if (request && !SendHTTPHeader(HTTP::TYPE_UNKNOWN, _pSocket, this->request, _pReader ? _pReader->mime() : MIME::TYPE_UNKNOWN, _pReader? _pReader->subMime() : nullptr, source.name().c_str(), description))
				return stop();
		}
		writeMedia(request);
	};
	// engine subscription BEFORE connect to be able to detect connection success/failure
	AUTO_ERROR(success = io.subscribe(ex = nullptr, _pSocket, pDecoder, nullptr, _onSocketFlush, _onSocketError, _onSocketDisconnection), description);
	if (!success) {
		_pSocket.reset();
		stop();
		return true;
	}
	return success;
}

void MediaSocket::Reader::writeMedia(const HTTP::Message& message) {
	if (message.ex)
		return stop<Ex::Protocol>(LOG_ERROR, message.ex);
	if (!message.pMedia) {
		if (_streaming)
			return stop<Ex::Net::Socket>(LOG_DEBUG, "end of stream");
		return stop<Ex::Protocol>(LOG_ERROR, "HTTP packet is not a media");
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

bool MediaSocket::Reader::starting(const Parameters& parameters) {
	_pReader->setParams(parameters);
	if (!_pSocket && !initSocket(parameters)) {
		stop();
		return false;
	}
	if (type == TYPE_UDP)
		return run();

	// Pulse connect if TCP/SRT
	if (!_pSocket->connect(ex = nullptr, address)) { // not use AUTO_ERROR because on WOULD_BLOCK it will display a wrong WARN
		stop(LOG_ERROR, ex);
		return false;
	}
	if (ex && ex.cast<Ex::Net::Socket>().code != NET_EWOULDBLOCK)
		WARN(description, ", ", ex);

	if (state()==STATE_STOPPED && type == TYPE_HTTP && !_httpAnswer) { // HTTP + first time!
		// send HTTP Header request!
		if (!SendHTTPHeader(HTTP::TYPE_GET, _pSocket, request, MIME::TYPE_UNKNOWN, nullptr, source.name().c_str(), description)) {
			stop();
			return false;
		}
	}
	return true; // wait onSocketFlush before to call run
}

void MediaSocket::Reader::stopping() {
	if(_pSocket)
		io.unsubscribe(_pSocket);
	_onRequest = nullptr;
	_onResponse = nullptr;
	if (_streaming) { // else source.reset useless was always connected!
		if (_pReader.unique()) // works also when _pReader is null (unique()==false)
			_pReader->flush(source);
		else
			source.reset(); // because the source.reset of _pReader->flush() can't be called (parallel!)
	}
	_streaming = false;
	// reset _pReader because could be used by different thread by new Socket and its decoding thread
	if(_pReader)
		_pReader = MediaReader::New(_pReader->subMime());
}


MediaSocket::Writer::Send::Send(Type type, const shared<string>& pName, const shared<Socket>& pSocket, const shared<MediaWriter>& pWriter) : Runner("MediaSocketSend"),
	_pSocket(pSocket), pWriter(pWriter), _pName(pName),
	onWrite([this, type](const Packet& packet) {
		UInt32 size = 0;
		Packet chunk(packet);
		while (chunk += size) {
			size = (type == TYPE_UDP || type == TYPE_SRT) && chunk.size() > Net::MTU_RELIABLE_SIZE ? Net::MTU_RELIABLE_SIZE : chunk.size();
			DUMP_RESPONSE(_pName->c_str(), chunk.data(), size, _pSocket->peerAddress());
			Exception ex;
			int result = _pSocket->write(ex, Packet(chunk, chunk.data(), size));
			if (result < 0) // write has failed, no more reliable!
				_pSocket->shutdown();
		};
	}) {
}

MediaSocket::Writer::Writer(Type type, string&& request, unique<MediaWriter>&& pWriter, const SocketAddress& address, IOSocket& io, const shared<TLS>& pTLS) :
	_pTLS(pTLS), request(move(request)), io(io), _sendTrack(0), _pWriter(move(pWriter)), _httpAnswer(false), _subscribed(false), address(address.host() ? address.host() : IPAddress::Loopback(), address.port()),
	MediaStream(type, "Stream target ", TypeToString(type), "://", address, request.empty() ? "" : "/", request, '|', String::Upper(pWriter->format())) {
	_onSocketDisconnection = [this]() { stop<Ex::Net::Socket>(LOG_DEBUG, this->address, " disconnection"); };
	_onSocketError = [this](const Exception& ex) { stop(state() == STATE_STARTING ? LOG_DEBUG : LOG_WARN, ex); };
}
MediaSocket::Writer::Writer(Type type, string&& request, unique<MediaWriter>&& pWriter, const shared<Socket>& pSocket, IOSocket& io) : _pSocket(pSocket),
	io(io), request(move(request)), _sendTrack(0), _pWriter(move(pWriter)), _httpAnswer(true), _subscribed(false), address(pSocket->peerAddress()),
	MediaStream(type, "Stream target ", TypeToString(type), "://", pSocket->peerAddress(), request.empty() ? "" : "/", request, '|', String::Upper(pWriter->format())) {
	_onSocketDisconnection = [this]() { stop<Ex::Net::Socket>(LOG_DEBUG, this->address, " disconnection"); };
	_onSocketError = [this](const Exception& ex) { stop(state() == STATE_STARTING ? LOG_DEBUG : LOG_WARN, ex); };
}

bool MediaSocket::Writer::initSocket(const Parameters& parameters) {
	if (_subscribed)
		return true;
	if (!MediaStream::initSocket(_pSocket, parameters, _pTLS))
		return false;
	AUTO_ERROR(_subscribed = io.subscribe(ex, _pSocket, nullptr, nullptr, _onSocketError, _onSocketDisconnection), description);
	if (!_subscribed)
		_pSocket.reset();
	return _subscribed;
}

bool MediaSocket::Writer::starting(const Parameters& parameters) {
	if (!_pWriter) {
		stop<Ex::Intern>(LOG_ERROR, "Unknown format type to write");
		return false;
	}
	if (!initSocket(parameters)) {
		stop();
		return false;
	}
	if (!_pName)  // Do nothing if not media beginning
		return true; // wait beginMedia before to run
	// beginMedia+not writable => Pulse connect
	if (!_pSocket->connect(ex, address)) { // not use AUTO_ERROR because on WOULD_BLOCK it will display a wrong WARN
		stop();
		return false;
	}
	if (ex && ex.cast<Ex::Net::Socket>().code != NET_EWOULDBLOCK)
		WARN(description, ", ", ex);
	return true; // wait first Media before to run
}

bool MediaSocket::Writer::beginMedia(const string& name) {
	bool streaming = _pName.operator bool();
	_pName.set(name); // to signal begin (before to call start)
	if (!start())
		return false;
	if (streaming)
		return true; // MBR switch!

	send<Send>(); // First media, send the beginMedia!
	if (type == TYPE_HTTP) { // HTTP
		// send HTTP Header request!
		if (!SendHTTPHeader(_httpAnswer? HTTP::TYPE_UNKNOWN : HTTP::TYPE_POST, _pSocket, request, _pWriter->mime(), _pWriter->subMime(), source.name().c_str(), description)) {
			stop();
			return false;
		}
	}
	return true;
}

bool MediaSocket::Writer::writeProperties(const Media::Properties& properties) {
	Media::Data::Type type(Media::Data::TYPE_UNKNOWN);
	const Packet& packet = properties.data(type);
	return send<MediaSend<Media::Data>>(0, type, packet);
}

bool MediaSocket::Writer::endMedia() {
	stop();
	return false;
}

void MediaSocket::Writer::stopping() {
	// Close socket to signal the end of media
	if (_pName) { // else not beginMedia or useless (was connecting!)
		send<EndSend>(); // _pWriter->endMedia()!
		_pName.reset();
	}
	if(_pSocket)
		io.unsubscribe(_pSocket);
	_subscribed = false;
}



} // namespace Mona
