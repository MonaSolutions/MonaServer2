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

HTTPMediaSender::HTTPMediaSender(const shared<Socket>& pSocket,
	const shared<const HTTP::Header>& pRequest,
	shared<Buffer>& pSetCookie,
	shared<MediaWriter>& pWriter) : HTTPSender("HTTPMediaSender", pSocket, pRequest, pSetCookie), _first(!pWriter),
	onWrite([this](const Packet& packet) { send(packet); }) {
	if (_first && pRequest->subMime)
		pWriter.reset(MediaWriter::New(pRequest->subMime));
	this->pWriter = pWriter;
}

void HTTPMediaSender::run(const HTTP::Header& request) {
	if (_first) {
		// first packet streaming
		if (send(HTTP_CODE_200, pWriter->mime(), pWriter->subMime(), Packet::Null()))
			pWriter->beginMedia(onWrite);
		return;
	}
	pWriter->endMedia(onWrite);
	HTTPSender::shutdown(); // no content-length => shutdown!
}



} // namespace Mona
