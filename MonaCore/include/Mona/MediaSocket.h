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
#include "Mona/MediaReader.h"
#include "Mona/MediaWriter.h"
#include "Mona/Socket.h"
#include "Mona/StreamData.h"
#include "Mona/Logs.h"

namespace Mona {


struct MediaSocket : virtual Static {

	struct Reader : Media::Stream, virtual Object {
		Reader(Media::Stream::Type type, const Path& path, MediaReader* pReader, const SocketAddress& address, IOSocket& io, const shared<TLS>& pTLS = nullptr);
		virtual ~Reader() { stop(); }

		void start();
		void start(Media::Source& source) { _pSource = &source; return start(); }
		bool running() const { return _subscribed; }
		void stop();

		const Path					path;
		const SocketAddress			address;
		IOSocket&					io;
		const shared<Socket>&		socket();
		Socket*						operator->() { return socket().get(); }

	private:
		std::string& buildDescription(std::string& description) { return String::Assign(description, "Stream source ", TypeToString(type), "://", address, path, '|', _pReader->format()); }

		struct Lost : Media::Base, virtual Object {
			Lost(Media::Type type, UInt32 lost) : Media::Base(type, 0, Packet::Null()), _lost(-Int32(lost)) {} // lost
			Lost(Media::Type type, UInt16 track, UInt32 lost) : Media::Base(type, track, Packet::Null()), _lost(lost) {} // lost
			void report(Media::Source& source) { _lost < 0 ? source.reportLost(type, -_lost) : source.reportLost(type, track, _lost); }
		private:
			Int32 _lost;
		};
		struct Decoder : Socket::Decoder, private Media::Source, private StreamData<const SocketAddress&>, virtual Object {
			typedef Event<void(Media::Base&)>		ON(Media);
			typedef Event<void(Lost& lost)>			ON(Lost);
			typedef Event<void()>					ON(Flush);
			typedef Event<void()>					ON(Reset);
			typedef Event<void(const char*& error)>	ON(Error);

			Decoder(const Handler& handler, const shared<MediaReader>& pReader, const std::string& name, Type type) :
				_name(name), _type(type), _rest(0), _handler(handler), _pReader(pReader) {}
			~Decoder() { _pReader->flush(*this); }

		private:
			UInt32 decode(shared<Buffer>& pBuffer, const SocketAddress& address, const shared<Socket>& pSocket);
			UInt32 onStreamData(Packet& buffer, const SocketAddress& address);
	
			void writeAudio(UInt16 track, const Media::Audio::Tag& tag, const Packet& packet) { _handler.queue<Media::Audio>(onMedia, track, tag, packet); }
			void writeVideo(UInt16 track, const Media::Video::Tag& tag, const Packet& packet) { _handler.queue<Media::Video>(onMedia, track, tag, packet); }
			void writeData(UInt16 track, Media::Data::Type type, const Packet& packet) { _handler.queue<Media::Data>(onMedia, track, type, packet); }
			void writeProperties(UInt16 track, DataReader& reader) { _handler.queue<Media::Data>(onMedia, track, reader); }
			void reportLost(Media::Type type, UInt32 lost) { _handler.queue(onLost, type, lost); }
			void reportLost(Media::Type type, UInt16 track, UInt32 lost) { _handler.queue(onLost, type, track, lost); }
			void flush() { _handler.queue(onFlush); }
			void reset() { _handler.queue(onReset); }
	
			shared<MediaReader>		_pReader;
			Type					_type;
			UInt8					_rest;
			std::string				_name;
			const Handler&			_handler;
		};

		Decoder::OnError			_onError;
		Decoder::OnLost				_onLost;
		Decoder::OnFlush			_onFlush;
		Decoder::OnReset			_onReset;
		Decoder::OnMedia			_onMedia;

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

		void start();
		bool running() const { return _subscribed; }
		void stop();

		const SocketAddress		address;
		IOSocket&				io;
		const Path				path;
		UInt64					queueing() const { return _pSocket ? _pSocket->queueing() : 0; }
		const shared<Socket>&	socket();
		Socket*					operator->() { return socket().get(); }


		bool beginMedia(const std::string& name, const Parameters& parameters);
		bool writeAudio(UInt16 track, const Media::Audio::Tag& tag, const Packet& packet, bool reliable) { return send<MediaSend<Media::Audio>>(track, tag, packet); }
		bool writeVideo(UInt16 track, const Media::Video::Tag& tag, const Packet& packet, bool reliable) { return send<MediaSend<Media::Video>>(track, tag, packet); }
		bool writeData(UInt16 track, Media::Data::Type type, const Packet& packet, bool reliable) { return send<MediaSend<Media::Data>>(track, type, packet); }
		void endMedia(const std::string& name);
		
	private:
		std::string& buildDescription(std::string& description) { return String::Assign(description, "Stream target ", TypeToString(type), "://", address, path, '|', _pWriter->format()); }

		template<typename SendType, typename ...Args>
		bool send(Args&&... args) {
			if (!_subscribed)
				return false; // Stream not started!
			Exception ex;
			bool success;
			AUTO_ERROR(success = io.threadPool.queue(ex, std::make_shared<SendType>(type, _pName, _pSocket, _pWriter, _pStreaming, args ...), _sendTrack), description());
			if (success)
				return true;
			Stream::stop(ex);
			return false;
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
				   UInt16 track, const typename MediaType::Tag& tag, const Packet& packet) : Send(type, pName, pSocket,pWriter, pStreaming), MediaType(track, tag, packet) {}
			bool run(Exception& ex) { pWriter->writeMedia(track, tag, *this, onWrite); return true; }
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
