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
#include "Mona/Logs.h"

namespace Mona {


struct MediaSocket : virtual Static {

	struct Reader : Media::Stream, virtual Object {
		Reader(Media::Stream::Type type, const Path& path, MediaReader* pReader, const SocketAddress& address, IOSocket& io, const shared<TLS>& pTLS = nullptr);
		virtual ~Reader() { stop(); }

		static MediaSocket::Reader* New(Media::Stream::Type type, const Path& path, const char* subMime, const SocketAddress& address, IOSocket& io, const shared<TLS>& pTLS = nullptr);
		static MediaSocket::Reader* New(Media::Stream::Type type, const Path& path, const SocketAddress& address, IOSocket& io, const shared<TLS>& pTLS = nullptr) { return New(type, path, path.extension().c_str(), address, io, pTLS); }

		void start();
		void start(Media::Source& source) { _pSource = &source; return start(); }
		bool running() const { return _subscribed; }
		void stop();

		const SocketAddress			address;
		IOSocket&					io;
		const shared<Socket>&		socket();
		Socket*						operator->() { return socket().get(); }

	private:
		std::string& buildDescription(std::string& description) { return String::Assign(description, "Stream source ", TypeToString(type), "://", address, path, '|', (_pReader ? _pReader->format() : "AUTO")); }
		void writeMedia(const HTTP::Message& message);

		struct Decoder : HTTPDecoder, virtual Object {
			Decoder(const Handler& handler, const shared<MediaReader>& pReader, const std::string& name, Type type) :
				_type(type), _rest(0), _pReader(pReader), HTTPDecoder(handler, Path::Null(), name) {}
			~Decoder() { if (_pReader.unique()) _pReader->flush(self); }

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
		Media::Source*			_pSource;
		shared<Socket>			_pSocket;
		shared<TLS>				_pTLS;
		bool					_subscribed;
		bool					_streaming;
	};


	struct Writer : Media::Target, Media::Stream, virtual Object {
		Writer(Media::Stream::Type type, const Path& path, MediaWriter* pWriter, const SocketAddress& address, IOSocket& io, const shared<TLS>& pTLS = nullptr);
		virtual ~Writer() { stop(); }

		static MediaSocket::Writer* New(Media::Stream::Type type, const Path& path, const char* subMime, const SocketAddress& address, IOSocket& io, const shared<TLS>& pTLS = nullptr);
		static MediaSocket::Writer* New(Media::Stream::Type type, const Path& path, const SocketAddress& address, IOSocket& io, const shared<TLS>& pTLS = nullptr) { return New(type, path, path.extension().c_str(), address, io, pTLS); }

		void start();
		bool running() const { return _subscribed; }
		void stop();

		const SocketAddress		address;
		IOSocket&				io;
		UInt64					queueing() const { return _pSocket ? _pSocket->queueing() : 0; }
		const shared<Socket>&	socket();
		Socket*					operator->() { return socket().get(); }


		bool beginMedia(const std::string& name);
		bool writeProperties(const Media::Properties& properties);
		bool writeAudio(UInt8 track, const Media::Audio::Tag& tag, const Packet& packet, bool reliable) { return send<MediaSend<Media::Audio>>(track, tag, packet); }
		bool writeVideo(UInt8 track, const Media::Video::Tag& tag, const Packet& packet, bool reliable) { return send<MediaSend<Media::Video>>(track, tag, packet); }
		bool writeData(UInt8 track, Media::Data::Type type, const Packet& packet, bool reliable) { return send<MediaSend<Media::Data>>(track, type, packet); }
		void endMedia();
		
	private:
		std::string& buildDescription(std::string& description) { return String::Assign(description, "Stream target ", TypeToString(type), "://", address, path, '|', _pWriter->format()); }

		template<typename SendType, typename ...Args>
		bool send(Args&&... args) {
			if (!_subscribed)
				return false; // Stream not started!
			io.threadPool.queue(new SendType(type, _pName, _pSocket, _pWriter, _pStreaming, args ...), _sendTrack);
			return true;
		}

		struct Send : Runner, virtual Object {
			Send(Type type, const shared<std::string>& pName, const shared<Socket>& pSocket, const shared<MediaWriter>& pWriter, const shared<volatile bool>& pStreaming);
		protected:
			MediaWriter::OnWrite	onWrite;
			shared<MediaWriter>		pWriter;
		private:
			virtual bool run(Exception& ex) { pWriter->beginMedia(onWrite); return true; }

			shared<Socket>			_pSocket;
			shared<volatile bool>	_pStreaming;
			shared<std::string>		_pName;
		};

		template<typename MediaType>
		struct MediaSend : Send, MediaType, virtual Object {
			MediaSend(Stream::Type type, const shared<std::string>& pName, const shared<Socket>& pSocket, const shared<MediaWriter>& pWriter, const shared<volatile bool>& pStreaming,
				UInt8 track, const typename MediaType::Tag& tag, const Packet& packet) : Send(type, pName, pSocket,pWriter, pStreaming), MediaType(tag, packet, track) {}
			bool run(Exception& ex) { pWriter->writeMedia(*this, onWrite); return true; }
		};
		struct EndSend : Send, virtual Object {
			EndSend(Stream::Type type, const shared<std::string>& pName, const shared<Socket>& pSocket, const shared<MediaWriter>& pWriter, const shared<volatile bool>& pStreaming) : Send(type, pName, pSocket, pWriter, pStreaming) {}
			bool run(Exception& ex) { pWriter->endMedia(onWrite); return true; }
		};

		Socket::OnDisconnection			_onDisconnection;
		Socket::OnFlush					_onFlush;
		Socket::OnError					_onError;

		shared<Socket>					_pSocket;
		shared<TLS>						_pTLS;
		shared<MediaWriter>				_pWriter;
		UInt16							_sendTrack;
		bool							_subscribed;
		shared<std::string>				_pName;
		shared<volatile bool>			_pStreaming;
	};
};

} // namespace Mona
