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

#include "Mona/HTTP/HTTPSender.h"
#include "Mona/HTTP/HTTPFileSender.h"
#include "Mona/Session.h"

using namespace std;


namespace Mona {

bool HTTPSender::flushing() const {
	if (!_flushing)
		return false;
	if (!_pSocket)
		return false;
	return _pSocket->queueing() ? true : false;
}


void HTTPSender::finalize(bool flush) {
	_flushing = false;
	if (!_pSocket)
		return;
	if (_chunked) {
		static const Packet EndChunk(EXPAND("\r\n0\r\n\r\n"));
		if (_chunked < 2)
			socketSend(EndChunk + 2);
		else
			socketSend(EndChunk);
	}
	if (!connection) {
		// close the connection
		_pSocket->shutdown();
	} else if (flush) {
		// send the onFlush signal!
		Exception ex;
		_pSocket->flush(ex);
	}
	// finalize send!
	_pSocket = nullptr;
}

bool HTTPSender::isFile() const {
	return typeid(HTTPFileSender) == typeid(self);
}

Buffer& HTTPSender::buffer() {
	if (!_pBuffer)
		_pBuffer.set(EXPAND("\r\n\r\n"));
	return *_pBuffer;
}

void HTTPSender::setCookies(shared<Buffer>& pSetCookie) {
	if (!pSetCookie || !hasHeader())
		return;
	if (!_pBuffer)
		_pBuffer = move(pSetCookie);
	else
		_pBuffer->resize(_pBuffer->size() - 4, true).append(pSetCookie->data(), pSetCookie->size());
	_pBuffer->append(EXPAND("\r\n\r\n"));
}

bool HTTPSender::run(Exception& ex) {
	if (!_pSocket) {
		Socket::SetException(NET_ECONNABORTED, ex);
		return false;
	}
	// look if we are on a HTTPSender which must run just one time
	_flushing = false;
	bool flush = flushing();
	_flushing = true;  // reset _flushing to make flushable() usable in run to detect socket queueing
	run();
	if(!flushing())
		finalize(flush);
	return true;
}

bool HTTPSender::socketSend(const Packet& packet) {
	if (!_pSocket)
		return false;
	if (!packet) {
		WARN(_pSocket->isSecure() ? "HTTPS empty packet to " : "HTTP empty packet to ", _pSocket->peerAddress());
		return true;
	}
	Exception ex;
	DUMP_RESPONSE(_pSocket->isSecure() ? "HTTPS" : "HTTP", packet.data(), packet.size(), _pSocket->peerAddress());
	int result = _pSocket->write(ex, packet);
	if (ex || result < 0)
		DEBUG(ex);
	if (result >= 0)
		return true;
	// no shutdown required, already done by write!
	_pSocket = nullptr;
	return false;
}

bool HTTPSender::send(const Packet& content) {
	if (!_pSocket)
		return false;
	if (pRequest->type == HTTP::TYPE_HEAD)
		return true;
	if (_chunked) {
		shared<Buffer> pBuffer(SET);
		if (_chunked < 2)
			++_chunked;
		else
			pBuffer->append(EXPAND("\r\n")); // prefix
		String::Append(*pBuffer, String::Format<UInt32>("%X", content.size()), "\r\n");
		if (!socketSend(Packet(pBuffer)))
			return false;
	}
	return socketSend(content);
}

bool HTTPSender::send(const char* code, MIME::Type mime, const char* subMime, UInt64 extraSize) {
	if (!_pSocket)
		return false;

	// COMPUTE SIZE
	const UInt8* headerEnd = _pBuffer ? _pBuffer->data() : NULL;
	if (headerEnd) {
		while (memcmp(headerEnd++, EXPAND("\r\n\r\n")) != 0);
		headerEnd += 3;
		extraSize += _pBuffer->size() - (headerEnd - _pBuffer->data());
	}

	shared<Buffer> pBuffer(SET);
	/// First line (HTTP/1.1 200 OK)
	String::Append(*pBuffer, "HTTP/1.1 ", code);

	/// Date + Mona
	String::Append(*pBuffer, "\r\nDate: ", String::Date(Date::FORMAT_HTTP), "\r\nServer: Mona");

	/// Last modified
	const Path& path = this->path();
	if (path) {
		Int64 lastChange = path.lastChange();
		if(lastChange) // otherwise doesn't exist on the disk, always request a new version (virtual File? See HTTPSegmentSender for example)
			String::Append(*pBuffer, "\r\nLast-Modified: ", String::Date(lastChange, Date::FORMAT_HTTP));
	}

	/// Content Type/length
	_chunked = false;
	if (mime) {  // If no mime type as "304 Not Modified" response or HTTPWriter::writeRaw to write itself content-type => no content!
		if(pRequest->forceText) {
			// trick to visualize in browser application file
			// ex: "http://localhost/test.m3u8:" allows to see its content in the browser
			mime = MIME::TYPE_TEXT;
			subMime = "plain; charset=utf-8";
		}
		MIME::Write(String::Append(*pBuffer, "\r\nContent-Type: "), mime, subMime);
	}
	if (extraSize == UINT64_MAX) {
		if (path || !mime) {
			// Transfer-Encoding: chunked!
			pBuffer->append(EXPAND("\r\nTransfer-Encoding: chunked"));
			_chunked = true;
		} else {
			// live => no content-length + live attributes + close on end
			pBuffer->append(EXPAND("\r\n" HTTP_LIVE_HEADER));
			connection = HTTP::CONNECTION_CLOSE; // write "connection: close" (session until end of socket)
		}
	// no content-length for any Informational response OR 204 no content response OR 304 not modified response
	// see https://tools.ietf.org/html/rfc7230#section-3.3.2
	} else if(code[0]>'3' || (code[0]>'1' && (code[1]!='0' || code[2] != '4')))
		String::Append(*pBuffer, "\r\nContent-Length: ", extraSize);

	/// Connection type, same than request!
	if (connection&HTTP::CONNECTION_KEEPALIVE) {
		String::Append(*pBuffer, "\r\nConnection: keep-alive");
		if (connection&HTTP::CONNECTION_UPGRADE)
			String::Append(*pBuffer, ", upgrade");
	} else if (connection&HTTP::CONNECTION_UPGRADE)
		String::Append(*pBuffer, "\r\nConnection: upgrade");
	else
		String::Append(*pBuffer, "\r\nConnection: close");

	if (crossOriginIsolated) {
		if (String::ICompare(path.extension(), "html") == 0)
			String::Append(*pBuffer, "\r\nCross-Origin-Opener-Policy: same-origin");
		if (String::ICompare(path.extension(), "html") == 0 || String::ICompare(path.extension(), "js") == 0)
			String::Append(*pBuffer, "\r\nCross-Origin-Embedder-Policy: require-corp");
	}
	
	/// allow cross request, indeed if onConnection has not been rejected, every cross request are allowed
	if (pRequest->origin && String::ICompare(pRequest->origin, pRequest->host) != 0)
		String::Append(*pBuffer, "\r\nAccess-Control-Allow-Origin: ", pRequest->origin);

	if (headerEnd)
		return socketSend(Packet(pBuffer)) && socketSend(pRequest->type == HTTP::TYPE_HEAD ? Packet(_pBuffer, _pBuffer->data(), headerEnd - _pBuffer->data()) : Packet(_pBuffer));
	// no _pBuffer
	String::Append(*pBuffer, "\r\n\r\n");
	return socketSend(Packet(pBuffer));
}


} // namespace Mona
