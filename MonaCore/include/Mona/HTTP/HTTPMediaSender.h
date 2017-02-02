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


namespace Mona {

struct HTTPMediaSender : HTTPSender, virtual Object {
	HTTPMediaSender(const shared<Socket>& pSocket,
		const shared<const HTTP::Header>& pRequest,
		shared<Buffer>& pSetCookie,
		shared<MediaWriter>& pWriter);

	bool hasHeader() const { return _first; }
protected:
	shared<MediaWriter>	pWriter;
	MediaWriter::OnWrite			onWrite;
private:
	void shutdown() {}
	void run(const HTTP::Header& request);
	bool _first;
};

template<typename MediaType>
struct HTTPMediaSend : HTTPMediaSender, MediaType, virtual Object {
	HTTPMediaSend(const shared<Socket>& pSocket, const shared<const HTTP::Header>& pRequest, shared<Buffer>& pSetCookie, shared<MediaWriter>& pWriter,
					UInt16 track, const typename MediaType::Tag& tag, const Packet& packet) : HTTPMediaSender(pSocket, pRequest, pSetCookie, pWriter), MediaType(track, tag, packet) {}
private:
	void run(const HTTP::Header& request) { pWriter->writeMedia(track, tag, *this, onWrite); }
};


} // namespace Mona
