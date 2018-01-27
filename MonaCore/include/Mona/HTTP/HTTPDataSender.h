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
	HTTPDataSender(const shared<const HTTP::Header>& pRequest,
		const char* code, MIME::Type mime=MIME::TYPE_UNKNOWN, const char* subMime=NULL) : _mime(mime), _code(code), _subMime(subMime), HTTPSender("HTTPDataSender", pRequest) {
		if (!mime || !subMime || !(_pWriter = Media::Data::NewWriter(Media::Data::ToType(subMime), buffer())))
			_pWriter = new StringWriter<>(buffer());
		else
			mime = MIME::TYPE_APPLICATION; // Fix mime => Media::Data::ToType success just for APPLICATION submime!
	}

	template <typename ...Args>
	HTTPDataSender(const shared<const HTTP::Header>& pRequest,
		const char* errorCode, Args&&... args) :
		_code(errorCode), _pWriter(NULL), _mime(MIME::TYPE_TEXT), _subMime("html; charset=utf-8"), HTTPSender("HTTPErrorSender", pRequest) {
		if (!_code)
			_code = HTTP_CODE_406;
		writeError(_code, std::forward<Args>(args)...);
	}

	virtual ~HTTPDataSender() { if (_pWriter) delete _pWriter; }


	DataWriter& writer() { return _pWriter ? *_pWriter : DataWriter::Null(); }

private:
	void run() { send(_code, _mime, _subMime); }

	const char*				_code;
	MIME::Type				_mime;
	const char*				_subMime;
	DataWriter*				_pWriter;
};


} // namespace Mona
