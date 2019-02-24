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

#include "Mona/HTTP/HTTPMediaSender.h"

using namespace std;


namespace Mona {

HTTPMediaSender::HTTPMediaSender(const shared<const HTTP::Header>& pRequest,
	const shared<Socket>& pSocket,
	shared<MediaWriter>& pWriter,
	Media::Base* pMedia) : HTTPSender("HTTPMediaSender", pRequest, pSocket), _pMedia(pMedia) {
	if ((_first = (pWriter ? false : true)))
		pWriter = MediaWriter::New(pRequest->subMime);
	_pWriter = pWriter;
}

HTTPMediaSender::~HTTPMediaSender() {
	if (_first || _pMedia)
		connection = HTTP::CONNECTION_KEEPALIVE;
}

void HTTPMediaSender::run() {
	MediaWriter::OnWrite onWrite([this](const Packet& packet) { send(packet); });
	if (_first) {
		// first packet streaming
		if (send(HTTP_CODE_200, _pWriter->mime(), _pWriter->subMime(), UINT64_MAX))
			_pWriter->beginMedia(onWrite);
		if(!_pMedia)
			return;
	} else if (!_pMedia)
		return _pWriter->endMedia(onWrite);

	return _pWriter->writeMedia(*_pMedia, onWrite);
}


} // namespace Mona
