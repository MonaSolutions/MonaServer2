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

/*!
File send,
- call io.subscribe(pFileSender, (File::Decoder*)pFileSender.get(), onFileReaden, onFileError)
- call io.read(pFileSender) to start file sending
- on pSocket.onFlush and if pFileSender.unique() recall io.read(pFileSender)
- on onFileReaden the file has been fully sent */
struct HTTPFileSender : HTTPSender, File, File::Decoder, virtual Object {
	NULLABLE(!HTTPSender::operator bool())
	
	HTTPFileSender(const shared<const HTTP::Header>& pRequest, const shared<Socket>& pSocket,
		const Path& file, Parameters& properties);

	const Path& path() const override { return self; }

private:
	void				run() override { ERROR(HTTPSender::name, " not runnable, read me with ioFile.read(pFileSender)"); }
	bool				load(Exception& ex);
	UInt32				decode(shared<Buffer>& pBuffer, bool end);
	const std::string*	search(char c);
	UInt32				generate(const Packet& packet, std::deque<Packet>& packets);

	Parameters				_properties;
	MIME::Type				_mime;
	const char*				_subMime;
	const char*				_protocol;

	// For search!
	Parameters::const_iterator	_result;
	UInt32						_pos;
	Int32						_step;
	UInt8						_stage;
};


} // namespace Mona
