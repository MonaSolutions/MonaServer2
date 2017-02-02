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
#include "Mona/HTTP/HTTPSender.h"
#include "Mona/StringWriter.h"

namespace Mona {


struct HTTPDataSender : HTTPSender, virtual Object {
	HTTPDataSender(const shared<Socket>& pSocket,
		const shared<const HTTP::Header>& pRequest,
		shared<Buffer>& pSetCookie,
		const char* code, MIME::Type mime, const char* subMime) : _mime(mime), _pBuffer(new Buffer(4, "\r\n\r\n")), _code(code), _subMime(subMime), HTTPSender("HTTPDataSender", pSocket, pRequest, pSetCookie) {
		if (!mime || !subMime || !(_pWriter = Media::Data::NewWriter(Media::Data::ToType(subMime), *_pBuffer)))
			_pWriter = new StringWriter(*_pBuffer);
	}

	template <typename ...Args>
	HTTPDataSender(const shared<Socket>& pSocket,
		const shared<const HTTP::Header>& pRequest,
		shared<Buffer>& pSetCookie,
		const char* errorCode, Args&&... args) :
		_pBuffer(new Buffer(4, "\r\n\r\n")), _code(errorCode), _pWriter(NULL), _mime(MIME::TYPE_TEXT), _subMime("html; charset=utf-8"), HTTPSender("HTTPErrorSender", pSocket, pRequest, pSetCookie) {
		writeError(*_pBuffer, errorCode, std::forward<Args>(args)...);
	}

	virtual ~HTTPDataSender() { if (_pWriter) delete _pWriter; }

	bool			hasHeader() const { return true; }
	DataWriter&		writer() { return _pWriter ? *_pWriter : DataWriter::Null(); }

private:
	void run(const HTTP::Header& request) { send(_code, _mime, _subMime, Packet(_pBuffer)); }

	const char*				_code;
	MIME::Type				_mime;
	const char*				_subMime;

	shared<Buffer>			_pBuffer;
	DataWriter*				_pWriter;
};


} // namespace Mona
