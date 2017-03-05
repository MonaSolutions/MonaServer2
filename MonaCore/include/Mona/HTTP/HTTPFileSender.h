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


struct HTTPFileSender : HTTPSender, FileReader, virtual Object {
	/*!
	File send */
	HTTPFileSender(const shared<Socket>& pSocket,
			   const shared<const HTTP::Header>& pRequest,
			   shared<Buffer>& pSetCookie,
				IOFile& ioFile, const Path& file, Parameters& properties);


	bool flush();

private:
	struct Decoder : File::Decoder {
		typedef Event<void()> ON(End);
		Decoder(const Handler& handler, const shared<Socket>& pSocket) : _pSocket(pSocket), _handler(handler) {}
	private:
		UInt32 decode(shared<Buffer>& pBuffer, bool end);
		const Handler& _handler;
		shared<Socket> _pSocket;
	};

	void  run(const HTTP::Header& request);
	bool  sendHeader(UInt64 fileSize);
	void  sendFile(const Packet& packet);
	
	shared<File::Decoder> newDecoder();

	Parameters				_properties;
	Path					_file;
	Path					_appPath;
	bool					_head;
};


} // namespace Mona
