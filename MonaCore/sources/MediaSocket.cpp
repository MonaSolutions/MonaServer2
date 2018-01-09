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

using namespace std;

namespace Mona {

MediaSocket::Reader* MediaSocket::Reader::New(Media::Stream::Type type, const Path& path, const char* subMime, const SocketAddress& address, IOSocket& io, const shared<TLS>& pTLS) {
	if (!*subMime && type == Media::Stream::TYPE_HTTP ) // Format can be empty with HTTP source
		return new MediaSocket::Reader(type, path, NULL, address, io, pTLS);
	MediaReader* pReader = MediaReader::New(subMime);
	return pReader ? new MediaSocket::Reader(type, path, pReader, address, io, pTLS) : NULL;
}

void MediaSocket::Reader::Decoder::decode(shared<Buffer>& pBuffer, const SocketAddress& address, const shared<Socket>& pSocket) {
	if (_pReader.unique()) // Work too for HTTP (if _pReader == NULL then _pReader.unique() == false)
		return;
	if (_address != address) {
		if (_address)
			_pReader->flush(*this); // address has changed, means that we are in UDP, it's a new stream (come from someone else)
		_address = address;
	}
	HTTPDecoder::decode(pBuffer, address, pSocket);
}

UInt32 MediaSocket::Reader::Decoder::onStreamData(Packet& buffer, Socket& socket) {
	if (_type == TYPE_HTTP)
		return HTTPDecoder::onStreamData(buffer, socket);
	_pReader->read(buffer, *this);
	return 0;
}

MediaSocket::Reader::Reader(Type type, const Path& path, MediaReader* pReader, const SocketAddress& address, IOSocket& io, const shared<TLS>& pTLS) :
	Media::Stream(type, path), _streaming(false), io(io), _pTLS(pTLS), address(address), _pReader(pReader), _subscribed(false) {
	_onSocketDisconnection = [this]() { Stream::stop<Ex::Net::Socket>(LOG_DEBUG, "disconnection"); };
	_onSocketError = [this](const Exception& ex) { Stream::stop(_streaming ? LOG_WARN : LOG_DEBUG, ex); };
}

void MediaSocket::Reader::start() {
	if (!_pSource) {
		Stream::stop<Ex::Intern>(LOG_ERROR, "call start(Media::Source& source) in first");
		return;
	}
	Exception ex;
	bool success;
	// Bind if UDP and not already bound
	if (!socket()->address() && type==TYPE_UDP) {
		AUTO_ERROR(success = _pSocket->bind(ex, address), description());
		if (!success) {
			_pSocket.reset();
			return Stream::stop(ex);
		}
	}

	// Bound decoder + engine
	if(!_subscribed) {
		shared<Decoder> pDecoder(new Decoder(io.handler, _pReader, _pSource->name(), type));
		pDecoder->onResponse = _onResponse = [this](HTTP::Response& response)->void {
			if (response.ex)
				return Stream::stop<Ex::Protocol>(LOG_ERROR, response.ex);
			if (!response.pMedia) {
				if(_streaming)
					return Stream::stop<Ex::Net::Socket>(LOG_DEBUG, "end of stream");
				return Stream::stop<Ex::Protocol>(LOG_ERROR, "HTTP response is not a media");
			}
			if (!_streaming) {
				_streaming = true;
				INFO(description(), " starts");
			}
			if (response.lost)
				_pSource->reportLost(response.pMedia->type, response.lost, response.pMedia->track);
			else if (response.pMedia->type)
				_pSource->writeMedia(*response.pMedia);
			else if(!response.flush && !response) // !response => if header it's the first "publish" command
				return _pSource->reset();
			if(response.flush)
				_pSource->flush();
		};
		pDecoder->onRequest = _onRequest = [this](HTTP::Request& request) {
			Stream::stop<Ex::Protocol>(LOG_ERROR, "HTTP request on a source target");
		};

		// engine subscription BEFORE connect to be able to detect connection success/failure
		if(!_onSocketFlush)
			_onSocketFlush = [this]() { _onSocketFlush = nullptr; };
		AUTO_ERROR(_subscribed = io.subscribe(ex, _pSocket, pDecoder, nullptr, _onSocketFlush, _onSocketError, _onSocketDisconnection), description());
		if (!_subscribed) {
			_pSocket.reset();
			return Stream::stop(ex);
		}
	}

	bool connected(_pSocket->peerAddress() ? true : false);
	if (type == TYPE_UDP || (connected && !_onSocketFlush))
		return;
	// Pulse connect
	if (!_pSocket->connect(ex=nullptr, address)) {
		if (ex.cast<Ex::Net::Socket>().code != NET_EWOULDBLOCK)
			return Stream::stop(LOG_ERROR, ex);
		ex = nullptr;
	} else if (ex)
		WARN(description(), ", ", ex);
	if (!connected && type == TYPE_HTTP) {
		// send HTTP Header request!
		shared<Buffer> pBuffer(new Buffer());
		BinaryWriter writer(*pBuffer);
		writer.write(EXPAND("GET ")).write(path);
		writer.write(EXPAND(" HTTP/1.1\r\nCache-Control: no-cache, no-store\r\nPragma: no-cache\r\nConnection: close\r\nUser-Agent: MonaServer\r\nHost: "));
		writer.write(address).write(EXPAND("\r\n\r\n"));
		DUMP_REQUEST(_pSource->name().c_str(), pBuffer->data(), pBuffer->size(), address);
		int sent;
		AUTO_ERROR((sent = _pSocket->write(ex, Packet(pBuffer)))>=0, description());
		if (sent < 0)
			return Stream::stop(ex);
	}
}

void MediaSocket::Reader::stop() {
	if (!_subscribed)
		return;
	io.unsubscribe(_pSocket);
	_pSocket.reset();
	_onRequest = nullptr;
	_onResponse = nullptr;
	if (_streaming) {
		if (_pReader.unique()) // works also when _pReader is null (unique()==false)
			_pReader->flush(*_pSource);
		else
			_pSource->reset(); // because the source.reset of _pReader->flush() can't be called (parallel!)
		INFO(description(), " stops");
		_streaming = false;
	}
	// reset _pReader because could be used by different thread by new Socket and its decoding thread
	if(_pReader)
		_pReader.reset(MediaReader::New(_pReader->subMime()));
}

const shared<Socket>& MediaSocket::Reader::socket() {
	if (!_pSocket) {
		if (_pTLS)
			_pSocket.reset(new TLS::Socket(type == TYPE_UDP ? Socket::TYPE_DATAGRAM : Socket::TYPE_STREAM, _pTLS));
		else
			_pSocket.reset(new Socket(type == TYPE_UDP ? Socket::TYPE_DATAGRAM : Socket::TYPE_STREAM));
		Exception ex;
		if (RecvBufferSize)
			AUTO_ERROR(_pSocket->setRecvBufferSize(ex, RecvBufferSize), description(), " receiving buffer setting");
		if (SendBufferSize)
			AUTO_ERROR(_pSocket->setSendBufferSize(ex, SendBufferSize), description(), " sending buffer setting");
	}
	return _pSocket;
}





MediaSocket::Writer* MediaSocket::Writer::New(Media::Stream::Type type, const Path& path, const char* subMime, const SocketAddress& address, IOSocket& io, const shared<TLS>& pTLS) {
	MediaWriter* pWriter = MediaWriter::New(subMime);
	return pWriter ? new MediaSocket::Writer(type, path, pWriter, address, io, pTLS) : NULL;
}

MediaSocket::Writer::Send::Send(Type type, const shared<string>& pName, const shared<Socket>& pSocket, const shared<MediaWriter>& pWriter, const shared<volatile bool>& pStreaming) : Runner("MediaSocketSend"), _pSocket(pSocket), pWriter(pWriter),
	_pStreaming(pStreaming), _pName(pName),
	onWrite([this, type](const Packet& packet) {
		Packet chunk(packet, packet.data(), 0);
		while (chunk.set(packet, chunk.data() + chunk.size())) {
			if (type == TYPE_UDP && chunk.size() > Net::MTU_RELIABLE_SIZE)
				chunk.shrink(Net::MTU_RELIABLE_SIZE);
			DUMP_RESPONSE(_pName->c_str(), chunk.data(), chunk.size(), _pSocket->peerAddress());
			Exception ex;
			UInt64 byteRate = _pSocket->sendByteRate(); // Get byteRate before write to start computing cycle on 0!
			int result = _pSocket->write(ex, chunk);
			if (result && !*_pStreaming && byteRate) {
				INFO("Stream target ", TypeToString(type), "://", _pSocket->peerAddress(), '|', this->pWriter->format(), " starts");
				*_pStreaming = true;
			}
			if (ex || result<0)
				WARN("Stream target ", TypeToString(type), "://", _pSocket->peerAddress(), '|', this->pWriter->format(), ", ", ex);
		};
	}) {
}

const shared<Socket>& MediaSocket::Writer::socket() {
	if (!_pSocket) {
		if (_pTLS)
			_pSocket.reset(new TLS::Socket(type == TYPE_UDP ? Socket::TYPE_DATAGRAM : Socket::TYPE_STREAM, _pTLS));
		else
			_pSocket.reset(new Socket(type == TYPE_UDP ? Socket::TYPE_DATAGRAM : Socket::TYPE_STREAM));
		Exception ex;
		if (RecvBufferSize)
			AUTO_ERROR(_pSocket->setRecvBufferSize(ex, RecvBufferSize), description(), " receiving buffer setting");
		if (SendBufferSize)
			AUTO_ERROR(_pSocket->setSendBufferSize(ex, SendBufferSize), description(), " sending buffer setting");
	}
	return _pSocket;
}

MediaSocket::Writer::Writer(Type type, const Path& path, MediaWriter* pWriter, const SocketAddress& address, IOSocket& io, const shared<TLS>& pTLS) :
	Media::Stream(type, path), io(io), _pTLS(pTLS), address(address), _sendTrack(0), _subscribed(false), _pWriter(pWriter), _pStreaming(new bool(false)) {
	_onDisconnection = [this]() { Stream::stop<Ex::Net::Socket>(LOG_WARN, this->address, "disconnection"); };
	_onError = [this](const Exception& ex) { Stream::stop(*_pStreaming ? LOG_WARN : LOG_DEBUG, ex); };
}

void MediaSocket::Writer::start() {
	Exception ex;
	if (!_subscribed) {
		// engine subscription BEFORE connect to be able to detect connection success/failure
		if (!_onFlush)
			_onFlush = [this]() { _onFlush = nullptr; };
		AUTO_ERROR(_subscribed = io.subscribe(ex, socket(), nullptr, _onFlush, _onError, _onDisconnection), description());
		if (!_subscribed) {
			_pSocket.reset();
			return Stream::stop(ex);
		}
	}
	if (!_pName)
		return; // Do nothing if not media beginning
	bool connected(_pSocket->peerAddress() ? true : false);
	if (connected && !_onFlush)
		return; // Do nothing if already connected!
	// Pulse connect if always not writable
	if (!_pSocket->connect(ex=nullptr, address)) {
		if (ex.cast<Ex::Net::Socket>().code != NET_EWOULDBLOCK)
			return Stream::stop(LOG_ERROR, ex);
		ex = nullptr;
	} else if (ex)
		WARN(description(), ", ", ex);
	if (!connected && type == TYPE_HTTP) {
		// send HTTP Header request!
		shared<Buffer> pBuffer(new Buffer());
		BinaryWriter writer(*pBuffer);
		writer.write(EXPAND("POST ")).write(path);
		writer.write(EXPAND("HTTP/1.1\r\nCache-Control: no-cache, no-store\r\nPragma: no-cache\r\nConnection: close\r\nUser-Agent: MonaServer\r\nHost: "));
		writer.write(address);
		MIME::Write(writer.write(EXPAND("\r\nContent-Type: ")), _pWriter->mime(), _pWriter->subMime()).write(EXPAND("\r\n\r\n"));
		DUMP_REQUEST(_pName->c_str(), pBuffer->data(), pBuffer->size(), address);
		int sent;
		AUTO_ERROR((sent = _pSocket->write(ex, Packet(pBuffer)))>=0, description());
		if (sent < 0)
			Stream::stop(ex);
	}
}

bool MediaSocket::Writer::beginMedia(const string& name) {
	bool streaming = _pName.operator bool();
	_pName.reset(new string(name));
	start(); // begin media, we can try to start here (just on beginMedia!)
	return streaming ? true : send<Send>();
}

void MediaSocket::Writer::endMedia() {
	send<EndSend>();
	stop();
}

void MediaSocket::Writer::stop() {
	// Close socket to signal the end of media
	if (!_subscribed)
		return;
	io.unsubscribe(_pSocket);
	_pSocket.reset();
	// reset _pWriter because could be used by different thread by new Socket and its sending thread
	_pWriter.reset(MediaWriter::New(_pWriter->subMime()));
	_pName.reset();
	_subscribed = false;
	if (*_pStreaming) {
		INFO(description(), " stops");
		_pStreaming.reset(new bool(false));
	}
}



} // namespace Mona
