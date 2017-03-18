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


struct HTTPSender : Runner, virtual Object {
	typedef Event<void()>	ON(Flush);

	virtual bool hasHeader() const { return true; }
	virtual bool flush() { return true; }

protected:
	HTTPSender(const char* name,
		const shared<Socket>& pSocket,
		const shared<const HTTP::Header>& pRequest,
		shared<Buffer>& pSetCookie) :
		_pRequest(pRequest), _pSetCookie(move(pSetCookie)), _pSocket(pSocket), _connection(pRequest->connection), Runner(name) {
	}

	bool send(const char* code, MIME::Type mime, const char* subMime, const Packet& packet, UInt64 extraSize=0);
	bool send(const Packet& packet) { return HTTP::Send(*_pSocket, packet); }
	bool send(const char* code) { return send(code, MIME::TYPE_UNKNOWN, NULL, Packet::Null()); }

	const shared<Socket>& socket() { return _pSocket; }
	virtual void shutdown() { _pSocket->shutdown(); }

protected:
	template <typename ...Args>
	bool sendError(const char* code, Args&&... args) {
		// override header
		shared<Buffer> pBuffer(new Buffer(4, "\r\n\r\n"));
		writeError(*pBuffer, code, std::forward<Args>(args)...);
		return send(code, MIME::TYPE_TEXT, "html; charset=utf-8", Packet(pBuffer));
	}

	template <typename ...Args>
	void writeError(Buffer& buffer, const char* code, Args&&... args) {
		BinaryWriter writer(buffer);
		HTML_BEGIN_COMMON_RESPONSE(writer, code)
			UInt32 size(writer.size());
			String::Append(writer, std::forward<Args>(args)...);
			if (size == writer.size()) // nothing has been written, write code in content!
				writer.write(code);
		HTML_END_COMMON_RESPONSE(_pRequest->host)
	}
private:
	virtual void run(const HTTP::Header& request) = 0;

	bool run(Exception&);

	shared<Socket>				_pSocket;
	UInt8						_connection;
	shared<const HTTP::Header>	_pRequest;
	shared<Buffer>				_pSetCookie;
};


} // namespace Mona
