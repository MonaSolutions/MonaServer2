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
#include "Mona/Socket.h"
#include "Mona/HTTP/HTTP.h"


namespace Mona {


/*!
HTTP send,
Send a HTTP response (or request? TODO) is used directly.
Children class are mainly used by call to TCPSender::send(pHTTPSender) */
struct HTTPSender : Runner, virtual Object {
	/*!
	If onEnd is set you must wait onEnd signal to release this HTTPSender,
	+ resend the HTTPSender on every socket.onFlush if !flushing() */
	typedef Event<void()>	ON(End);

	
	HTTPSender(const char* name,
		const shared<const HTTP::Header>& pRequest,
		const shared<Socket>& pSocket) : _end(false), pHandler(NULL), _chunked(0), _pSocket(pSocket), // hold socket to the sending (answer even if server falls)
		pRequest(pRequest), connection(pRequest->connection), Runner(name), crossOriginIsolated(false) {}
	virtual ~HTTPSender() {
		onEnd = nullptr; // useless to call onFlush, no more handle on this HTTPSender
		end();
	} 


	bool flushing() const { return pHandler && _pSocket->queueing() ? true : false; }

	bool isFile() const;
	

	virtual bool hasHeader() const { return true; }

	/*!
	Set pHandler on construction to works in asynchronous way:
	pause if socket queueing and wait socket.onFlush */
	const Handler*	pHandler;
	bool crossOriginIsolated;
	void setCookies(shared<Buffer>& pSetCookie);

	Buffer& buffer();

	/*!
	Send HTTP header
	If extraSize=UINT64_MAX + (path() || !mime): Transfer-Encoding: chunked
	If extraSize=UINT64_MAX + !path(): live streaming => no content-length, live attributes and close on end of response */
	bool send(const char* code, MIME::Type mime = MIME::TYPE_UNKNOWN, const char* subMime = NULL, UInt64 extraSize = 0);
	/*!
	Send HTTP body content */
	bool send(const Packet& content);
	/*!
	Finalize send */
	void end();

	template <typename ...Args>
	bool sendError(const char* code, Args&&... args) {
		writeError(code, std::forward<Args>(args)...);
		return send(code, MIME::TYPE_TEXT, "html; charset=utf-8");
	}
	
protected:

	const SocketAddress& peerAddress() { return _pSocket ? _pSocket->peerAddress() : SocketAddress::Wildcard(); }

	shared<const HTTP::Header> pRequest;
	UInt8					   connection;


	template <typename ...Args>
	void writeError(const char* code, Args&&... args) {
		HTML_BEGIN_COMMON_RESPONSE(buffer(), code)
			UInt32 size(__writer.size());
			String::Append(__writer, std::forward<Args>(args)...);
			if (size == __writer.size()) // nothing has been written, write code in content!
				String::Append(__writer, code);
		HTML_END_COMMON_RESPONSE(pRequest->host)
	}

private:
	virtual const Path& path() const { return Path::Null(); }

	bool socketSend(const Packet& packet);
	virtual bool run(Exception&);
	/*!
	must return end if finished, otherwise false */
	virtual bool run() { ERROR(name, " not runnable"); return true; }

	shared<Buffer>				_pBuffer;
	UInt8						_chunked;
	bool						_end;
	shared<Socket>			    _pSocket;
};


} // namespace Mona
