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

unique<MediaSocket::Reader> MediaSocket::Reader::New(Media::Stream::Type type, const Path& path, Media::Source& source, const char* subMime, const SocketAddress& address, IOSocket& io, const shared<TLS>& pTLS) {
	if (!*subMime && type == Media::Stream::TYPE_HTTP ) // Format can be empty with HTTP source
		return make_unique<MediaSocket::Reader>(type, path, source, unique<MediaReader>(), address, io, pTLS);
	unique<MediaReader> pReader(MediaReader::New(subMime));
	return pReader ? make_unique<MediaSocket::Reader>(type, path, source, move(pReader), address, io, pTLS) : nullptr;
}

void MediaSocket::Reader::Decoder::decode(shared<Buffer>& pBuffer, const SocketAddress& address, const shared<Socket>& pSocket) {
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

UInt32 MediaSocket::Reader::Decoder::onStreamData(Packet& buffer, const shared<Socket>& pSocket) {
	if (_type == TYPE_HTTP)
		return HTTPDecoder::onStreamData(buffer, pSocket);
	_pReader->read(buffer, *this);
	return 0;
}

MediaSocket::Reader::Reader(Type type, const Path& path, Media::Source& source, unique<MediaReader>&& pReader, const SocketAddress& address, IOSocket& io, const shared<TLS>& pTLS) :
	Media::Stream(type, path, source), _streaming(false), io(io), _pTLS(pTLS), address(address), _pReader(move(pReader)) {
	_onSocketDisconnection = [this]() { stop<Ex::Net::Socket>(LOG_DEBUG, "disconnection"); };
	_onSocketFlush = [this]() { finalizeStart(); };
	_onSocketError = [this](const Exception& ex) { stop(Stream::starting() ? LOG_DEBUG : LOG_WARN, ex); };
}

void MediaSocket::Reader::writeMedia(const HTTP::Message& message) {
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

bool MediaSocket::Reader::starting(const Parameters& parameters) {
	if (!_pSocket) {
		_pSocket = newSocket(parameters, _pTLS);
		bool success;
		if(type == TYPE_UDP) { // Bind if UDP
			AUTO_ERROR(success = _pSocket->bind(ex, address), description());
			if (!success) {
				_pSocket.reset();
				stop();
				return true;
			}
		}
		Decoder* pDecoder(new Decoder(io.handler, _pReader, source.name(), type));
		pDecoder->onResponse = _onResponse = [this](HTTP::Response& response)->void { writeMedia(response); };
		pDecoder->onRequest = _onRequest = [this](HTTP::Request& request) {
			if (type == Media::Stream::TYPE_HTTP)
				return stop<Ex::Protocol>(LOG_ERROR, "HTTP request on a source target");
			writeMedia(request);
		};
		// engine subscription BEFORE connect to be able to detect connection success/failure
		AUTO_ERROR(success = io.subscribe(ex = nullptr, _pSocket, pDecoder, nullptr, _onSocketFlush, _onSocketError, _onSocketDisconnection), description());
		if (!success) {
			_pSocket.reset();
			stop();
			return true;
		}
	}

	if (type == TYPE_UDP)
		return true;
	// Pulse connect if TCP/SRT
	if (!_pSocket->connect(ex = nullptr, address)) { // not use AUTO_ERROR because on WOULD_BLOCK it will display a wrong WARN
		stop(LOG_ERROR, ex);
		return true;
	}
	if (ex && ex.cast<Ex::Net::Socket>().code != NET_EWOULDBLOCK)
		WARN(description(), ", ", ex);

	if (!Stream::running() && type == TYPE_HTTP) { // HTTP + first time!
		// send HTTP Header request!
		shared<Buffer> pBuffer(SET);
		String::Append(*pBuffer, "GET ", path, " HTTP/1.1\r\nCache-Control: no-cache, no-store\r\nPragma: no-cache\r\nConnection: close\r\nUser-Agent: MonaServer\r\nHost: ", address, "\r\n\r\n");
		DUMP_REQUEST(source.name().c_str(), pBuffer->data(), pBuffer->size(), address);
		int sent;
		AUTO_ERROR((sent = _pSocket->write(ex = nullptr, Packet(pBuffer)))>=0, description());
		if (sent < 0) {
			stop();
			return true;
		}
	}
	return false; // wait onSocketFlush to finalize start!
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




unique<MediaSocket::Writer> MediaSocket::Writer::New(Media::Stream::Type type, const Path& path, const char* subMime, const SocketAddress& address, IOSocket& io, const shared<TLS>& pTLS) {
	unique<MediaWriter> pWriter(MediaWriter::New(subMime));
	return pWriter ? make_unique<MediaSocket::Writer>(type, path, move(pWriter), address, io, pTLS) : nullptr;
}

MediaSocket::Writer::Send::Send(Type type, const shared<string>& pName, const shared<Socket>& pSocket, const shared<MediaWriter>& pWriter) : Runner("MediaSocketSend"), _pSocket(pSocket), pWriter(pWriter), _pName(pName),
	onWrite([this, type](const Packet& packet) {
		UInt32 size = 0;
		Packet chunk(packet);
		while (chunk += size) {
			size = (type == TYPE_UDP || type == TYPE_SRT) && chunk.size() > Net::MTU_RELIABLE_SIZE ? Net::MTU_RELIABLE_SIZE : chunk.size();
			DUMP_RESPONSE(_pName->c_str(), chunk.data(), size, _pSocket->peerAddress());
			Exception ex;
			int result;
			AUTO_WARN((result = _pSocket->write(ex, Packet(chunk, chunk.data(), size))) >= 0, "Stream target ", TypeToString(type), "://", _pSocket->peerAddress(), '|', String::Upper(this->pWriter->format()));
			if (result < 0) // write has failed, no more reliable!
				_pSocket->shutdown();
		};
	}) {
}

MediaSocket::Writer::Writer(Type type, const Path& path, unique<MediaWriter>&& pWriter, const SocketAddress& address, IOSocket& io, const shared<TLS>& pTLS) :
	Media::Stream(type, path), io(io), _pTLS(pTLS), address(address), _sendTrack(0), _pWriter(move(pWriter)) {
	_onSocketDisconnection = [this]() { stop<Ex::Net::Socket>(LOG_WARN, this->address, " disconnection"); };
	_onSocketError = [this](const Exception& ex) { stop(Stream::starting() ? LOG_DEBUG : LOG_WARN, ex); };
}
MediaSocket::Writer::Writer(Type type, const Path& path, unique<MediaWriter>&& pWriter, const shared<Socket>& pSocket, IOSocket& io) : _pSocket(pSocket),
	Media::Stream(type, path), io(io), address(pSocket->peerAddress()), _sendTrack(0), _pWriter(move(pWriter)) {
	_onSocketDisconnection = [this]() { stop<Ex::Net::Socket>(LOG_WARN, this->address, " disconnection"); };
	_onSocketError = [this](const Exception& ex) { stop(Stream::starting() ? LOG_DEBUG : LOG_WARN, ex); };
	newSocket();
}

bool MediaSocket::Writer::newSocket(const Parameters& parameters) {
	if (!_pSocket)
		_pSocket = Media::Stream::newSocket(parameters, _pTLS);
	bool success;
	AUTO_ERROR(success = io.subscribe(ex, _pSocket, nullptr, nullptr, _onSocketError, _onSocketDisconnection), description());
	if (!success)
		_pSocket.reset();
	return success;
}

bool MediaSocket::Writer::starting(const Parameters& parameters) {
	if (!_pSocket && !newSocket(parameters)) {
		stop();
		return true;
	}
	if (!_pName)  // Do nothing if not media beginning
		return false; // wait beginMedia
	// beginMedia+not writable => Pulse connect
	if (!_pSocket->connect(ex, address)) { // not use AUTO_ERROR because on WOULD_BLOCK it will display a wrong WARN
		stop();
		return true;
	}
	if (ex && ex.cast<Ex::Net::Socket>().code != NET_EWOULDBLOCK)
		WARN(description(), ", ", ex);
	return false; // wait onSocketFlush + first Media data
}

bool MediaSocket::Writer::beginMedia(const string& name) {
	bool streaming = _pName.operator bool();
	_pName.set(name);
	if (streaming)
		return true; // MBR switch!
	start();
	if (!running())
		return false;
	if (type == TYPE_HTTP) { // first time + HTTP
		// send HTTP Header request!
		shared<Buffer> pBuffer(SET);
		String::Append(*pBuffer, "POST ", path, " HTTP/1.1\r\nCache-Control: no-cache, no-store\r\nPragma: no-cache\r\nConnection: close\r\nUser-Agent: MonaServer\r\nHost: ", address, "\r\nContent-Type: ");
		String::Append(MIME::Write(*pBuffer, _pWriter->mime(), _pWriter->subMime()), "\r\n\r\n");
		DUMP_REQUEST(name.c_str(), pBuffer->data(), pBuffer->size(), address);
		int sent;
		AUTO_ERROR((sent = _pSocket->write(ex = nullptr, Packet(pBuffer))) >= 0, description());
		if (sent < 0) {
			stop();
			return false;
		}
	}
	send<Send>(); // First media, send the beginMedia!
	return true;
}

bool  MediaSocket::Writer::writeProperties(const Media::Properties& properties) {
	Media::Data::Type type;
	const Packet& packet = properties(type);
	return send<MediaSend<Media::Data>>(0, type, packet);
}

void MediaSocket::Writer::endMedia() {
	stop();
}

void MediaSocket::Writer::stopping() {
	// Close socket to signal the end of media
	if (!Stream::starting()) // else not beginMedia or useless (was connecting!)
		send<EndSend>(); // _pWriter->endMedia()!
	_pName.reset();
	if(_pSocket)
		io.unsubscribe(_pSocket);
}



} // namespace Mona
