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

#pragma once

#include "Mona/Mona.h"
#include "Mona/HTTP/HTTPDecoder.h"

namespace Mona {


struct MediaSocket : virtual Static {

	struct Reader : Media::Stream, virtual Object {
		static unique<MediaSocket::Reader> New(Media::Stream::Type type, const Path& path, Media::Source& source, const char* subMime, const SocketAddress& address, IOSocket& io, const shared<TLS>& pTLS = nullptr);
		static unique<MediaSocket::Reader> New(Media::Stream::Type type, const Path& path, Media::Source& source, const SocketAddress& address, IOSocket& io, const shared<TLS>& pTLS = nullptr) { return New(type, path, source, path.extension().c_str(), address, io, pTLS); }

		Reader(Media::Stream::Type type, const Path& path, Media::Source& source, unique<MediaReader>&& pReader, const SocketAddress& address, IOSocket& io, const shared<TLS>& pTLS = nullptr);
		virtual ~Reader() { stop(); }

		const SocketAddress			address;
		IOSocket&					io;
		shared<const Socket>		socket() const { return _pSocket ? _pSocket : nullptr; }
	
	private:
		bool starting(const Parameters& parameters);
		void stopping();
	
		std::string& buildDescription(std::string& description) { return String::Assign(description, "Stream source ", TypeToString(type), "://", address, path, '|', String::Upper(_pReader ? _pReader->format() : "AUTO")); }
		void writeMedia(const HTTP::Message& message);

		struct Decoder : HTTPDecoder, virtual Object {
			Decoder(const Handler& handler, const shared<MediaReader>& pReader, const std::string& name, Type type) :
				_type(type), _rest(0), _pReader(pReader), HTTPDecoder(handler, Path::Null(), name.c_str()) {}

		private:
			void   decode(shared<Buffer>& pBuffer, const SocketAddress& address, const shared<Socket>& pSocket);
			UInt32 onStreamData(Packet& buffer, const shared<Socket>& pSocket);

			shared<MediaReader>		_pReader;
			Type					_type;
			SocketAddress			_address;
			UInt32					_rest;
		};

		HTTPDecoder::OnRequest	_onRequest;
		HTTPDecoder::OnResponse _onResponse;
	
		Socket::OnDisconnection	_onSocketDisconnection;
		Socket::OnFlush			_onSocketFlush;
		Socket::OnError			_onSocketError;

		shared<MediaReader>		_pReader;
		shared<Socket>			_pSocket;
		shared<TLS>				_pTLS;
		bool					_streaming;
	};


	struct Writer : Media::Target, Media::Stream, virtual Object {
		static unique<MediaSocket::Writer> New(Media::Stream::Type type, const Path& path, const char* subMime, const SocketAddress& address, IOSocket& io, const shared<TLS>& pTLS = nullptr);
		static unique<MediaSocket::Writer> New(Media::Stream::Type type, const Path& path, const SocketAddress& address, IOSocket& io, const shared<TLS>& pTLS = nullptr) { return New(type, path, path.extension().c_str(), address, io, pTLS); }

		Writer(Media::Stream::Type type, const Path& path, unique<MediaWriter>&& pWriter, const SocketAddress& address, IOSocket& io, const shared<TLS>& pTLS = nullptr);
		Writer(Media::Stream::Type type, const Path& path, unique<MediaWriter>&& pWriter, const shared<Socket>& pSocket, IOSocket& io);
		virtual ~Writer() { stop(); }

		const SocketAddress		address;
		IOSocket&				io;
		UInt64					queueing() const { return _pSocket ? _pSocket->queueing() : 0; }
		shared<const Socket>	socket() const { return _pSocket ? _pSocket : nullptr; }
		
		bool beginMedia(const std::string& name);
		bool writeProperties(const Media::Properties& properties);
		bool writeAudio(UInt8 track, const Media::Audio::Tag& tag, const Packet& packet, bool reliable) { return send<MediaSend<Media::Audio>>(track, tag, packet); }
		bool writeVideo(UInt8 track, const Media::Video::Tag& tag, const Packet& packet, bool reliable) { return send<MediaSend<Media::Video>>(track, tag, packet); }
		bool writeData(UInt8 track, Media::Data::Type type, const Packet& packet, bool reliable) { return send<MediaSend<Media::Data>>(track, type, packet); }
		void endMedia();
		
	private:
		bool starting(const Parameters& parameters);
		void stopping();
	
		std::string& buildDescription(std::string& description) { return String::Assign(description, "Stream target ", TypeToString(type), "://", address, path, '|', String::Upper(_pWriter->format())); }
		bool newSocket(const Parameters& parameters = Parameters::Null());
		
		template<typename SendType, typename ...Args>
		bool send(UInt8 track, Args&&... args) {
			if (!_onSocketFlush)
				finalizeStart();
			if (!_pSocket)
				return false; // Stream not started!
			io.threadPool.queue<SendType>(_sendTrack, type, _pName, _pSocket, _pWriter, track,  std::forward<Args>(args)...);
			return true;
		}
		template<typename SendType> // beginMedia or endMedia!
		void send() { io.threadPool.queue<SendType>(_sendTrack, type, _pName, _pSocket, _pWriter); }

		struct Send : Runner, virtual Object {
			Send(Type type, const shared<std::string>& pName, const shared<Socket>& pSocket, const shared<MediaWriter>& pWriter);
		protected:
			MediaWriter::OnWrite	onWrite;
			shared<MediaWriter>		pWriter;
		private:
			virtual bool run(Exception& ex) { pWriter->beginMedia(onWrite); return true; }

			shared<Socket>			_pSocket;
			shared<std::string>		_pName;
		};

		template<typename MediaType>
		struct MediaSend : Send, MediaType, virtual Object {
			MediaSend(Stream::Type type, const shared<std::string>& pName, const shared<Socket>& pSocket, const shared<MediaWriter>& pWriter,
				UInt8 track, const typename MediaType::Tag& tag, const Packet& packet) : Send(type, pName, pSocket,pWriter), MediaType(tag, packet, track) {}
			bool run(Exception& ex) { pWriter->writeMedia(*this, onWrite); return true; }
		};
		struct EndSend : Send, virtual Object {
			EndSend(Stream::Type type, const shared<std::string>& pName, const shared<Socket>& pSocket, const shared<MediaWriter>& pWriter) : Send(type, pName, pSocket, pWriter) {}
			bool run(Exception& ex) { pWriter->endMedia(onWrite); return true; }
		};

		Socket::OnDisconnection			_onSocketDisconnection;
		Socket::OnFlush					_onSocketFlush;
		Socket::OnError					_onSocketError;

		shared<Socket>					_pSocket;
		shared<TLS>						_pTLS;
		shared<MediaWriter>				_pWriter;
		UInt16							_sendTrack;
		shared<std::string>				_pName;
	};
};

} // namespace Mona
