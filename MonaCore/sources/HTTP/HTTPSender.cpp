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
#include "Mona/Session.h"

using namespace std;


namespace Mona {

bool HTTPSender::run(Exception&) {
	run(*_pRequest);
	if (_connection == HTTP::CONNECTION_CLOSE)
		shutdown();
	return true;
}

bool HTTPSender::send(const char* code, MIME::Type mime, const char* subMime, const Packet& packet, UInt64 extraSize) {
	// WRITE HEADER

	// COMPUTE SIZE
	if (packet) {
		const UInt8* current = packet.data();
		while (memcmp(current++, EXPAND("\r\n\r\n")) != 0);
		extraSize += packet.size() - (current - packet.data() + 3);
	}

	shared_ptr<Buffer> pBuffer(new Buffer());
	BinaryWriter writer(*pBuffer);

	/// First line (HTTP/1.1 200 OK)
	writer.write(EXPAND("HTTP/1.1 ")).write(code);

	/// Date + Mona
	string buffer;
	writer.write(EXPAND("\r\nDate: ")).write(Date().toString(Date::HTTP_FORMAT, buffer));
	writer.write(EXPAND("\r\nServer: Mona"));

	/// Content Type/length
	if (mime) {  // If no mime type, as "304 Not Modified" response, or HTTPWriter::writeRaw which write itself content-type, no content!
		MIME::Write(writer.write(EXPAND("\r\nContent-Type: ")), mime, subMime);
		if (!packet) {
			// no content-length
			writer.write(EXPAND("\r\nCache-Control: no-cache, no-store\r\nPragma: no-cache"));
			_connection = HTTP::CONNECTION_CLOSE; // write "connection: close" (session until end of socket)
		} else
			String::Append(writer.write(EXPAND("\r\nContent-Length: ")), extraSize);
	}

	/// Connection type, same than request!
	if (_connection&HTTP::CONNECTION_KEEPALIVE) {
		writer.write(EXPAND("\r\nConnection: keep-alive"));
		if (_connection&HTTP::CONNECTION_UPGRADE)
			writer.write(EXPAND(", upgrade"));
		if (_connection&HTTP::CONNECTION_CLOSE)
			writer.write(EXPAND(", close"));
	} else if (_connection&HTTP::CONNECTION_UPGRADE) {
		writer.write(EXPAND("\r\nConnection: upgrade"));
		if (_connection&HTTP::CONNECTION_CLOSE)
			writer.write(EXPAND(", close"));
	} else if (_connection&HTTP::CONNECTION_CLOSE)
		writer.write(EXPAND("\r\nConnection: close"));

	/// allow cross request, indeed if onConnection has not been rejected, every cross request are allowed
	if (_pRequest->origin && String::ICompare(_pRequest->origin, _pRequest->serverAddress) != 0)
		writer.write(EXPAND("\r\nAccess-Control-Allow-Origin: ")).write(_pRequest->origin);

	/// write Cookies line
	if (_pSetCookie)
		writer.write(*_pSetCookie);

	if (_pRequest->type == HTTP::TYPE_HEAD || !packet)
		writer.write(EXPAND("\r\n\r\n"));

	// SEND HEADER
	if (!send(Packet(pBuffer)))
		return false;

	// SEND CONTENT
	return _pRequest->type != HTTP::TYPE_HEAD && packet ? send(packet) : true;
}


} // namespace Mona
