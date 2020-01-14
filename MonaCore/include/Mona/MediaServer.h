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
#include "Mona/MediaSocket.h"
#include "Mona/SRT.h"
#include "Mona/HTTP/HTTPDecoder.h"

namespace Mona {

struct MediaServer : virtual Static {
	enum Type {
#if defined(SRT_API)
		TYPE_SRT = MediaStream::TYPE_SRT,
#endif
		TYPE_TCP = MediaStream::TYPE_TCP,
		TYPE_HTTP = MediaStream::TYPE_HTTP

	};

	struct Reader : MediaStream, virtual Object {
		static unique<MediaServer::Reader> New(Exception& ex, MediaStream::Type type, const char* request, Media::Source& source, const SocketAddress& address, IOSocket& io, std::string&& format = "") { return New(ex, type, request, source, address, io, nullptr, std::move(format)); }
		static unique<MediaServer::Reader> New(Exception& ex, MediaStream::Type type, const char* request, Media::Source& source, const SocketAddress& address, IOSocket& io, const shared<TLS>& pTLS, std::string&& format = "");

		Reader(MediaServer::Type type, const char* request, unique<MediaReader>&& pReader, Media::Source& source, const SocketAddress& address, IOSocket& io, const shared<TLS>& pTLS = nullptr);
		virtual ~Reader() { stop(); }

		const SocketAddress		address;
		const std::string		request;
		IOSocket&				io;
		shared<const Socket>	socket() const { return _pSocket ? _pSocket : nullptr; }

	private:
		bool starting(const Parameters& parameters);
		void stopping();

		Socket::OnAccept	_onConnnection;
		Socket::OnError		_onError;

		shared<Socket>					_pSocket;
		shared<TLS>						_pTLS;
		shared<MediaSocket::Reader>		_pTarget;
		const char*						_subMime; // can be null (HTTP auto case for example)
	};

	struct Writer : MediaStream, virtual Object {
		static unique<MediaServer::Writer> New(Exception& ex, MediaStream::Type type, const char* request, const SocketAddress& address, IOSocket& io, std::string&& format = "") { return New(ex, type, request, address, io, nullptr, std::move(format)); }
		static unique<MediaServer::Writer> New(Exception& ex, MediaStream::Type type, const char* request, const SocketAddress& address, IOSocket& io, const shared<TLS>& pTLS, std::string&& format = "");

		Writer(MediaServer::Type type, const char* request, unique<MediaWriter>&& pWriter, const SocketAddress& address, IOSocket& io, const shared<TLS>& pTLS = nullptr);
		virtual ~Writer() { stop(); }

		const SocketAddress		address;
		const std::string		request;
		IOSocket&				io;
		shared<const Socket>	socket() const { return _pSocket ? _pSocket : nullptr; }

	private:
		bool starting(const Parameters& parameters);
		void stopping();

		Socket::OnAccept		_onConnnection;
		Socket::OnError			_onError;

		shared<Socket>			_pSocket;
		shared<TLS>				_pTLS;
		const char*				_subMime;
	};
};

} // namespace Mona
