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
	HTTPDataSender(const shared<const HTTP::Header>& pRequest, const shared<Socket>& pSocket,
		const char* code, MIME::Type mime=MIME::TYPE_UNKNOWN, const char* subMime=NULL, const Packet& packet = Packet::Null()) :
		_packet(std::move(packet)), _mime(mime), _code(code), _subMime(subMime), HTTPSender("HTTPDataSender", pRequest, pSocket) {
	}


	DataWriter& writer() {
		if (!_pWriter) {
			if (_mime && _subMime) {
				_pWriter = Media::Data::NewWriter(Media::Data::ToType(_subMime), buffer());
				if (_pWriter) {
					_mime = MIME::TYPE_APPLICATION; // Fix mime => Media::Data::ToType success just for APPLICATION submime!
					return *_pWriter;
				}
			}
			_pWriter.set<StringWriter<>>(buffer());
		}
		return *_pWriter;
	}

private:
	void run() {
		if(send(_code, _mime, _subMime, _packet.size()))
			send(_packet);
	}

	const char*				_code;
	MIME::Type				_mime;
	const char*				_subMime;
	unique<DataWriter>		_pWriter;
	Packet					_packet;
};


} // namespace Mona
